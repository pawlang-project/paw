//===--- type_system.h - Type System Manager ---------------------*- C++ -*-===//
//
// PawLang Compiler - Type System
//
//===----------------------------------------------------------------------===//

#ifndef PAW_TYPE_SYSTEM_H
#define PAW_TYPE_SYSTEM_H

#include "type.h"
#include "primitive_types.h"
#include "composite_types.h"
#include "generic_types.h"
#include "generic_template.h"
#include "type_registry.h"
#include "utils/arena.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace pawc {

/// TypeSystem - typessystemmanager
///
/// responsibilities：
/// - createandmanageAlltypes
/// - typeslookupandregister
/// - typescompatibilitycheck
class TypeSystem {
public:
    TypeSystem();
    ~TypeSystem();  // needself/fromdefinitiondestruct/destructionfuturecleanupArenainobject
    
    // Non-copyable
    TypeSystem(const TypeSystem&) = delete;
    TypeSystem& operator=(const TypeSystem&) = delete;
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // basetypesget（18types/kinds）
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    Type* getI8Type();
    Type* getI16Type();
    Type* getI32Type();
    Type* getI64Type();
    Type* getI128Type();
    
    Type* getU8Type();
    Type* getU16Type();
    Type* getU32Type();
    Type* getU64Type();
    Type* getU128Type();
    
    Type* getF8Type();
    Type* getF16Type();
    Type* getF32Type();
    Type* getF64Type();
    Type* getF128Type();
    
    Type* getBoolType();
    Type* getCharType();
    Type* getStringType();
    Type* getVoidType();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // compositetypescreate
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    ArrayType* getArrayType(Type* element, size_t size);
    SliceType* getSliceType(Type* element);
    TupleType* getTupleType(std::vector<Type*> elements);
    
    OptionalType* getOptionalType(Type* inner);
    ResultType* getResultType(Type* ok_type);
    
    ReferenceType* getReferenceType(Type* pointee, bool is_mutable);
    FunctionType* getFunctionType(std::vector<Type*> params, Type* ret);
    GenericType* getGenericType(const std::string& name);
    SelfType* getSelfType();
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // User-defined types
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    void registerStruct(StructType* type);
    void registerEnum(EnumType* type);
    void registerInterface(InterfaceType* type);
    
    Type* lookupType(const std::string& name);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // genericsystem
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    /// registergenerictemplate
    void registerGenericTemplate(GenericTemplate* tmpl);
    
    /// lookupgenerictemplate
    GenericTemplate* lookupGenericTemplate(const std::string& name);
    
    /// instantiationgenerictypes
    /// @param template_name templatename（like/such as"Option"）
    /// @param type_args typesparameter（like/such as[i32]）
    /// @return instantiationof/thetypes（like/such asOption_i32）
    Type* instantiateGeneric(const std::string& template_name,
                            const std::vector<Type*>& type_args);
    
    /// typeparametersubstitution（public，for/provideTypeCheckeruse）
    Type* substituteType(Type* type, const std::unordered_map<std::string, Type*>& substitution);
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // typequery
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    bool isAssignable(Type* from, Type* to);
    bool equals(Type* t1, Type* t2);
    
private:
    void initializePrimitiveTypes();
    
    Arena arena_;
    std::unordered_map<std::string, Type*> primitive_cache_;
    std::unordered_map<std::string, Type*> named_types_;
    std::vector<std::unique_ptr<Type>> type_pool_;
    
    // genericsystem
    std::unordered_map<std::string, GenericTemplate*> generic_templates_;
    std::unordered_map<std::string, Type*> instantiated_types_;
    std::vector<std::unique_ptr<GenericTemplate>> template_pool_;
};

} // namespace pawc

#endif // PAW_TYPE_SYSTEM_H
