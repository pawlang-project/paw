// src/cli/progress.rs
use std::io::{self, Write};
use std::time::Instant;
use crate::cli::colors::{ColorSupport, ansi};

/// 彩色进度条实现（类似 pip 风格）
pub struct ProgressBar {
    width: usize,
    current: usize,
    total: usize,
    label: String,
    start_time: Instant,
    color_support: ColorSupport,
}

impl ProgressBar {
    pub fn new(width: usize, total: usize, label: String) -> Self {
        Self { 
            width, 
            current: 0, 
            total, 
            label,
            start_time: Instant::now(),
            color_support: ColorSupport::detect(),
        }
    }
    
    pub fn update(&mut self, current: usize) {
        self.current = current;
        self.render();
    }
    
    fn render(&self) {
        if self.total == 0 {
            return;
        }
        
        let progress = (self.current as f64 / self.total as f64).min(1.0);
        let filled = (progress * self.width as f64) as usize;
        
        // 计算速度（每秒处理的项目数）
        let elapsed = self.start_time.elapsed().as_secs_f64();
        let speed = if elapsed > 0.0 { self.current as f64 / elapsed } else { 0.0 };
        
        if self.color_support.is_enabled() {
            self.render_colored(progress, filled, speed);
        } else {
            self.render_plain(progress, filled, speed);
        }
        
        io::stdout().flush().unwrap();
    }
    
    fn render_colored(&self, progress: f64, filled: usize, speed: f64) {
        print!("\r{}{}{} ", ansi::BOLD, self.label, ansi::RESET);
        
        // 绘制简洁的线条式进度条
        for i in 0..self.width {
            if i < filled {
                // 已完成的部分 - 绿色等号
                print!("{}{}{}", ansi::GREEN, "=", ansi::RESET);
            } else if i == filled && filled < self.width {
                // 当前进度位置 - 青色大于号
                print!("{}{}{}", ansi::CYAN, ">", ansi::RESET);
            } else {
                // 未完成的部分 - 灰色减号
                print!("{}{}{}", ansi::BRIGHT_BLACK, "-", ansi::RESET);
            }
        }
        
        // 显示进度百分比和速度
        print!(" {}{:.1}%{}{}", 
               ansi::BLUE, progress * 100.0, ansi::RESET,
               if speed > 0.0 { format!(" {}{:.1}/s{}", ansi::YELLOW, speed, ansi::RESET) } else { String::new() });
    }
    
    fn render_plain(&self, progress: f64, filled: usize, speed: f64) {
        print!("\r{} ", self.label);
        
        // 绘制简洁的简单进度条
        for i in 0..self.width {
            if i < filled {
                print!("=");
            } else if i == filled && filled < self.width {
                print!(">");
            } else {
                print!("-");
            }
        }
        
        print!(" {:.1}%", progress * 100.0);
        if speed > 0.0 {
            print!(" {:.1}/s", speed);
        }
    }
    
    pub fn finish(&self) {
        let elapsed = self.start_time.elapsed().as_secs_f64();
        if self.color_support.is_enabled() {
            println!("\n{}{} completed in {:.2}s{}", 
                     ansi::GREEN, self.label, elapsed, ansi::RESET);
        } else {
            println!("\n{} completed in {:.2}s", self.label, elapsed);
        }
    }
}
