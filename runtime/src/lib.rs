mod io;
mod mem;

pub use io::*;
pub use mem::*;


// Windows
// # 安装目标（如未安装）
// rustup target add x86_64-pc-windows-gnu
// # 构建静态库
// cargo build -p runtime --release --target x86_64-pc-windows-gnu
// # 产物位置：
// #   runtime/target/x86_64-pc-windows-gnu/release/libruntime.a


// Linux
// rustup target add x86_64-unknown-linux-gnu
// cargo build -p runtime --release --target x86_64-unknown-linux-gnu
// # 产物：
// #   runtime/target/x86_64-unknown-linux-gnu/release/libruntime.a

// macOS
// rustup target add x86_64-apple-darwin aarch64-apple-darwin
// cargo build -p runtime --release --target x86_64-apple-darwin
// cargo build -p runtime --release --target aarch64-apple-darwin
// # 产物：
// #   runtime/target/<triple>/release/libruntime.a