#[path = "./common/lib.rs"]
mod common;

use std::time::Duration;

use common::{run_tests, test_search_fails, test_search_succeeds};

// TODO: Support `#[test]`.
fn basic_tests() -> Result<(), ()> {
    test_search_succeeds(
        common::CliCommand::Cpp(),
        &[
            "-M",
            "1",
            "samples/main/3x3x3.tws",
            "samples/main/tperm.scr",
        ],
        None,
        " R2 D' F2 U F2 R2 U R2 U' R2",
        Some(Duration::from_secs(10)),
    )?;

    test_search_fails(
        common::CliCommand::Cpp(),
        &["examples/test-cases/wildcard_conflict.tws"],
        None,
        "",
        Some(Duration::from_secs(10)),
    )?;

    // If no tests failed until now, we're okay!
    Ok(())
}

fn main() {
    run_tests(basic_tests)
}
