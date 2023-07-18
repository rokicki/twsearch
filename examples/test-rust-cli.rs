#[path = "./common/lib.rs"]
mod common;

use common::{run_tests, test_search_succeeds};

// TODO: Support `#[test]`.
fn copy_of_basic_cpp_cli_tests() -> Result<(), ()> {
    test_search_succeeds(
        common::CliCommand::Rust(),
        &["samples/main/3x3x3.tws", "samples/main/tperm.scr"],
        None,
        " R2 D' F2 U F2 R2 U R2 U' R2",
    )?;

    // TODO: uncomment once the C++ CLI test passes.
    // test_search_fails(
    //     common::CliCommand::Rust(),
    //     &["examples/test-cases/wildcard_conflict.tws"],
    //     None,
    //     "",
    // )?;

    // If no tests failed until now, we're okay!
    Ok(())
}

// TODO: Support `#[test]`.
fn json_tests() -> Result<(), ()> {
    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--moves",
            "U,F,R,D",
            "samples/json/3x3x3-Reid-supercube.def.json",
            "samples/json/A-Perm.scramble.json",
        ],
        None,
        "R2 D R2 D' F2 R2 U' R2 U F2",
    )?;

    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--moves",
            "F,R,B",
            "samples/json/3x3x3-Reid-supercube.def.json",
            "samples/json/Unoriented-A-Perm.json",
        ],
        None,
        "R B R' F R B' R' F'", // TODO: this may be a different 8-move alg
    )?;

    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--moves",
            "U,F,R,D",
            "samples/json/3x3x3-Reid-supercube.def.json",
            "samples/json/T-Perm.scramble.json",
        ],
        None,
        "R2 D' F2 U F2 R2 U R2 U' R2 D", // TODO: this may be a different alg
    )?;

    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--moves",
            "F,R,U",
            "samples/json/3x3x3-Reid-supercube.def.json",
            "samples/json/Unoriented-Y-Perm.scramble.json",
        ],
        None,
        "F R U R' U' F' U", // TODO: this may be a different alg
    )?;

    // If no tests failed until now, we're okay!
    Ok(())
}

fn main() {
    run_tests(copy_of_basic_cpp_cli_tests);
    run_tests(json_tests);
}
