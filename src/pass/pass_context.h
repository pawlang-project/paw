//===--- pass_context.h - Pass Shared Context --------------------*- C++ -*-===//
//
// PawLang Compiler - Pass Context
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PASS_CONTEXT_H
#define PAW_PASS_CONTEXT_H

#include <any>
#include <unordered_map>
#include <string>

namespace pawc {

// Forward declarations
class DiagnosticEngine;
class TypeSystem;
class SymbolTable;
class ModuleLoader;
class Stmt;

/// PassContext - Pass之间共享的上下文
///
/// 提供:
/// - 诊断引擎访问
/// - 类型系统访问
/// - 符号表访问
/// - Pass结果缓存
/// - 编译选项
class PassContext {
public:
    PassContext(DiagnosticEngine* diag, 
                TypeSystem* types,
                SymbolTable* symbols)
        : diagnostics_(diag), type_system_(types), symbol_table_(symbols) {}
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 核心系统访问
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    DiagnosticEngine* getDiagnostics() const { return diagnostics_; }
    TypeSystem* getTypeSystem() const { return type_system_; }
    SymbolTable* getSymbolTable() const { return symbol_table_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Pass结果缓存
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// 缓存Pass分析结果
    template<typename T>
    void cacheAnalysisResult(const std::string& pass_name, const T& result) {
        analysis_cache_[pass_name] = result;
    }
    
    /// 获取缓存的分析结果
    template<typename T>
    bool getCachedResult(const std::string& pass_name, T& result) {
        auto it = analysis_cache_.find(pass_name);
        if (it != analysis_cache_.end()) {
            try {
                result = std::any_cast<T>(it->second);
                return true;
            } catch (const std::bad_any_cast&) {
                return false;
            }
        }
        return false;
    }
    
    /// 清除缓存
    void clearCache() { analysis_cache_.clear(); }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // 编译选项
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void setOptLevel(int level) { opt_level_ = level; }
    int getOptLevel() const { return opt_level_; }
    
    void setVerbose(bool v) { verbose_ = v; }
    bool isVerbose() const { return verbose_; }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // AST访问（用于CodeGen等Pass）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void setAST(const std::vector<std::unique_ptr<Stmt>>* ast) { ast_ = ast; }
    const std::vector<std::unique_ptr<Stmt>>* getAST() const { return ast_; }
    
private:
    DiagnosticEngine* diagnostics_;
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    
    // AST（由Parser生成，传递给后续Pass）
    const std::vector<std::unique_ptr<Stmt>>* ast_ = nullptr;
    
    // Pass分析结果缓存
    std::unordered_map<std::string, std::any> analysis_cache_;
    
    // 编译选项
    int opt_level_ = 0;
    bool verbose_ = false;
};

} // namespace pawc

#endif // PAW_PASS_CONTEXT_H
