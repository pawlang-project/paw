#include "builtins.h"
#include <llvm/IR/DerivedTypes.h>

namespace pawc {

Builtins::Builtins(llvm::LLVMContext& context, llvm::Module& module)
    : context_(context), module_(module) {}

void Builtins::declareAll() {
    // 声明libc函数
    declarePrintf();
    // strlen, strcmp等由用户通过extern声明（支持size类型）
    // declareStrcat();
    // declareStrcpy();
    // declareStrlen();
    // declareMalloc();
    // 实现print和println
    declarePrint();
    declarePrintln();
    declareEprint();
    declareEprintln();
}

llvm::Function* Builtins::getFunction(const std::string& name) {
    auto it = builtins_.find(name);
    if (it != builtins_.end()) {
        return it->second;
    }
    return nullptr;
}

bool Builtins::isBuiltin(const std::string& name) const {
    return builtins_.find(name) != builtins_.end();
}

llvm::FunctionType* Builtins::createPrintFunctionType() {
    // void (ptr) - 接受字符串指针，无返回值
    return llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {llvm::PointerType::get(context_, 0)},
        false  // 不是变参函数
    );
}

void Builtins::declarePrintf() {
    // 声明 printf: i32 printf(i8*, ...)
    llvm::FunctionType* printf_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {llvm::PointerType::get(context_, 0)},
        true  // 变参函数
    );
    
    llvm::Function::Create(
        printf_type,
        llvm::Function::ExternalLinkage,
        "printf",
        &module_
    );
}

void Builtins::declareStrcat() {
    // 声明 strcat: i8* strcat(i8* dest, i8* src)
    llvm::FunctionType* strcat_type = llvm::FunctionType::get(
        llvm::PointerType::get(context_, 0),
        {llvm::PointerType::get(context_, 0), llvm::PointerType::get(context_, 0)},
        false
    );
    
    llvm::Function::Create(
        strcat_type,
        llvm::Function::ExternalLinkage,
        "strcat",
        &module_
    );
}

void Builtins::declareStrcpy() {
    // 声明 strcpy: i8* strcpy(i8* dest, i8* src)
    llvm::FunctionType* strcpy_type = llvm::FunctionType::get(
        llvm::PointerType::get(context_, 0),
        {llvm::PointerType::get(context_, 0), llvm::PointerType::get(context_, 0)},
        false
    );
    
    llvm::Function::Create(
        strcpy_type,
        llvm::Function::ExternalLinkage,
        "strcpy",
        &module_
    );
}

void Builtins::declareStrlen() {
    // 不在这里声明strlen了，让用户通过extern声明
    // 这样可以使用正确的size类型
    // Builtins只声明print/println/eprint/eprintln
}

void Builtins::declareMalloc() {
    // 声明 malloc: i8* malloc(i64)
    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
        llvm::PointerType::get(context_, 0),
        {llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function::Create(
        malloc_type,
        llvm::Function::ExternalLinkage,
        "malloc",
        &module_
    );
}

void Builtins::declarePrint() {
    // 实现 print 函数: void print(const char*)
    llvm::FunctionType* print_type = createPrintFunctionType();
    llvm::Function* print_func = llvm::Function::Create(
        print_type,
        llvm::Function::LinkOnceODRLinkage,  // 允许多个定义，链接时保留一个
        "print",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", print_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取printf函数
    llvm::Function* printf_func = module_.getFunction("printf");
    
    // 调用printf
    llvm::Value* str_arg = print_func->arg_begin();
    builder.CreateCall(printf_func, {str_arg});
    builder.CreateRetVoid();
    
    builtins_["print"] = print_func;
}

void Builtins::declarePrintln() {
    // 实现 println 函数: void println(const char*)
    llvm::FunctionType* println_type = createPrintFunctionType();
    llvm::Function* println_func = llvm::Function::Create(
        println_type,
        llvm::Function::LinkOnceODRLinkage,  // 允许多个定义，链接时保留一个
        "println",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", println_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取printf函数
    llvm::Function* printf_func = module_.getFunction("printf");
    
    // 创建格式字符串 "%s\n"
    llvm::Value* format_str = builder.CreateGlobalString("%s\n", "println_fmt");
    
    // 调用printf(format, str)
    llvm::Value* str_arg = println_func->arg_begin();
    builder.CreateCall(printf_func, {format_str, str_arg});
    builder.CreateRetVoid();
    
    builtins_["println"] = println_func;
}

void Builtins::declareEprint() {
    // 实现 eprint 函数: void eprint(const char*) - 输出到stderr
    llvm::FunctionType* eprint_type = createPrintFunctionType();
    llvm::Function* eprint_func = llvm::Function::Create(
        eprint_type,
        llvm::Function::LinkOnceODRLinkage,
        "eprint",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", eprint_func);
    llvm::IRBuilder<> builder(entry);
    
    // 声明fprintf: i32 fprintf(FILE*, const char*, ...)
    llvm::FunctionType* fprintf_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {llvm::PointerType::get(context_, 0), llvm::PointerType::get(context_, 0)},
        true
    );
    llvm::Function* fprintf_func = llvm::Function::Create(
        fprintf_type,
        llvm::Function::ExternalLinkage,
        "fprintf",
        &module_
    );
    
    // 声明stderr: extern FILE* stderr
    llvm::GlobalVariable* stderr_var = new llvm::GlobalVariable(
        module_,
        llvm::PointerType::get(context_, 0),
        false,  // not constant
        llvm::GlobalValue::ExternalLinkage,
        nullptr,
        "__stderrp"  // macOS上stderr的符号名
    );
    
    // 调用fprintf(stderr, str)
    llvm::Value* stderr_ptr = builder.CreateLoad(
        llvm::PointerType::get(context_, 0), stderr_var, "stderr"
    );
    llvm::Value* str_arg = eprint_func->arg_begin();
    builder.CreateCall(fprintf_func, {stderr_ptr, str_arg});
    builder.CreateRetVoid();
    
    builtins_["eprint"] = eprint_func;
}

void Builtins::declareEprintln() {
    // 实现 eprintln 函数: void eprintln(const char*) - 输出到stderr带换行
    llvm::FunctionType* eprintln_type = createPrintFunctionType();
    llvm::Function* eprintln_func = llvm::Function::Create(
        eprintln_type,
        llvm::Function::LinkOnceODRLinkage,
        "eprintln",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", eprintln_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取fprintf函数
    llvm::Function* fprintf_func = module_.getFunction("fprintf");
    if (!fprintf_func) {
        llvm::FunctionType* fprintf_type = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context_),
            {llvm::PointerType::get(context_, 0), llvm::PointerType::get(context_, 0)},
            true
        );
        fprintf_func = llvm::Function::Create(
            fprintf_type,
            llvm::Function::ExternalLinkage,
            "fprintf",
            &module_
        );
    }
    
    // 获取stderr
    llvm::GlobalVariable* stderr_var = module_.getGlobalVariable("__stderrp");
    if (!stderr_var) {
        stderr_var = new llvm::GlobalVariable(
            module_,
            llvm::PointerType::get(context_, 0),
            false,
            llvm::GlobalValue::ExternalLinkage,
            nullptr,
            "__stderrp"
        );
    }
    
    // 创建格式字符串 "%s\n"
    llvm::Value* format_str = builder.CreateGlobalString("%s\n", "eprintln_fmt");
    
    // 调用fprintf(stderr, format, str)
    llvm::Value* stderr_ptr = builder.CreateLoad(
        llvm::PointerType::get(context_, 0), stderr_var, "stderr"
    );
    llvm::Value* str_arg = eprintln_func->arg_begin();
    builder.CreateCall(fprintf_func, {stderr_ptr, format_str, str_arg});
    builder.CreateRetVoid();
    
    builtins_["eprintln"] = eprintln_func;
}

} // namespace pawc

