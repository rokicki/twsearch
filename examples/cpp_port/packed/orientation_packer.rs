#![cfg(feature = "orientation_packer")]

use super::byte_conversions::{u8_to_usize, usize_to_u8, PackedOrientationWithMod};

const NUM_BYTE_VALUES: usize = 0x100;
const BOGUS_PACKED_VALUE: PackedOrientationWithMod = 0xFF;
// TODO: Avoid using this to hardcode the outer size of `transformation_lookup`.
// Setting `MAX_NUM_ORIENTATIONS` is usually way larger than necessary, although
// the wasted space is only ≈25KB per orbit. Ideally, we should allow
// `transformation_lookup` to be much smaller by using another direct allocation
// (without taking the performance hit of `Vec`, which is noticeable in this
// situation).
const MAX_NUM_ORIENTATIONS: usize = 107;

#[derive(Debug)]
pub struct OrientationWithMod {
    pub orientation: usize,
    pub orientation_mod: usize,
}

const BOGUS_ORIENTATION_WITH_MOD: OrientationWithMod = OrientationWithMod {
    orientation: 0xFE,
    orientation_mod: 0xFD,
};

#[derive(Debug)]
pub struct OrientationPacker {
    // from `[orientation delta][old PackedValue]` to new `PackedValue`
    // Dense for each array up the number of valid `OrientationWithMod` values.
    transformation_lookup: [[PackedOrientationWithMod; NUM_BYTE_VALUES]; MAX_NUM_ORIENTATIONS],
    // from `[PackedValue]` to `OrientationWithMod`
    // Dense for each array up the number of valid `OrientationWithMod` values.
    unpacking_table: [OrientationWithMod; NUM_BYTE_VALUES],
    // From `[orientation_mod]` to the `PackedValue` of `(orientation_mod, 0)`
    // Sparse — only for valid `orientation_mod` values.
    packing_table: [PackedOrientationWithMod; NUM_BYTE_VALUES],
}

/// For a given `num_orientations`, an orbit has a limited set of valid
/// (orientation_mod, orientation pairs). For example, an orbit with 6
/// orientations has:
///
/// - (1, 0) ↔️ 0
/// - (2, 0) ↔️ 1
/// - (2, 1) ↔️ 2
/// - (3, 0) ↔️ 3
/// - (3, 1) ↔️ 4
/// - (3, 2) ↔️ 5
/// - (0, 0) ↔️ 6
/// - (0, 1) ↔️ 7
/// - (0, 2) ↔️ 8
/// - (0, 3) ↔️ 9
/// - (0, 4) ↔️ 10
/// - (0, 5) ↔️ 11
///
/// `OrientationPacker` can translate between these representations,
/// as well as applying a transformation to the packed representation
/// efficiently. This replaces arithmetic with simple lookups for `PackedKState` logic.

impl OrientationPacker {
    pub fn new(num_orientations: usize) -> Self {
        let mut unpacking_table: [OrientationWithMod; NUM_BYTE_VALUES] =
            [BOGUS_ORIENTATION_WITH_MOD; NUM_BYTE_VALUES];
        let mut packing_table = [BOGUS_PACKED_VALUE; NUM_BYTE_VALUES];

        let mut num_packed_values_sofar: u8 = 0;

        // Ignore an idiom suggestion by Clippy that doesn't work here (because we use `orientation_mod` as a value, not just as an index into `packing_table`).
        #[allow(clippy::needless_range_loop)]
        for orientation_mod in 0..NUM_BYTE_VALUES {
            let factor = if orientation_mod == 0 {
                num_orientations
            } else {
                orientation_mod
            };
            if num_orientations % factor != 0 {
                continue;
            }
            packing_table[orientation_mod] = num_packed_values_sofar; // Note: this is sparse, so we only assign once per `orientation_mod`, not once per packed value.
            for orientation in 0..factor {
                unpacking_table[u8_to_usize(num_packed_values_sofar)] = OrientationWithMod {
                    orientation,
                    orientation_mod,
                };
                num_packed_values_sofar += 1;
            }
        }

        let mut transformation_lookup: [[u8; NUM_BYTE_VALUES]; MAX_NUM_ORIENTATIONS] =
            [[BOGUS_PACKED_VALUE; NUM_BYTE_VALUES]; MAX_NUM_ORIENTATIONS];
        // Ignore an idiom suggestion by Clippy that doesn't work here (because we use `orientation_mod` as a value, not just as an index into `packing_table`).
        #[allow(clippy::needless_range_loop)]
        for orientation_delta in 0..num_orientations {
            for packed_value in 0..num_packed_values_sofar {
                let orientation_with_mod = &unpacking_table[u8_to_usize(packed_value)];
                let new_orientation = (orientation_with_mod.orientation + orientation_delta)
                    % if orientation_with_mod.orientation_mod == 0 {
                        num_orientations
                    } else {
                        orientation_with_mod.orientation_mod
                    };
                transformation_lookup[orientation_delta][u8_to_usize(packed_value)] = packing_table
                    [orientation_with_mod.orientation_mod]
                    + usize_to_u8(new_orientation)
            }
        }

        Self {
            transformation_lookup,
            unpacking_table,
            packing_table,
        }
    }

    pub fn transform(
        &self,
        packed_value: PackedOrientationWithMod,
        orientation_delta: usize,
    ) -> PackedOrientationWithMod {
        self.transformation_lookup[orientation_delta][u8_to_usize(packed_value)]
    }

    #[allow(dead_code)]
    pub fn unpack(&self, packed_value: PackedOrientationWithMod) -> &OrientationWithMod {
        &self.unpacking_table[u8_to_usize(packed_value)]
    }

    pub fn pack(&self, orientation_with_mod: OrientationWithMod) -> PackedOrientationWithMod {
        self.packing_table[orientation_with_mod.orientation_mod]
            + usize_to_u8(orientation_with_mod.orientation)
    }
}
