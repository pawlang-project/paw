# Function Argument Validation - Technical Notes

**Date**: October 9, 2025  
**Version**: v0.1.3  
**Status**: Deferred to future release

---

## 🔍 Investigation

### Question
Can PawLang detect errors like `let i = add(32);` where `add` expects 2 arguments?

### Current Behavior (v0.1.3)

**PawLang Compiler**: ✅ Passes
- Type inference works: `i` is inferred as `i32` ✅
- **No argument count validation** ⚠️
- Generates C code: `int32_t i = add(32);`

**C Compiler (GCC)**: ❌ Fails
```
error: too few arguments to function call, expected 2, have 1
```

---

## 🧪 Implementation Attempt

### What We Tried

Added argument validation in `TypeChecker.checkExpr()` for `.call` expressions:

```zig
// Check argument count
if (call.args.len != func.params.len) {
    error: Function expects X arguments, but got Y
}

// Check argument types (non-generic functions only)
if (func.type_params.len == 0) {
    for (call.args, func.params) |arg, param| {
        if (!isTypeCompatible(arg_type, param_type)) {
            error: Type mismatch
        }
    }
}
```

### Results

✅ **Successes**:
- Argument count validation works perfectly
- Error messages are clear and helpful
- Non-generic functions type-checked correctly

❌ **Problems**:
- Conflicts with generic function system
- Module system tests fail (panic)
- Complex edge cases with type inference

---

## 🤔 Analysis

### Why It's Complex

1. **Generic Functions**
   ```paw
   fn identity<T>(x: T) -> T { x }
   
   let result = identity(42);  // T = i32, inferred
   ```
   - Parameter type is `.generic` (not concrete)
   - Need full type inference to validate
   - Requires tracking type substitutions

2. **Module System Interaction**
   - Imported functions checked separately
   - Symbol table lookup timing issues
   - Potential infinite recursion in type checking

3. **Type Inference Dependency**
   - Need to infer types before validating
   - But validation happens during inference
   - Circular dependency problem

---

## ✅ Current Solution

### Validate in Two Stages

**Stage 1: PawLang (Current)**
- ✅ Type inference
- ✅ Basic type checking
- ❌ No argument validation (intentional)

**Stage 2: C Compiler**
- ✅ Argument count validation
- ✅ Argument type validation  
- ✅ Full semantic checking

### Advantages

1. **Simpler Implementation**
   - No conflict with generics
   - No module system issues
   - Clean separation of concerns

2. **Reliable Error Detection**
   - C compiler catches all errors
   - Well-tested validation
   - Clear error messages

3. **Faster Development**
   - Focus on core features
   - Avoid complex edge cases
   - Ship v0.1.3 quickly

---

## 🔮 Future Work

### v0.1.4 or v0.2.0

When we have time for proper implementation:

#### 1. Full Type Inference First
Build complete type inference with:
- Bidirectional inference
- Type substitution tracking
- Constraint solving

#### 2. Then Add Validation
With full inference, validation becomes easier:
```zig
fn validate CallExpr(call: Call) {
    // 1. Infer all types (including generics)
    // 2. Validate argument count
    // 3. Validate argument types
    // 4. Return validated type
}
```

#### 3. Better Error Messages
```
Error: Function 'add' expects 2 arguments, but got 1
  let i = add(32);
          ^^^^^^^^
  
Expected: add(i32, i32)
Got:      add(i32)
```

---

## 💡 Recommendations

### For v0.1.3 Release

**Do Not Add** argument validation:
- ❌ Conflicts with generic system
- ❌ Causes module system panics
- ❌ Complex to implement correctly
- ❌ Not critical (C compiler validates)

**Focus On**:
- ✅ Type inference (core feature)
- ✅ Stability (23/25 tests pass)
- ✅ Documentation
- ✅ Quick release

### For Users

**Current behavior is acceptable**:

```paw
// This will compile in PawLang
let i = add(32);

// But fail in C compilation with clear error:
// error: too few arguments to function call, expected 2, have 1
```

Users still get error feedback, just from C compiler instead of PawLang compiler.

---

## 📊 Risk Assessment

### Low Risk

**Not Adding Validation**:
- ⚠️ Users see C errors (not ideal UX)
- ✅ But still get clear error messages
- ✅ No runtime bugs (caught at compile time)
- ✅ v0.1.3 ships stable and fast

**Adding Validation**:
- ❌ Breaks existing tests
- ❌ Causes panics
- ❌ Delays v0.1.3 release
- ❌ Complex to debug

### Decision: Low Risk Path

Ship v0.1.3 without argument validation. Add it properly in future version with full type inference.

---

## 🎯 Action Plan

### v0.1.3 (Now)
- ✅ Ship with type inference
- ✅ Document current limitation
- ✅ Note in RELEASE_NOTES

### v0.1.4 or v0.2.0 (Future)
- 🔲 Implement full type inference
- 🔲 Add argument validation
- 🔲 Better error messages
- 🔲 Fix module system issues

---

## 📝 Documentation

### Add to RELEASE_NOTES

**Known Limitations**:
- Function argument count not validated by PawLang compiler
- C compiler will catch these errors
- Will be added in future release

**Example**:
```paw
fn add(a: i32, b: i32) -> i32 { a + b }

let i = add(32);  // PawLang: compiles
                  // GCC: error (too few arguments)
```

---

## 🏁 Conclusion

**Argument validation is deferred** to allow v0.1.3 to ship with stable type inference.

**This is the right decision** because:
1. Type inference is the main feature ⭐
2. C compiler validates arguments anyway
3. Proper validation needs more infrastructure
4. v0.1.3 can ship quickly and stably

**Focus**: Ship great type inference now, add validation later.

---

**Status**: Investigation complete, decision made  
**Next**: Finalize v0.1.3 release

