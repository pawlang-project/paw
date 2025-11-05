//===--- semantic_context.h - Semantic Analysis Context ---------*- C++ -*-===//
//
// semantic analysiscontext - share/sharedof/thesemanticanalysisstateandresourcessource
//
//===----------------------------------------------------------------------===//

#ifndef PAW_SEMANTIC_CONTEXT_H
#define PAW_SEMANTIC_CONTEXT_H

#include "middleend/types/type_system.h"
#include "middleend/symbol/symbol_table.h"
#include "diagnostics/diagnostic_engine.h"

namespace pawc {

/// SemanticContext - semanticanalysiscontext
///
/// in/atdifferentof/thesemanticanalysiscomponent（TypeChecker, TypeInference, InterfaceValidatoretc）
/// betweenshare/sharedstateandresourcessource
class SemanticContext {
    TypeSystem* type_system_;
    SymbolTable* symbol_table_;
    DiagnosticEngine* diagnostics_;
    
    // currentanalysisstate
    Type* current_function_return_type_ = nullptr;
    bool in_loop_ = false;
    Type* current_self_type_ = nullptr;  // currentinterfaceimplementationof/thetypes
    
public:
    SemanticContext(TypeSystem* ts, SymbolTable* st, DiagnosticEngine* diag)
        : type_system_(ts), symbol_table_(st), diagnostics_(diag) {}
    
    // resourcessourcevisit
    TypeSystem* getTypeSystem() const { return type_system_; }
    SymbolTable* getSymbolTable() const { return symbol_table_; }
    DiagnosticEngine* getDiagnostics() const { return diagnostics_; }
    
    // state management
    void setCurrentFunctionReturnType(Type* type) { current_function_return_type_ = type; }
    Type* getCurrentFunctionReturnType() const { return current_function_return_type_; }
    
    void setInLoop(bool in_loop) { in_loop_ = in_loop; }
    bool isInLoop() const { return in_loop_; }
    
    void setCurrentSelfType(Type* type) { current_self_type_ = type; }
    Type* getCurrentSelfType() const { return current_self_type_; }
};

} // namespace pawc

#endif // PAW_SEMANTIC_CONTEXT_H

