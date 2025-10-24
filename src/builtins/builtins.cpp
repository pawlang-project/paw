#include "builtins.h"
#include <llvm/IR/DerivedTypes.h>

namespace pawc {

Builtins::Builtins(llvm::LLVMContext& context, llvm::Module& module)
    : context_(context), module_(module) {}

void Builtins::declareAll() {
    // 声明libc函数
    declarePrintf();
    // 字符串函数(strlen, strcmp等)由用户通过extern声明
    // 但为了支持字符串拼接，我们声明这些常用函数
    declareStrlen();
    declareStrcpy();
    declareStrcat();
    declareMalloc();  // 启用malloc（用于struct堆分配和字符串拼接）
    declareMemcpy();  // 启用memcpy（用于struct拷贝）
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
    // 声明 strlen: i64 strlen(i8*)
    llvm::FunctionType* strlen_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::PointerType::get(context_, 0)},
        false
    );
    
    llvm::Function::Create(
        strlen_type,
        llvm::Function::ExternalLinkage,
        "strlen",
        &module_
    );
}

void Builtins::declareMalloc() {
    // 声明 malloc: ptr malloc(i64)
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

void Builtins::declareMemcpy() {
    // 声明 memcpy: ptr memcpy(ptr dest, ptr src, i64 n)
    llvm::FunctionType* memcpy_type = llvm::FunctionType::get(
        llvm::PointerType::get(context_, 0),
        {llvm::PointerType::get(context_, 0), 
         llvm::PointerType::get(context_, 0),
         llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function::Create(
        memcpy_type,
        llvm::Function::ExternalLinkage,
        "memcpy",
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
    
    // 跨平台 stderr 输出的最佳方案：
    // 使用 write(2, buf, len) - 文件描述符 2 就是 stderr
    // Windows: _write(2, ...)
    // Unix/Linux/macOS: write(2, ...)
    
    // 声明 strlen: i64 strlen(const char*)
    llvm::FunctionType* strlen_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::PointerType::get(context_, 0)},
        false
    );
    llvm::Function* strlen_func = module_.getFunction("strlen");
    if (!strlen_func) {
        strlen_func = llvm::Function::Create(
            strlen_type,
            llvm::Function::ExternalLinkage,
            "strlen",
            &module_
        );
    }
    
    // 声明 write: i64 write(i32 fd, const char* buf, i64 count)
    llvm::FunctionType* write_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::Type::getInt32Ty(context_), llvm::PointerType::get(context_, 0), llvm::Type::getInt64Ty(context_)},
        false
    );
    llvm::Function* write_func = module_.getFunction("write");
    if (!write_func) {
        write_func = llvm::Function::Create(
            write_type,
            llvm::Function::ExternalLinkage,
            "write",
            &module_
        );
    }
    
    // 获取字符串长度
    llvm::Value* str_arg = eprint_func->arg_begin();
    llvm::Value* len = builder.CreateCall(strlen_func, {str_arg});
    
    // 调用 write(2, str, len) - 2 是 stderr 的文件描述符
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, str_arg, len});
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
    
    // 同样使用 write(2, ...) 输出到 stderr
    
    // 获取 strlen 和 write 函数
    llvm::Function* strlen_func = module_.getFunction("strlen");
    llvm::Function* write_func = module_.getFunction("write");
    
    // 获取字符串长度
    llvm::Value* str_arg = eprintln_func->arg_begin();
    llvm::Value* len = builder.CreateCall(strlen_func, {str_arg});
    
    // 调用 write(2, str, len) 输出字符串
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, str_arg, len});
    
    // 输出换行符 '\n'
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    builder.CreateRetVoid();
    
    builtins_["eprintln"] = eprintln_func;
}

} // namespace pawc

