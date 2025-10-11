# Architecture Support

PawLang supports multiple architectures through native compilation and QEMU-based testing.

## Tested Architectures - All Fully Tested!

All architectures run the **complete test suite** including:
- ✅ C backend compilation and execution
- ✅ LLVM backend compilation and execution
- ✅ Self-contained distribution (works without system LLVM)
- ✅ All example programs and integration tests

### Native Testing (Direct hardware)

#### Linux
- **x86_64** (Ubuntu 24.04, 22.04 LTS) - Primary platform

#### macOS
- **x86_64** (Intel Mac) - macOS 13
- **ARM64** (Apple Silicon M1/M2/M3) - macOS Latest

#### Windows
- **x86_64** - Windows Server 2022

### QEMU Testing (Full runtime tests in emulation)

All tests run in QEMU with real execution - not just compilation!

#### Linux (via QEMU)
- **aarch64** (ARM64) - AWS Graviton, Raspberry Pi 4+, Apple M-series
- **armv7** (ARM32) - Raspberry Pi 2/3, embedded systems
- **riscv64** (RISC-V 64-bit) - Growing open-source ecosystem
- **ppc64le** (PowerPC 64 LE) - IBM POWER8/9/10 systems
- **s390x** (IBM Z) - IBM mainframes

## Platform Coverage

- **Native Testing**: 5 platforms (x86_64 Linux/Windows/macOS + ARM64 macOS)
- **QEMU Testing**: 5 Linux architectures (aarch64, armv7, riscv64, ppc64le, s390x)
- **Total Coverage**: 10 architectures across 3 operating systems

## Testing Strategy

### All Platforms (Native + QEMU)
Every architecture runs the **complete test suite**:
- ✅ C backend compilation and runtime
- ✅ LLVM backend compilation and runtime
- ✅ Conditional compilation (no-LLVM build test)
- ✅ Self-contained distribution validation
- ✅ System LLVM removal test (critical!)
- ✅ All example programs
- ✅ Integration tests

### Key Tests

**1. Basic Compilation**
- Hello World (C backend)
- Type casting (C backend)
- Integration test (C backend)
- LLVM backend test

**2. Self-Contained Distribution** (Most Important!)
- Build distribution package
- **Remove system LLVM completely**
- Test C backend (should work)
- Test LLVM backend (should work with bundled LLVM!)
- Verify bundled libraries

This proves the distribution truly works standalone!

## Why This Approach?

1. **Complete Coverage**: Every architecture gets full runtime testing
2. **Real-World Validation**: QEMU runs actual binaries, not just compilation
3. **Distribution Confidence**: Self-contained test ensures users can run without dependencies
4. **Future-Proof**: Supports emerging architectures (RISC-V, etc.)

## Adding New Architectures

To add support for a new architecture:

1. **If GitHub Actions supports native runners**:
   - Add to matrix with native runner
   - Enable full test suite

2. **If only cross-compilation is available**:
   - Add to matrix with `cross_target` parameter
   - Build-only validation
   - Example:
     ```yaml
     - os: ubuntu-latest
       arch: Linux ARM32 (cross-compile)
       cross_target: arm-linux
     ```

## Architecture Priority

Priority for native testing:
1. x86_64 (Linux, macOS, Windows) - Highest
2. ARM64 (macOS Apple Silicon, Linux) - High
3. Other architectures - Build verification only

## Future Plans

- Add ARM64 Windows support when runners become available
- Consider adding ARM32 (armv7) cross-compilation
- Evaluate LoongArch64 when demand increases

