use super::bitboard::*;
use super::moves::{Move, MoveList};
use super::position::{CastlingRights, Position};
use super::types::{Color, Role, Square};

const PROMOTIONS: [Role; 4] = [Role::Queen, Role::Rook, Role::Bishop, Role::Knight];

impl Position {
    /// Generates pseudo-legal moves whose destination is in `targets`.
    pub fn pseudo_legal_moves(&self, targets: u64, out: &mut MoveList) {
        let us = self.turn;
        let own = self.color_bb(us);
        let them = self.color_bb(us.opposite());
        let occupied = own | them;
        let dest = targets & !own;

        for from in Bits(self.pieces(us, Role::Knight)) {
            push_all(out, from, KNIGHT_ATTACKS[from as usize] & dest);
        }
        let diagonal = self.pieces(us, Role::Bishop) | self.pieces(us, Role::Queen);
        for from in Bits(diagonal) {
            push_all(out, from, bishop_attacks(from as usize, occupied) & dest);
        }
        let straight = self.pieces(us, Role::Rook) | self.pieces(us, Role::Queen);
        for from in Bits(straight) {
            push_all(out, from, rook_attacks(from as usize, occupied) & dest);
        }
        if let Some(king) = self.king_square(us) {
            push_all(out, king.0, KING_ATTACKS[king.index()] & dest);
            self.castling_moves(king, targets, out);
        }
        self.pawn_moves(targets, out);
    }

    fn pawn_moves(&self, targets: u64, out: &mut MoveList) {
        let us = self.turn;
        let occupied = self.occupied();
        let them = self.color_bb(us.opposite());
        let ep_bb = self.ep_square.map_or(0, |sq| sq.bb());
        let (push, start_rank, last_rank): (i8, u64, u64) = match us {
            Color::White => (8, RANK_2, RANK_8),
            Color::Black => (-8, RANK_7, RANK_1),
        };

        for from in Bits(self.pieces(us, Role::Pawn)) {
            let from_bb = 1u64 << from;
            let one = (from as i8 + push) as u8;
            if occupied & (1u64 << one) == 0 {
                push_pawn(out, from, one, targets, last_rank);
                if from_bb & start_rank != 0 {
                    let two = (one as i8 + push) as u8;
                    if occupied & (1u64 << two) == 0 && targets & (1u64 << two) != 0 {
                        out.push(Move::new(Square(from), Square(two)));
                    }
                }
            }
            let captures = PAWN_ATTACKS[us.index()][from as usize] & (them | ep_bb);
            for to in Bits(captures) {
                push_pawn(out, from, to, targets, last_rank);
            }
        }
    }

    fn castling_moves(&self, king: Square, targets: u64, out: &mut MoveList) {
        let us = self.turn;
        let (king_side, queen_side, rank) = match us {
            Color::White => (CastlingRights::WHITE_KING, CastlingRights::WHITE_QUEEN, 0),
            Color::Black => (CastlingRights::BLACK_KING, CastlingRights::BLACK_QUEEN, 7),
        };
        if king != Square::new(4, rank) || self.castling.0 & (king_side | queen_side) == 0 {
            return;
        }
        let occupied = self.occupied();
        let enemy = us.opposite();
        let rooks = self.pieces(us, Role::Rook);
        let safe = |files: &[u8]| {
            files
                .iter()
                .all(|&f| !self.is_attacked(Square::new(f, rank), enemy, occupied))
        };
        let empty = |files: &[u8]| {
            files
                .iter()
                .all(|&f| occupied & Square::new(f, rank).bb() == 0)
        };

        let g = Square::new(6, rank);
        if self.castling.has(king_side)
            && targets & g.bb() != 0
            && rooks & Square::new(7, rank).bb() != 0
            && empty(&[5, 6])
            && safe(&[4, 5, 6])
        {
            out.push(Move::new(king, g));
        }
        let c = Square::new(2, rank);
        if self.castling.has(queen_side)
            && targets & c.bb() != 0
            && rooks & Square::new(0, rank).bb() != 0
            && empty(&[1, 2, 3])
            && safe(&[4, 3, 2])
        {
            out.push(Move::new(king, c));
        }
    }

    /// All legal moves.
    pub fn legal_moves(&self) -> MoveList {
        let mut moves = MoveList::new();
        self.pseudo_legal_moves(!0, &mut moves);
        moves.retain(|m| !self.leaves_king_in_check(m));
        moves
    }

    pub fn is_checkmate(&self) -> bool {
        self.in_check() && self.legal_moves().is_empty()
    }

    pub fn is_stalemate(&self) -> bool {
        !self.in_check() && self.legal_moves().is_empty()
    }

    /// Counts leaf nodes of the legal move tree; used to validate move generation.
    pub fn perft(&self, depth: u32) -> u64 {
        let moves = self.legal_moves();
        if depth <= 1 {
            return if depth == 0 { 1 } else { moves.len() as u64 };
        }
        moves
            .as_slice()
            .iter()
            .map(|&m| {
                let mut next = *self;
                next.play_unchecked(m);
                next.perft(depth - 1)
            })
            .sum()
    }
}

#[inline]
fn push_all(out: &mut MoveList, from: u8, targets: u64) {
    for to in Bits(targets) {
        out.push(Move::new(Square(from), Square(to)));
    }
}

#[inline]
fn push_pawn(out: &mut MoveList, from: u8, to: u8, targets: u64, last_rank: u64) {
    let to_bb = 1u64 << to;
    if targets & to_bb == 0 {
        return;
    }
    if to_bb & last_rank != 0 {
        for role in PROMOTIONS {
            out.push(Move {
                from: Square(from),
                to: Square(to),
                promotion: Some(role),
            });
        }
    } else {
        out.push(Move::new(Square(from), Square(to)));
    }
}
