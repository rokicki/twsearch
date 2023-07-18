#[path = "./common/lib.rs"]
mod common;

use common::{run_tests, test_search_fails, test_search_succeeds};

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

    test_search_fails(
        common::CliCommand::Rust(),
        &[
            "--moves",
            "U,R,F",
            "samples/json/3x3x3/3x3x3-Reid.def.json",
            "samples/json/3x3x3/FRURUF.search-pattern.json",
        ],
        None,
        "11: ! scramble position permutation doesn't match solved",
    )?;

    test_search_fails(
        common::CliCommand::Rust(),
        &[
            "--experimental-target-pattern",
            "samples/json/3x3x3/OLL-or-CLS.target-pattern.json",
            "--moves",
            "U,R,F",
            "samples/json/3x3x3/3x3x3-Reid.def.json",
            "samples/json/3x3x3/FRURUF.exact.search-pattern.json",
        ],
        None,
        "11: ! scramble position permutation doesn't match solved",
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
    run_tests(json_tests);
}
