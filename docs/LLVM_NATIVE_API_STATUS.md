# LLVM Native API Status

## Current Status (v0.1.4)

❌ **Native LLVM API integration is NOT yet supported**

✅ **Text-based LLVM IR generation works perfectly**

---

## Why Native API Doesn't Work Yet

### Problem 1: Dynamic Library Requirement
- `llvm-zig` bindings require `libLLVM.dylib` (unified dynamic library)
- Our local build only generates static libraries (`.a` files)
- Need to rebuild LLVM with `LLVM_BUILD_LLVM_DYLIB=ON`

### Problem 2: llvm-zig System Dependency
- `llvm-zig` itself searches for system LLVM (`/opt/homebrew/opt/llvm`)
- Even with local LLVM, it still tries to link system paths
- Requires patching `llvm-zig` or using system LLVM

### Problem 3: Complex Linking
- LLVM has 100+ separate libraries
- Correct linking order is critical
- Static linking is very complex

---

## Current Solution: Text Mode

**Text-based IR generation is the recommended approach for v0.1.4:**

```bash
# Build PawLang (no LLVM required)
zig build

# Generate LLVM IR
./zig-out/bin/pawc hello.paw --backend=llvm

# Compile with system tools
clang output.ll -o hello
./hello
```

**Advantages:**
- ✅ No LLVM dependency
- ✅ Works on all platforms
- ✅ Simple and reliable
- ✅ Easy to debug
- ✅ Human-readable output

---

## Future Plans: v0.1.5+

### Option A: System LLVM (Easiest)
```bash
# macOS
brew install llvm

# Linux
sudo apt install llvm-19-dev

# Use system LLVM with llvm-zig
zig build -Dwith-llvm=true
```

**Pros:** Simple, well-tested  
**Cons:** System pollution, version conflicts

### Option B: Build Dynamic LLVM (Complex)
```bash
# Modify build_llvm_local.sh to add:
-DLLVM_BUILD_LLVM_DYLIB=ON \
-DLLVM_LINK_LLVM_DYLIB=ON

# Rebuild LLVM (adds ~2GB more)
./scripts/build_llvm_local.sh
```

**Pros:** Self-contained  
**Cons:** Larger size, longer build time

### Option C: Direct LLVM C API (Advanced)
- Write direct C bindings to LLVM
- Skip `llvm-zig` wrapper
- More control, more work

**Pros:** Full control, optimized  
**Cons:** Significant development time

---

## Recommendation

**For v0.1.4:** Continue using text mode (current default)

**For v0.1.5+:** Evaluate based on user feedback
- If users need native API → System LLVM approach
- If self-contained is critical → Dynamic library build
- If performance is key → Direct C API

---

## Technical Details

### What We Have
```
llvm/install/
├── lib/
│   ├── libLLVMCore.a          ✅ Static library
│   ├── libLLVMSupport.a       ✅ Static library
│   ├── libclang.dylib         ✅ Clang dynamic lib
│   └── ... (100+ .a files)
├── bin/
│   ├── llvm-config            ✅ Config tool
│   └── clang                  ✅ Compiler
└── include/                   ✅ Headers
```

### What We Need for llvm-zig
```
llvm/install/lib/
└── libLLVM.dylib              ❌ Missing!
```

### Build Command Failure
```bash
$ zig build -Dwith-llvm=true

# Error: unable to find dynamic system library 'LLVM'
# Searched:
#   - llvm/install/lib/libLLVM.dylib  ❌ Not found
#   - /opt/homebrew/lib/libLLVM.dylib ❌ Not installed
```

---

## Testing Native API (When Ready)

```zig
// Example native LLVM usage (future)
const llvm = @import("llvm");

pub fn main() !void {
    const context = llvm.Context.create();
    defer context.dispose();
    
    const module = context.createModule("test");
    defer module.dispose();
    
    // Build IR with native API
    const builder = context.createBuilder();
    defer builder.dispose();
    
    // ... more code
}
```

---

## Conclusion

**v0.1.4 Status:**
- ✅ Text mode: Production ready
- ❌ Native API: Not yet supported
- 📋 Planned: v0.1.5+

**Current workflow is stable and recommended for all users.**

