use std::{env, process};

fn main() {
    for argument in env::args() {
        println!("{argument}");
    }
    let res = cargo_run_bin::cli::run();

    dbg!(&res);

    // Only reached if run-bin code fails, otherwise process exits early from within
    // binary::run.
    if let Err(res) = res {
        eprintln!("\x1b[31m{}\x1b[0m", format!("run-bin failed: {res}"));
        process::exit(1);
    }
}
