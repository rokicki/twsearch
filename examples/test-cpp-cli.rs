#[path = "./common/lib.rs"]
mod common;

use common::{run_tests, test_command_fails, test_command_succeeds};

// TODO: Support `#[test]`.
fn basic_tests() -> Result<(), ()> {
    test_command_succeeds(
        &[
            "-M",
            "1",
            "samples/main/3x3x3.tws",
            "samples/main/tperm.scr",
        ],
        None,
        " R2 D' F2 U F2 R2 U R2 U' R2",
    )?;

    test_command_fails(&["examples/test-cases/wildcard_conflict.tws"], None, "")?;

    // If no tests failed until now, we're okay!
    Ok(())
}

fn main() {
    run_tests(basic_tests)
}
