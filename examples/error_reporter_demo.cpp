// 演示新的错误报告系统
// 这个文件展示了如何使用ErrorReporter

#include "pawc/error_reporter.h"
#include <iostream>

int main() {
    pawc::ErrorReporter reporter;
    
    // 设置源代码
    std::string code = R"(fn main() -> i32 {
    let x: i32 = "hello";  // 类型不匹配
    return x;
})";
    
    reporter.setSourceCode("example.paw", code);
    
    // 报告一个错误
    pawc::SourceLocation loc("example.paw", 2, 18);
    std::vector<pawc::ErrorHint> hints = {
        pawc::ErrorHint("expected type 'i32', found type 'string'"),
        pawc::ErrorHint("try using an integer literal like '42' instead")
    };
    
    reporter.reportError("mismatched types", loc, hints);
    
    // 报告一个警告
    pawc::SourceLocation warn_loc("example.paw", 2, 9);
    reporter.reportWarning("unused variable 'x'", warn_loc, {
        pawc::ErrorHint("consider prefixing with an underscore: '_x'")
    });
    
    // 打印摘要
    reporter.printSummary();
    
    return reporter.hasErrors() ? 1 : 0;
}

