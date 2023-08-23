use std::alloc::{alloc, dealloc};

use super::{packed_kpuzzle::PackedKPuzzleOrbitInfo, PackedKPuzzle};
pub struct PackedKTransformation {
    pub packed_kpuzzle: PackedKPuzzle,
    pub bytes: *mut u8,
}

impl Drop for PackedKTransformation {
    fn drop(&mut self) {
        unsafe { dealloc(self.bytes, self.packed_kpuzzle.data.layout) }
    }
}

impl PackedKTransformation {
    pub fn new(packed_kpuzzle: PackedKPuzzle) -> Self {
        let bytes = unsafe { alloc(packed_kpuzzle.data.layout) };
        Self {
            packed_kpuzzle,
            bytes,
        }
    }
    // TODO: dedup with PackedKTransformation, or at least implement as a trait?
    pub fn get_piece_or_permutation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize) -> u8 {
        unsafe {
            self.bytes
                .add(orbit_info.pieces_or_pemutations_offset + i)
                .read()
        }
    }

    // TODO: dedup with PackedKTransformation, or at least implement as a trait?
    pub fn get_orientation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize) -> u8 {
        unsafe { self.bytes.add(orbit_info.orientations_offset + i).read() }
    }

    // TODO: dedup with PackedKTransformation, or at least implement as a trait?
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

    // TODO: dedup with PackedKTransformation, or at least implement as a trait?
    pub fn set_orientation(&self, orbit_info: &PackedKPuzzleOrbitInfo, i: usize, value: u8) {
        unsafe {
            self.bytes
                .add(orbit_info.orientations_offset + i)
                .write(value)
        }
    }
}
