// src/cli/args.rs
// 命令行参数解析

use std::env;

#[derive(Debug, Clone)]
pub enum BuildProfile {
    Dev,
    Release,
}

#[derive(Debug, Clone)]
pub enum Command {
    Build {
        profile: BuildProfile,
        target: Option<String>,
        quiet: bool,
        input_file: Option<String>,
    },
    ListTargets,
    Help,
}

#[derive(Debug)]
pub struct CliArgs {
    pub command: Command,
}

impl CliArgs {
    pub fn parse() -> Result<Self, String> {
        let args: Vec<String> = env::args().skip(1).collect();
        
        if args.is_empty() {
            return Err("Usage: pawc <command> [options]".to_string());
        }

        match args[0].as_str() {
            "build" => {
                if args.len() < 2 {
                    return Err("Usage: pawc build <dev|release> [options]".to_string());
                }

                let profile = match args[1].as_str() {
                    "dev" => BuildProfile::Dev,
                    "release" => BuildProfile::Release,
                    _ => return Err("Invalid build profile, use 'dev' or 'release'".to_string()),
                };

                let mut quiet = false;
                let mut target = None;
                let mut input_file = None;
                let mut i = 2;

                // 解析可选参数
                while i < args.len() {
                    match args[i].as_str() {
                        "--quiet" => {
                            quiet = true;
                            i += 1;
                        }
                        "--target" => {
                            if i + 1 >= args.len() {
                                return Err("--target requires an argument".to_string());
                            }
                            target = Some(args[i + 1].clone());
                            i += 2;
                        }
                        _ => {
                            // If not starting with --, treat as input file
                            if !args[i].starts_with("--") {
                                input_file = Some(args[i].clone());
                                i += 1;
                            } else {
                                return Err(format!("Unknown argument: {}", args[i]));
                            }
                        }
                    }
                }

                Ok(CliArgs {
                    command: Command::Build {
                        profile,
                        target,
                        quiet,
                        input_file,
                    },
                })
            }
            "--list-targets" => {
                Ok(CliArgs {
                    command: Command::ListTargets,
                })
            }
                    "--help" | "-h" => {
                        Ok(CliArgs {
                            command: Command::Help,
                        })
                    }
                    _ => {
                        Err(format!("Unknown command: {}. Use --help for help", args[0]))
                    }
        }
    }

    pub fn print_help() {
        println!("PawLang Compiler - Cross-compilation support");
        println!("");
        println!("Usage:");
        println!("  pawc build <dev|release> [options]");
        println!("  pawc --list-targets");
        println!("  pawc --help");
        println!("");
        println!("Options:");
        println!("  --target <triple>    Specify target platform (cross-compilation)");
        println!("  --quiet             Quiet mode, only output result path");
        println!("  --list-targets      List all supported target platforms");
        println!("  --help, -h          Show this help information");
        println!("");
        println!("Examples:");
        println!("  pawc build dev                         # Build for current platform");
        println!("  pawc build dev --target x86_64-unknown-linux-gnu  # Cross-compile to Linux");
        println!("  pawc build dev --target x86_64-pc-windows-gnu  # Cross-compile to Windows");
    }

    pub fn print_supported_targets() {
        println!("Supported target platforms:");
        println!("  x86_64-unknown-linux-gnu    Linux x64");
        println!("  x86_64-pc-windows-gnu       Windows x64");
        println!("  x86_64-apple-darwin         macOS x64");
    }

    pub fn print_usage_error() {
        eprintln!("usage: pawc build <dev|release> [--quiet] [--target <triple>] [<input_file>]");
        eprintln!("       pawc --list-targets");
        eprintln!("");
        eprintln!("Supported targets:");
        eprintln!("  x86_64-unknown-linux-gnu    Linux x64");
        eprintln!("  x86_64-pc-windows-gnu       Windows x64");
        eprintln!("  x86_64-apple-darwin         macOS x64");
    }
}
