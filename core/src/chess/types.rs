use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Color {
    White = 0,
    Black = 1,
}

impl Color {
    pub const ALL: [Color; 2] = [Color::White, Color::Black];

    #[inline]
    pub fn opposite(self) -> Color {
        match self {
            Color::White => Color::Black,
            Color::Black => Color::White,
        }
    }

    #[inline]
    pub fn index(self) -> usize {
        self as usize
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Role {
    Pawn = 0,
    Knight = 1,
    Bishop = 2,
    Rook = 3,
    Queen = 4,
    King = 5,
}

impl Role {
    pub const ALL: [Role; 6] = [
        Role::Pawn,
        Role::Knight,
        Role::Bishop,
        Role::Rook,
        Role::Queen,
        Role::King,
    ];

    #[inline]
    pub fn index(self) -> usize {
        self as usize
    }

    /// Upper-case SAN letter; pawns have none.
    pub fn san_char(self) -> Option<char> {
        match self {
            Role::Pawn => None,
            Role::Knight => Some('N'),
            Role::Bishop => Some('B'),
            Role::Rook => Some('R'),
            Role::Queen => Some('Q'),
            Role::King => Some('K'),
        }
    }

    pub fn from_char(c: char) -> Option<Role> {
        match c.to_ascii_lowercase() {
            'p' => Some(Role::Pawn),
            'n' => Some(Role::Knight),
            'b' => Some(Role::Bishop),
            'r' => Some(Role::Rook),
            'q' => Some(Role::Queen),
            'k' => Some(Role::King),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Piece {
    pub color: Color,
    pub role: Role,
}

impl Piece {
    pub fn fen_char(self) -> char {
        let c = self.role.san_char().unwrap_or('P');
        match self.color {
            Color::White => c,
            Color::Black => c.to_ascii_lowercase(),
        }
    }

    pub fn from_fen_char(c: char) -> Option<Piece> {
        let role = Role::from_char(c)?;
        let color = if c.is_ascii_uppercase() {
            Color::White
        } else {
            Color::Black
        };
        Some(Piece { color, role })
    }
}

/// Square index, a1 = 0, b1 = 1, ..., h8 = 63.
#[derive(Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct Square(pub u8);

impl Square {
    #[inline]
    pub const fn new(file: u8, rank: u8) -> Square {
        Square(rank * 8 + file)
    }

    #[inline]
    pub const fn file(self) -> u8 {
        self.0 & 7
    }

    #[inline]
    pub const fn rank(self) -> u8 {
        self.0 >> 3
    }

    #[inline]
    pub const fn index(self) -> usize {
        self.0 as usize
    }

    #[inline]
    pub const fn bb(self) -> u64 {
        1u64 << self.0
    }

    pub fn parse(s: &str) -> Option<Square> {
        let b = s.as_bytes();
        if b.len() != 2 {
            return None;
        }
        Square::from_chars(b[0], b[1])
    }

    pub fn from_chars(file: u8, rank: u8) -> Option<Square> {
        if (b'a'..=b'h').contains(&file) && (b'1'..=b'8').contains(&rank) {
            Some(Square::new(file - b'a', rank - b'1'))
        } else {
            None
        }
    }

    pub fn file_char(self) -> char {
        (b'a' + self.file()) as char
    }

    pub fn rank_char(self) -> char {
        (b'1' + self.rank()) as char
    }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}{}", self.file_char(), self.rank_char())
    }
}

impl fmt::Debug for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}
