# 📋 PawLang v0.2.0 Release Plan

## 🎯 Version Theme: Standard Library & Language Features

**Planned Release**: November 2025  
**Focus**: Complete standard library implementation and language enhancements  
**Builds On**: v0.1.9 developer tools and error reporting foundation  

---

## 📚 Standard Library Implementation

### 1. JSON Parser (High Priority)

**API** (Already defined in v0.1.9):
- `parse(json_str: string) -> JsonValue`
- `stringify(value: JsonValue) -> string`
- `is_valid(json_str: string) -> bool`
- `get_field(obj: JsonValue, key: string) -> JsonValue`
- `get_index(arr: JsonValue, index: i32) -> JsonValue`

**Implementation Tasks**:
- [ ] Lexer for JSON tokens
- [ ] Recursive descent parser
- [ ] JsonValue enum variant handling
- [ ] Object and Array construction
- [ ] String escaping and unescaping
- [ ] Number parsing (int and float)
- [ ] Boolean and null handling
- [ ] Error handling and reporting
- [ ] Serialization (stringify)
- [ ] Pretty printing option
- [ ] Comprehensive test suite

**Example Usage**:
```paw
import json.{parse, stringify};

let json_str = '{"name": "Alice", "age": 30}';
let parsed = parse(json_str);
let name = get_field(parsed, "name");
print(name);  // "Alice"
```

### 2. File System API (High Priority)

**API** (Already defined in v0.1.9):
- `read_file(path: string) -> string`
- `write_file(path: string, content: string) -> bool`
- `exists(path: string) -> bool`
- `create_dir(path: string) -> bool`
- `remove_file(path: string) -> bool`
- `read_dir(path: string) -> Vec<string>`
- `file_size(path: string) -> i64`
- `join_path(base: string, part: string) -> string`

**Implementation Tasks**:
- [ ] Cross-platform file I/O (via Zig std.fs)
- [ ] Error handling (file not found, permission denied, etc.)
- [ ] Directory operations
- [ ] Path manipulation utilities
- [ ] File metadata queries
- [ ] Safety checks
- [ ] Comprehensive test suite
- [ ] Platform-specific path handling

**Example Usage**:
```paw
import fs.{read_file, write_file, exists};

if (exists("config.json")) {
    let content = read_file("config.json");
    print(content);
}

write_file("output.txt", "Hello, PawLang!");
```

---

## 🔤 Language Feature Enhancements

### 3. Enhanced Type Inference (Medium Priority)

**Planned Features**:
- [ ] Array literal type inference: `[1, 2, 3]` → `Vec<i32>`
- [ ] Function return type inference
- [ ] Generic type parameter inference: `identity(42)` → `T = i32`
- [ ] Struct field type inference from initialization
- [ ] Binary operation result type inference
- [ ] Match expression type unification

**Test Cases**: Already added in `tests/types/inference_v2.paw`

**Example**:
```paw
// Before (v0.1.9): Must specify type
let arr: Vec<i32> = [1, 2, 3];

// After (v0.2.0): Type inferred
let arr = [1, 2, 3];  // Automatically Vec<i32>
```

### 4. Generic System Improvements (Medium Priority)

**Planned Features**:
- [ ] Generic trait bounds
- [ ] Where clauses for constraints
- [ ] Associated types
- [ ] Generic method constraints
- [ ] Better error messages for generic mismatches
- [ ] Generic type aliases

**Example**:
```paw
// Generic with trait bounds
fn sort<T: Comparable>(arr: Vec<T>) -> Vec<T> {
    // Implementation
}

// Where clauses
fn complex<T, U>(a: T, b: U) -> T 
where T: Display, U: Clone {
    // Implementation
}
```

---

## ⚡ Performance Optimizations

### 5. Parallel Type Checking (Low Priority, v0.2.0+)

**Goal**: Utilize multi-core CPUs for faster compilation

**Approach**:
- Analyze function/type dependencies
- Parallel type checking for independent modules
- Thread pool for type checking tasks
- Synchronization for shared symbol table

**Expected Impact**: 20-30% faster compilation on multi-core systems

### 6. AST Caching (Low Priority, v0.2.0+)

**Goal**: Avoid re-parsing unchanged files

**Approach**:
- Cache parsed AST with file hash
- Incremental re-parsing
- Dependency tracking
- Cache invalidation strategy

**Expected Impact**: 50-70% faster recompilation

---

## 🧪 Testing Strategy

### Test Coverage Goals
- [ ] JSON parser: 100 test cases
- [ ] File system: 50 test cases (all platforms)
- [ ] Type inference: 30 test cases
- [ ] Generics: 20 additional test cases

### CI/CD
- All 8 platforms must pass
- Performance benchmarks
- Memory leak detection
- Regression testing

---

## 📅 Development Timeline

### Phase 1: Standard Library (2-3 weeks)
- Week 1: JSON parser implementation
- Week 2: File system API implementation
- Week 3: Testing and bug fixes

### Phase 2: Language Features (1-2 weeks)
- Week 4: Type inference enhancements
- Week 5: Generic system improvements

### Phase 3: Polish & Release (1 week)
- Week 6: Documentation, testing, release preparation

**Total Estimated Time**: 4-6 weeks

---

## 🎯 Success Criteria

v0.2.0 will be considered complete when:
- ✅ JSON parser passes all tests
- ✅ File system API works on all 8 platforms
- ✅ Type inference handles common cases
- ✅ Generic system supports trait bounds
- ✅ All CI platforms pass
- ✅ Documentation is complete
- ✅ No memory leaks
- ✅ Performance is acceptable

---

## 📝 Notes

**Why defer to v0.2.0?**:
1. **Quality First**: Standard library needs thorough testing
2. **Cross-Platform**: File I/O must work perfectly on all platforms
3. **User Expectations**: JSON parser should be production-ready
4. **v0.1.9 Coherence**: Current release is already excellent and complete

**v0.1.9 Achievement**:
- Excellent developer tools
- Significantly improved error messages
- VSCode support
- REPL foundation
- Clear roadmap

This solid foundation makes v0.2.0 development much easier!

---

**Status**: 📋 Planning  
**Target**: November 2025  
**Dependencies**: v0.1.9 release complete  

