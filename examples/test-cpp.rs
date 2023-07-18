use std::{
    io::Write,
    process::{exit, Command, Stdio},
    str,
};

const DEBUG_PRINT_ARGS: bool = false;

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

fn run_tests(test_fn: fn() -> Result<(), ()>) {
    match test_fn() {
        Ok(_) => (),
        Err(_) => {
            eprintln!("At least one tests failed. Exiting");
            exit(1)
        }
    }
}

fn main() {
    run_tests(basic_tests)
}

/********/

static BIN_PATH: &str = "./build/bin/twsearch";

pub(crate) fn test_command_succeeds(
    args: &[&str],
    stdin: Option<&[u8]>,
    expect_stdout_to_contain: &str,
) -> Result<(), ()> {
    println!("----------------");
    println!("{} {}", BIN_PATH, args.join(" "));
    let stdout = match run_command(BIN_PATH, args, stdin) {
        Ok(stdout) => stdout,
        Err(stderr) => {
            println!("❌");
            eprintln!("twsearch failed with stderr:\n{}\n", stderr);
            return Ok(());
        }
    };
    if stdout.contains(expect_stdout_to_contain) {
        println!("✅");
        Ok(())
    } else {
        println!("❌");
        eprintln!(
            "Expected stdout to contain:\n{}\n",
            expect_stdout_to_contain
        );
        eprintln!("Stdout was:\n{}\n", stdout);
        Err(())
    }
}

pub(crate) fn test_command_fails(
    args: &[&str],
    stdin: Option<&[u8]>,
    expect_stderr_to_contain: &str,
) -> Result<(), ()> {
    println!("----------------");
    println!("{} {}", BIN_PATH, args.join(" "));
    let stderr = match run_command(BIN_PATH, args, stdin) {
        Err(stderr) => stderr,
        Ok(stdout) => {
            println!("❌");
            eprintln!("twsearch should have failed\n");
            eprintln!("stdout contained:\n{}\n", stdout);
            return Ok(());
        }
    };
    if stderr.contains(expect_stderr_to_contain) {
        println!("✅");
        Ok(())
    } else {
        println!("❌");
        eprintln!(
            "Expected stderr to contain:\n{}\n",
            expect_stderr_to_contain
        );
        eprintln!("Stderr was:\n{}\n", stderr);
        Err(())
    }
}

// Returns either stdout on success, or stderr as an error.
pub(crate) fn run_command(
    command_name: &str,
    args: &[&str],
    stdin: Option<&[u8]>,
) -> Result<String, String> {
    if DEBUG_PRINT_ARGS {
        println!("args: {}", args.join(" "));
    };
    let child = Command::new(command_name)
        .args(args.iter())
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn();
    let mut child = match child {
        Ok(child) => child,
        Err(e) => {
            return Err(e.to_string());
        }
    };

    if let Some(stdin) = stdin {
        let child_stdin = child.stdin.as_mut().unwrap(); // TODO
        match child_stdin.write_all(stdin) {
            Ok(output) => output,
            Err(_) => return Err("Could not write to stdin for a command.".to_owned()),
        }
    }

    let output = match child.wait_with_output() {
        Ok(output) => output,
        Err(e) => return Err(e.to_string()),
    };

    if !output.status.success() {
        let stderr_text: &str =
            str::from_utf8(&output.stderr).map_err(|_| "Could not convert stderr to UTF-8")?;
        return Err(stderr_text.to_owned());
    }

    Ok(str::from_utf8(&output.stdout)
        .map_err(|_| "Could not convert stdout to UTF-8")?
        .to_owned())
}
