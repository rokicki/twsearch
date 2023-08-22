use std::time::Instant;

use cubing::{parse_move, puzzles::cube3x3x3_kpuzzle};

use crate::cpp_port::PackedKPuzzle;

#[path = "./cpp_port/mod.rs"]
mod cpp_port;

fn main() {
    let packed_kpuzzle = PackedKPuzzle::try_from(cube3x3x3_kpuzzle()).unwrap();

    let m = |s: &str| {
        packed_kpuzzle
            .transformation_from_move(&parse_move!(s).unwrap())
            .unwrap()
    };

    let move_transformations = vec![
        m("U"),
        m("U2"),
        m("U'"),
        m("L"),
        m("L2"),
        m("L'"),
        m("F"),
        m("F2"),
        m("F'"),
        m("R"),
        m("R2"),
        m("R'"),
        m("B"),
        m("B2"),
        m("B'"),
        m("D"),
        m("D2"),
        m("D'"),
    ];

    let mut state = packed_kpuzzle.start_state();
    let start = Instant::now();
    for i in 0..1000000 {
        state = state.apply_transformation(&packed_kpuzzle, &move_transformations[i % 18]);
    }
    println!("{:?}", state.bytes);
    let duration = start.elapsed();
    println!("Time elapsed: {:?}", duration);
}
