use cubing::puzzles::cube3x3x3_kpuzzle;

use crate::cpp_port::PackedKPuzzle;

#[path = "./cpp_port/mod.rs"]
mod cpp_port;

fn main() {
    let packed_kpuzzle = PackedKPuzzle::try_from(cube3x3x3_kpuzzle()).unwrap();
    let start_state = packed_kpuzzle.start_state();
    println!("{:?}", start_state.bytes)
}
