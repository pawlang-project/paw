# PawLang Cross-Compilation Guide

PawLang now supports cross-compilation to 3 core target platforms!

## Supported Target Platforms

- `x86_64-unknown-linux-gnu` - Linux x64
- `x86_64-pc-windows-gnu` - Windows x64
- `x86_64-apple-darwin` - macOS x64

> **Note**: `aarch64-apple-darwin` (macOS ARM64) is currently not supported due to Cranelift limitations in version 0.124.1. This may be added in future versions when better support becomes available.

## Usage

### Basic Syntax
```bash
pawc build <dev|release> [options]
```

> **CLI Refactoring**: PawLang now uses a dedicated CLI module for argument parsing, providing better error messages and help information in English.

### Options
- `--target <triple>` - Specify target platform (cross-compilation)
- `--quiet` - Quiet mode, only output result path
- `--list-targets` - List all supported target platforms
- `--help, -h` - Show help information

### Examples

#### Build for Current Platform
```bash
pawc build release
pawc build dev
```

#### Cross-compile to Linux
```bash
pawc build release --target x86_64-unknown-linux-gnu
pawc build dev --target x86_64-unknown-linux-gnu
```

#### Cross-compile to Windows
```bash
pawc build release --target x86_64-pc-windows-gnu
pawc build dev --target x86_64-pc-windows-gnu
```

#### Cross-compile to macOS
```bash
pawc build release --target x86_64-apple-darwin
pawc build dev --target x86_64-apple-darwin
```

### View Supported Targets
```bash
pawc --list-targets
```

## Output Directory Structure

Compiled files are placed in the following directory structure:
```
target/
├── debug/           # or release/
│   ├── x86_64-unknown-linux-gnu/
│   │   ├── out.o
│   │   └── Paw
│   ├── x86_64-pc-windows-gnu/
│   │   ├── out.obj
│   │   ├── Paw.exe
│   │   └── Paw.pdb
│   └── x86_64-apple-darwin/
│       ├── out.o
│       └── Paw
```

## Technical Implementation

### Object File Format Generation
- **Windows**: Direct COFF generation via Cranelift
- **macOS**: Direct Mach-O generation via Cranelift
- **Linux**: Direct ELF generation via Cranelift

### User Interface
- **English Output**: All user-facing messages, help text, and error messages are in English
- **Progress Indicators**: Colored progress bars with compilation speed indicators
- **Error Reporting**: Clear, descriptive error messages with usage hints

### Linker
Uses Zig as the cross-compilation linker:
- Automatic target platform detection
- System library linking
- Object file format handling
- Minimal external dependencies

### Runtime Library
- Automatically compiled for target platform
- Static linking support
- Cross-platform compatibility

## Important Notes

1. **Zig Dependency**: Uses Zig compiler for cross-compilation linking
2. **Target Limitation**: Focuses on 3 core platforms to reduce complexity
3. **ISA Support**: Uses Cranelift for native object file generation
4. **No objcopy Required**: Direct object file generation eliminates external dependencies

## Environment Variables

- `PAW_TARGET` - Set default target platform
- `PAWRT_LIB` - Specify runtime library path
- `PAW_VERBOSE=1` - Enable verbose output
- `PAW_SKIP_RUNTIME_BUILD` - Skip runtime compilation

## Troubleshooting

### Common Issues

1. **Zig Not Found**: Ensure Zig is installed and in PATH
2. **ISA Not Supported**: Check if Cranelift supports the target architecture
3. **Link Failure**: Verify target platform system libraries are available
4. **Object Format Issues**: Ensure Cranelift generates correct format for target

### Debug Mode
```bash
PAW_VERBOSE=1 pawc build dev --target x86_64-unknown-linux-gnu
```

This will show detailed compilation and linking process information.
