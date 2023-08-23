use std::alloc::{alloc, dealloc, Layout};

use super::{packed_kpuzzle::PackedKPuzzleOrbitInfo, PackedKPuzzle, PackedKTransformation};

pub struct PackedKState {
    pub layout: Layout,
    pub bytes: *mut u8,
}

impl Drop for PackedKState {
    fn drop(&mut self) {
        unsafe { dealloc(self.bytes, self.layout) }
    }
}

impl PackedKState {
    pub fn new(layout: Layout) -> Self {
        let bytes = unsafe { alloc(layout) };
        Self { layout, bytes }
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
        let mut new_state = PackedKState::new(self.layout);
        self.apply_transformation_into(packed_kpuzzle, transformation, &mut new_state);
        new_state
    }

    // Adapted from https://github.com/cubing/cubing.rs/blob/b737c6a36528e9984b45b29f9449a9a330c272fb/src/kpuzzle/state.rs#L31-L82
    // TODO: dedup the implementation (but avoid runtime overhead for the shared abstraction).
    pub fn apply_transformation_into(
        &self,
        packed_kpuzzle: &PackedKPuzzle,
        transformation: &PackedKTransformation,
        into_state: &mut PackedKState,
    ) {
        for orbit_info in &packed_kpuzzle.data.orbit_iteration_info {
            // TODO: optimization when either value is the identity.
            for i in 0..orbit_info.num_pieces {
                let transformation_idx = transformation.get_piece_or_permutation(orbit_info, i);

                let new_piece_permutation = self.get_piece_or_permutation(
                    orbit_info,
                    std::convert::Into::<usize>::into(transformation_idx),
                );
                into_state.set_piece_or_permutation(orbit_info, i, new_piece_permutation);

                let previous_piece_orientation = self.get_orientation(
                    orbit_info,
                    std::convert::Into::<usize>::into(transformation_idx),
                );
                let new_piece_orientation = orbit_info.table[std::convert::Into::<usize>::into(
                    previous_piece_orientation + transformation.get_orientation(orbit_info, i),
                )];
                into_state.set_orientation(orbit_info, i, new_piece_orientation);
            }
        }
    }

    // pub fn hash(&self) -> u64 {
    //     cityhash::city_hash_64(&self.bytes)
    // }
}
