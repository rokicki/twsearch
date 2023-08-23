use std::alloc::{alloc, dealloc};

use super::{packed_kpuzzle::PackedKPuzzleOrbitInfo, PackedKPuzzle, PackedKTransformation};

pub struct PackedKState {
    pub packed_kpuzzle: PackedKPuzzle,
    // pub bytes: [u8; 52],
    pub bytes: *mut u8,
}

impl Drop for PackedKState {
    fn drop(&mut self) {
        unsafe { dealloc(self.bytes, self.packed_kpuzzle.data.layout) }
    }
}

impl PackedKState {
    pub fn new(packed_kpuzzle: PackedKPuzzle) -> Self {
        let bytes = unsafe { alloc(packed_kpuzzle.data.layout) };
        Self {
            packed_kpuzzle,
            bytes,
        }
    }

    pub fn get_piece_or_permutation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize) -> u8 {
        unsafe {
            self.bytes
                .add(orbit_info.pieces_or_pemutations_offset + i)
                .read()
        }
    }

    pub fn get_orientation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize) -> u8 {
        unsafe { self.bytes.add(orbit_info.orientations_offset + i).read() }
    }

    pub fn set_piece_or_permutation(
        &self,
        orbit_info: &PackedKPuzzleOrbitInfo,
        i: usize,
        value: u8,
    ) {
        unsafe {
            self.bytes
                .add(orbit_info.pieces_or_pemutations_offset + i)
                .write(value)
        }
    }

    pub fn set_orientation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize, value: u8) {
        unsafe {
            self.bytes
                .add(orbit_info.orientations_offset + i)
                .write(value)
        }
    }

    // Adapted from https://github.com/cubing/cubing.rs/blob/b737c6a36528e9984b45b29f9449a9a330c272fb/src/kpuzzle/state.rs#L31-L82
    // TODO: dedup the implementation (but avoid runtime overhead for the shared abstraction).
    pub fn apply_transformation(
        &self,
        packed_kpuzzle: &PackedKPuzzle,
        transformation: &PackedKTransformation,
    ) -> PackedKState {
        let new_state = PackedKState::new(self.packed_kpuzzle.clone());
        for orbit_info in &packed_kpuzzle.data.orbit_iteration_info {
            // TODO: optimization when either value is the identity.
            for i in 0..orbit_info.num_pieces {
                let transformation_idx = transformation.get_piece_or_permutation(orbit_info, i);

                let new_piece_permutation = self.get_piece_or_permutation(
                    orbit_info,
                    std::convert::Into::<usize>::into(transformation_idx),
                );
                new_state.set_piece_or_permutation(orbit_info, i, new_piece_permutation);

                let previous_piece_orientation = self.get_orientation(
                    orbit_info,
                    std::convert::Into::<usize>::into(transformation_idx),
                );
                let new_piece_orientation = orbit_info.table[std::convert::Into::<usize>::into(
                    previous_piece_orientation + transformation.get_orientation(orbit_info, i),
                )];
                new_state.set_orientation(orbit_info, i, new_piece_orientation);
            }
        }

        new_state
    }

    // pub fn hash(&self) -> u64 {
    //     cityhash::city_hash_64(&self.bytes)
    // }
}
