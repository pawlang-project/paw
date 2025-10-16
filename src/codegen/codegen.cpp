/**
 * @file codegen.cpp
 * @brief PawLang LLVM代码生成器实现
 * 
 * 负责将AST转换为LLVM IR，支持完整的PawLang语言特性：
 * - 泛型系统（函数、Struct、Enum）- 完整单态化
 * - 泛型struct内部方法（静态、实例）
 * - 模块系统和跨模块调用
 * - 错误处理（T?类型、?操作符、ok/err）
 * - 模式匹配（is表达式、match表达式）
 * - 类型推导和类型转换
 * - Struct引用语义（统一指针传递）
 * - 数组（定长、不定长、多维）
 * - 字符串和字符类型
 * - 循环控制（4种loop + break/continue）
 * 
 * 文件组织：
 * - 第1部分：初始化和核心接口
 * - 第2部分：类型转换系统
 * - 第3部分：表达式生成
 * - 第4部分：语句生成
 * - 第5部分：泛型实例化
 * 
 * @note 文件大小：3936行（计划未来拆分）
 * @todo 拆分为多个文件：expr、stmt、type、struct、match
 * 
 * @version 0.2.1
 * @date 2025-10-16
 */

#include "codegen.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/IR/LegacyPassManager.h"
#include <iostream>

namespace pawc {

// ============================================================================
// 第1部分：初始化和核心接口
// ============================================================================

/**
 * @brief 构造CodeGenerator（单文件模式）
 * @param module_name 模块名称
 * 
 * 初始化LLVM组件：
 * - LLVMContext：独立的编译上下文
 * - Module：IR模块容器
 * - IRBuilder：IR指令构建器
 * - Builtins：内置函数管理器
 * - DataLayout：目标平台数据布局（确保正确对齐）
 */
CodeGenerator::CodeGenerator(const std::string& module_name)
    : current_function_(nullptr), current_function_return_type_(nullptr),
      current_struct_(nullptr), current_struct_name_(""), 
      current_is_method_(false), 
      module_name_(module_name), symbol_table_(nullptr) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    builtins_ = std::make_unique<Builtins>(*context_, *module_);
    
    // 设置目标平台的DataLayout（确保所有类型使用正确对齐）
    llvm::InitializeNativeTarget();
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (target) {
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(triple, "generic", "", opt, RM);
        if (target_machine) {
            module_->setDataLayout(target_machine->createDataLayout());
        }
    }
    
    // 声明所有内置函数
    builtins_->declareAll();
}

/**
 * @brief 构造CodeGenerator（多文件模式）
 * @param module_name 模块名称
 * @param symbol_table 符号表指针（用于跨模块查找）
 * 
 * 与单文件模式相同，但额外支持：
 * - 跨模块类型查找
 * - 跨模块函数调用
 * - 泛型跨模块实例化
 */
CodeGenerator::CodeGenerator(const std::string& module_name, SymbolTable* symbol_table)
    : current_function_(nullptr), current_function_return_type_(nullptr),
      current_struct_(nullptr), current_struct_name_(""), 
      current_is_method_(false), 
      module_name_(module_name), symbol_table_(symbol_table) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    builtins_ = std::make_unique<Builtins>(*context_, *module_);
    
    // 设置目标平台的DataLayout（确保所有类型使用正确对齐）
    llvm::InitializeNativeTarget();
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (target) {
        llvm::TargetOptions opt;
        auto RM = std::optional<llvm::Reloc::Model>();
        auto target_machine = target->createTargetMachine(triple, "generic", "", opt, RM);
        if (target_machine) {
            module_->setDataLayout(target_machine->createDataLayout());
        }
    }
    
    // 声明所有内置函数
    builtins_->declareAll();
}

/**
 * @brief 生成LLVM IR代码（主入口）
 * @param program AST程序节点
 * @return true 成功，false 验证失败
 * 
 * 采用两遍编译：
 * 1. 第一遍：注册所有类型定义（Struct和Enum）
 *    - 允许函数签名引用这些类型
 *    - 处理泛型定义注册
 * 2. 第二遍：生成函数、语句等
 *    - 类型已完全可用
 *    - 支持前向引用
 * 
 * 最后使用llvm::verifyModule()验证生成的IR正确性。
 */
bool CodeGenerator::generate(const Program& program) {
    // 第一遍：注册所有类型定义（Struct和Enum）
    // 这样函数签名中可以引用这些类型
    for (const auto& stmt : program.statements) {
        if (stmt->kind == Stmt::Kind::Struct) {
            generateStructStmt(static_cast<const StructStmt*>(stmt.get()));
        } else if (stmt->kind == Stmt::Kind::Enum) {
            generateEnumStmt(static_cast<const EnumStmt*>(stmt.get()));
        }
    }
    
    // 第二遍：生成其他语句（函数等）
    for (const auto& stmt : program.statements) {
        // 跳过已经处理的类型定义
        if (stmt->kind != Stmt::Kind::Struct && stmt->kind != Stmt::Kind::Enum) {
            generateStmt(stmt.get());
        }
    }
    
    // 验证模块
    std::string error_str;
    llvm::raw_string_ostream error_stream(error_str);
    if (llvm::verifyModule(*module_, &error_stream)) {
        std::cerr << "Module verification failed:\n" << error_str << std::endl;
        return false;
    }
    
    return true;
}

void CodeGenerator::printIR() {
    module_->print(llvm::outs(), nullptr);
}

void CodeGenerator::saveIR(const std::string& filename) {
    std::error_code EC;
    llvm::raw_fd_ostream file(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return;
    }
    module_->print(file, nullptr);
}

bool CodeGenerator::compileToObject(const std::string& filename) {
    // 只初始化本地目标（不是All）
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    
    // 使用 Triple 对象 (LLVM 21+)
    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    module_->setTargetTriple(triple);
    
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (!target) {
        std::cerr << "Target lookup failed: " << error << std::endl;
        return false;
    }
    
    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto RM = std::optional<llvm::Reloc::Model>();
    // 使用新API (LLVM 21+)
    auto target_machine = target->createTargetMachine(triple, CPU, features, opt, RM);
    
    module_->setDataLayout(target_machine->createDataLayout());
    
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;
    
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
        return false;
    }
    
    pass.run(*module_);
    dest.flush();
    
    return true;
}

// ============================================================================
// 第2部分：类型转换系统
// ============================================================================

/**
 * 将AST类型节点转换为LLVM类型
 * 支持：基础类型、数组、泛型、Named类型
 */
llvm::Type* CodeGenerator::convertType(const Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(*context_);
    }
    
    switch (type->kind) {
        case Type::Kind::Primitive: {
            auto prim_type = static_cast<const PrimitiveTypeNode*>(type);
            return convertPrimitiveType(prim_type->prim_type);
        }
        case Type::Kind::Named: {
            auto named_type = static_cast<const NamedTypeNode*>(type);
            
            // 检查是否有泛型参数
            if (!named_type->generic_args.empty()) {
                // 泛型struct: Box<i32>
                auto it = generic_structs_.find(named_type->name);
                if (it != generic_structs_.end()) {
                    return instantiateGenericStruct(named_type->name, named_type->generic_args);
                }
                // 泛型enum: Option<i32>
                auto enum_it = generic_enums_.find(named_type->name);
                if (enum_it != generic_enums_.end()) {
                    return instantiateGenericEnum(named_type->name, named_type->generic_args);
                }
            }
            
            // 先尝试查找enum类型（enum作为值类型，优先检查）
            auto enum_type = getEnumType(named_type->name);
            if (enum_type) {
                return enum_type;  // enum作为值类型 {i32, i64}
            }
            
            // 如果没找到enum，尝试从符号表导入（跨模块类型）
            if (symbol_table_ && !enum_type) {
                // 尝试从所有模块导入此类型
                auto symbol = symbol_table_->lookup(named_type->name, module_name_);
                if (symbol && symbol->kind == SymbolTable::SymbolKind::Type) {
                    importTypeFromModule(named_type->name, symbol->module);
                    // 重新尝试查找
                    enum_type = getEnumType(named_type->name);
                    if (enum_type) {
                        return enum_type;
                    }
                }
            }
            
            // 再尝试查找struct类型（struct作为指针传递）
            auto struct_type = getOrCreateStructType(named_type->name);
            if (struct_type) {
                return llvm::PointerType::get(*context_, 0);  // struct作为指针传递
            }
            
            // 默认按i32处理
            return llvm::Type::getInt32Ty(*context_);
        }
        case Type::Kind::Array: {
            auto array_type = static_cast<const ArrayTypeNode*>(type);
            llvm::Type* elem_type = convertType(array_type->element_type.get());
            
            // 处理大小待推导的情况（size == -1）
            // 注意：在变量声明时，size=-1应该从初始化器推导，由外层处理
            // 只有在函数参数时才返回PointerType
            if (array_type->size == -1 || array_type->size < 0) {
                // 待推导大小的数组：
                // - 如果是函数参数：返回PointerType
                // - 如果是变量声明：外层会从初始化器推导，这里返回i32作为占位符
                // 使用一个小的默认大小作为占位（外层会覆盖）
                return llvm::ArrayType::get(elem_type, 1);  // 占位符，外层会重新设置
            }
            
            return llvm::ArrayType::get(elem_type, array_type->size);
        }
        case Type::Kind::Generic: {
            // 泛型参数在单态化后应该被替换
            // 这里暂时返回i32
            return llvm::Type::getInt32Ty(*context_);
        }
        case Type::Kind::Optional: {
            // Optional类型: T?
            // 内部表示为 enum { Value(T), Error(string) }
            // 简化为 {i32 tag, T value, ptr error_msg}
            auto opt_type = static_cast<const OptionalTypeNode*>(type);
            
            // 使用resolveGenericType而不是convertType，这样T可以被正确解析
            llvm::Type* inner_type = resolveGenericType(opt_type->inner_type.get());
            
            // 使用struct表示: { i32 tag, T value, ptr error_msg }
            std::vector<llvm::Type*> fields = {
                llvm::Type::getInt32Ty(*context_),           // tag: 0=Value, 1=Error
                inner_type,                                   // value
                llvm::PointerType::get(*context_, 0)         // error_msg
            };
            return llvm::StructType::get(*context_, fields);
        }
        case Type::Kind::SelfType: {
            // Self类型：在struct方法中表示当前struct类型
            if (current_struct_name_.empty()) {
                std::cerr << "Error: 'Self' can only be used in struct methods" << std::endl;
                return llvm::Type::getInt32Ty(*context_);
            }
            
            auto struct_type = getOrCreateStructType(current_struct_name_);
            if (!struct_type) {
                return llvm::Type::getInt32Ty(*context_);
            }
            
            // 区分关联函数和实例方法：
            // - 关联函数（如new）：返回值类型（struct本身）
            // - 实例方法（有self）：返回指针类型
            if (current_is_method_) {
                // 实例方法：返回指针
                return llvm::PointerType::get(*context_, 0);
            } else {
                // 关联函数：返回值类型
                return struct_type;
            }
        }
        default:
            return llvm::Type::getVoidTy(*context_);
    }
}

llvm::StructType* CodeGenerator::getOrCreateStructType(const std::string& name) {
    // 1. 先查本地缓存
    auto it = struct_types_.find(name);
    if (it != struct_types_.end()) {
        return it->second;
    }
    
    // 2. 查找本地struct定义
    auto def_it = struct_defs_.find(name);
    if (def_it != struct_defs_.end()) {
        // 创建struct类型
        auto def = def_it->second;
        std::vector<llvm::Type*> field_types;
        for (const auto& field : def->fields) {
            field_types.push_back(convertType(field.type.get()));
        }
        
        auto struct_type = llvm::StructType::create(*context_, field_types, name);
        struct_types_[name] = struct_type;
        return struct_type;
    }
    
    // 3. 查找跨模块的泛型struct实例（通过SymbolTable）
    if (symbol_table_) {
        // 使用通用lookup查找跨模块类型
        auto symbol = symbol_table_->lookup(name, module_name_);
        if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->type) {
            // 找到了！从其他模块导入类型
            importTypeFromModule(name, symbol->module);
            
            // 再次检查是否成功导入
            auto imported_it = struct_types_.find(name);
            if (imported_it != struct_types_.end()) {
                return imported_it->second;
            }
        }
    }
    
    return nullptr;
}

llvm::Type* CodeGenerator::getEnumType(const std::string& name) {
    // Enum表示为tagged union: { i32 tag, [max_size] data }
    auto it = enum_defs_.find(name);
    if (it == enum_defs_.end()) {
        return nullptr;
    }
    
    // 简化: 使用 {i32, i64} 表示enum
    // tag = variant索引, data = 关联值（简化为i64）
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),  // tag
        llvm::Type::getInt64Ty(*context_)   // data
    };
    return llvm::StructType::get(*context_, fields);
}

llvm::Type* CodeGenerator::convertPrimitiveType(PrimitiveType type) {
    switch (type) {
        case PrimitiveType::I8: return llvm::Type::getInt8Ty(*context_);
        case PrimitiveType::I16: return llvm::Type::getInt16Ty(*context_);
        case PrimitiveType::I32: return llvm::Type::getInt32Ty(*context_);
        case PrimitiveType::I64: return llvm::Type::getInt64Ty(*context_);
        case PrimitiveType::I128: return llvm::Type::getInt128Ty(*context_);
        case PrimitiveType::U8: return llvm::Type::getInt8Ty(*context_);
        case PrimitiveType::U16: return llvm::Type::getInt16Ty(*context_);
        case PrimitiveType::U32: return llvm::Type::getInt32Ty(*context_);
        case PrimitiveType::U64: return llvm::Type::getInt64Ty(*context_);
        case PrimitiveType::U128: return llvm::Type::getInt128Ty(*context_);
        case PrimitiveType::F32: return llvm::Type::getFloatTy(*context_);
        case PrimitiveType::F64: return llvm::Type::getDoubleTy(*context_);
        case PrimitiveType::BOOL: return llvm::Type::getInt1Ty(*context_);
        case PrimitiveType::CHAR: return llvm::Type::getInt8Ty(*context_);
        case PrimitiveType::STRING: return llvm::PointerType::get(*context_, 0); // LLVM 21+
        case PrimitiveType::VOID: return llvm::Type::getVoidTy(*context_);
        default: return llvm::Type::getVoidTy(*context_);
    }
}

// ============================================================================
// 第3部分：表达式生成（Expression Generation）
// ============================================================================

/**
 * 表达式生成入口
 * 根据表达式类型分发到具体的生成函数
 */
llvm::Value* CodeGenerator::generateExpr(const Expr* expr) {
    switch (expr->kind) {
        case Expr::Kind::Integer: {
            auto int_expr = static_cast<const IntegerExpr*>(expr);
            return llvm::ConstantInt::get(*context_, llvm::APInt(32, int_expr->value, true));
        }
        case Expr::Kind::Float: {
            auto float_expr = static_cast<const FloatExpr*>(expr);
            return llvm::ConstantFP::get(*context_, llvm::APFloat(float_expr->value));
        }
        case Expr::Kind::Boolean: {
            auto bool_expr = static_cast<const BooleanExpr*>(expr);
            return llvm::ConstantInt::get(*context_, llvm::APInt(1, bool_expr->value ? 1 : 0));
        }
        case Expr::Kind::String: {
            auto str_expr = static_cast<const StringExpr*>(expr);
            // Create global string and return pointer
            return builder_->CreateGlobalStringPtr(str_expr->value, "str");
        }
        case Expr::Kind::Identifier:
            return generateIdentifierExpr(static_cast<const IdentifierExpr*>(expr));
        case Expr::Kind::Binary:
            return generateBinaryExpr(static_cast<const BinaryExpr*>(expr));
        case Expr::Kind::Unary:
            return generateUnaryExpr(static_cast<const UnaryExpr*>(expr));
        case Expr::Kind::Call:
            return generateCallExpr(static_cast<const CallExpr*>(expr));
        case Expr::Kind::Assign:
            return generateAssignExpr(static_cast<const AssignExpr*>(expr));
        case Expr::Kind::ArrayLiteral:
            return generateArrayLiteralExpr(static_cast<const ArrayLiteralExpr*>(expr));
        case Expr::Kind::Index:
            return generateIndexExpr(static_cast<const IndexExpr*>(expr));
        case Expr::Kind::MemberAccess:
            return generateMemberAccessExpr(static_cast<const MemberAccessExpr*>(expr));
        case Expr::Kind::StructLiteral:
            return generateStructLiteralExpr(static_cast<const StructLiteralExpr*>(expr));
        case Expr::Kind::EnumVariant:
            return generateEnumVariantExpr(static_cast<const EnumVariantExpr*>(expr));
        case Expr::Kind::Match:
            return generateMatchExpr(static_cast<const MatchExpr*>(expr));
        case Expr::Kind::Is:
            return generateIsExpr(static_cast<const IsExpr*>(expr));
        case Expr::Kind::IfExpr:
            return generateIfExpr(static_cast<const IfExpr*>(expr));
        case Expr::Kind::Try:
            return generateTryExpr(static_cast<const TryExpr*>(expr));
        case Expr::Kind::Ok:
            return generateOkExpr(static_cast<const OkExpr*>(expr));
        case Expr::Kind::Err:
            return generateErrExpr(static_cast<const ErrExpr*>(expr));
        case Expr::Kind::Cast:
            return generateCastExpr(static_cast<const CastExpr*>(expr));
        default:
            return nullptr;
    }
}

llvm::Value* CodeGenerator::generateBinaryExpr(const BinaryExpr* expr) {
    llvm::Value* left = generateExpr(expr->left.get());
    llvm::Value* right = generateExpr(expr->right.get());
    
    if (!left || !right) return nullptr;
    
    // Check if it's字符串操作
    bool is_ptr_left = left->getType()->isPointerTy();
    bool is_ptr_right = right->getType()->isPointerTy();
    
    if (expr->op == BinaryExpr::Op::Add && is_ptr_left && is_ptr_right) {
        // String concatenation: s1 + s2
        llvm::Function* strlen_func = module_->getFunction("strlen");
        llvm::Function* malloc_func = module_->getFunction("malloc");
        llvm::Function* strcpy_func = module_->getFunction("strcpy");
        llvm::Function* strcat_func = module_->getFunction("strcat");
        
        // Calculate length
        llvm::Value* len1 = builder_->CreateCall(strlen_func, {left}, "len1");
        llvm::Value* len2 = builder_->CreateCall(strlen_func, {right}, "len2");
        llvm::Value* total_len = builder_->CreateAdd(len1, len2, "total_len");
        llvm::Value* alloc_size = builder_->CreateAdd(
            total_len,
            llvm::ConstantInt::get(*context_, llvm::APInt(64, 1)),  // +1 for null
            "alloc_size"
        );
        
        // Allocate memory
        llvm::Value* result = builder_->CreateCall(malloc_func, {alloc_size}, "str_result");
        
        // Copy and concatenate
        builder_->CreateCall(strcpy_func, {result, left});
        builder_->CreateCall(strcat_func, {result, right});
        
        return result;
    }
    
    // Type matching check: integer types need unified bit width
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        unsigned left_bits = left->getType()->getIntegerBitWidth();
        unsigned right_bits = right->getType()->getIntegerBitWidth();
        
        if (left_bits != right_bits) {
            // Promote to larger type
            if (left_bits < right_bits) {
                left = builder_->CreateSExt(left, right->getType(), "promote_left");
            } else {
                right = builder_->CreateSExt(right, left->getType(), "promote_right");
            }
        }
    }
    
    switch (expr->op) {
        case BinaryExpr::Op::Add: return builder_->CreateAdd(left, right, "addtmp");
        case BinaryExpr::Op::Sub: return builder_->CreateSub(left, right, "subtmp");
        case BinaryExpr::Op::Mul: return builder_->CreateMul(left, right, "multmp");
        case BinaryExpr::Op::Div: return builder_->CreateSDiv(left, right, "divtmp");
        case BinaryExpr::Op::Mod: return builder_->CreateSRem(left, right, "modtmp");
        case BinaryExpr::Op::Eq: return builder_->CreateICmpEQ(left, right, "eqtmp");
        case BinaryExpr::Op::Ne: return builder_->CreateICmpNE(left, right, "netmp");
        case BinaryExpr::Op::Lt: return builder_->CreateICmpSLT(left, right, "lttmp");
        case BinaryExpr::Op::Le: return builder_->CreateICmpSLE(left, right, "letmp");
        case BinaryExpr::Op::Gt: return builder_->CreateICmpSGT(left, right, "gttmp");
        case BinaryExpr::Op::Ge: return builder_->CreateICmpSGE(left, right, "getmp");
        case BinaryExpr::Op::And: return builder_->CreateAnd(left, right, "andtmp");
        case BinaryExpr::Op::Or: return builder_->CreateOr(left, right, "ortmp");
        default: return nullptr;
    }
}

llvm::Value* CodeGenerator::generateUnaryExpr(const UnaryExpr* expr) {
    llvm::Value* operand = generateExpr(expr->operand.get());
    if (!operand) return nullptr;
    
    switch (expr->op) {
        case UnaryExpr::Op::Neg:
            return builder_->CreateNeg(operand, "negtmp");
        case UnaryExpr::Op::Not:
            return builder_->CreateNot(operand, "nottmp");
        default:
            return nullptr;
    }
}

llvm::Value* CodeGenerator::generateCallExpr(const CallExpr* expr) {
    // Check if it's方法调用：obj.method()
    if (expr->callee->kind == Expr::Kind::MemberAccess) {
        const MemberAccessExpr* member_expr = static_cast<const MemberAccessExpr*>(expr->callee.get());
        
        // 获取对象的地址（而不是值）
        llvm::Value* obj_ptr = nullptr;
        std::string obj_name;
        
        if (member_expr->object->kind == Expr::Kind::Identifier) {
            // 从named_values_直接获取指针（alloca）
            obj_name = static_cast<const IdentifierExpr*>(member_expr->object.get())->name;
            auto it = named_values_.find(obj_name);
            if (it != named_values_.end()) {
                obj_ptr = it->second;  // 这已经是alloca指针
            }
        } else {
            // 其他情况（如临时值），需要生成表达式
            obj_ptr = generateExpr(member_expr->object.get());
        }
        
        if (!obj_ptr) return nullptr;
        
        // 查找方法 - 尝试所有struct
        for (const auto& [struct_name, methods] : struct_methods_) {
            auto method_it = methods.find(member_expr->member);
            if (method_it != methods.end()) {
                llvm::Function* method_func = method_it->second;
                
                // 构建参数列表：第一个参数是this指针
                std::vector<llvm::Value*> args;
                
                // 【关键修复】：在新的struct语义下，struct变量是alloca ptr
                // obj_ptr是alloca的地址，需要load获取实际的heap指针
                llvm::Value* actual_obj_ptr = obj_ptr;
                
                // 如果有obj_name并且在variable_types_中是StructType
                if (!obj_name.empty()) {
                    auto type_it = variable_types_.find(obj_name);
                    if (type_it != variable_types_.end() && type_it->second->isStructTy()) {
                        // obj_ptr是alloca ptr，load获取heap指针
                        actual_obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0), 
                            obj_ptr, 
                            obj_name + "_heap_ptr"
                        );
                    }
                }
                
                args.push_back(actual_obj_ptr);  // this指针（heap指针）
                
                for (const auto& arg : expr->arguments) {
                    llvm::Value* arg_val = generateExpr(arg.get());
                    if (arg_val) args.push_back(arg_val);
                }
                
                // Call method
                if (method_func->getReturnType()->isVoidTy()) {
                    return builder_->CreateCall(method_func, args);
                }
                return builder_->CreateCall(method_func, args, "methodcall");
            }
        }
        
        std::cerr << "Method not found: " << member_expr->member << std::endl;
        return nullptr;
    }
    
    // 普通函数调用或关联函数
    if (expr->callee->kind != Expr::Kind::Identifier) {
        std::cerr << "Can only call functions by name" << std::endl;
        return nullptr;
    }
    
    auto callee_name = static_cast<const IdentifierExpr*>(expr->callee.get())->name;
    
    // 【新增】检查是否是泛型struct静态方法调用：Pair::new<K,V>()
    if (!expr->module_prefix.empty() && !expr->type_arguments.empty()) {
        // 检查module_prefix是否是泛型struct
        auto generic_struct_it = generic_structs_.find(expr->module_prefix);
        const StructStmt* generic_struct = nullptr;
        
        if (generic_struct_it != generic_structs_.end()) {
            generic_struct = generic_struct_it->second;
        } else if (symbol_table_) {
            // 跨模块查找泛型struct
            auto symbol = symbol_table_->lookup(expr->module_prefix, module_name_);
            if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->ast_node) {
                const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
                if (struct_def && !struct_def->generic_params.empty()) {
                    generic_struct = struct_def;
                    generic_structs_[expr->module_prefix] = generic_struct;  // 缓存
                }
            }
        }
        
        if (generic_struct) {
            // 确认是泛型struct，实例化struct
            std::string struct_mangled = mangleGenericName(expr->module_prefix, expr->type_arguments);
            
            // 实例化struct（如果尚未实例化）
            llvm::Type* struct_type = instantiateGenericStruct(expr->module_prefix, expr->type_arguments);
            if (!struct_type) {
                std::cerr << "Failed to instantiate generic struct: " << expr->module_prefix << std::endl;
                return nullptr;
            }
            
            // 构造方法的mangled name
            // 例如：Pair::new + <i32, i32> -> new_i32_i32
            std::string method_mangled = callee_name + "_" + 
                struct_mangled.substr(expr->module_prefix.length() + 1);
            
            // 查找实例化的方法
            auto func_it = functions_.find(method_mangled);
            if (func_it == functions_.end()) {
                std::cerr << "Static method not found: " << expr->module_prefix 
                         << "::" << callee_name << std::endl;
                return nullptr;
            }
            
            llvm::Function* method_func = func_it->second;
            
            // Generate arguments
            std::vector<llvm::Value*> args;
            for (const auto& arg : expr->arguments) {
                llvm::Value* arg_val = generateExpr(arg.get());
                if (arg_val) args.push_back(arg_val);
            }
            
            // 调用静态方法
            if (method_func->getReturnType()->isVoidTy()) {
                return builder_->CreateCall(method_func, args);
            }
            return builder_->CreateCall(method_func, args, "static_method_call");
        }
    }
    
    // Check if it's跨模块调用 module::function()
    if (!expr->module_prefix.empty() && symbol_table_) {
        // Check if it's泛型调用
        if (!expr->type_arguments.empty()) {
            // 跨模块泛型调用：从符号表获取AST并在当前模块实例化
            auto symbol = symbol_table_->lookupInModule(expr->module_prefix, callee_name);
            if (!symbol) {
                std::cerr << "Function not found in module " << expr->module_prefix 
                         << ": " << callee_name << std::endl;
                return nullptr;
            }
            
            // Check if it's泛型函数
            if (symbol->kind != SymbolTable::SymbolKind::GenericFunction) {
                std::cerr << "Function " << callee_name << " is not a generic function" << std::endl;
                return nullptr;
            }
            
            // 从符号表获取AST定义
            const FunctionStmt* generic_func_ast = static_cast<const FunctionStmt*>(symbol->ast_node);
            if (!generic_func_ast) {
                std::cerr << "Generic function AST not found: " << callee_name << std::endl;
                return nullptr;
            }
            
            // 将泛型函数临时添加到当前CodeGenerator的generic_functions_
            generic_functions_[callee_name] = generic_func_ast;
            
            // 在当前模块实例化泛型函数
            llvm::Function* inst_func = instantiateGenericFunction(callee_name, expr->type_arguments);
            if (!inst_func) {
                std::cerr << "Failed to instantiate cross-module generic function: " << callee_name << std::endl;
                return nullptr;
            }
            
            // Generate arguments（使用辅助函数处理数组传递）
            std::vector<llvm::Value*> args;
            for (const auto& arg : expr->arguments) {
                llvm::Value* arg_val = generateArgumentValue(arg.get());
                if (arg_val) args.push_back(arg_val);
            }
            
            // 调用实例化的函数
            if (inst_func->getReturnType()->isVoidTy()) {
                return builder_->CreateCall(inst_func, args);
            }
            return builder_->CreateCall(inst_func, args, "cross_module_generic_call");
        }
        
        // 普通跨模块调用
        auto symbol = symbol_table_->lookupInModule(expr->module_prefix, callee_name);
        if (!symbol) {
            std::cerr << "Function not found in module " << expr->module_prefix 
                     << ": " << callee_name << std::endl;
            return nullptr;
        }
        
        if (!symbol_table_->isAccessible(*symbol, module_name_)) {
            std::cerr << "Function " << callee_name << " in module " 
                     << expr->module_prefix << " is not accessible" << std::endl;
            return nullptr;
        }
        
        // 重要：不能直接使用其他模块的llvm::Function*，因为它来自不同的Context
        // 需要在当前模块中声明这个函数
        llvm::Function* external_func_orig = llvm::cast<llvm::Function>(symbol->value);
        
        // 检查当前模块是否已有此函数声明
        llvm::Function* local_func = module_->getFunction(callee_name);
        if (!local_func) {
            // 在当前模块中创建函数声明（使用当前Context的类型）
            llvm::FunctionType* func_type = external_func_orig->getFunctionType();
            
            // 创建参数类型列表（使用当前Context）
            std::vector<llvm::Type*> param_types;
            for (unsigned i = 0; i < func_type->getNumParams(); i++) {
                llvm::Type* param_type = func_type->getParamType(i);
                // 转换类型到当前Context
                param_types.push_back(convertTypeToCurrentContext(param_type));
            }
            
            // 转换返回类型
            llvm::Type* return_type = convertTypeToCurrentContext(func_type->getReturnType());
            
            // 创建函数类型
            llvm::FunctionType* local_func_type = llvm::FunctionType::get(
                return_type, param_types, func_type->isVarArg()
            );
            
            // 创建函数声明（ExternalLinkage）
            local_func = llvm::Function::Create(
                local_func_type,
                llvm::Function::ExternalLinkage,
                callee_name,
                module_.get()
            );
        }
        
        // Generate arguments（使用辅助函数处理数组传递）
        std::vector<llvm::Value*> args;
        for (const auto& arg : expr->arguments) {
            llvm::Value* arg_val = generateArgumentValue(arg.get());
            if (arg_val) args.push_back(arg_val);
        }
        
        if (local_func->getReturnType()->isVoidTy()) {
            return builder_->CreateCall(local_func, args);
        }
        return builder_->CreateCall(local_func, args, "cross_module_call");
    }
    
    // Check if it's内置函数
    if (builtins_->isBuiltin(callee_name)) {
        return generateBuiltinCall(callee_name, expr->arguments);
    }
    
    // Check if it's泛型调用
    llvm::Function* callee = nullptr;
    if (!expr->type_arguments.empty()) {
        // 泛型函数调用：实例化
        callee = instantiateGenericFunction(callee_name, expr->type_arguments);
        if (!callee) {
            std::cerr << "Failed to instantiate generic function: " << callee_name << std::endl;
            return nullptr;
        }
    } else {
        // 查找用户定义的函数
        auto it = functions_.find(callee_name);
        if (it == functions_.end()) {
            // 可能是泛型函数但没有显式类型参数（暂不支持类型推导）
            if (isGenericFunction(callee_name)) {
                std::cerr << "Generic function requires explicit type arguments: " << callee_name << std::endl;
                return nullptr;
            }
            std::cerr << "Unknown function: " << callee_name << std::endl;
            return nullptr;
        }
        callee = it->second;
    }
    
    if (callee->arg_size() != expr->arguments.size()) {
        std::cerr << "Incorrect number of arguments" << std::endl;
        return nullptr;
    }
    
    std::vector<llvm::Value*> args;
    for (const auto& arg : expr->arguments) {
        llvm::Value* arg_val = generateArgumentValue(arg.get());
        if (!arg_val) return nullptr;
        args.push_back(arg_val);
    }
    
    // void函数不需要名字
    if (callee->getReturnType()->isVoidTy()) {
        return builder_->CreateCall(callee, args);
    }
    return builder_->CreateCall(callee, args, "calltmp");
}

// Helper function: generate function call arguments (handle array passing)
llvm::Value* CodeGenerator::generateArgumentValue(const Expr* arg) {
    // If argument is array variable, pass address not value
    if (arg->kind == Expr::Kind::Identifier) {
        std::string arg_name = static_cast<const IdentifierExpr*>(arg)->name;
        auto val_it = named_values_.find(arg_name);
        auto type_it = variable_types_.find(arg_name);
        
        if (val_it != named_values_.end() && type_it != variable_types_.end()) {
            // Check if it's数组类型
            if (llvm::isa<llvm::ArrayType>(type_it->second)) {
                // Array: pass alloca pointer directly
                return val_it->second;
            }
        }
    }
    
    // Non-array or other cases: generate normally
    return generateExpr(arg);
}

llvm::Value* CodeGenerator::generateBuiltinCall(const std::string& name, const std::vector<ExprPtr>& arguments) {
    llvm::Function* builtin_func = builtins_->getFunction(name);
    if (!builtin_func) {
        std::cerr << "Unknown builtin: " << name << std::endl;
        return nullptr;
    }
    
    // Check argument count
    if (builtin_func->arg_size() != arguments.size()) {
        std::cerr << "Incorrect number of arguments for " << name << std::endl;
        return nullptr;
    }
    
    // Generate arguments
    std::vector<llvm::Value*> args;
    for (const auto& arg : arguments) {
        args.push_back(generateExpr(arg.get()));
        if (!args.back()) return nullptr;
    }
    
    // Call built-in function
    if (builtin_func->getReturnType()->isVoidTy()) {
        return builder_->CreateCall(builtin_func, args);
    }
    return builder_->CreateCall(builtin_func, args, name + "_result");
}

// ============================================================================
// 第4部分：语句生成（Statement Generation）
// ============================================================================

/**
 * 语句生成入口
 * 根据语句类型分发到具体的生成函数
 */
void CodeGenerator::generateFunctionStmt(const FunctionStmt* stmt) {
    // 如果是泛型函数，保存定义
    if (!stmt->generic_params.empty()) {
        generic_functions_[stmt->name] = stmt;
        
        // 如果是pub泛型函数，注册到符号表（保存AST定义）
        // 这样其他模块可以访问泛型函数的AST并实例化
        if (stmt->is_public && symbol_table_) {
            symbol_table_->registerGenericFunction(module_name_, stmt->name, stmt->is_public, stmt);
        }
        
        return;
    }
    
    // 设置当前是否为实例方法（用于Self类型解析）
    current_is_method_ = stmt->is_method;
    
    std::vector<llvm::Type*> param_types;
    
    // 如果是方法（有self参数），第一个参数是struct指针
    if (stmt->is_method && !current_struct_name_.empty()) {
        llvm::Type* struct_type = getOrCreateStructType(current_struct_name_);
        param_types.push_back(llvm::PointerType::get(*context_, 0));  // struct*
    }
    
    // 其他参数
    for (const auto& param : stmt->parameters) {
        if (!param.is_self) {  // self已经处理过了
            llvm::Type* param_type = convertType(param.type.get());
            
            // 数组参数传递指针而不是值
            if (llvm::isa<llvm::ArrayType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            // 【新增】：struct参数传递指针而不是值
            if (llvm::isa<llvm::StructType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            param_types.push_back(param_type);
        }
    }
    
    llvm::Type* return_type = stmt->return_type ? 
        convertType(stmt->return_type.get()) : llvm::Type::getVoidTy(*context_);
    
    // 【新增】：struct返回值改为指针
    if (return_type && llvm::isa<llvm::StructType>(return_type)) {
        return_type = llvm::PointerType::get(*context_, 0);
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function* func = llvm::Function::Create(
        func_type, llvm::Function::ExternalLinkage, stmt->name, module_.get()
    );
    
    functions_[stmt->name] = func;
    
    // 注册到符号表（如果有）
    if (symbol_table_) {
        symbol_table_->registerFunction(module_name_, stmt->name, stmt->is_public, func);
    }
    
    // 设置参数名
    size_t idx = 0;
    if (stmt->is_method && !current_struct_name_.empty()) {
        func->args().begin()->setName("self");
        idx = 1;
    }
    
    for (size_t i = 0; i < stmt->parameters.size(); i++) {
        if (!stmt->parameters[i].is_self) {
            (func->args().begin() + idx)->setName(stmt->parameters[i].name);
            idx++;
        }
    }
    
    if (stmt->body) {
        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context_, "entry", func);
        builder_->SetInsertPoint(bb);
        
        current_function_ = func;
        current_function_return_type_ = stmt->return_type.get();  // 保存返回类型用于ok/err
        named_values_.clear();
        
        // 创建局部变量（记录参数类型）
        idx = 0;
        for (auto& arg : func->args()) {
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                arg.getType(), nullptr, arg.getName()
            );
            builder_->CreateStore(&arg, alloca);
            named_values_[std::string(arg.getName())] = alloca;
            variable_types_[std::string(arg.getName())] = arg.getType();  // 记录参数类型
        }
        
        generateStmt(stmt->body.get());
        
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::ConstantInt::get(return_type, 0));
            }
        }
        
        llvm::verifyFunction(*func);
    }
    
    // 重置方法标志
    current_is_method_ = false;
}


/**
 * Extern声明生成
 * 创建外部函数声明（用于调用C函数）
 */
void CodeGenerator::generateExternStmt(const ExternStmt* stmt) {
    // 检查是否已经声明过（避免重复）
    llvm::Function* existing = module_->getFunction(stmt->name);
    if (existing) {
        functions_[stmt->name] = existing;
        return;
    }
    
    // 转换参数类型
    std::vector<llvm::Type*> param_types;
    for (const auto& param : stmt->parameters) {
        param_types.push_back(convertType(param.type.get()));
    }
    
    // 转换返回类型
    llvm::Type* return_type = convertType(stmt->return_type.get());
    
    // 创建函数类型
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        return_type, param_types, false
    );
    
    // 创建外部函数声明
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        stmt->name,
        module_.get()
    );
    
    // 注册函数到本地映射
    functions_[stmt->name] = func;
    
    // extern函数也注册到符号表（其他模块可能需要调用）
    // 注意：extern函数不是pub，但需要在模块内可见
    if (symbol_table_) {
        symbol_table_->registerFunction(module_name_, stmt->name, false, func);
    }
}

/**
 * Struct语句生成
 * 处理：泛型struct定义、字段类型、方法生成
 */
void CodeGenerator::generateStructStmt(const StructStmt* stmt) {
    // 如果是泛型struct，保存定义但不立即生成
    if (!stmt->generic_params.empty()) {
        generic_structs_[stmt->name] = stmt;
        
        // 【新增】保存泛型struct的方法定义
        // 使用 "StructName::methodName" 作为key
        for (const auto& method : stmt->methods) {
            std::string method_key = stmt->name + "::" + method->name;
            generic_struct_methods_[method_key] = method.get();
        }
        
        // 注册泛型struct定义到SymbolTable（保存AST）
        if (symbol_table_ && stmt->is_public) {
            symbol_table_->registerType(module_name_, stmt->name, stmt->is_public, 
                                       nullptr,  // 泛型定义没有具体LLVM Type
                                       static_cast<const void*>(stmt));
        }
        
        return;
    }
    
    // 注册struct定义
    struct_defs_[stmt->name] = stmt;
    
    // 预先创建struct类型
    llvm::Type* struct_type = getOrCreateStructType(stmt->name);
    
    // 注册到符号表（如果有）
    if (symbol_table_ && struct_type) {
        symbol_table_->registerType(module_name_, stmt->name, stmt->is_public, struct_type, stmt);
    }
    
    // 生成struct内的方法
    current_struct_ = stmt;
    current_struct_name_ = stmt->name;
    
    for (const auto& method : stmt->methods) {
        generateFunctionStmt(method.get());
        
        // 注册方法到struct_methods_
        std::string method_name = method->name;
        llvm::Function* func = functions_[method_name];
        if (func) {
            struct_methods_[stmt->name][method_name] = func;
        }
    }
    
    current_struct_ = nullptr;
    current_struct_name_ = "";
}

void CodeGenerator::generateEnumStmt(const EnumStmt* stmt) {
    // 如果是泛型enum，保存定义但不立即生成
    if (!stmt->generic_params.empty()) {
        generic_enums_[stmt->name] = stmt;
        return;
    }
    
    // 注册enum定义
    enum_defs_[stmt->name] = stmt;
    
    // 获取enum类型
    llvm::Type* enum_type = getEnumType(stmt->name);
    
    // 注册到符号表（如果有）
    if (symbol_table_ && enum_type) {
        symbol_table_->registerType(module_name_, stmt->name, stmt->is_public, enum_type, stmt);
    }
}

void CodeGenerator::generateImplStmt(const ImplStmt* stmt) {
    // 查找对应的struct定义
    auto struct_it = struct_defs_.find(stmt->type_name);
    if (struct_it != struct_defs_.end()) {
        current_struct_ = struct_it->second;
    }
    
    // 生成所有方法
    for (const auto& method : stmt->methods) {
        generateFunctionStmt(method.get());
    }
    
    current_struct_ = nullptr;
}

/**
 * 赋值表达式生成
 * 支持：变量赋值、成员赋值
 */
llvm::Value* CodeGenerator::generateAssignExpr(const AssignExpr* expr) {
    llvm::Value* val = generateExpr(expr->value.get());
    if (!val) return nullptr;
    
    // 索引赋值：arr[i] = value 或 s[i] = value
    if (expr->target_expr && expr->target_expr->kind == Expr::Kind::Index) {
        const IndexExpr* index_expr = static_cast<const IndexExpr*>(expr->target_expr.get());
        
        // 生成索引值
        llvm::Value* index_val = generateExpr(index_expr->index.get());
        if (!index_val) return nullptr;
        
        // 获取数组/字符串的指针
        llvm::Value* array_ptr = nullptr;
        if (index_expr->array->kind == Expr::Kind::Identifier) {
            std::string array_name = static_cast<const IdentifierExpr*>(index_expr->array.get())->name;
            auto it = named_values_.find(array_name);
            if (it != named_values_.end()) {
                array_ptr = it->second;
            }
        }
        
        if (!array_ptr) {
            std::cerr << "Cannot access index of unknown array/string" << std::endl;
            return nullptr;
        }
        
        // 获取数组元素类型
        auto type_it = variable_types_.find(static_cast<const IdentifierExpr*>(index_expr->array.get())->name);
        if (type_it == variable_types_.end()) {
            std::cerr << "Unknown array/string type for index assignment" << std::endl;
            return nullptr;
        }
        
        llvm::Type* array_type = type_it->second;
        
        // Check if it's字符串类型（指针）
        if (array_type->isPointerTy()) {
            // 字符串索引写入：s[i] = 'A'
            llvm::Value* str_ptr = builder_->CreateLoad(
                llvm::PointerType::get(*context_, 0),
                array_ptr,
                "strload"
            );
            
            // GEP到指定字符
            llvm::Value* char_ptr = builder_->CreateGEP(
                llvm::Type::getInt8Ty(*context_),
                str_ptr,
                index_val,
                "stridx"
            );
            
            // 存储字符
            builder_->CreateStore(val, char_ptr);
            return val;
        } else if (array_type->isArrayTy()) {
            // 数组索引写入：arr[i] = value
            llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(array_type);
            
            // GEP到指定元素
            llvm::Value* elem_ptr = builder_->CreateGEP(
                arr_type,
                array_ptr,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0), index_val},
                "elem_ptr"
            );
            
            // 存储值
            builder_->CreateStore(val, elem_ptr);
            return val;
        } else {
            std::cerr << "Invalid type for index assignment" << std::endl;
            return nullptr;
        }
    }
    
    // 成员赋值：obj.field = value
    if (expr->target_expr && expr->target_expr->kind == Expr::Kind::MemberAccess) {
        const MemberAccessExpr* member_expr = static_cast<const MemberAccessExpr*>(expr->target_expr.get());
        
        // 获取对象指针
        llvm::Value* obj_ptr = nullptr;
        llvm::Type* struct_value_type = nullptr;
        
        if (member_expr->object->kind == Expr::Kind::Identifier) {
            std::string obj_name = static_cast<const IdentifierExpr*>(member_expr->object.get())->name;
            
            // 处理self
            if (obj_name == "self" && !current_struct_name_.empty()) {
                auto self_it = named_values_.find("self");
                if (self_it != named_values_.end()) {
                    llvm::Value* self_alloca = self_it->second;
                    obj_ptr = builder_->CreateLoad(
                        llvm::PointerType::get(*context_, 0),
                        self_alloca,
                        "self"
                    );
                    auto struct_type = getOrCreateStructType(current_struct_name_);
                    struct_value_type = struct_type;
                }
            } else {
                // 普通变量
                auto it = named_values_.find(obj_name);
                if (it != named_values_.end()) {
                    obj_ptr = it->second;
                    auto type_it = variable_types_.find(obj_name);
                    if (type_it != variable_types_.end()) {
                        struct_value_type = type_it->second;
                    }
                }
            }
        }
        
        if (!obj_ptr) {
            std::cerr << "Cannot access member of unknown object" << std::endl;
            return nullptr;
        }
        
        // 找到字段并生成GEP
        for (const auto& [struct_name, struct_def] : struct_defs_) {
            auto struct_type = getOrCreateStructType(struct_name);
            
            if (struct_value_type && struct_type != struct_value_type) {
                continue;
            }
            
            int field_idx = 0;
            for (const auto& field : struct_def->fields) {
                if (field.name == member_expr->member) {
                    llvm::Value* field_ptr = builder_->CreateStructGEP(
                        struct_type, obj_ptr, field_idx, "field_ptr"
                    );
                    builder_->CreateStore(val, field_ptr);
                    return val;
                }
                field_idx++;
            }
        }
        
        std::cerr << "Unknown field: " << member_expr->member << std::endl;
        return nullptr;
    }
    
    // 普通变量赋值：x = value
    auto it = named_values_.find(expr->target);
    if (it != named_values_.end()) {
        builder_->CreateStore(val, it->second);
        return val;
    }
    
    std::cerr << "Unknown variable: " << expr->target << std::endl;
    return nullptr;
}

/**
 * 成员访问表达式
 * 支持：struct.field访问
 */
llvm::Value* CodeGenerator::generateMemberAccessExpr(const MemberAccessExpr* expr) {
    // 获取对象指针（必须是指针，用于GEP）
    llvm::Value* obj_ptr = nullptr;
    llvm::Type* struct_value_type = nullptr;
    
    // Check if it's标识符（变量名）
    if (expr->object->kind == Expr::Kind::Identifier) {
        std::string obj_name = static_cast<const IdentifierExpr*>(expr->object.get())->name;
        
        // 如果是self，并且在方法中
        if (obj_name == "self" && !current_struct_name_.empty()) {
            auto self_it = named_values_.find("self");
            if (self_it != named_values_.end()) {
                llvm::Value* self_alloca = self_it->second;
                
                // self_alloca存储的是指针，需要load
                obj_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    self_alloca,
                    "self"
                );
                
                // 获取struct类型
                auto struct_type = getOrCreateStructType(current_struct_name_);
                struct_value_type = struct_type;
            }
        } else {
            // 普通变量：obj_ptr应该是alloca（指向struct值的指针）
            auto it = named_values_.find(obj_name);
            if (it != named_values_.end()) {
                // 从variable_types_获取实际的struct值类型
                auto type_it = variable_types_.find(obj_name);
                if (type_it != variable_types_.end()) {
                    struct_value_type = type_it->second;
                    
                    // 【新逻辑】：处理不同的类型
                    if (struct_value_type->isPointerTy()) {
                        // struct变量现在存储ptr：需要load得到实际的struct指针
                        obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0),
                            it->second,
                            obj_name + "_ptr"
                        );
                        // 注意：variable_types_可能存储的是StructType（参数），需要保留
                    } else if (struct_value_type->isStructTy()) {
                        // struct参数传递：variable_types_存储StructType，alloca存储ptr
                        // 需要load得到实际struct指针
                        obj_ptr = builder_->CreateLoad(
                            llvm::PointerType::get(*context_, 0),
                            it->second,
                            obj_name + "_loaded"
                        );
                    } else {
                        // 非指针（旧语义）：直接使用alloca
                        obj_ptr = it->second;
                    }
                } else {
                    obj_ptr = it->second;  // fallback
                }
            }
        }
    } else {
        // 复杂表达式：生成表达式
        llvm::Value* obj = generateExpr(expr->object.get());
        if (!obj) return nullptr;
        
        // 如果是指针，直接使用；如果是值，需要创建临时alloca
        if (obj->getType()->isPointerTy()) {
            obj_ptr = obj;
        } else if (obj->getType()->isStructTy()) {
            // 创建临时alloca存储struct值
            llvm::AllocaInst* temp = builder_->CreateAlloca(obj->getType(), nullptr, "temp_struct");
            builder_->CreateStore(obj, temp);
            obj_ptr = temp;
            struct_value_type = obj->getType();
        } else {
            return nullptr;
        }
    }
    
    if (!obj_ptr) return nullptr;
    
    // 尝试所有struct定义，找到匹配的字段
    // 【优先使用精确类型】：如果struct_value_type是StructType，直接使用它
    if (struct_value_type && struct_value_type->isStructTy()) {
        // 直接使用精确的struct类型
        llvm::StructType* exact_struct = llvm::cast<llvm::StructType>(struct_value_type);
        
        // 找到对应的struct定义
        for (const auto& [struct_name, struct_def] : struct_defs_) {
            auto struct_type = getOrCreateStructType(struct_name);
            if (struct_type == exact_struct) {
                // 找到匹配的定义
                int field_idx = 0;
                for (const auto& field : struct_def->fields) {
                    if (field.name == expr->member) {
                        // 找到字段！
                        llvm::Value* field_ptr = builder_->CreateStructGEP(
                            struct_type, obj_ptr, field_idx, "field_ptr"
                        );
                        return builder_->CreateLoad(
                            struct_type->getElementType(field_idx), 
                            field_ptr, 
                            expr->member
                        );
                    }
                    field_idx++;
                }
            }
        }
    }
    
    // Fallback：遍历所有struct（用于不明确类型的情况）
    for (const auto& [struct_name, struct_def] : struct_defs_) {
        auto struct_type = getOrCreateStructType(struct_name);
        
        int field_idx = 0;
        for (const auto& field : struct_def->fields) {
            if (field.name == expr->member) {
                // 找到字段！obj_ptr是指向struct的指针
                llvm::Value* field_ptr = builder_->CreateStructGEP(
                    struct_type, obj_ptr, field_idx, "field_ptr"
                );
                return builder_->CreateLoad(
                    struct_type->getElementType(field_idx), 
                    field_ptr, 
                    expr->member
                );
            }
            field_idx++;
        }
    }
    
    std::cerr << "Unknown field: " << expr->member << std::endl;
    return nullptr;
}

llvm::Value* CodeGenerator::generateStructLiteralExpr(const StructLiteralExpr* expr) {
    // 解析泛型struct名称（如果在泛型context中）
    // "Pair_K_V" -> "Pair_i32_string"
    std::string resolved_name = resolveGenericStructName(expr->type_name);
    
    // 先从struct_types_直接查找（支持泛型实例如Box_i32）
    auto type_it = struct_types_.find(resolved_name);
    llvm::StructType* struct_type = nullptr;
    
    if (type_it != struct_types_.end()) {
        struct_type = type_it->second;
    } else {
        // 如果直接找不到，尝试通过getOrCreateStructType
        struct_type = getOrCreateStructType(resolved_name);
    }
    
    if (!struct_type) {
        std::cerr << "Unknown struct type: " << expr->type_name 
                  << " (resolved: " << resolved_name << ")" << std::endl;
        return nullptr;
    }
    
    // 1. 在栈上分配临时struct
    llvm::Value* temp_alloca = builder_->CreateAlloca(struct_type, nullptr, "struct_temp");
    
    // 2. 初始化字段（使用resolved_name）
    auto def_it = struct_defs_.find(resolved_name);
    if (def_it != struct_defs_.end()) {
        const StructStmt* struct_def = def_it->second;
        for (size_t i = 0; i < expr->fields.size() && i < struct_def->fields.size(); i++) {
            llvm::Value* field_val = generateExpr(expr->fields[i].value.get());
            if (field_val) {
                // 获取字段的目标类型
                llvm::Type* target_type = struct_type->getElementType(i);
                
                // 类型转换（如果需要）
                if (field_val->getType()->isIntegerTy() && target_type->isIntegerTy()) {
                    unsigned src_bits = field_val->getType()->getIntegerBitWidth();
                    unsigned dst_bits = target_type->getIntegerBitWidth();
                    
                    if (src_bits < dst_bits) {
                        // 扩展到更大的类型
                        field_val = builder_->CreateSExt(field_val, target_type, "field_sext");
                    } else if (src_bits > dst_bits) {
                        // 截断到更小的类型
                        field_val = builder_->CreateTrunc(field_val, target_type, "field_trunc");
                    }
                }
                
                llvm::Value* field_ptr = builder_->CreateStructGEP(struct_type, temp_alloca, i);
                builder_->CreateStore(field_val, field_ptr);
            }
        }
    }
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t struct_size = data_layout.getTypeAllocSize(struct_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, struct_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "struct_heap");
    
    // 4. 拷贝struct到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_alloca, size_val});
    }
    
    // 5. 返回堆指针
    return heap_ptr;
}

llvm::Value* CodeGenerator::generateEnumVariantExpr(const EnumVariantExpr* expr) {
    // 尝试查找enum类型（可能是泛型实例化的mangled name）
    llvm::Type* enum_type = getEnumType(expr->enum_name);
    
    // 如果直接找不到，可能是泛型enum，需要从enum_defs_查找
    std::string enum_name = expr->enum_name;
    if (!enum_type) {
        // 检查enum_defs_中是否有这个类型
        auto it = enum_defs_.find(expr->enum_name);
        if (it != enum_defs_.end()) {
            // 找到了，构造类型
            std::vector<llvm::Type*> fields = {
                llvm::Type::getInt32Ty(*context_),  // tag
                llvm::Type::getInt64Ty(*context_)   // data
            };
            enum_type = llvm::StructType::get(*context_, fields);
        } else {
            std::cerr << "Unknown enum type: " << expr->enum_name << std::endl;
            return nullptr;
        }
    }
    
    // 分配enum实例
    llvm::Value* alloca = builder_->CreateAlloca(enum_type);
    
    // 设置tag（variant索引）
    auto def_it = enum_defs_.find(expr->enum_name);
    if (def_it != enum_defs_.end()) {
        int tag = 0;
        for (const auto& variant : def_it->second->variants) {
            if (variant.name == expr->variant_name) break;
            tag++;
        }
        
        llvm::Value* tag_ptr = builder_->CreateStructGEP(
            static_cast<llvm::StructType*>(enum_type), alloca, 0
        );
        builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, tag)), tag_ptr);
        
        // 设置data（简化: 只支持一个i32值）
        if (!expr->values.empty()) {
            llvm::Value* val = generateExpr(expr->values[0].get());
            if (val) {
                llvm::Value* data_ptr = builder_->CreateStructGEP(
                    static_cast<llvm::StructType*>(enum_type), alloca, 1
                );
                llvm::Value* extended = builder_->CreateSExtOrTrunc(val, llvm::Type::getInt64Ty(*context_));
                builder_->CreateStore(extended, data_ptr);
            }
        }
    }
    
    // 返回load的值而不是指针（enum是值类型）
    return builder_->CreateLoad(enum_type, alloca, "enum_val");
}



llvm::Value* CodeGenerator::generateArrayLiteralExpr(const ArrayLiteralExpr* expr) {
    if (expr->elements.empty()) {
        return nullptr;
    }
    
    // 注意：数组字面量应该使用变量声明中的类型，而不是从第一个元素推导
    // generateArrayLiteralExpr不应该独立推导类型
    // 这个函数主要在generateLetStmt中的特殊路径使用，那里已经知道目标类型
    
    // 简化实现：返回nullptr，让外层处理
    // 实际的数组初始化在generateLetStmt中完成
    return nullptr;
}

llvm::Value* CodeGenerator::generateIndexExpr(const IndexExpr* expr) {
    // 获取数组/字符串变量名和类型
    llvm::Value* array_ptr = nullptr;
    llvm::Type* array_type = nullptr;
    
    if (expr->array->kind == Expr::Kind::Identifier) {
        std::string array_name = static_cast<const IdentifierExpr*>(expr->array.get())->name;
        auto ptr_it = named_values_.find(array_name);
        auto type_it = variable_types_.find(array_name);
        
        if (ptr_it != named_values_.end() && type_it != variable_types_.end()) {
            array_ptr = ptr_it->second;   // alloca指针
            array_type = type_it->second; // 数组/字符串类型
            
            // Check if it's数组参数（在array_element_types_中有记录）
            auto elem_it = array_element_types_.find(array_name);
            if (elem_it != array_element_types_.end()) {
                // 这是数组参数：arr[i] -> T
                llvm::Type* elem_type = elem_it->second;
                llvm::Value* index_val = generateExpr(expr->index.get());
                if (!index_val) return nullptr;
                
                // 加载数组指针
                llvm::Value* arr_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    array_ptr,
                    "arrload"
                );
                
                // GEP到指定元素
                llvm::Value* elem_ptr = builder_->CreateGEP(
                    elem_type,
                    arr_ptr,
                    index_val,
                    "elemptr"
                );
                
                // 加载元素
                return builder_->CreateLoad(elem_type, elem_ptr, "elemload");
            }
            
            // Check if it's字符串类型
            if (array_type->isPointerTy() && !array_type->isArrayTy()) {
                // 这是字符串：s[i] -> char
                llvm::Value* index_val = generateExpr(expr->index.get());
                if (!index_val) return nullptr;
                
                // 加载字符串指针
                llvm::Value* str_ptr = builder_->CreateLoad(
                    llvm::PointerType::get(*context_, 0),
                    array_ptr,
                    "strload"
                );
                
                // GEP到指定字符
                llvm::Value* char_ptr = builder_->CreateGEP(
                    llvm::Type::getInt8Ty(*context_),
                    str_ptr,
                    index_val,
                    "stridx"
                );
                
                // 加载字符
                return builder_->CreateLoad(
                    llvm::Type::getInt8Ty(*context_),
                    char_ptr,
                    "charload"
                );
            }
        }
    } else if (expr->array->kind == Expr::Kind::Index) {
        // 多维数组：递归处理但返回指针
        const IndexExpr* inner = static_cast<const IndexExpr*>(expr->array.get());
        
        // 获取基础数组
        if (inner->array->kind != Expr::Kind::Identifier) {
            std::cerr << "Nested index only supports identifiers" << std::endl;
            return nullptr;
        }
        
        std::string base_name = static_cast<const IdentifierExpr*>(inner->array.get())->name;
        auto ptr_it = named_values_.find(base_name);
        auto type_it = variable_types_.find(base_name);
        
        if (ptr_it == named_values_.end() || type_it == variable_types_.end()) {
            std::cerr << "Unknown array: " << base_name << std::endl;
            return nullptr;
        }
        
        llvm::Value* base_ptr = ptr_it->second;
        llvm::Type* base_type = type_it->second;
        
        if (!llvm::isa<llvm::ArrayType>(base_type)) {
            std::cerr << "Base is not an array" << std::endl;
            return nullptr;
        }
        
        // 第一级索引
        llvm::Value* first_idx = generateExpr(inner->index.get());
        if (!first_idx) return nullptr;
        
        // GEP到第一级元素（指针）
        llvm::Value* first_ptr = builder_->CreateInBoundsGEP(
            base_type, base_ptr,
            {llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)), first_idx},
            "first_ptr"
        );
        
        // 元素类型
        llvm::Type* first_elem = llvm::cast<llvm::ArrayType>(base_type)->getElementType();
        
        array_ptr = first_ptr;
        array_type = first_elem;
        
    } else {
        std::cerr << "Unsupported array expression" << std::endl;
        return nullptr;
    }
    
    if (!array_ptr || !array_type) return nullptr;
    
    llvm::Value* index = generateExpr(expr->index.get());
    if (!index) return nullptr;
    
    // 获取元素类型
    llvm::Type* elem_type = nullptr;
    if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(array_type)) {
        elem_type = arr_ty->getElementType();
    } else {
        std::cerr << "Variable is not an array type" << std::endl;
        return nullptr;
    }
    
    // 数组索引: GEP array_ptr, 0, index
    llvm::Value* indices[] = {
        llvm::ConstantInt::get(*context_, llvm::APInt(64, 0)),
        index
    };
    
    llvm::Value* elem_ptr = builder_->CreateInBoundsGEP(
        array_type, array_ptr, indices, "elem_ptr"
    );
    
    // 加载元素值
    return builder_->CreateLoad(elem_type, elem_ptr, "elem");
}


// ====== 泛型支持 ======

// 泛型名称修饰：Box<i32> → Box_i32
std::string CodeGenerator::mangleGenericName(const std::string& base_name, const std::vector<TypePtr>& type_args) {
    std::string mangled = base_name;
    for (const auto& arg : type_args) {
        mangled += "_";
        if (arg->kind == Type::Kind::Named) {
            const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg.get());
            mangled += named->name;
        } else if (arg->kind == Type::Kind::Primitive) {
            const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg.get());
            // 简化类型名
            switch (prim->prim_type) {
                case PrimitiveType::I32: mangled += "i32"; break;
                case PrimitiveType::I64: mangled += "i64"; break;
                case PrimitiveType::STRING: mangled += "string"; break;
                default: mangled += "T"; break;
            }
        }
    }
    return mangled;
}

// 解析泛型类型（替换类型参数）
llvm::Type* CodeGenerator::resolveGenericType(const Type* type) {
    // 处理Self类型（在泛型struct方法中）
    if (type->kind == Type::Kind::SelfType) {
        if (!current_struct_name_.empty()) {
            // 返回当前struct的指针类型（统一语义）
            return llvm::PointerType::get(*context_, 0);
        }
    }
    
    if (type->kind == Type::Kind::Generic) {
        const GenericTypeNode* gen = static_cast<const GenericTypeNode*>(type);
        
        // 在type_param_map_中查找当前类型参数的具体类型
        for (const auto& entry : type_param_map_) {
            if (entry.first == gen->name && !entry.second.empty()) {
                return entry.second.begin()->second;
            }
        }
    }
    return convertType(type);
}

// 解析泛型struct名称：将"Pair_K_V"解析为"Pair_i32_string"
std::string CodeGenerator::resolveGenericStructName(const std::string& mangled_name) {
    // 分析mangled_name，提取base_name和类型参数占位符
    // 例如："Pair_K_V" -> "Pair" + ["K", "V"]
    size_t first_underscore = mangled_name.find('_');
    if (first_underscore == std::string::npos) {
        return mangled_name;  // 没有下划线，不是泛型实例
    }
    
    std::string base_name = mangled_name.substr(0, first_underscore);
    std::string params_part = mangled_name.substr(first_underscore + 1);
    
    // 解析参数部分（简化：按_分割）
    std::vector<std::string> type_params;
    std::string current_param;
    for (char c : params_part) {
        if (c == '_') {
            if (!current_param.empty()) {
                type_params.push_back(current_param);
                current_param.clear();
            }
        } else {
            current_param += c;
        }
    }
    if (!current_param.empty()) {
        type_params.push_back(current_param);
    }
    
    // 解析每个类型参数
    std::vector<std::string> resolved_types;
    for (const auto& param : type_params) {
        // Check if it's泛型参数（单字母大写）
        if (param.length() == 1 && std::isupper(param[0])) {
            // 在type_param_map_中查找具体类型
            auto it = type_param_map_.find(param);
            if (it != type_param_map_.end() && !it->second.empty()) {
                llvm::Type* concrete_type = it->second.begin()->second;
                // 将LLVM Type转为字符串名称
                std::string type_str = "unknown";
                if (concrete_type->isIntegerTy()) {
                    unsigned bits = concrete_type->getIntegerBitWidth();
                    type_str = "i" + std::to_string(bits);
                } else if (concrete_type->isPointerTy()) {
                    type_str = "string";  // 假设指针是string
                }
                resolved_types.push_back(type_str);
            } else {
                resolved_types.push_back(param);  // 未找到，保持原样
            }
        } else {
            resolved_types.push_back(param);  // 非泛型参数，保持原样
        }
    }
    
    // 重新组装
    std::string result = base_name;
    for (const auto& t : resolved_types) {
        result += "_" + t;
    }
    
    return result;
}

// Check if it's泛型函数
bool CodeGenerator::isGenericFunction(const std::string& name) {
    return generic_functions_.find(name) != generic_functions_.end();
}

/**
 * @brief 实例化泛型函数（单态化）
 * @param name 泛型函数名
 * @param type_args 类型参数列表
 * @return llvm::Function* 实例化的函数，失败返回nullptr
 * 
 * 实现泛型函数的单态化（monomorphization）：
 * 1. 生成修饰名称（如sum_i32）
 * 2. 检查是否已实例化（缓存）
 * 3. 建立类型参数映射（T -> i32）
 * 4. 解析参数类型和返回类型
 * 5. 创建LLVM函数
 * 6. 生成函数体（使用type_param_map_）
 * 
 * @note 支持跨模块泛型函数调用
 * @note 自动处理数组参数的元素类型记录
 */
llvm::Function* CodeGenerator::instantiateGenericFunction(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    auto it = generic_functions_.find(name);
    if (it == generic_functions_.end()) {
        return nullptr;
    }
    
    const FunctionStmt* generic_func = it->second;
    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto func_it = functions_.find(mangled_name);
    if (func_it != functions_.end()) {
        return func_it->second;
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_func->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_func->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 生成具体函数
    std::vector<llvm::Type*> param_types;
    for (const auto& param : generic_func->parameters) {
        if (!param.is_self) {
            llvm::Type* param_type = resolveGenericType(param.type.get());
            
            // 对于数组参数，使用指针类型（数组按指针传递）
            if (llvm::isa<llvm::ArrayType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            // 【新增】：对于struct参数，使用指针类型（struct按指针传递）
            if (llvm::isa<llvm::StructType>(param_type)) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            // 【新增】：对于T?参数，使用指针类型（T?按指针传递）
            // 注意：resolveGenericType返回的已经是Optional<T> struct，我们需要指针
            if (param.type->kind == Type::Kind::Optional) {
                param_type = llvm::PointerType::get(*context_, 0);
            }
            
            param_types.push_back(param_type);
        }
    }
    
    llvm::Type* return_type = generic_func->return_type ?
        resolveGenericType(generic_func->return_type.get()) :
        llvm::Type::getVoidTy(*context_);
    
    // 【新增】：对于struct返回值，使用指针类型
    if (return_type && llvm::isa<llvm::StructType>(return_type)) {
        return_type = llvm::PointerType::get(*context_, 0);
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        mangled_name,
        module_.get()
    );
    
    functions_[mangled_name] = func;
    
    // 预实例化返回类型中的泛型struct（如果有）
    if (generic_func->return_type && generic_func->return_type->kind == Type::Kind::Named) {
        const NamedTypeNode* named_ret = static_cast<const NamedTypeNode*>(generic_func->return_type.get());
        if (!named_ret->generic_args.empty()) {
            // 返回类型是泛型struct实例（如Pair<K, V>或Pair<V, K>）
            // 需要将泛型参数映射到具体类型
            
            std::vector<const Type*> concrete_args_raw;
            for (const auto& arg : named_ret->generic_args) {
                if (arg->kind == Type::Kind::Generic) {
                    // 找到这个泛型参数在函数generic_params中的索引
                    const GenericTypeNode* gen = static_cast<const GenericTypeNode*>(arg.get());
                    for (size_t i = 0; i < generic_func->generic_params.size(); i++) {
                        if (generic_func->generic_params[i].name == gen->name) {
                            // 使用对应的具体类型（原始指针）
                            if (i < type_args.size()) {
                                concrete_args_raw.push_back(type_args[i].get());
                            }
                            break;
                        }
                    }
                } else {
                    // 非泛型参数，直接使用（原始指针）
                    concrete_args_raw.push_back(arg.get());
                }
            }
            
            // 实例化struct（使用原始指针数组）
            if (!concrete_args_raw.empty()) {
                // 调用重载版本
                std::string struct_mangled = named_ret->name;
                for (const auto* arg_ptr : concrete_args_raw) {
                    struct_mangled += "_";
                    if (arg_ptr->kind == Type::Kind::Primitive) {
                        const PrimitiveTypeNode* prim = static_cast<const PrimitiveTypeNode*>(arg_ptr);
                        switch (prim->prim_type) {
                            case PrimitiveType::I32: struct_mangled += "i32"; break;
                            case PrimitiveType::I64: struct_mangled += "i64"; break;
                            case PrimitiveType::STRING: struct_mangled += "string"; break;
                            default: struct_mangled += "T"; break;
                        }
                    } else if (arg_ptr->kind == Type::Kind::Named) {
                        const NamedTypeNode* named = static_cast<const NamedTypeNode*>(arg_ptr);
                        struct_mangled += named->name;
                    }
                }
                
                // 检查是否已实例化
                if (struct_types_.find(struct_mangled) == struct_types_.end()) {
                    // 手动实例化
                    auto gen_struct_it = generic_structs_.find(named_ret->name);
                    if (gen_struct_it == generic_structs_.end() && symbol_table_) {
                        auto symbol = symbol_table_->lookup(named_ret->name, module_name_);
                        if (symbol && symbol->ast_node) {
                            const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
                            if (struct_def && !struct_def->generic_params.empty()) {
                                generic_structs_[named_ret->name] = struct_def;
                                gen_struct_it = generic_structs_.find(named_ret->name);
                            }
                        }
                    }
                    
                    if (gen_struct_it != generic_structs_.end()) {
                        const StructStmt* gen_struct = gen_struct_it->second;
                        
                        // 建立临时type_param_map_
                        std::map<std::string, std::map<std::string, llvm::Type*>> temp_map;
                        for (size_t i = 0; i < concrete_args_raw.size() && i < gen_struct->generic_params.size(); i++) {
                            llvm::Type* concrete_type = convertType(concrete_args_raw[i]);
                            temp_map[gen_struct->generic_params[i].name][struct_mangled] = concrete_type;
                        }
                        
                        // 保存旧map
                        auto saved_map = type_param_map_;
                        type_param_map_ = temp_map;
                        
                        // 创建struct
                        llvm::StructType* st = llvm::StructType::create(*context_, struct_mangled);
                        struct_types_[struct_mangled] = st;
                        struct_defs_[struct_mangled] = gen_struct;
                        
                        std::vector<llvm::Type*> field_types;
                        for (const auto& field : gen_struct->fields) {
                            field_types.push_back(resolveGenericType(field.type.get()));
                        }
                        st->setBody(field_types);
                        
                        // 恢复map
                        type_param_map_ = saved_map;
                    }
                }
            }
        }
    }
    
    // 生成函数体
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*context_, "entry", func);
    auto old_insert_point = builder_->saveIP();
    builder_->SetInsertPoint(bb);
    
    // 保存旧的named_values
    auto old_named_values = named_values_;
    auto old_variable_types = variable_types_;
    auto old_array_element_types = array_element_types_;
    
    // 保存参数
    size_t idx = 0;
    for (auto& arg : func->args()) {
        const auto& param = generic_func->parameters[idx];
        
        // Check if it's数组参数
        if (param.type->kind == Type::Kind::Array) {
            // 数组参数：arg是ptr，直接创建ptr的alloca
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, param.name
            );
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 记录为指针类型（因为数组参数实际上是指针）
            variable_types_[param.name] = llvm::PointerType::get(*context_, 0);
            
            // 记录元素类型
            const ArrayTypeNode* array_type = static_cast<const ArrayTypeNode*>(param.type.get());
            llvm::Type* elem_type = resolveGenericType(array_type->element_type.get());
            array_element_types_[param.name] = elem_type;
        } else if (param.type->kind == Type::Kind::Optional) {
            // 【新增】：T?参数也是指针
            llvm::Type* param_type = resolveGenericType(param.type.get());
            // T?参数：arg是ptr
            llvm::AllocaInst* alloca = builder_->CreateAlloca(
                llvm::PointerType::get(*context_, 0), nullptr, param.name
            );
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 【关键】：记录具体的Optional<T>类型，而不是通用ptr
            // 这样is表达式时能找到正确的Optional定义
            variable_types_[param.name] = param_type;  // 记录Optional<T> StructType
        } else if (param.type->kind == Type::Kind::Named) {
            // 【新增】：struct参数也是指针
            llvm::Type* param_type = resolveGenericType(param.type.get());
            if (llvm::isa<llvm::StructType>(param_type)) {
                // struct参数：arg是ptr
                llvm::AllocaInst* alloca = builder_->CreateAlloca(
                    llvm::PointerType::get(*context_, 0), nullptr, param.name
                );
                builder_->CreateStore(&arg, alloca);
                named_values_[param.name] = alloca;
                
                // 【关键】：记录具体的struct类型，而不是通用ptr
                // 这样成员访问时能找到正确的struct定义
                variable_types_[param.name] = param_type;  // 记录StructType
            } else {
                // 非struct的Named类型（如enum）：正常处理
                llvm::AllocaInst* alloca = builder_->CreateAlloca(param_type, nullptr, param.name);
                builder_->CreateStore(&arg, alloca);
                named_values_[param.name] = alloca;
                variable_types_[param.name] = param_type;
            }
        } else {
            // 其他类型参数：正常处理
            llvm::Type* param_type = resolveGenericType(param.type.get());
            llvm::AllocaInst* alloca = builder_->CreateAlloca(param_type, nullptr, param.name);
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            variable_types_[param.name] = param_type;
        }
        
        idx++;
    }
    
    // 生成函数体
    generateStmt(generic_func->body.get());
    
    // 如果没有terminator，添加默认返回
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder_->CreateRetVoid();
        }
    }
    
    // 恢复
    named_values_ = old_named_values;
    variable_types_ = old_variable_types;
    array_element_types_ = old_array_element_types;
    type_param_map_ = old_map;
    builder_->restoreIP(old_insert_point);
    
    llvm::verifyFunction(*func);
    
    return func;
}

// 实例化泛型struct
llvm::Type* CodeGenerator::instantiateGenericStruct(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    const StructStmt* generic_struct = nullptr;
    
    // 1. 先查本地generic_structs_
    auto it = generic_structs_.find(name);
    if (it != generic_structs_.end()) {
        generic_struct = it->second;
    } else if (symbol_table_) {
        // 2. 查找跨模块的泛型struct定义
        auto symbol = symbol_table_->lookup(name, module_name_);
        if (symbol && symbol->kind == SymbolTable::SymbolKind::Type && symbol->ast_node) {
            // Check if it'sstruct定义（简化：假设有generic_params就是泛型struct）
            const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
            if (struct_def && !struct_def->generic_params.empty()) {
                generic_struct = struct_def;
                // 添加到本地缓存
                generic_structs_[name] = generic_struct;
            }
        }
    }
    
    if (!generic_struct) {
        return nullptr;
    }
    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto type_it = struct_types_.find(mangled_name);
    if (type_it != struct_types_.end()) {
        return type_it->second;  // 返回struct类型本身
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_struct->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_struct->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 先创建不透明struct类型，防止递归
    llvm::StructType* struct_type = llvm::StructType::create(*context_, mangled_name);
    struct_types_[mangled_name] = struct_type;
    struct_defs_[mangled_name] = generic_struct;  // 注册定义
    
    // 然后填充字段类型
    std::vector<llvm::Type*> field_types;
    for (const auto& field : generic_struct->fields) {
        field_types.push_back(resolveGenericType(field.type.get()));
    }
    struct_type->setBody(field_types);
    
    // 【新增】实例化泛型struct的所有方法
    instantiateGenericStructMethods(generic_struct, mangled_name, struct_type, type_args);
    
    // 恢复
    type_param_map_ = old_map;
    
    // 注册泛型struct实例到SymbolTable（如果有）
    if (symbol_table_ && generic_struct->is_public) {
        symbol_table_->registerGenericStructInstance(
            module_name_,
            mangled_name,
            name,
            true,  // 继承pub属性
            struct_type,
            static_cast<const void*>(generic_struct)
        );
    }
    
    return struct_type;  // 返回struct类型本身，不是指针
}

// 实例化泛型enum
llvm::Type* CodeGenerator::instantiateGenericEnum(
    const std::string& name,
    const std::vector<TypePtr>& type_args) {
    
    auto it = generic_enums_.find(name);
    if (it == generic_enums_.end()) {
        return nullptr;
    }
    
    const EnumStmt* generic_enum = it->second;
    
    // 生成修饰后的名称
    std::string mangled_name = mangleGenericName(name, type_args);
    
    // 检查是否已经实例化
    auto enum_it = enum_defs_.find(mangled_name);
    if (enum_it != enum_defs_.end()) {
        // 已经实例化，直接返回类型
        std::vector<llvm::Type*> fields = {
            llvm::Type::getInt32Ty(*context_),  // tag
            llvm::Type::getInt64Ty(*context_)   // data
        };
        return llvm::StructType::get(*context_, fields);
    }
    
    // 建立类型参数映射
    std::map<std::string, std::map<std::string, llvm::Type*>> old_map = type_param_map_;
    for (size_t i = 0; i < generic_enum->generic_params.size() && i < type_args.size(); i++) {
        const std::string& param_name = generic_enum->generic_params[i].name;
        llvm::Type* concrete_type = convertType(type_args[i].get());
        type_param_map_[param_name][mangled_name] = concrete_type;
    }
    
    // 注册实例化的enum定义（使用mangled_name）
    enum_defs_[mangled_name] = generic_enum;
    
    // 恢复
    type_param_map_ = old_map;
    
    // Enum表示为 {i32 tag, i64 data}（简化）
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),  // tag
        llvm::Type::getInt64Ty(*context_)   // data
    };
    
    return llvm::StructType::get(*context_, fields);
}


// ============================================================================
// 跨模块类型转换
// ============================================================================

/**
 * 将来自其他Context的类型转换为当前Context的类型
 * 用于跨模块函数调用时的类型匹配
 */
llvm::Type* CodeGenerator::convertTypeToCurrentContext(llvm::Type* type) {
    if (!type) return nullptr;
    
    // 基本类型直接映射到当前Context
    if (type->isVoidTy()) {
        return llvm::Type::getVoidTy(*context_);
    } else if (type->isIntegerTy()) {
        unsigned bits = type->getIntegerBitWidth();
        return llvm::Type::getIntNTy(*context_, bits);
    } else if (type->isFloatTy()) {
        return llvm::Type::getFloatTy(*context_);
    } else if (type->isDoubleTy()) {
        return llvm::Type::getDoubleTy(*context_);
    } else if (type->isPointerTy()) {
        // LLVM 21使用opaque pointers
        return llvm::PointerType::get(*context_, 0);
    } else if (type->isArrayTy()) {
        llvm::ArrayType* array_type = llvm::cast<llvm::ArrayType>(type);
        llvm::Type* elem_type = convertTypeToCurrentContext(array_type->getElementType());
        return llvm::ArrayType::get(elem_type, array_type->getNumElements());
    } else if (type->isStructTy()) {
        // 结构体类型 - 尝试通过名称查找或重建
        llvm::StructType* struct_type = llvm::cast<llvm::StructType>(type);
        
        // 如果有名字，尝试在当前模块中查找同名类型
        if (struct_type->hasName()) {
            std::string type_name = struct_type->getName().str();
            
            // 尝试从struct_types_中查找
            auto it = struct_types_.find(type_name);
            if (it != struct_types_.end()) {
                return it->second;
            }
            
            // 尝试从当前Context查找
            llvm::StructType* local_struct = llvm::StructType::getTypeByName(*context_, type_name);
            if (local_struct) {
                return local_struct;
            }
        }
        
        // 匿名结构体或无法重建的，创建等价结构
        std::vector<llvm::Type*> field_types;
        for (unsigned i = 0; i < struct_type->getNumElements(); i++) {
            llvm::Type* field_type = struct_type->getElementType(i);
            field_types.push_back(convertTypeToCurrentContext(field_type));
        }
        
        // 如果有名字，创建命名类型；否则创建匿名类型
        if (struct_type->hasName()) {
            std::string type_name = struct_type->getName().str();
            llvm::StructType* new_struct = llvm::StructType::create(*context_, field_types, type_name);
            struct_types_[type_name] = new_struct;
            return new_struct;
        } else {
            return llvm::StructType::get(*context_, field_types);
        }
    }
    
    // 默认返回i32
    return llvm::Type::getInt32Ty(*context_);
}

/**
 * 从其他模块导入类型定义
 * 在当前模块中重建Struct/Enum类型
 */
void CodeGenerator::importTypeFromModule(const std::string& type_name, const std::string& from_module) {
    if (!symbol_table_) return;
    
    // 查找类型符号
    auto symbol = symbol_table_->lookupInModule(from_module, type_name);
    if (!symbol || symbol->kind != SymbolTable::SymbolKind::Type) {
        return;
    }
    
    if (!symbol_table_->isAccessible(*symbol, module_name_)) {
        std::cerr << "Type " << type_name << " in module " << from_module 
                  << " is not accessible" << std::endl;
        return;
    }
    
    // 检查是否已经导入
    if (struct_types_.find(type_name) != struct_types_.end() ||
        enum_defs_.find(type_name) != enum_defs_.end()) {
        return;  // 已导入
    }
    
    // 根据AST节点重建类型
    if (symbol->ast_node) {
        // 检查是Struct还是Enum（通过尝试转换）
        const StructStmt* struct_def = static_cast<const StructStmt*>(symbol->ast_node);
        const EnumStmt* enum_def = static_cast<const EnumStmt*>(symbol->ast_node);
        
        // 简单判断：如果type是StructType，则是Struct
        if (symbol->type && symbol->type->isStructTy()) {
            // 重建Struct类型
            llvm::StructType* original = llvm::cast<llvm::StructType>(symbol->type);
            
            std::vector<llvm::Type*> field_types;
            for (unsigned i = 0; i < original->getNumElements(); i++) {
                llvm::Type* field_type = original->getElementType(i);
                field_types.push_back(convertTypeToCurrentContext(field_type));
            }
            
            llvm::StructType* new_struct = llvm::StructType::create(*context_, field_types, type_name);
            struct_types_[type_name] = new_struct;
            
            // 保存定义（如果有）
            if (struct_def) {
                struct_defs_[type_name] = struct_def;
            }
        } else {
            // 重建Enum类型
            std::vector<llvm::Type*> enum_fields = {
                llvm::Type::getInt32Ty(*context_),  // tag
                llvm::Type::getInt64Ty(*context_)   // data
            };
            
            // Enum不需要创建命名类型，直接使用
            if (enum_def) {
                enum_defs_[type_name] = enum_def;
            }
        }
    }
}

/**
 * 标识符表达式生成
 * 支持：普通变量、self、数组、struct
 */
llvm::Value* CodeGenerator::generateIdentifierExpr(const IdentifierExpr* expr) {
    auto it = named_values_.find(expr->name);
    if (it != named_values_.end()) {
        // 获取变量类型
        llvm::Type* var_type = nullptr;
        auto type_it = variable_types_.find(expr->name);
        if (type_it != variable_types_.end()) {
            var_type = type_it->second;
        }
        
        // 特殊处理self：在方法中，self存储为alloca(ptr)，load得到指向struct的指针
        if (expr->name == "self" && !current_struct_name_.empty()) {
            return builder_->CreateLoad(llvm::PointerType::get(*context_, 0), it->second, "self");
        }
        
        // 数组类型返回指针（alloca）
        if (var_type && var_type->isArrayTy()) {
            return it->second;  // 返回数组的指针
        }
        
        // struct类型：需要区分两种情况
        // 1. 本地struct变量：alloca ptr，存储heap指针，需要load
        // 2. 函数参数struct：alloca ptr，存储heap指针，需要load
        // 【重要】：在新语义下，所有struct都是alloca ptr，需要load来获取实际heap指针
        if (var_type && var_type->isStructTy()) {
            // 检查alloca的类型：如果是ptr，说明需要load
            llvm::Type* alloca_type = it->second->getType();
            if (alloca_type->isPointerTy()) {
                // alloca ptr -> load -> heap指针
                return builder_->CreateLoad(llvm::PointerType::get(*context_, 0), it->second, expr->name);
            } else {
                // 旧逻辑：alloca struct -> 返回指针
                return it->second;
            }
        }
        
        // 加载其他类型的值
        if (var_type) {
            return builder_->CreateLoad(var_type, it->second, expr->name);
        }
        
        return builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), it->second, expr->name);
    }
    
    std::cerr << "Unknown variable: " << expr->name << std::endl;
    return nullptr;
}

/**
 * If表达式生成
 * 语法: if condition { then_expr } else { else_expr }
 * 使用LLVM的select指令或PHI节点
 */
llvm::Value* CodeGenerator::generateIfExpr(const IfExpr* expr) {
    // 生成条件
    llvm::Value* cond = generateExpr(expr->condition.get());
    if (!cond) return nullptr;
    
    // 创建基本块
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "if_then", func);
    llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*context_, "if_else", func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "if_merge", func);
    
    // 条件分支
    builder_->CreateCondBr(cond, then_bb, else_bb);
    
    // Then分支
    builder_->SetInsertPoint(then_bb);
    llvm::Value* then_val = generateExpr(expr->then_expr.get());
    if (!then_val) return nullptr;
    builder_->CreateBr(merge_bb);
    then_bb = builder_->GetInsertBlock();  // 更新（可能被表达式修改）
    
    // Else分支
    builder_->SetInsertPoint(else_bb);
    llvm::Value* else_val = generateExpr(expr->else_expr.get());
    if (!else_val) return nullptr;
    builder_->CreateBr(merge_bb);
    else_bb = builder_->GetInsertBlock();  // 更新（可能被表达式修改）
    
    // Merge分支（使用PHI节点）
    builder_->SetInsertPoint(merge_bb);
    
    // 确保两个分支的类型一致
    llvm::Type* then_type = then_val->getType();
    llvm::Type* else_type = else_val->getType();
    
    if (then_type != else_type) {
        // 类型不匹配时报错
        std::cerr << "Error: if expression branches must have the same type" << std::endl;
        return nullptr;
    }
    
    llvm::PHINode* phi = builder_->CreatePHI(then_type, 2, "if_result");
    phi->addIncoming(then_val, then_bb);
    phi->addIncoming(else_val, else_bb);
    
    return phi;
}

/**
 * Cast表达式生成（类型转换）
 * 语法: expr as target_type
 */
llvm::Value* CodeGenerator::generateCastExpr(const CastExpr* expr) {
    llvm::Value* val = generateExpr(expr->expression.get());
    if (!val) return nullptr;
    
    llvm::Type* target_type = resolveGenericType(expr->target_type.get());
    if (!target_type) return nullptr;
    
    llvm::Type* source_type = val->getType();
    
    // 如果类型相同，直接返回
    if (source_type == target_type) {
        return val;
    }
    
    // 整数 → 整数（扩展或截断）
    if (source_type->isIntegerTy() && target_type->isIntegerTy()) {
        unsigned src_bits = source_type->getIntegerBitWidth();
        unsigned tgt_bits = target_type->getIntegerBitWidth();
        
        if (src_bits < tgt_bits) {
            return builder_->CreateSExt(val, target_type, "sext");
        } else if (src_bits > tgt_bits) {
            return builder_->CreateTrunc(val, target_type, "trunc");
        }
        return val;
    }
    
    // 整数 → 浮点
    if (source_type->isIntegerTy() && target_type->isFloatingPointTy()) {
        return builder_->CreateSIToFP(val, target_type, "sitofp");
    }
    
    // 浮点 → 整数
    if (source_type->isFloatingPointTy() && target_type->isIntegerTy()) {
        return builder_->CreateFPToSI(val, target_type, "fptosi");
    }
    
    // 浮点 → 浮点
    if (source_type->isFloatingPointTy() && target_type->isFloatingPointTy()) {
        unsigned src_bits = source_type->getScalarSizeInBits();
        unsigned tgt_bits = target_type->getScalarSizeInBits();
        
        if (src_bits < tgt_bits) {
            return builder_->CreateFPExt(val, target_type, "fpext");
        } else if (src_bits > tgt_bits) {
            return builder_->CreateFPTrunc(val, target_type, "fptrunc");
        }
        return val;
    }
    
    return val;
}

/**
 * 创建Optional类型结构: {i32 tag, T value, ptr error_msg}
 * tag: 0 = Value, 1 = Error
 */
llvm::StructType* CodeGenerator::createOptionalType(llvm::Type* value_type) {
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),           // tag
        value_type,                                   // value
        llvm::PointerType::get(*context_, 0)         // error_msg (i8*)
    };
    return llvm::StructType::get(*context_, fields);
}

/**
 * 确保T?类型有对应的enum定义，用于模式匹配
 * 创建虚拟的 enum { Value(T), Error(string) }
 */
void CodeGenerator::ensureOptionalEnumDef(llvm::Type* value_type, const std::string& type_name) {
    // 检查是否已经创建
    if (enum_defs_.find(type_name) != enum_defs_.end()) {
        return;
    }
    
    // 创建variants
    std::vector<EnumVariant> variants;
    
    // Value变体（泛型，关联值类型稍后处理）
    EnumVariant value_variant;
    value_variant.name = "Value";
    value_variant.location = SourceLocation();
    // 注意：不添加associated_types，模式匹配时会动态处理
    variants.push_back(std::move(value_variant));
    
    // Error变体（关联string）
    EnumVariant error_variant;
    error_variant.name = "Error";
    error_variant.location = SourceLocation();
    error_variant.associated_types.push_back(
        std::make_unique<PrimitiveTypeNode>(PrimitiveType::STRING, SourceLocation())
    );
    variants.push_back(std::move(error_variant));
    
    // 创建EnumStmt（使用构造函数）
    EnumStmt* optional_enum = new EnumStmt(
        type_name,
        std::vector<GenericParam>(),  // 无泛型参数
        std::move(variants),
        true,  // is_public
        SourceLocation()
    );
    
    // 注册到enum_defs_
    enum_defs_[type_name] = optional_enum;
}

/**
 * Try表达式生成: expr?
 * 如果expr是Error，立即返回Error；否则提取Value
 */
llvm::Value* CodeGenerator::generateTryExpr(const TryExpr* expr) {
    llvm::Value* val = generateExpr(expr->expression.get());
    if (!val) return nullptr;
    
    // 假设val是Optional类型: {i32 tag, T value, ptr error_msg}
    llvm::Type* optional_type = val->getType();
    if (!optional_type->isStructTy()) {
        std::cerr << "Error: ? operator can only be used on Optional types" << std::endl;
        return nullptr;
    }
    
    llvm::StructType* struct_type = llvm::cast<llvm::StructType>(optional_type);
    if (struct_type->getNumElements() != 3) {
        std::cerr << "Error: Invalid Optional type structure" << std::endl;
        return nullptr;
    }
    
    // 如果val是struct值（而不是指针），先存储到alloca
    llvm::Value* opt_ptr = val;
    if (!val->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder_->CreateAlloca(struct_type, nullptr, "opt_temp");
        builder_->CreateStore(val, temp);
        opt_ptr = temp;
    }
    
    // 创建基本块
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* value_bb = llvm::BasicBlock::Create(*context_, "try_value", func);
    llvm::BasicBlock* error_bb = llvm::BasicBlock::Create(*context_, "try_error", func);
    
    // 加载tag字段（索引0）
    llvm::Value* tag_ptr = builder_->CreateStructGEP(struct_type, opt_ptr, 0, "tag_ptr");
    llvm::Value* tag = builder_->CreateLoad(llvm::Type::getInt32Ty(*context_), tag_ptr, "tag");
    
    // 检查tag: 0=Value, 1=Error
    llvm::Value* is_error = builder_->CreateICmpEQ(
        tag,
        llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)),
        "is_error"
    );
    builder_->CreateCondBr(is_error, error_bb, value_bb);
    
    // Error分支：返回Error
    builder_->SetInsertPoint(error_bb);
    llvm::Value* error_val = builder_->CreateLoad(struct_type, opt_ptr, "error_val");
    builder_->CreateRet(error_val);  // 返回Optional（包含错误）
    
    // Value分支：提取值
    builder_->SetInsertPoint(value_bb);
    llvm::Value* value_ptr = builder_->CreateStructGEP(struct_type, opt_ptr, 1, "value_ptr");
    llvm::Type* value_type = struct_type->getElementType(1);
    llvm::Value* extracted_value = builder_->CreateLoad(value_type, value_ptr, "extracted");
    
    return extracted_value;
}

/**
 * Ok表达式生成: ok(value)
 * 创建Optional类型的Value变体，返回struct值（不是指针）
 */
llvm::Value* CodeGenerator::generateOkExpr(const OkExpr* expr) {
    llvm::Value* val = generateExpr(expr->value.get());
    if (!val) return nullptr;
    
    // 创建Optional类型
    llvm::StructType* optional_type = createOptionalType(val->getType());
    
    // 为T?类型创建enum定义（用于模式匹配）
    // 使用类型的字符串表示作为名称
    std::string type_name = "Optional";  // 简化：所有T?使用相同的enum名称
    ensureOptionalEnumDef(val->getType(), type_name);
    
    // 1. 在栈上创建临时Optional
    llvm::AllocaInst* temp_result = builder_->CreateAlloca(optional_type, nullptr, "ok_temp");
    
    // 2. 初始化字段
    // 设置tag = 0 (Value)
    llvm::Value* tag_ptr = builder_->CreateStructGEP(optional_type, temp_result, 0, "tag_ptr");
    builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, 0)), tag_ptr);
    
    // 设置value
    llvm::Value* value_ptr = builder_->CreateStructGEP(optional_type, temp_result, 1, "value_ptr");
    builder_->CreateStore(val, value_ptr);
    
    // 设置error_msg = null
    llvm::Value* error_ptr = builder_->CreateStructGEP(optional_type, temp_result, 2, "error_ptr");
    builder_->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(*context_, 0)), error_ptr);
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t optional_size = data_layout.getTypeAllocSize(optional_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, optional_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "ok_heap");
    
    // 4. 拷贝到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_result, size_val});
    }
    
    // 5. 返回堆指针（统一语义）
    return heap_ptr;
}

/**
 * Err表达式生成: err(message)
 * 创建Optional类型的Error变体，需要从函数返回类型推导T
 */
llvm::Value* CodeGenerator::generateErrExpr(const ErrExpr* expr) {
    llvm::Value* msg = generateExpr(expr->message.get());
    if (!msg) return nullptr;
    
    // 从函数返回类型推导T类型
    llvm::Type* value_type = llvm::Type::getInt32Ty(*context_);  // 默认i32
    
    if (current_function_return_type_) {
        // 如果返回类型是T?，提取T
        if (auto opt_type = dynamic_cast<const OptionalTypeNode*>(current_function_return_type_)) {
            value_type = convertType(opt_type->inner_type.get());
        } else {
            // 不是Optional类型，使用函数返回类型本身
            value_type = convertType(current_function_return_type_);
        }
    }
    
    // 创建Optional类型: {i32 tag, T value, ptr error_msg}
    llvm::StructType* optional_type = createOptionalType(value_type);
    
    // 为T?类型创建enum定义（用于模式匹配）
    std::string type_name = "Optional";
    ensureOptionalEnumDef(value_type, type_name);
    
    // 1. 在栈上创建临时Optional
    llvm::AllocaInst* temp_result = builder_->CreateAlloca(optional_type, nullptr, "err_temp");
    
    // 2. 初始化字段
    // 设置tag = 1 (Error)
    llvm::Value* tag_ptr = builder_->CreateStructGEP(optional_type, temp_result, 0, "tag_ptr");
    builder_->CreateStore(llvm::ConstantInt::get(*context_, llvm::APInt(32, 1)), tag_ptr);
    
    // 设置value = 0 (零值，类型匹配T)
    llvm::Value* value_ptr = builder_->CreateStructGEP(optional_type, temp_result, 1, "value_ptr");
    llvm::Value* zero_value = llvm::Constant::getNullValue(value_type);
    builder_->CreateStore(zero_value, value_ptr);
    
    // 设置error_msg
    llvm::Value* error_ptr = builder_->CreateStructGEP(optional_type, temp_result, 2, "error_ptr");
    builder_->CreateStore(msg, error_ptr);
    
    // 3. 分配堆内存
    const llvm::DataLayout& data_layout = module_->getDataLayout();
    uint64_t optional_size = data_layout.getTypeAllocSize(optional_type);
    llvm::Value* size_val = llvm::ConstantInt::get(*context_, llvm::APInt(64, optional_size));
    
    llvm::Function* malloc_func = module_->getFunction("malloc");
    if (!malloc_func) {
        std::cerr << "malloc not found!" << std::endl;
        return nullptr;
    }
    
    llvm::Value* heap_ptr = builder_->CreateCall(malloc_func, {size_val}, "err_heap");
    
    // 4. 拷贝到堆
    llvm::Function* memcpy_func = module_->getFunction("memcpy");
    if (memcpy_func) {
        builder_->CreateCall(memcpy_func, {heap_ptr, temp_result, size_val});
    }
    
    // 5. 返回堆指针（统一语义）
    return heap_ptr;
}

/**
 * @brief 实例化泛型struct的所有方法（单态化）
 * @param generic_struct 泛型struct定义
 * @param struct_mangled_name Struct修饰名称（如Pair_i32_string）
 * @param struct_type Struct的LLVM类型
 * @param type_args 类型参数列表
 * 
 * 当泛型struct被实例化时自动调用，生成所有方法的具体版本。
 * 
 * 处理流程：
 * 1. 遍历泛型struct的所有方法定义
 * 2. 为每个方法生成修饰名称（如new_i32_string）
 * 3. 构造参数类型列表：
 *    - self参数：struct指针（ptr）
 *    - 其他参数：解析泛型类型
 * 4. 确定返回类型（支持Self类型）
 * 5. 创建LLVM函数并注册
 * 6. 生成方法体：
 *    - 设置current_struct_上下文
 *    - 处理self参数
 *    - 生成方法体语句
 * 7. 恢复上下文
 * 
 * @note 支持静态方法和实例方法
 * @note 自动处理Self类型解析
 * @note 方法注册到struct_methods_映射
 */
void CodeGenerator::instantiateGenericStructMethods(
    const StructStmt* generic_struct,
    const std::string& struct_mangled_name,
    llvm::StructType* struct_type,
    const std::vector<TypePtr>& type_args) {
    
    if (!generic_struct || generic_struct->methods.empty()) {
        return;  // 没有方法，直接返回
    }
    
    // 【关键】保存并设置当前struct上下文，用于Self类型解析
    auto old_current_struct = current_struct_;
    auto old_current_struct_name = current_struct_name_;
    current_struct_ = generic_struct;
    current_struct_name_ = struct_mangled_name;
    
    // 遍历所有方法并实例化
    for (const auto& method : generic_struct->methods) {
        // 构造方法的mangled name
        // 例如：Pair_i32_i32 -> new_i32_i32, get_first_i32_i32
        std::string method_mangled = method->name + "_" + 
            struct_mangled_name.substr(generic_struct->name.length() + 1);
        
        // 检查是否已实例化
        if (functions_.find(method_mangled) != functions_.end()) {
            continue;  // 已存在，跳过
        }
        
        // 构造参数类型列表
        std::vector<llvm::Type*> param_types;
        
        for (const auto& param : method->parameters) {
            llvm::Type* param_type = nullptr;
            
            if (param.is_self) {
                // self参数是struct指针
                param_type = llvm::PointerType::get(*context_, 0);
            } else {
                // 普通参数，使用resolveGenericType解析
                param_type = resolveGenericType(param.type.get());
                
                // struct和数组参数传指针
                if (param_type && (llvm::isa<llvm::StructType>(param_type) || 
                    llvm::isa<llvm::ArrayType>(param_type))) {
                    param_type = llvm::PointerType::get(*context_, 0);
                }
            }
            
            if (param_type) {
                param_types.push_back(param_type);
            }
        }
        
        // 确定返回类型
        llvm::Type* return_type = nullptr;
        if (method->return_type) {
            return_type = resolveGenericType(method->return_type.get());
            
            // struct返回值使用指针
            if (return_type && llvm::isa<llvm::StructType>(return_type)) {
                return_type = llvm::PointerType::get(*context_, 0);
            }
        } else {
            return_type = llvm::Type::getVoidTy(*context_);
        }
        
        // 创建函数
        llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
        llvm::Function* func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            method_mangled,
            module_.get()
        );
        
        // 注册函数
        functions_[method_mangled] = func;
        struct_methods_[struct_mangled_name][method->name] = func;
        
        // 生成函数体
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*context_, "entry", func);
        auto old_insert_point = builder_->saveIP();
        builder_->SetInsertPoint(entry_bb);
        
        // 保存当前上下文
        auto old_named_values = named_values_;
        auto old_variable_types = variable_types_;
        auto old_current_function = current_function_;
        auto old_current_struct = current_struct_;
        auto old_current_struct_name = current_struct_name_;
        auto old_is_method = current_is_method_;
        
        current_function_ = func;
        current_struct_ = generic_struct;
        current_struct_name_ = struct_mangled_name;
        current_is_method_ = false;
        
        // 设置参数
        size_t arg_idx = 0;
        for (auto& arg : func->args()) {
            const auto& param = method->parameters[arg_idx];
            
            llvm::AllocaInst* alloca = builder_->CreateAlloca(arg.getType(), nullptr, param.name);
            builder_->CreateStore(&arg, alloca);
            named_values_[param.name] = alloca;
            
            // 对于self参数，记录实际的struct类型
            if (param.is_self) {
                variable_types_[param.name] = struct_type;
                current_is_method_ = true;
            } else {
                variable_types_[param.name] = arg.getType();
            }
            
            arg_idx++;
        }
        
        // 生成方法体
        if (method->body) {
            generateStmt(method->body.get());
        }
        
        // 如果没有终止指令，添加默认返回
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (func->getReturnType()->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
            }
        }
        
        // 恢复上下文
        named_values_ = old_named_values;
        variable_types_ = old_variable_types;
        current_function_ = old_current_function;
        current_struct_ = old_current_struct;
        current_struct_name_ = old_current_struct_name;
        current_is_method_ = old_is_method;
        builder_->restoreIP(old_insert_point);
    }
    
    // 恢复外层struct上下文
    current_struct_ = old_current_struct;
    current_struct_name_ = old_current_struct_name;
}

} // namespace pawc
