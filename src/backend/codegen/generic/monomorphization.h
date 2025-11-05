//===--- monomorphization.h - Generic Monomorphization ----------*- C++ -*-===//
//
// genericmonomorphization - LLVM CodeGenlevelof/thegenericinstantiation
// willmonomorphizationback/afterof/theASTconvertis/asLLVM IR
//
//===----------------------------------------------------------------------===//

#ifndef PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H
#define PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H

#include <string>
#include <vector>
#include <unordered_map>

namespace llvm {
    class Type;
    class Value;
    class Function;
    class StructType;
}

namespace pawc {

class Type;
class CodeGenContext;

/// GenericMonomorphization - genericmonomorphizationutility（CodeGenlevel）
///
/// in/atLLVM IRgeneratestageprocessgenerictypesof/theinstantiation
/// Works together with MonomorphizationPass:
///   - MonomorphizationPass: ASTlevelof/themonomorphization（compile/compilationearlyearly）
///   - GenericMonomorphization: LLVMlevelof/thetypesmap（CodeGenstage）
class GenericMonomorphization {
    CodeGenContext* context_;
    
    // generictypesinstancecache
    std::unordered_map<std::string, llvm::Type*> type_cache_;
    std::unordered_map<std::string, llvm::Function*> function_cache_;
    
public:
    explicit GenericMonomorphization(CodeGenContext* context);
    
    /// willmonomorphizationback/afterof/thetypesmaptoLLVMtypes
    /// @param paw_type monomorphizationback/afterof/thePawLangtypes
    /// @param instance_name instancename (e.g./for example "Box_i32")
    /// @return LLVMtypes
    llvm::Type* mapMonomorphizedType(Type* paw_type, 
                                     const std::string& instance_name);
    
    /// getorcreatemonomorphizationfunction
    /// @param instance_name functioninstancename
    /// @param param_types parametertypeslist
    /// @param return_type returntypes
    /// @return LLVMfunction
    llvm::Function* getOrCreateMonomorphizedFunction(
        const std::string& instance_name,
        const std::vector<llvm::Type*>& param_types,
        llvm::Type* return_type);
    
    /// Clear cache
    void clearCache();
};

} // namespace pawc

#endif // PAW_CODEGEN_GENERIC_MONOMORPHIZATION_H

