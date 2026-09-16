use super::position::{CastlingRights, Position};
use super::types::{Color, Piece, Role, Square};
use super::ChessError;

pub const STARTING_FEN: &str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

impl Position {
    /// Parses a FEN string. Move clocks are optional.
    pub fn from_fen(fen: &str) -> Result<Position, ChessError> {
        let err = |msg: &str| ChessError(format!("invalid FEN '{fen}': {msg}"));
        let mut fields = fen.split_whitespace();
        let board = fields.next().ok_or_else(|| err("empty"))?;
        let mut pos = Position::empty();

        let ranks: Vec<&str> = board.split('/').collect();
        if ranks.len() != 8 {
            return Err(err("board must have 8 ranks"));
        }
        for (i, row) in ranks.iter().enumerate() {
            let rank = 7 - i as u8;
            let mut file = 0u8;
            for c in row.chars() {
                if let Some(d) = c.to_digit(10) {
                    file += d as u8;
                } else {
                    let piece = Piece::from_fen_char(c).ok_or_else(|| err("bad piece"))?;
                    if file >= 8 {
                        return Err(err("rank too long"));
                    }
                    pos.put(Square::new(file, rank), piece);
                    file += 1;
                }
            }
            if file != 8 {
                return Err(err("rank has wrong length"));
            }
        }

        pos.turn = match fields.next() {
            None | Some("w") => Color::White,
            Some("b") => Color::Black,
            Some(_) => return Err(err("bad side to move")),
        };

        if let Some(castling) = fields.next() {
            for c in castling.chars() {
                pos.castling.0 |= match c {
                    'K' => CastlingRights::WHITE_KING,
                    'Q' => CastlingRights::WHITE_QUEEN,
                    'k' => CastlingRights::BLACK_KING,
                    'q' => CastlingRights::BLACK_QUEEN,
                    '-' => 0,
                    _ => return Err(err("bad castling rights")),
                };
            }
        }

        if let Some(ep) = fields.next() {
            if ep != "-" {
                pos.ep_square = Some(Square::parse(ep).ok_or_else(|| err("bad en passant"))?);
            }
        }
        if let Some(n) = fields.next() {
            pos.halfmove_clock = n.parse().map_err(|_| err("bad halfmove clock"))?;
        }
        if let Some(n) = fields.next() {
            pos.fullmove_number = n.parse().map_err(|_| err("bad fullmove number"))?;
        }

        for color in Color::ALL {
            if pos.pieces(color, Role::King).count_ones() != 1 {
                return Err(err("each side needs exactly one king"));
            }
        }
        let opponent_king = pos.king_square(pos.turn.opposite()).unwrap();
        if pos.is_attacked(opponent_king, pos.turn, pos.occupied()) {
            return Err(err("side not to move is in check"));
        }
        Ok(pos)
    }

    pub fn to_fen(&self) -> String {
        let mut fen = String::with_capacity(90);
        for rank in (0..8).rev() {
            let mut empty = 0;
            for file in 0..8 {
                match self.piece_at(Square::new(file, rank)) {
                    Some(p) => {
                        if empty > 0 {
                            fen.push(char::from_digit(empty, 10).unwrap());
                            empty = 0;
                        }
                        fen.push(p.fen_char());
                    }
                    None => empty += 1,
                }
            }
            if empty > 0 {
                fen.push(char::from_digit(empty, 10).unwrap());
            }
            if rank > 0 {
                fen.push('/');
            }
        }
        fen.push_str(if self.turn == Color::White { " w " } else { " b " });
        let c = self.castling;
        if c.0 == 0 {
            fen.push('-');
        }
        for (flag, ch) in [
            (CastlingRights::WHITE_KING, 'K'),
            (CastlingRights::WHITE_QUEEN, 'Q'),
            (CastlingRights::BLACK_KING, 'k'),
            (CastlingRights::BLACK_QUEEN, 'q'),
        ] {
            if c.has(flag) {
                fen.push(ch);
            }
        }
        match self.ep_square {
            Some(sq) => fen.push_str(&format!(" {sq}")),
            None => fen.push_str(" -"),
        }
        fen.push_str(&format!(" {} {}", self.halfmove_clock, self.fullmove_number));
        fen
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn startpos_roundtrip() {
        let pos = Position::from_fen(STARTING_FEN).unwrap();
        assert_eq!(pos, Position::startpos());
        assert_eq!(pos.to_fen(), STARTING_FEN);
    }

    #[test]
    fn rejects_garbage() {
        assert!(Position::from_fen("").is_err());
        assert!(Position::from_fen("8/8/8/8/8/8/8/8 w - - 0 1").is_err());
        assert!(Position::from_fen("rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -").is_err());
    }
}
