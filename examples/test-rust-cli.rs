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
            "U,R,F",
            "samples/json/3x3x3/3x3x3-Reid.def.json",
            "samples/json/3x3x3/FRURUF.exact.search-pattern.json",
        ],
        None,
        "F R U R' U' F'",
    )?;

    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--experimental-target-pattern",
            "samples/json/3x3x3/OLL-or-CLS.target-pattern.json",
            "--moves",
            "U,R,F",
            "samples/json/3x3x3/3x3x3-Reid.def.json",
            "samples/json/3x3x3/FRURUF.search-pattern.json",
        ],
        None,
        "F R U R' U' F'",
    )?;

    test_search_succeeds(
        common::CliCommand::Rust(),
        &[
            "--experimental-target-pattern",
            "samples/json/3x3x3/ELS-FR.target-pattern.json",
            "--moves",
            "U,R,r",
            "samples/json/3x3x3/3x3x3-Reid.def.json",
            "samples/json/3x3x3/ELS-E3.search-pattern.json",
        ],
        None,
        "r U' R' U R U r'",
    )?;

    // If no tests failed until now, we're okay!
    Ok(())
}

fn main() {
    run_tests(copy_of_basic_cpp_cli_tests);
    run_tests(json_tests);
}
