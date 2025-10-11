# 🚀 Quick Distribution Reference

## Create Distribution Package

```bash
# One command to create platform-specific package
zig build package
```

**Output:**
- Windows: `pawlang-windows.zip` (~200MB)
- macOS: `pawlang-macos.tar.gz` (~800KB)
- Linux: `pawlang-linux.tar.gz` (~800KB)

---

## Package Contents

All packages include:
- ✅ Compiler executable (`pawc` / `pawc.exe`)
- ✅ All LLVM libraries (bundled)
- ✅ Smart launcher script (auto-configures paths)
- ✅ Example `.paw` files
- ✅ Documentation (README, USAGE, LICENSE)
- ✅ Both C and LLVM backends

---

## User Experience

**All platforms - same experience:**

1. Extract archive
2. Run: `./pawc hello.paw` or `pawc.bat hello.paw`
3. **That's it!** No installation, no configuration needed.

Both backends work out-of-the-box:
```bash
./pawc program.paw              # C backend (default)
./pawc program.paw --backend=llvm  # LLVM backend
./pawc program.paw --backend=llvm -O2  # LLVM with optimization
```

---

## Build Commands

| Command | Description |
|---------|-------------|
| `zig build` | Development build |
| `zig build dist` | Prepare distribution files |
| `zig build package` | Create distribution archive |
| `zig build help-dist` | Show distribution help |

---

## CI/CD Integration

GitHub Actions automatically:
1. Builds for all platforms (Windows, macOS, Linux)
2. Tests both C and LLVM backends
3. Copies LLVM libraries to output
4. Validates functionality

All you need to do:
```bash
git push
```

---

## What Makes This Special

✨ **Zero Configuration**
- Users don't need to install LLVM
- No environment variables to set
- No PATH modifications needed

✨ **Complete Functionality**
- Both C and LLVM backends included
- All optimization levels available
- Full language features work

✨ **Cross-Platform Consistency**
- Same user experience everywhere
- Same commands on all platforms
- Same feature set

✨ **Professional Distribution**
- Includes documentation
- Includes examples
- Ready for GitHub Releases

---

## Testing Distribution Locally

```bash
# Build package
zig build package

# Test extraction (macOS/Linux)
cd /tmp && tar -xzf path/to/pawlang-*.tar.gz
./pawc examples/hello.paw
./pawc examples/hello.paw --backend=llvm

# Both should work without any setup!
```

---

## Upload to GitHub Releases

1. Create release on GitHub
2. Upload platform-specific archives:
   - `pawlang-windows.zip`
   - `pawlang-macos.tar.gz`
   - `pawlang-linux.tar.gz`
3. Users download, extract, and run!

---

**Last Updated:** 2025-10-11  
**Version:** 0.1.7+

