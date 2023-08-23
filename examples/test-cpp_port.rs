use std::time::Instant;

use cubing::{parse_alg, parse_move, puzzles::cube3x3x3_kpuzzle};

use crate::cpp_port::PackedKPuzzle;

#[path = "./cpp_port/mod.rs"]
mod cpp_port;

const PRINT_FINAL_STATE: bool = true;

// Run using: cargo run --release --example test-cpp_port
fn main() {
    let num_moves = 10_000_000;
    test_packed(num_moves);
    test_unpacked(num_moves / 10);
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

    let mut current = packed_kpuzzle.start_state();
    let mut other = packed_kpuzzle.start_state();
    let start = Instant::now();
    for i in (0..num_moves).step_by(2) {
        current.apply_transformation_into(&move_transformations[i % 18], &mut other);
        other.apply_transformation_into(&move_transformations[(i + 1) % 18], &mut current);
    }
    if PRINT_FINAL_STATE {
        println!("{:?}", current.byte_slice());
        println!("Hash: 0x{:x}", current.hash());
    }
    let duration = start.elapsed();
    println!(
        "--------\nTime elapsed for {} moves (packed) without hashing (minimal allocation): {:?} ({:.2}M moves/s)",
        num_moves,
        duration,
        (std::convert::TryInto::<f64>::try_into(num_moves as u32).unwrap()
            / duration.as_secs_f64()
            / std::convert::TryInto::<f64>::try_into(1_000_000).unwrap())
    );

    let mut state = packed_kpuzzle.start_state();
    let start = Instant::now();
    for i in 0..num_moves {
        state = state.apply_transformation(&move_transformations[i % 18]);
    }
    if PRINT_FINAL_STATE {
        println!("{:?}", state.byte_slice());
        println!("Hash: 0x{:x}", state.hash());
    }
    let duration = start.elapsed();
    println!(
        "--------\nTime elapsed for {} moves (packed) without hashing: {:?} ({:.2}M moves/s)",
        num_moves,
        duration,
        (std::convert::TryInto::<f64>::try_into(num_moves as u32).unwrap()
            / duration.as_secs_f64()
            / std::convert::TryInto::<f64>::try_into(1_000_000).unwrap())
    );

    let mut current = packed_kpuzzle.start_state();
    let mut other = packed_kpuzzle.start_state();
    let start = Instant::now();
    for i in (0..num_moves).step_by(2) {
        current.apply_transformation_into(&move_transformations[i % 18], &mut other);
        _ = current.hash();
        other.apply_transformation_into(&move_transformations[(i + 1) % 18], &mut current);
        _ = other.hash();
    }
    if PRINT_FINAL_STATE {
        println!("{:?}", current.byte_slice());
        println!("Hash: 0x{:x}", current.hash());
    }
    let duration = start.elapsed();
    println!(
        "--------\nTime elapsed for {} moves (packed) with hashing (minimal allocation): {:?} ({:.2}M moves/s)",
        num_moves,
        duration,
        (std::convert::TryInto::<f64>::try_into(num_moves as u32).unwrap()
            / duration.as_secs_f64()
            / std::convert::TryInto::<f64>::try_into(1_000_000).unwrap())
    );

    let mut state = packed_kpuzzle.start_state();
    let start = Instant::now();
    for i in 0..num_moves {
        state = state.apply_transformation(&move_transformations[i % 18]);
        // _ = state.hash()
    }
    if PRINT_FINAL_STATE {
        println!("{:?}", state.byte_slice());
        println!("Hash: 0x{:x}", state.hash());
    }
    let duration = start.elapsed();
    println!(
        "--------\nTime elapsed for {} moves (packed) with hashing: {:?} ({:.2}M moves/s)",
        num_moves,
        duration,
        (std::convert::TryInto::<f64>::try_into(num_moves as u32).unwrap()
            / duration.as_secs_f64()
            / std::convert::TryInto::<f64>::try_into(1_000_000).unwrap())
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
    // println!("{:?}", state.state_data);
    // Only works for a million
    // assert_eq!(
    //     state,
    //     kpuzzle
    //         .start_state()
    //         .apply_alg(&parse_alg!("U2 F2 L2 U2 D2 F2 R2 F2 R'").unwrap())
    //         .unwrap()
    // );
    let duration = start.elapsed();
    println!(
        "--------\nTime elapsed for {} moves (unpacked) without hashing: {:?} ({:.2}M moves/s)",
        num_moves,
        duration,
        (std::convert::TryInto::<f64>::try_into(num_moves as u32).unwrap()
            / duration.as_secs_f64()
            / std::convert::TryInto::<f64>::try_into(1_000_000).unwrap())
        .round()
            / std::convert::TryInto::<f64>::try_into(10).unwrap()
    );
}
