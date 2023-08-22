use std::time::Instant;

use cubing::{parse_alg, parse_move, puzzles::cube3x3x3_kpuzzle};

use crate::cpp_port::PackedKPuzzle;

#[path = "./cpp_port/mod.rs"]
mod cpp_port;

// Run using: cargo run --release --example test-cpp_port
fn main() {
    let num_moves = 100000000;
    test_packed(num_moves);
    test_unpacked(num_moves);
}

fn test_packed(num_moves: usize) {
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
    for i in 0..num_moves {
        state = state.apply_transformation(&packed_kpuzzle, &move_transformations[i % 18]);
    }
    println!("{:?}", state.bytes);
    let duration = start.elapsed();
    println!(
        "Time elapsed for {} moves (packed): {:?}",
        num_moves, duration
    );
}

fn test_unpacked(num_moves: usize) {
    let kpuzzle = cube3x3x3_kpuzzle();
    let m = |s: &str| {
        kpuzzle
            .transformation_from_alg(&parse_alg!(s).unwrap())
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

    let mut state = kpuzzle.start_state();
    let start = Instant::now();
    for i in 0..num_moves {
        state = state.apply_transformation(&move_transformations[i % 18]);
    }
    assert_eq!(
        state,
        kpuzzle
            .start_state()
            .apply_alg(&parse_alg!("U2 F2 L2 U2 D2 F2 R2 F2 R'").unwrap())
            .unwrap()
    );
    let duration = start.elapsed();
    println!(
        "Time elapsed for {} moves (unpacked): {:?}",
        num_moves, duration
    );
}
