# 🐾 PawLang VSCode Extension

Syntax highlighting and language support for PawLang.

## Features

- ✅ **Syntax Highlighting**: Full syntax highlighting for `.paw` files
- ✅ **Auto-Completion**: Bracket and quote auto-closing
- ✅ **Smart Indentation**: Automatic indentation rules
- ✅ **Comment Support**: Line and block comments

## Installation

### Option 1: Manual Installation (Current)

1. Copy this folder to your VSCode extensions directory:
   - **macOS/Linux**: `~/.vscode/extensions/paw-language-support-0.1.9/`
   - **Windows**: `%USERPROFILE%\.vscode\extensions\paw-language-support-0.1.9\`

2. Restart VSCode

3. Open any `.paw` file - syntax highlighting will be active!

### Option 2: From Marketplace (Coming Soon)

```bash
# Future: Install from VSCode marketplace
code --install-extension pawlang.paw-language-support
```

## Supported Syntax

### Keywords
- **Control Flow**: `if`, `else`, `while`, `for`, `return`, `break`, `continue`, `match`
- **Declarations**: `let`, `mut`, `fn`, `type`, `struct`, `enum`, `pub`, `const`
- **Other**: `as`, `in`, `self`, `import`

### Types
- **Primitives**: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`, `bool`, `string`, `void`
- **Custom Types**: Pascal case (e.g., `Vec`, `Box`, `Result`)

### Operators
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logical**: `&&`, `||`, `!`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`
- **Arrow**: `->`, `=>`

### Comments
- **Line**: `// comment`
- **Block**: `/* comment */`

## Example

```paw
// Function with generics
fn add<T>(a: T, b: T) -> T {
    return a + b;
}

// Struct definition
type Point = struct {
    x: i32;
    y: i32;
};

// Entry point
fn main() {
    let p = Point { x: 10, y: 20 };
    let sum = add(p.x, p.y);
    print(sum);
}
```

## Development

To modify the syntax highlighting:

1. Edit `paw.tmLanguage.json`
2. Test in VSCode (Developer: Inspect Editor Tokens and Scopes)
3. Reload window to see changes

## Issues & Feedback

Report issues at: https://github.com/pawlang-project/paw/issues

## License

MIT License - see [LICENSE](../../LICENSE)

---

**Made with ❤️ for PawLang developers**

