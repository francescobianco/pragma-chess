use super::bitboard::*;
use super::moves::Move;
use super::types::{Color, Piece, Role, Square};
use super::ChessError;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct CastlingRights(pub u8);

impl CastlingRights {
    pub const WHITE_KING: u8 = 1;
    pub const WHITE_QUEEN: u8 = 2;
    pub const BLACK_KING: u8 = 4;
    pub const BLACK_QUEEN: u8 = 8;
    pub const ALL: CastlingRights = CastlingRights(15);

    #[inline]
    pub fn has(self, flag: u8) -> bool {
        self.0 & flag != 0
    }
}

/// Castling rights that survive a move touching the given square.
const fn castling_masks() -> [u8; 64] {
    let mut masks = [15u8; 64];
    masks[0] = 15 & !CastlingRights::WHITE_QUEEN; // a1
    masks[7] = 15 & !CastlingRights::WHITE_KING; // h1
    masks[4] = 15 & !(CastlingRights::WHITE_KING | CastlingRights::WHITE_QUEEN); // e1
    masks[56] = 15 & !CastlingRights::BLACK_QUEEN; // a8
    masks[63] = 15 & !CastlingRights::BLACK_KING; // h8
    masks[60] = 15 & !(CastlingRights::BLACK_KING | CastlingRights::BLACK_QUEEN); // e8
    masks
}

static CASTLING_MASKS: [u8; 64] = castling_masks();

/// A chess position backed by bitboards.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Position {
    pub(crate) by_color: [u64; 2],
    pub(crate) by_role: [u64; 6],
    pub(crate) turn: Color,
    pub(crate) castling: CastlingRights,
    /// En passant target square as it would appear in FEN, set after any double push.
    pub(crate) ep_square: Option<Square>,
    pub(crate) halfmove_clock: u16,
    pub(crate) fullmove_number: u16,
}

impl Default for Position {
    fn default() -> Self {
        Position::startpos()
    }
}

impl Position {
    pub fn empty() -> Position {
        Position {
            by_color: [0; 2],
            by_role: [0; 6],
            turn: Color::White,
            castling: CastlingRights(0),
            ep_square: None,
            halfmove_clock: 0,
            fullmove_number: 1,
        }
    }

    pub fn startpos() -> Position {
        let mut p = Position::empty();
        let back = [
            Role::Rook,
            Role::Knight,
            Role::Bishop,
            Role::Queen,
            Role::King,
            Role::Bishop,
            Role::Knight,
            Role::Rook,
        ];
        for (file, &role) in back.iter().enumerate() {
            let f = file as u8;
            p.put(Square::new(f, 0), Piece { color: Color::White, role });
            p.put(Square::new(f, 1), Piece { color: Color::White, role: Role::Pawn });
            p.put(Square::new(f, 6), Piece { color: Color::Black, role: Role::Pawn });
            p.put(Square::new(f, 7), Piece { color: Color::Black, role });
        }
        p.castling = CastlingRights::ALL;
        p
    }

    #[inline]
    pub fn turn(&self) -> Color {
        self.turn
    }

    #[inline]
    pub fn castling(&self) -> CastlingRights {
        self.castling
    }

    #[inline]
    pub fn ep_square(&self) -> Option<Square> {
        self.ep_square
    }

    pub fn halfmove_clock(&self) -> u16 {
        self.halfmove_clock
    }

    pub fn fullmove_number(&self) -> u16 {
        self.fullmove_number
    }

    #[inline]
    pub fn occupied(&self) -> u64 {
        self.by_color[0] | self.by_color[1]
    }

    #[inline]
    pub fn color_bb(&self, color: Color) -> u64 {
        self.by_color[color.index()]
    }

    #[inline]
    pub fn role_bb(&self, role: Role) -> u64 {
        self.by_role[role.index()]
    }

    #[inline]
    pub fn pieces(&self, color: Color, role: Role) -> u64 {
        self.by_color[color.index()] & self.by_role[role.index()]
    }

    pub fn piece_at(&self, sq: Square) -> Option<Piece> {
        let bb = sq.bb();
        let color = if self.by_color[0] & bb != 0 {
            Color::White
        } else if self.by_color[1] & bb != 0 {
            Color::Black
        } else {
            return None;
        };
        let role = Role::ALL
            .into_iter()
            .find(|r| self.by_role[r.index()] & bb != 0)?;
        Some(Piece { color, role })
    }

    #[inline]
    pub(crate) fn role_at(&self, sq: Square) -> Option<Role> {
        let bb = sq.bb();
        if self.occupied() & bb == 0 {
            return None;
        }
        Role::ALL
            .into_iter()
            .find(|r| self.by_role[r.index()] & bb != 0)
    }

    pub(crate) fn put(&mut self, sq: Square, piece: Piece) {
        let bb = sq.bb();
        self.by_color[piece.color.index()] |= bb;
        self.by_role[piece.role.index()] |= bb;
    }

    pub(crate) fn clear(&mut self, sq: Square) {
        let mask = !sq.bb();
        for bb in self.by_color.iter_mut() {
            *bb &= mask;
        }
        for bb in self.by_role.iter_mut() {
            *bb &= mask;
        }
    }

    pub fn king_square(&self, color: Color) -> Option<Square> {
        let kings = self.pieces(color, Role::King);
        (kings != 0).then(|| Square(kings.trailing_zeros() as u8))
    }

    /// Whether `sq` is attacked by any piece of color `by`, given an occupancy.
    #[inline]
    pub fn is_attacked(&self, sq: Square, by: Color, occupied: u64) -> bool {
        let s = sq.index();
        let them = self.by_color[by.index()];
        let queens = self.by_role[Role::Queen.index()];
        PAWN_ATTACKS[by.opposite().index()][s] & them & self.by_role[Role::Pawn.index()] != 0
            || KNIGHT_ATTACKS[s] & them & self.by_role[Role::Knight.index()] != 0
            || KING_ATTACKS[s] & them & self.by_role[Role::King.index()] != 0
            || bishop_attacks(s, occupied)
                & them
                & (self.by_role[Role::Bishop.index()] | queens)
                != 0
            || rook_attacks(s, occupied) & them & (self.by_role[Role::Rook.index()] | queens) != 0
    }

    pub fn in_check(&self) -> bool {
        match self.king_square(self.turn) {
            Some(k) => self.is_attacked(k, self.turn.opposite(), self.occupied()),
            None => false,
        }
    }

    /// Applies a move without checking legality. The move must at least be
    /// pseudo-legal (a piece of the side to move stands on `from`).
    pub fn play_unchecked(&mut self, m: Move) {
        let us = self.turn;
        let from = m.from;
        let to = m.to;
        let role = self.role_at(from).expect("play_unchecked: empty from square");
        let captured = self.role_at(to);

        self.halfmove_clock += 1;
        if role == Role::Pawn || captured.is_some() {
            self.halfmove_clock = 0;
        }

        if captured.is_some() {
            self.clear(to);
        } else if role == Role::Pawn && Some(to) == self.ep_square && from.file() != to.file() {
            let victim = match us {
                Color::White => Square(to.0 - 8),
                Color::Black => Square(to.0 + 8),
            };
            self.clear(victim);
        }

        self.clear(from);
        self.put(
            to,
            Piece {
                color: us,
                role: m.promotion.unwrap_or(role),
            },
        );

        if role == Role::King && from.0.abs_diff(to.0) == 2 {
            let rank = from.rank();
            let (rook_from, rook_to) = if to.file() == 6 {
                (Square::new(7, rank), Square::new(5, rank))
            } else {
                (Square::new(0, rank), Square::new(3, rank))
            };
            self.clear(rook_from);
            self.put(rook_to, Piece { color: us, role: Role::Rook });
        }

        self.castling.0 &= CASTLING_MASKS[from.index()] & CASTLING_MASKS[to.index()];

        self.ep_square = if role == Role::Pawn && from.0.abs_diff(to.0) == 16 {
            Some(Square((from.0 + to.0) / 2))
        } else {
            None
        };

        if us == Color::Black {
            self.fullmove_number += 1;
        }
        self.turn = us.opposite();
    }

    /// Plays a move after verifying it is legal.
    pub fn play(&mut self, m: Move) -> Result<(), ChessError> {
        if !self.is_legal(m) {
            return Err(ChessError(format!("illegal move {m}")));
        }
        self.play_unchecked(m);
        Ok(())
    }

    pub fn is_legal(&self, m: Move) -> bool {
        let mut moves = super::moves::MoveList::new();
        self.pseudo_legal_moves(m.to.bb(), &mut moves);
        moves.as_slice().contains(&m) && !self.leaves_king_in_check(m)
    }

    /// Whether playing a pseudo-legal move exposes the mover's king.
    #[inline]
    pub(crate) fn leaves_king_in_check(&self, m: Move) -> bool {
        let mut after = *self;
        after.play_unchecked(m);
        match after.king_square(self.turn) {
            Some(k) => after.is_attacked(k, self.turn.opposite(), after.occupied()),
            None => false,
        }
    }

    /// The en passant square only if the side to move has a pawn able to
    /// capture on it. Positions that differ only by an unusable en passant
    /// square are the same position for search purposes.
    pub fn capturable_ep_square(&self) -> Option<Square> {
        let ep = self.ep_square?;
        let attackers = PAWN_ATTACKS[self.turn.opposite().index()][ep.index()]
            & self.pieces(self.turn, Role::Pawn);
        (attackers != 0).then_some(ep)
    }

    /// Equality of board, side to move, castling rights and usable en passant,
    /// ignoring move clocks.
    pub fn same_position(&self, other: &Position) -> bool {
        self.by_color == other.by_color
            && self.by_role == other.by_role
            && self.turn == other.turn
            && self.castling == other.castling
            && self.capturable_ep_square() == other.capturable_ep_square()
    }
}
