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

/// PassResult - Passexecuteresults
struct PassResult {
    bool success = true;
    std::string message;
    double execution_time_ms = 0.0;
    
    PassResult() = default;
    PassResult(bool s, const std::string& msg = "") 
        : success(s), message(msg) {}
};

/// PassBase - Pass base class (uses CRTP optimization)
///
/// Uses CRTP (Curiously Recurring Template Pattern) to eliminate virtual function call overhead
/// 
/// usemethod：
/// ```cpp
/// class MyPass : public PassBase<MyPass> {
/// public:
///     static std::string name() { return "MyPass"; }
///     
///     PassResult runImpl(PassContext* context) {
///         // Passimplementation
///         return PassResult{true, "Success"};
///     }
/// };
/// ```
template<typename Derived>
class PassBase {
public:
    /// runPass (CRTP static polymorphism)
    PassResult run(PassContext* context) {
        auto start = std::chrono::high_resolution_clock::now();
        
        PassResult results = static_cast<Derived*>(this)->runImpl(context);
        
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        results.execution_time_ms = time_ms;
        return results;
    }
    
    virtual ~PassBase() = default;
};

} // namespace pawc

#endif // PAW_PASS_H
