// src/cli/output.rs
use std::path::Path;
use crate::cli::colors::{ColorSupport, ansi};

/// 输出格式化器
pub struct OutputFormatter {
    color_support: ColorSupport,
}

impl OutputFormatter {
    pub fn new() -> Self {
        Self {
            color_support: ColorSupport::detect(),
        }
    }
    
    /// 格式化成功消息
    pub fn success(&self, profile: &str, target: &str, source: &Path, output: &Path, duration: f64, size: &str) {
        let source_rel = source.file_name().unwrap_or(source.as_os_str()).to_string_lossy();
        let output_rel = output.file_name().unwrap_or(output.as_os_str()).to_string_lossy();
        
        if self.color_support.is_enabled() {
            println!(
                "{}build {} [{}]  {} -> {}  ({}s, {}){}",
                ansi::GREEN,
                profile,
                target,
                source_rel,
                output_rel,
                format!("{:.2}", duration),
                size,
                ansi::RESET
            );
        } else {
            println!(
                "build {} [{}]  {} -> {}  ({}s, {})",
                profile,
                target,
                source_rel,
                output_rel,
                format!("{:.2}", duration),
                size
            );
        }
    }
    
    /// 格式化错误消息
    pub fn error(&self, message: &str) {
        if self.color_support.is_enabled() {
            eprintln!("{}{}{}", ansi::RED, message, ansi::RESET);
        } else {
            eprintln!("{}", message);
        }
    }
    
    /// 格式化警告消息
    pub fn warning(&self, message: &str) {
        if self.color_support.is_enabled() {
            eprintln!("{}{}{}", ansi::YELLOW, message, ansi::RESET);
        } else {
            eprintln!("{}", message);
        }
    }
    
    /// 格式化信息消息
    pub fn info(&self, message: &str) {
        if self.color_support.is_enabled() {
            eprintln!("{}{}{}", ansi::BLUE, message, ansi::RESET);
        } else {
            eprintln!("{}", message);
        }
    }
    
    /// 计算人类可读的文件大小
    pub fn human_size(bytes: u64) -> String {
        if bytes >= 1_048_576 {
            format!("{:.1} MB", bytes as f64 / 1_048_576.0)
        } else if bytes >= 1024 {
            format!("{:.1} KB", bytes as f64 / 1024.0)
        } else {
            format!("{} B", bytes)
        }
    }
}
