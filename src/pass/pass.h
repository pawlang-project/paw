//===--- pass.h - Pass Base Class (CRTP) -------------------------*- C++ -*-===//
//
// PawLang Compiler - Pass System Core
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PASS_H
#define PAW_PASS_H

#include <string>
#include <vector>
#include <chrono>

namespace pawc {

// Forward declarations
class PassContext;

/// PassResult - Pass执行结果
struct PassResult {
    bool success = true;
    std::string message;
    double execution_time_ms = 0.0;
    
    PassResult() = default;
    PassResult(bool s, const std::string& msg = "") 
        : success(s), message(msg) {}
};

/// PassBase - Pass基类（使用CRTP优化）
///
/// 使用CRTP（Curiously Recurring Template Pattern）来消除虚函数调用开销
/// 
/// 使用方法：
/// ```cpp
/// class MyPass : public PassBase<MyPass> {
/// public:
///     static std::string name() { return "MyPass"; }
///     
///     PassResult runImpl(PassContext* context) {
///         // Pass实现
///         return PassResult{true, "Success"};
///     }
/// };
/// ```
template<typename Derived>
class PassBase {
public:
    /// 运行Pass（CRTP静态多态）
    PassResult run(PassContext* context) {
        auto start = std::chrono::high_resolution_clock::now();
        
        PassResult result = static_cast<Derived*>(this)->runImpl(context);
        
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        result.execution_time_ms = time_ms;
        return result;
    }
    
    virtual ~PassBase() = default;
};

} // namespace pawc

#endif // PAW_PASS_H
