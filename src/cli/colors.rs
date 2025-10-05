// src/cli/colors.rs
use std::env;

/// 颜色支持检测和管理
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ColorSupport {
    /// 支持彩色输出
    Enabled,
    /// 不支持彩色输出
    Disabled,
}

impl ColorSupport {
    /// 检测当前终端是否支持颜色
    pub fn detect() -> Self {
        // 检查 NO_COLOR 环境变量
        if env::var("NO_COLOR").is_ok() {
            return Self::Disabled;
        }
        
        // 检查 FORCE_COLOR 环境变量
        if let Ok(force) = env::var("FORCE_COLOR") {
            return if force == "0" { Self::Disabled } else { Self::Enabled };
        }
        
        // 检查 TERM 环境变量
        if let Ok(term) = env::var("TERM") {
            if term == "dumb" {
                return Self::Disabled;
            }
        }
        
        // Windows 检查
        #[cfg(windows)]
        {
            if let Ok(ansi) = env::var("ANSICON") {
                if ansi.is_empty() {
                    return Self::Disabled;
                }
            }
        }
        
        // 默认支持颜色（大多数现代终端都支持）
        Self::Enabled
    }
    
    /// 是否支持颜色
    pub fn is_enabled(self) -> bool {
        matches!(self, Self::Enabled)
    }
}

/// ANSI 颜色代码
pub mod ansi {
    pub const RESET: &str = "\x1b[0m";
    pub const BOLD: &str = "\x1b[1m";
    pub const DIM: &str = "\x1b[2m";
    
    // 前景色
    pub const BLACK: &str = "\x1b[30m";
    pub const RED: &str = "\x1b[31m";
    pub const GREEN: &str = "\x1b[32m";
    pub const YELLOW: &str = "\x1b[33m";
    pub const BLUE: &str = "\x1b[34m";
    pub const MAGENTA: &str = "\x1b[35m";
    pub const CYAN: &str = "\x1b[36m";
    pub const WHITE: &str = "\x1b[37m";
    
    // 亮色
    pub const BRIGHT_BLACK: &str = "\x1b[90m";
    pub const BRIGHT_RED: &str = "\x1b[91m";
    pub const BRIGHT_GREEN: &str = "\x1b[92m";
    pub const BRIGHT_YELLOW: &str = "\x1b[93m";
    pub const BRIGHT_BLUE: &str = "\x1b[94m";
    pub const BRIGHT_MAGENTA: &str = "\x1b[95m";
    pub const BRIGHT_CYAN: &str = "\x1b[96m";
    pub const BRIGHT_WHITE: &str = "\x1b[97m";
}
