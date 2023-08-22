use crate::{
    get_packed_orientation, get_packed_piece_or_permutation, set_packed_orientation,
    set_packed_piece_or_permutation, set_packed_piece_or_permutation_and_orientation,
};

use super::{PackedKPuzzle, PackedKTransformation};

pub struct PackedKState {
    // pub packed_kpuzzle: PackedKPuzzle,
    pub bytes: [u8; 52],
}

impl PackedKState {
    // Adapted from https://github.com/cubing/cubing.rs/blob/b737c6a36528e9984b45b29f9449a9a330c272fb/src/kpuzzle/state.rs#L31-L82
    // TODO: dedup the implementation (but avoid runtime overhead for the shared abstraction).
    pub fn apply_transformation(
        &self,
        packed_kpuzzle: &PackedKPuzzle,
        transformation: &PackedKTransformation,
    ) -> PackedKState {
        let mut bytes: [u8; 52] = [0; 52];
        for orbit_info in &packed_kpuzzle.data.orbit_iteration_info {
            // TODO: optimization when either value is the identity.
            for i in 0..orbit_info.num_pieces {
                let transformation_idx = std::convert::Into::<usize>::into(
                    get_packed_piece_or_permutation!(transformation.bytes, orbit_info, i),
                );

                let new_piece_permutation =
                    get_packed_piece_or_permutation!(self.bytes, orbit_info, transformation_idx);
                let previous_piece_orientation =
                    get_packed_orientation!(self.bytes, orbit_info, transformation_idx);
                let new_piece_orientation = orbit_info.table[std::convert::Into::<usize>::into(
                    previous_piece_orientation
                        + get_packed_orientation!(transformation.bytes, orbit_info, i),
                )];
                set_packed_piece_or_permutation_and_orientation!(
                    bytes,
                    orbit_info,
                    i,
                    new_piece_permutation,
                    new_piece_orientation
                );
            }
        }

        PackedKState { bytes }
    }
}
