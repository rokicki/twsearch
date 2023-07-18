use std::{
    io::Write,
    process::{exit, Command, Stdio},
    str,
};

const DEBUG_PRINT_ARGS: bool = false;

pub fn run_tests(test_fn: fn() -> Result<(), ()>) {
    match test_fn() {
        Ok(_) => (),
        Err(_) => {
            eprintln!("At least one tests failed. Exiting");
            exit(1)
        }
    }
}

/********/

static BIN_PATH_CPP: &str = "./build/bin/twsearch";
static BIN_PATH_RUST: &str = "cargo";

#[allow(dead_code)] // Workaround for how we are hacking together our tests.
pub enum CliCommand {
    Cpp(),
    Rust(),
}

impl CliCommand {
    fn binary_path(&self) -> &'static str {
        match self {
            CliCommand::Cpp() => BIN_PATH_CPP,
            CliCommand::Rust() => BIN_PATH_RUST,
        }
    }
    fn prefix_args(&self) -> &[&str] {
        match self {
            CliCommand::Cpp() => &[],
            CliCommand::Rust() => &[
                "run",
                "--quiet", // Suppress deprecation warnings for transitive dependencies (`buf_redux`, `multipart`): https://github.com/tomaka/rouille/issues/271
                "--",
                "search",
                "--debug-print-serialized-json",
                "--num-threads",
                "1",
            ],
        }
    }
}

#[allow(dead_code)] // Workaround for how we are hacking together our tests.
pub(crate) fn test_search_succeeds(
    cli_command: CliCommand,
    args: &[&str],
    stdin: Option<&[u8]>,
    expect_stdout_to_contain: &str,
) -> Result<(), ()> {
    let stdout = match run_search_command(cli_command, args, stdin) {
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

#[allow(dead_code)] // Workaround for how we are hacking together our tests.
pub(crate) fn test_search_fails(
    cli_command: CliCommand,
    args: &[&str],
    stdin: Option<&[u8]>,
    expect_stderr_to_contain: &str,
) -> Result<(), ()> {
    let stderr = match run_search_command(cli_command, args, stdin) {
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

pub(crate) fn run_search_command(
    cli_command: CliCommand,
    args: &[&str],
    stdin: Option<&[u8]>,
) -> Result<String, String> {
    let binary_path = cli_command.binary_path();
    let full_args = [cli_command.prefix_args(), args].concat();
    println!("----------------");
    println!("{} {}", binary_path, full_args.join(" "));
    run_command(binary_path, full_args, stdin)
}

// Returns either stdout on success, or stderr as an error.
pub(crate) fn run_command(
    command_name: &str,
    args: Vec<&str>,
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
