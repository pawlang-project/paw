//! Diagnostic - Enhanced Error Reporting System
//!
//! This module provides Rust-style error messages with:
//! - Source code locations
//! - Colorful output
//! - Code snippets
//! - Smart suggestions
//!
//! Example output:
//!   error: cannot cast from 'string' to 'i32'
//!      --> app.paw:5:14
//!      |
//!    5 |     let x = name as i32;
//!      |              ^^^^ invalid cast
//!      |
//!      = note: string cannot be cast to numeric types
//!      = help: try parsing instead

const std = @import("std");
const Token = @import("token.zig").Token;

// ============================================================================
// Span - Source Code Location
// ============================================================================

/// Represents a span of source code
pub const Span = struct {
    filename: []const u8,
    start_line: usize,
    start_col: usize,
    end_line: usize,
    end_col: usize,
    
    pub fn init(filename: []const u8, start_line: usize, start_col: usize, end_line: usize, end_col: usize) Span {
        return Span{
            .filename = filename,
            .start_line = start_line,
            .start_col = start_col,
            .end_line = end_line,
            .end_col = end_col,
        };
    }
    
    /// Create span from a single position
    pub fn fromPosition(filename: []const u8, line: usize, col: usize) Span {
        return Span.init(filename, line, col, line, col);
    }
};

// ============================================================================
// Diagnostic Level
// ============================================================================

pub const DiagnosticLevel = enum {
    Error,
    Warning,
    Note,
    Help,
    
    pub fn toString(self: DiagnosticLevel) []const u8 {
        return switch (self) {
            .Error => "error",
            .Warning => "warning",
            .Note => "note",
            .Help => "help",
        };
    }
    
    pub fn color(self: DiagnosticLevel) []const u8 {
        return switch (self) {
            .Error => "\x1b[1;31m",    // Bold Red
            .Warning => "\x1b[1;33m",  // Bold Yellow
            .Note => "\x1b[1;36m",     // Bold Cyan
            .Help => "\x1b[1;32m",     // Bold Green
        };
    }
};

// ============================================================================
// Diagnostic - Error/Warning/Note/Help
// ============================================================================

pub const Diagnostic = struct {
    level: DiagnosticLevel,
    message: []const u8,
    span: ?Span,
    notes: []const []const u8,
    help: ?[]const u8,
    
    pub fn init(
        level: DiagnosticLevel,
        message: []const u8,
        span: ?Span,
        notes: []const []const u8,
        help: ?[]const u8,
    ) Diagnostic {
        return Diagnostic{
            .level = level,
            .message = message,
            .span = span,
            .notes = notes,
            .help = help,
        };
    }
    
    /// Create diagnostic from token position
    pub fn fromToken(
        level: DiagnosticLevel,
        message: []const u8,
        token: Token,
        notes: []const []const u8,
        help: ?[]const u8,
    ) Diagnostic {
        const span = Span.fromPosition(token.filename, token.line, token.column);
        return Diagnostic.init(level, message, span, notes, help);
    }
    
    /// Create simple error (no span, no notes)
    pub fn simpleError(message: []const u8) Diagnostic {
        return Diagnostic.init(.Error, message, null, &[_][]const u8{}, null);
    }
    
    /// Print diagnostic to stderr with colors and source code snippet
    pub fn print(self: Diagnostic, allocator: std.mem.Allocator) !void {
        const stderr = std.io.getStdErr().writer();
        
        // Print main error message with color
        try stderr.print("{s}{s}\x1b[0m: {s}\n", .{
            self.level.color(),
            self.level.toString(),
            self.message,
        });
        
        // Print source location if available
        if (self.span) |span| {
            try stderr.print("   {s}--> {s}:{d}:{d}\x1b[0m\n", .{
                "\x1b[1;36m",  // Cyan
                span.filename,
                span.start_line,
                span.start_col,
            });
            
            // Print source code snippet
            try printSourceSnippet(allocator, span, stderr);
        }
        
        // Print notes
        for (self.notes) |note| {
            try stderr.print("   {s}= note\x1b[0m: {s}\n", .{
                "\x1b[1;36m",  // Cyan
                note,
            });
        }
        
        // Print help
        if (self.help) |help| {
            try stderr.print("   {s}= help\x1b[0m: {s}\n", .{
                "\x1b[1;32m",  // Green
                help,
            });
        }
        
        try stderr.print("\n", .{});
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Print source code snippet with error marker
fn printSourceSnippet(allocator: std.mem.Allocator, span: Span, writer: anytype) !void {
    // Read source file
    const source = std.fs.cwd().readFileAlloc(
        allocator,
        span.filename,
        10 * 1024 * 1024,
    ) catch {
        // If we can't read the file, just skip the snippet
        return;
    };
    defer allocator.free(source);
    
    // Split into lines
    var lines = std.mem.splitScalar(u8, source, '\n');
    var current_line: usize = 1;
    
    // Find and print the relevant line
    while (lines.next()) |line| {
        if (current_line == span.start_line) {
            // Print line number gutter
            try writer.print("   {s}|\x1b[0m\n", .{"\x1b[1;36m"});  // Cyan
            
            // Print line number and code
            try writer.print(" {s}{d:>3} |\x1b[0m {s}\n", .{
                "\x1b[1;36m",  // Cyan
                current_line,
                line,
            });
            
            // Print error marker
            try writer.print("   {s}|", .{"\x1b[1;36m"});  // Cyan
            
            // Calculate spaces before ^
            var i: usize = 0;
            while (i < span.start_col) : (i += 1) {
                try writer.print(" ", .{});
            }
            
            // Print ^ markers
            try writer.print("\x1b[1;31m", .{});  // Red
            const marker_len = if (span.end_col > span.start_col) 
                span.end_col - span.start_col + 1 
            else 
                1;
            i = 0;
            while (i < marker_len) : (i += 1) {
                try writer.print("^", .{});
            }
            
            try writer.print("\x1b[0m\n", .{});
            break;
        }
        current_line += 1;
    }
}

// ============================================================================
// Color Helpers
// ============================================================================

pub const Color = struct {
    pub const Reset = "\x1b[0m";
    pub const Bold = "\x1b[1m";
    
    pub const Red = "\x1b[31m";
    pub const Green = "\x1b[32m";
    pub const Yellow = "\x1b[33m";
    pub const Blue = "\x1b[34m";
    pub const Magenta = "\x1b[35m";
    pub const Cyan = "\x1b[36m";
    
    pub const BoldRed = "\x1b[1;31m";
    pub const BoldGreen = "\x1b[1;32m";
    pub const BoldYellow = "\x1b[1;33m";
    pub const BoldBlue = "\x1b[1;34m";
    pub const BoldCyan = "\x1b[1;36m";
};

