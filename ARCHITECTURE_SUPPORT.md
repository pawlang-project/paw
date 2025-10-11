# Architecture Support

PawLang supports multiple architectures through native compilation on GitHub Actions runners.

## Tested Architectures

All architectures run the **complete test suite** including:
- ✅ C backend compilation and execution
- ✅ LLVM backend compilation and execution
- ✅ Self-contained distribution (works without system LLVM)
- ✅ All example programs and integration tests
- ✅ Runtime validation after system LLVM removal

### Native Testing Platforms

#### Linux x86_64
- **Ubuntu 24.04** - Latest LTS release
- **Ubuntu 22.04 LTS** - Long-term support

#### macOS
- **x86_64** (Intel Mac) - macOS 13, tested on GitHub Actions
- **ARM64** (Apple Silicon M1/M2/M3) - macOS Latest, tested on GitHub Actions

#### Windows
- **x86_64** - Windows Server 2022, tested on GitHub Actions

## Platform Coverage

- **Tested Platforms**: 5 platforms (2 Linux x86_64 + 2 macOS + 1 Windows)
- **User Coverage**: 99% of desktop and server users
- **Architectures**: x86_64 and ARM64 (Apple Silicon)

## Testing Strategy

### Complete Test Suite (All 5 Platforms)

Every platform runs the **full test suite**:
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

1. **Fast & Reliable**: Native runners provide fast, stable CI (~3-4 minutes)
2. **Real Hardware**: Testing on actual hardware, not emulation
3. **99% Coverage**: Covers essentially all desktop and server users
4. **Production-Ready**: All platforms ready for immediate release
5. **Easy Maintenance**: Simple, well-tested runner configurations

## Adding New Platforms

To add support for a new platform when GitHub Actions provides native runners:

1. Add to the matrix in `.github/workflows/ci.yml`:
   ```yaml
   - os: <new-runner>
     arch: <architecture-name>
     platform: <platform-name>
   ```

2. Ensure LLVM installation step supports the new platform
3. All existing tests will run automatically

## CI Performance

- **Average CI Time**: 3-4 minutes
- **Fastest**: macOS ARM64 (~1.5 minutes)
- **Slowest**: Windows x86_64 (~4.5 minutes)
- **Parallel Execution**: All 5 platforms run simultaneously

## Future Plans

- Add ARM64 Windows support when GitHub Actions provides native runners
- Add ARM64 Linux support if GitHub Actions adds native ARM64 Linux runners
- Monitor for new architecture support from GitHub Actions

