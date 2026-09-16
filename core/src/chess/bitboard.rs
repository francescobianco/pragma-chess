//! Precomputed attack tables. Sliding pieces use classical ray lookups,
//! which are simple and fast enough for import; magics can come later
//! if benchmarks ask for them.

pub const FILE_A: u64 = 0x0101_0101_0101_0101;
pub const FILE_H: u64 = FILE_A << 7;
pub const RANK_1: u64 = 0xff;
pub const RANK_2: u64 = RANK_1 << 8;
pub const RANK_7: u64 = RANK_1 << 48;
pub const RANK_8: u64 = RANK_1 << 56;

const fn leaper_table(deltas: &[(i8, i8)]) -> [u64; 64] {
    let mut table = [0u64; 64];
    let mut sq = 0;
    while sq < 64 {
        let file = (sq % 8) as i8;
        let rank = (sq / 8) as i8;
        let mut i = 0;
        while i < deltas.len() {
            let f = file + deltas[i].0;
            let r = rank + deltas[i].1;
            if f >= 0 && f < 8 && r >= 0 && r < 8 {
                table[sq] |= 1u64 << (r * 8 + f);
            }
            i += 1;
        }
        sq += 1;
    }
    table
}

pub static KNIGHT_ATTACKS: [u64; 64] = leaper_table(&[
    (1, 2),
    (2, 1),
    (2, -1),
    (1, -2),
    (-1, -2),
    (-2, -1),
    (-2, 1),
    (-1, 2),
]);

pub static KING_ATTACKS: [u64; 64] = leaper_table(&[
    (0, 1),
    (1, 1),
    (1, 0),
    (1, -1),
    (0, -1),
    (-1, -1),
    (-1, 0),
    (-1, 1),
]);

/// Squares attacked by a pawn of the given color standing on each square.
pub static PAWN_ATTACKS: [[u64; 64]; 2] = [
    leaper_table(&[(-1, 1), (1, 1)]),
    leaper_table(&[(-1, -1), (1, -1)]),
];

// Ray directions. The first four increase the square index, the last four decrease it.
const DIRECTIONS: [(i8, i8); 8] = [
    (0, 1),   // N
    (1, 0),   // E
    (1, 1),   // NE
    (-1, 1),  // NW
    (0, -1),  // S
    (-1, 0),  // W
    (-1, -1), // SW
    (1, -1),  // SE
];

const fn ray_tables() -> [[u64; 64]; 8] {
    let mut rays = [[0u64; 64]; 8];
    let mut d = 0;
    while d < 8 {
        let mut sq = 0;
        while sq < 64 {
            let mut f = (sq % 8) as i8 + DIRECTIONS[d].0;
            let mut r = (sq / 8) as i8 + DIRECTIONS[d].1;
            while f >= 0 && f < 8 && r >= 0 && r < 8 {
                rays[d][sq] |= 1u64 << (r * 8 + f);
                f += DIRECTIONS[d].0;
                r += DIRECTIONS[d].1;
            }
            sq += 1;
        }
        d += 1;
    }
    rays
}

static RAYS: [[u64; 64]; 8] = ray_tables();

#[inline]
fn ray_attacks(dir: usize, sq: usize, occupied: u64) -> u64 {
    let ray = RAYS[dir][sq];
    let blockers = ray & occupied;
    if blockers == 0 {
        return ray;
    }
    let first = if dir < 4 {
        blockers.trailing_zeros()
    } else {
        63 - blockers.leading_zeros()
    } as usize;
    ray ^ RAYS[dir][first]
}

#[inline]
pub fn rook_attacks(sq: usize, occupied: u64) -> u64 {
    ray_attacks(0, sq, occupied)
        | ray_attacks(1, sq, occupied)
        | ray_attacks(4, sq, occupied)
        | ray_attacks(5, sq, occupied)
}

#[inline]
pub fn bishop_attacks(sq: usize, occupied: u64) -> u64 {
    ray_attacks(2, sq, occupied)
        | ray_attacks(3, sq, occupied)
        | ray_attacks(6, sq, occupied)
        | ray_attacks(7, sq, occupied)
}

/// Iterates over the set squares of a bitboard.
#[derive(Clone, Copy)]
pub struct Bits(pub u64);

impl Iterator for Bits {
    type Item = u8;

    #[inline]
    fn next(&mut self) -> Option<u8> {
        if self.0 == 0 {
            None
        } else {
            let sq = self.0.trailing_zeros() as u8;
            self.0 &= self.0 - 1;
            Some(sq)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn leapers() {
        assert_eq!(KNIGHT_ATTACKS[0], (1 << 10) | (1 << 17));
        assert_eq!(KING_ATTACKS[0].count_ones(), 3);
        assert_eq!(PAWN_ATTACKS[0][8], 1 << 17);
        assert_eq!(PAWN_ATTACKS[1][9], (1 << 0) | (1 << 2));
    }

    #[test]
    fn sliders() {
        // Rook on a1, blocker on a4: a2, a3, a4 plus the whole first rank.
        let occ = 1u64 << 24;
        let att = rook_attacks(0, occ);
        assert_eq!(att, (1 << 8) | (1 << 16) | (1 << 24) | (RANK_1 & !1));
        // Bishop on d4 with empty board sees 13 squares.
        assert_eq!(bishop_attacks(27, 0).count_ones(), 13);
    }
}
