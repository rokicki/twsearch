#[path = "./common/lib.rs"]
mod common;

use common::run_tests;

// TODO: Support `#[test]`.
fn basic_tests() -> Result<(), ()> {
    // If no tests failed until now, we're okay!
    Ok(())
}

fn main() {
    run_tests(basic_tests)
}
