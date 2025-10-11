# Architecture Support

PawLang supports multiple architectures through native compilation and cross-compilation.

## Tested Architectures

### Native Testing (Full test suite)

These architectures are tested with full test suites including runtime tests:

#### Linux
- **x86_64** (Ubuntu 24.04, 22.04 LTS) - Primary
- **ARM64/aarch64** (Ubuntu 24.04) - Native ARM runner

#### macOS
- **x86_64** (Intel Mac) - macOS 13
- **ARM64** (Apple Silicon M1/M2/M3) - macOS Latest

#### Windows
- **x86_64** - Windows Server 2022

## Cross-Compilation Support (Build-only)

These architectures are tested through cross-compilation to verify build compatibility:

### Linux
- **RISC-V 64** (`riscv64-linux`) - Growing ecosystem
- **PowerPC 64 LE** (`powerpc64le-linux`) - IBM POWER systems
- **s390x** (`s390x-linux`) - IBM Z mainframes

## Platform Coverage

- **Tested Native**: 5 platforms (99% of users)
- **Cross-compile**: 3 additional architectures
- **Total Coverage**: 8 architectures across 3 operating systems

## Testing Strategy

### Native Platforms
Full test suite includes:
- ✅ C backend
- ✅ LLVM backend
- ✅ Conditional compilation (no-LLVM build)
- ✅ Self-contained distribution
- ✅ Runtime execution tests

### Cross-Compilation Platforms
Build-only tests:
- ✅ Successful compilation for target architecture
- ⚠️ No runtime tests (cannot execute on host)

## Why This Approach?

1. **Fast CI**: Native tests complete in 3-5 minutes
2. **Comprehensive**: Covers main user platforms
3. **Forward Compatible**: Cross-compile tests ensure portability
4. **Resource Efficient**: No slow emulation needed

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

