use cubing::{parse_move, puzzles::cube3x3x3_kpuzzle};

use crate::cpp_port::PackedKPuzzle;

#[path = "./cpp_port/mod.rs"]
mod cpp_port;

fn main() {
    let packed_kpuzzle = PackedKPuzzle::try_from(cube3x3x3_kpuzzle()).unwrap();

    let start_state = packed_kpuzzle.start_state();
    println!("{:?}", start_state.bytes);

    #[allow(non_snake_case)]
    let transformation_R = packed_kpuzzle
        .transformation_from_move(&parse_move!("R").unwrap())
        .unwrap();
    println!("{:?}", transformation_R.bytes);

    let transformed = start_state.apply_transformation(&transformation_R);
    println!("{:?}", transformed.bytes);

    let transformed = transformed.apply_transformation(&transformation_R);
    println!("{:?}", transformed.bytes);

    let transformed = transformed.apply_transformation(&transformation_R);
    println!("{:?}", transformed.bytes);

    let transformed = transformed.apply_transformation(&transformation_R);
    println!("{:?}", transformed.bytes);
}
