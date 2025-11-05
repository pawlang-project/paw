//===--- compiler.h - Compiler Main Class -----------------------*- C++ -*-===//
//
// PawLang Compiler - Main Compiler Class
//
//===----------------------------------------------------------------------===//

#ifndef PAW_COMPILER_H
#define PAW_COMPILER_H

#include "options.h"
#include "diagnostics/diagnostic_engine.h"
#include "middleend/types/type_system.h"
#include "middleend/symbol/symbol_table.h"
#include "pass/pass_manager.h"
#include "pass/pass_context.h"

#include <memory>
#include <string>

namespace pawc {

/// Compiler - compilermainclass
///
/// Responsible for coordinating entire compilation processcompilationflow，usePassManagermanageAllPass
class Compiler {
public:
    explicit Compiler(const CompilerOptions& options);
    ~Compiler();  // explicitstyle/formdestruct/destructionwith/tocleanupPassContextcache
    
    /// compile/compilationsourcefile
    /// \param source_file sourcefilepath
    /// \return true indicates success
    bool compile(const std::string& source_file);
    
    /// Get diagnostic engine
    DiagnosticEngine* getDiagnostics() { return diagnostics_.get(); }
    
    /// gettypessystem
    TypeSystem* getTypeSystem() { return type_system_.get(); }
    
    /// getsymboltable
    SymbolTable* getSymbolTable() { return symbol_table_.get(); }
    
private:
    CompilerOptions options_;
    
    // Core components
    std::unique_ptr<DiagnosticEngine> diagnostics_;
    std::unique_ptr<TypeSystem> type_system_;
    std::unique_ptr<SymbolTable> symbol_table_;
    std::unique_ptr<PassContext> pass_context_;
    std::unique_ptr<PassManager> pass_manager_;
    
    // initializecompilercomponent
    void initialize();
    
    // configurePassflow
    void setupPasses();
    
    // readsourcefile
    std::string readSourceFile(const std::string& filename);
    
    // processcompile/compilationresults
    bool processResults();
};

} // namespace pawc

#endif // PAW_COMPILER_H
