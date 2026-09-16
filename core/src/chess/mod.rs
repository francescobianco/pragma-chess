//! Chess model: squares, pieces, bitboards, positions, moves, SAN and hashing.
//!
//! This layer is used on the hot path of import and search, so it avoids
//! allocations: positions are small `Copy` values backed by bitboards.

pub mod bitboard;
pub mod fen;
pub mod movegen;
pub mod moves;
pub mod position;
pub mod san;
pub mod types;
pub mod zobrist;

pub use moves::Move;
pub use position::{CastlingRights, Position};
pub use types::{Color, Piece, Role, Square};

use std::fmt;

/// Error produced when a FEN, SAN or move cannot be applied to a position.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ChessError(pub String);

impl fmt::Display for ChessError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for ChessError {}
