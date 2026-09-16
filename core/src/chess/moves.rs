use super::types::{Role, Square};
use std::fmt;

/// A chess move. Castling is encoded as the king moving two squares.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct Move {
    pub from: Square,
    pub to: Square,
    pub promotion: Option<Role>,
}

impl Move {
    pub const fn new(from: Square, to: Square) -> Move {
        Move {
            from,
            to,
            promotion: None,
        }
    }

    /// Compact 16-bit encoding used by the storage layer:
    /// bits 0-5 from, 6-11 to, 12-14 promotion (0 none, 1 N, 2 B, 3 R, 4 Q).
    /// A valid move never encodes to 0 because from != to.
    #[inline]
    pub fn encode(self) -> u16 {
        let promo = match self.promotion {
            None => 0,
            Some(role) => role.index() as u16,
        };
        self.from.0 as u16 | (self.to.0 as u16) << 6 | promo << 12
    }

    #[inline]
    pub fn decode(bits: u16) -> Option<Move> {
        let from = Square((bits & 63) as u8);
        let to = Square(((bits >> 6) & 63) as u8);
        if from == to {
            return None;
        }
        let promotion = match (bits >> 12) & 7 {
            0 => None,
            1 => Some(Role::Knight),
            2 => Some(Role::Bishop),
            3 => Some(Role::Rook),
            4 => Some(Role::Queen),
            _ => return None,
        };
        Some(Move {
            from,
            to,
            promotion,
        })
    }

    /// UCI long algebraic notation, e.g. `e2e4`, `e7e8q`.
    pub fn to_uci(self) -> String {
        self.to_string()
    }

    pub fn from_uci(s: &str) -> Option<Move> {
        let b = s.as_bytes();
        if b.len() != 4 && b.len() != 5 {
            return None;
        }
        let from = Square::from_chars(b[0], b[1])?;
        let to = Square::from_chars(b[2], b[3])?;
        let promotion = match b.get(4) {
            None => None,
            Some(&c) => match Role::from_char(c as char)? {
                r @ (Role::Knight | Role::Bishop | Role::Rook | Role::Queen) => Some(r),
                _ => return None,
            },
        };
        Some(Move {
            from,
            to,
            promotion,
        })
    }
}

impl fmt::Display for Move {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}{}", self.from, self.to)?;
        if let Some(role) = self.promotion {
            write!(f, "{}", role.san_char().unwrap().to_ascii_lowercase())?;
        }
        Ok(())
    }
}

impl fmt::Debug for Move {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

/// Fixed-capacity move buffer, avoids heap allocation during generation.
pub struct MoveList {
    moves: [Move; 256],
    len: usize,
}

impl MoveList {
    pub fn new() -> MoveList {
        MoveList {
            moves: [Move::new(Square(0), Square(0)); 256],
            len: 0,
        }
    }

    #[inline]
    pub fn push(&mut self, m: Move) {
        self.moves[self.len] = m;
        self.len += 1;
    }

    #[inline]
    pub fn as_slice(&self) -> &[Move] {
        &self.moves[..self.len]
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn retain(&mut self, mut keep: impl FnMut(Move) -> bool) {
        let mut j = 0;
        for i in 0..self.len {
            if keep(self.moves[i]) {
                self.moves[j] = self.moves[i];
                j += 1;
            }
        }
        self.len = j;
    }
}

impl Default for MoveList {
    fn default() -> Self {
        MoveList::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encoding_roundtrip() {
        for s in ["e2e4", "a7a8q", "h2h1n", "e1g1", "b7c8r"] {
            let m = Move::from_uci(s).unwrap();
            assert_ne!(m.encode(), 0);
            assert_eq!(Move::decode(m.encode()), Some(m));
            assert_eq!(m.to_uci(), s);
        }
        assert_eq!(Move::decode(0), None);
    }
}
