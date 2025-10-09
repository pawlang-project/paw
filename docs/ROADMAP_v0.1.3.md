# PawLang v0.1.3 Development Roadmap

**Target Release**: November 2025  
**Duration**: 1-2 weeks  
**Focus**: Type Inference & Developer Experience

---

## 🎯 Core Goal: Automatic Type Inference

### Current State (v0.1.2)
```paw
// Must specify type explicitly
let vec: Vec<i32> = Vec<i32>::new();
let result: i32 = add(1, 2);
```

### Target State (v0.1.3)
```paw
// Type inferred automatically
let vec = Vec<i32>::new();      // vec: Vec<i32>
let result = add(1, 2);         // result: i32
```

---

## 📋 Implementation Plan

### Phase 1: Basic Type Inference (Days 1-2)
**Goal**: Infer types from initialization expressions

#### 1.1 Let Declarations
```paw
let x = 42;              // infer: i32
let y = 3.14;            // infer: f64
let s = "hello";         // infer: string
let b = true;            // infer: bool
```

**Technical**:
- Modify `TypeChecker.checkLetDecl()`
- Add `inferTypeFromExpr()` function
- Update AST to support optional type annotations

#### 1.2 Function Calls
```paw
fn add(a: i32, b: i32) -> i32 { a + b }

let sum = add(1, 2);     // infer: i32
```

**Technical**:
- Extract return type from function signature
- Propagate type through call expression

#### 1.3 Generic Instantiation
```paw
let vec = Vec<i32>::new();           // infer: Vec<i32>
let boxed = Box<string>::new("hi");  // infer: Box<string>
```

**Technical**:
- Extract generic instance type from static method call
- Handle method return types

---

### Phase 2: Advanced Inference (Days 3-4)
**Goal**: Bidirectional type inference

#### 2.1 Function Arguments
```paw
fn process(vec: Vec<i32>) -> i32 { vec.length() }

let v = Vec<i32>::new();
let result = process(v);  // v and result types inferred
```

#### 2.2 Struct Fields
```paw
type Point = struct { x: i32, y: i32, }

let p = Point { x: 1, y: 2 };  // p: Point
let x = p.x;                    // x: i32
```

#### 2.3 Array Literals
```paw
let arr = [1, 2, 3];       // infer: [i32; 3]
let empty = [];            // error: cannot infer
let typed = [1, 2, 3]: [i32; 3];  // explicit when needed
```

---

### Phase 3: Error Messages (Day 5)
**Goal**: Helpful error messages when inference fails

#### Examples
```paw
// Error: cannot infer type
let x;  // no initializer

// Error: ambiguous type
let x = [];  // empty array

// Suggestion
let x: i32;  // provide type annotation
let x = []: [i32];  // or specify array type
```

**Technical**:
- Add inference failure diagnostics
- Suggest type annotations
- Show inferred types in verbose mode

---

### Phase 4: Testing & Documentation (Days 6-7)
**Goal**: Comprehensive testing and docs

#### Test Cases
1. Basic type inference (literals, calls)
2. Generic type inference
3. Complex expressions
4. Error cases
5. Edge cases (empty arrays, null, etc.)

#### Documentation
1. Update README.md with type inference examples
2. Add type inference guide
3. Update CHANGELOG.md
4. Write migration guide

---

## 🔧 Technical Details

### AST Changes
```zig
// Before
pub const LetDecl = struct {
    name: []const u8,
    type: Type,      // always required
    init: ?Expr,
    is_mut: bool,
};

// After
pub const LetDecl = struct {
    name: []const u8,
    type: ?Type,     // optional!
    init: ?Expr,
    is_mut: bool,
};
```

### TypeChecker Changes
```zig
// New function
fn inferTypeFromExpr(self: *TypeChecker, expr: Expr) !Type {
    switch (expr) {
        .int_literal => return Type.i32,
        .string_literal => return Type.string,
        .call => |call| {
            // Get function return type
            const func = try self.lookupFunction(call.callee);
            return func.return_type;
        },
        .static_method_call => |smc| {
            // Get method return type
            const method = try self.lookupMethod(smc);
            return self.substituteGenerics(method.return_type, smc.type_args);
        },
        // ...
    }
}
```

### Parser Changes
```zig
// Allow optional type annotation
fn parseLetDecl(self: *Parser) !LetDecl {
    _ = try self.consume(.kw_let);
    const is_mut = self.match(.kw_mut);
    const name = try self.consume(.identifier);
    
    // Type annotation is now optional
    const type_annotation = if (self.match(.colon))
        try self.parseType()
    else
        null;
    
    // ...
}
```

---

## 🧪 Test Strategy

### Unit Tests
```paw
// test_type_inference_basic.paw
fn test_literals() {
    let x = 42;
    let y = 3.14;
    let s = "hello";
    assert_eq(typeof(x), "i32");
}

// test_type_inference_generics.paw
fn test_generics() {
    let vec = Vec<i32>::new();
    assert_eq(typeof(vec), "Vec<i32>");
}
```

### Integration Tests
- Compile existing examples without type annotations
- Verify generated C code is identical
- Check error messages for inference failures

---

## 📊 Success Metrics

### Code Quality
- [ ] All existing tests pass
- [ ] 20+ new type inference tests
- [ ] Zero regressions

### Developer Experience
- [ ] Less boilerplate (no redundant type annotations)
- [ ] Clear error messages
- [ ] Helpful diagnostics

### Documentation
- [ ] Complete type inference guide
- [ ] Updated examples
- [ ] Migration guide from v0.1.2

---

## 🚀 Release Checklist

### Code
- [ ] Implement basic inference
- [ ] Implement advanced inference
- [ ] Error handling
- [ ] All tests passing

### Documentation
- [ ] Update README.md
- [ ] Update CHANGELOG.md
- [ ] Write RELEASE_NOTES_v0.1.3.md
- [ ] Update examples

### Testing
- [ ] Unit tests
- [ ] Integration tests
- [ ] Example programs
- [ ] Memory leak check

### Release
- [ ] Create v0.1.3 branch
- [ ] Final testing
- [ ] Git tag
- [ ] GitHub release
- [ ] Announcement

---

## 🎯 Stretch Goals (Optional)

### If time permits:

#### 1. Type Annotations in Errors
```bash
Error: type mismatch
  let x: i32 = "hello";
              ^^^^^^^ expected i32, found string
```

#### 2. Partial Type Inference
```paw
let vec: Vec<_> = Vec::new();  // infer element type
```

#### 3. Type Aliases
```paw
type UserId = i32;
let id: UserId = 42;  // or let id = 42: UserId;
```

---

## 📅 Timeline

| Day | Task | Status |
|-----|------|--------|
| 1-2 | Basic inference (literals, calls) | 🔲 |
| 3-4 | Advanced inference (generics, structs) | 🔲 |
| 5   | Error messages | 🔲 |
| 6-7 | Testing & docs | 🔲 |
| 8   | Polish & release | 🔲 |

---

## 🤝 Getting Started

### For Contributors

1. **Fork** the repository
2. **Create branch**: `git checkout -b feature/type-inference`
3. **Start with**: `src/typechecker.zig`
4. **Add tests**: `tests/test_type_inference.paw`
5. **Submit PR** when ready

### For Users

Once v0.1.3 is released:
```bash
git pull origin main
git checkout v0.1.3
zig build
```

---

## 💡 Resources

- [Hindley-Milner Type Inference](https://en.wikipedia.org/wiki/Hindley%E2%80%93Milner_type_system)
- [Rust Type Inference](https://doc.rust-lang.org/book/ch03-02-data-types.html)
- [TypeScript Type Inference](https://www.typescriptlang.org/docs/handbook/type-inference.html)

---

**Questions?** Open an issue or discussion on GitHub!

**Ready to start?** Let's build v0.1.3! 🚀

