#include "builtins.h"
#include <llvm/IR/DerivedTypes.h>
#include <iostream>
#include <unordered_set>

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
    
    // 错误处理和调试
    declarePanic();
    declareAssert();
    declareUnreachable();
    declareTodo();
    declareUnimplemented();
    
    // 编译器intrinsics（标记为builtin，实际在codegen中内联）
    declareLen();
    declareIsEmpty();
    declareDebug();
    declareAbs();
    declareMin();
    declareMax();
    declareClamp();
    declarePow();
    declareSqrt();
    declareFloor();
    declareCeil();
    declareRound();
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

bool Builtins::isIntrinsic(const std::string& name) const {
    // 编译器intrinsics列表
    static const std::unordered_set<std::string> intrinsics = {
        "len",
        "is_empty",
        "debug",
        "abs",
        "min",
        "max",
        "clamp",
        "pow",
        "sqrt",
        "floor",
        "ceil",
        "round"
    };
    return intrinsics.find(name) != intrinsics.end();
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

void Builtins::declareLen() {
    // len()是编译器intrinsic，不创建实际函数
    // 只是标记它存在，实际在codegen_expr.cpp中内联展开
    // 这样builtins_->isBuiltin("len")会返回true
    
    // 创建一个dummy函数声明（不会被实际调用）
    llvm::FunctionType* len_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::PointerType::get(context_, 0)},  // 占位符类型
        false
    );
    
    llvm::Function* len_func = llvm::Function::Create(
        len_type,
        llvm::Function::ExternalLinkage,
        "len_intrinsic",  // 实际不会被调用
        &module_
    );
    
    builtins_["len"] = len_func;
}

void Builtins::declareIsEmpty() {
    // is_empty()是编译器intrinsic，不创建实际函数
    // 只是标记它存在，实际在codegen_expr.cpp中内联展开
    
    llvm::FunctionType* is_empty_type = llvm::FunctionType::get(
        llvm::Type::getInt1Ty(context_),
        {llvm::PointerType::get(context_, 0)},
        false
    );
    
    llvm::Function* is_empty_func = llvm::Function::Create(
        is_empty_type,
        llvm::Function::ExternalLinkage,
        "is_empty_intrinsic",
        &module_
    );
    
    builtins_["is_empty"] = is_empty_func;
}

void Builtins::declarePanic() {
    // 实现 panic 函数: void panic(const char*) - 打印错误并终止程序
    llvm::FunctionType* panic_type = createPrintFunctionType();
    llvm::Function* panic_func = llvm::Function::Create(
        panic_type,
        llvm::Function::LinkOnceODRLinkage,
        "panic",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", panic_func);
    llvm::IRBuilder<> builder(entry);
    
    // 声明 exit: void exit(i32)
    llvm::FunctionType* exit_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {llvm::Type::getInt32Ty(context_)},
        false
    );
    llvm::Function* exit_func = module_.getFunction("exit");
    if (!exit_func) {
        exit_func = llvm::Function::Create(
            exit_type,
            llvm::Function::ExternalLinkage,
            "exit",
            &module_
        );
    }
    
    // 获取 write 函数（用于输出到stderr）
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
    
    llvm::Function* strlen_func = module_.getFunction("strlen");
    
    // 输出 "PANIC: "
    llvm::Value* panic_prefix = builder.CreateGlobalString("PANIC: ", "panic_prefix");
    llvm::Value* prefix_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 7);
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, panic_prefix, prefix_len});
    
    // 输出错误消息
    llvm::Value* msg_arg = panic_func->arg_begin();
    llvm::Value* msg_len = builder.CreateCall(strlen_func, {msg_arg});
    builder.CreateCall(write_func, {stderr_fd, msg_arg, msg_len});
    
    // 输出换行
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    // 调用 exit(1)
    builder.CreateCall(exit_func, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
    builder.CreateUnreachable();
    
    builtins_["panic"] = panic_func;
}

void Builtins::declareAssert() {
    // 实现 assert 函数: void assert(bool condition, const char* message)
    llvm::FunctionType* assert_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {llvm::Type::getInt1Ty(context_), llvm::PointerType::get(context_, 0)},
        false
    );
    llvm::Function* assert_func = llvm::Function::Create(
        assert_type,
        llvm::Function::LinkOnceODRLinkage,
        "assert",
        &module_
    );
    
    // 创建基本块
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", assert_func);
    llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(context_, "assert_fail", assert_func);
    llvm::BasicBlock* pass_bb = llvm::BasicBlock::Create(context_, "assert_pass", assert_func);
    
    llvm::IRBuilder<> builder(entry);
    
    // 获取参数
    auto arg_it = assert_func->arg_begin();
    llvm::Value* condition = arg_it++;
    llvm::Value* message = arg_it;
    
    // 条件分支
    builder.CreateCondBr(condition, pass_bb, fail_bb);
    
    // 失败分支：直接输出并终止（不调用panic）
    builder.SetInsertPoint(fail_bb);
    
    // 声明必要的函数
    llvm::FunctionType* exit_type = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context_),
        {llvm::Type::getInt32Ty(context_)},
        false
    );
    llvm::Function* exit_func = module_.getFunction("exit");
    if (!exit_func) {
        exit_func = llvm::Function::Create(
            exit_type,
            llvm::Function::ExternalLinkage,
            "exit",
            &module_
        );
    }
    
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
    
    llvm::Function* strlen_func = module_.getFunction("strlen");
    
    // 输出 "ASSERTION FAILED: "
    llvm::Value* assert_prefix = builder.CreateGlobalString("ASSERTION FAILED: ", "assert_prefix");
    llvm::Value* prefix_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 18);
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, assert_prefix, prefix_len});
    
    // 输出错误消息
    llvm::Value* msg_len = builder.CreateCall(strlen_func, {message});
    builder.CreateCall(write_func, {stderr_fd, message, msg_len});
    
    // 输出换行
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    // 调用 exit(1)
    builder.CreateCall(exit_func, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
    builder.CreateUnreachable();
    
    // 成功分支：直接返回
    builder.SetInsertPoint(pass_bb);
    builder.CreateRetVoid();
    
    builtins_["assert"] = assert_func;
}

void Builtins::declareUnreachable() {
    // 实现 unreachable 函数: void unreachable(const char*)
    llvm::FunctionType* unreachable_type = createPrintFunctionType();
    llvm::Function* unreachable_func = llvm::Function::Create(
        unreachable_type,
        llvm::Function::LinkOnceODRLinkage,
        "unreachable",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", unreachable_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取必要的函数
    llvm::Function* exit_func = module_.getFunction("exit");
    llvm::Function* write_func = module_.getFunction("write");
    llvm::Function* strlen_func = module_.getFunction("strlen");
    
    // 输出 "UNREACHABLE: "
    llvm::Value* prefix = builder.CreateGlobalString("UNREACHABLE: ", "unreachable_prefix");
    llvm::Value* prefix_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 13);
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, prefix, prefix_len});
    
    // 输出错误消息
    llvm::Value* msg_arg = unreachable_func->arg_begin();
    llvm::Value* msg_len = builder.CreateCall(strlen_func, {msg_arg});
    builder.CreateCall(write_func, {stderr_fd, msg_arg, msg_len});
    
    // 输出换行
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    // 调用 exit(1)
    builder.CreateCall(exit_func, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
    builder.CreateUnreachable();
    
    builtins_["unreachable"] = unreachable_func;
}

void Builtins::declareTodo() {
    // 实现 todo 函数: void todo(const char*)
    llvm::FunctionType* todo_type = createPrintFunctionType();
    llvm::Function* todo_func = llvm::Function::Create(
        todo_type,
        llvm::Function::LinkOnceODRLinkage,
        "todo",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", todo_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取必要的函数
    llvm::Function* exit_func = module_.getFunction("exit");
    llvm::Function* write_func = module_.getFunction("write");
    llvm::Function* strlen_func = module_.getFunction("strlen");
    
    // 输出 "TODO: "
    llvm::Value* prefix = builder.CreateGlobalString("TODO: ", "todo_prefix");
    llvm::Value* prefix_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 6);
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, prefix, prefix_len});
    
    // 输出错误消息
    llvm::Value* msg_arg = todo_func->arg_begin();
    llvm::Value* msg_len = builder.CreateCall(strlen_func, {msg_arg});
    builder.CreateCall(write_func, {stderr_fd, msg_arg, msg_len});
    
    // 输出换行
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    // 调用 exit(1)
    builder.CreateCall(exit_func, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
    builder.CreateUnreachable();
    
    builtins_["todo"] = todo_func;
}

void Builtins::declareUnimplemented() {
    // 实现 unimplemented 函数: void unimplemented(const char*)
    llvm::FunctionType* unimplemented_type = createPrintFunctionType();
    llvm::Function* unimplemented_func = llvm::Function::Create(
        unimplemented_type,
        llvm::Function::LinkOnceODRLinkage,
        "unimplemented",
        &module_
    );
    
    // 创建函数体
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", unimplemented_func);
    llvm::IRBuilder<> builder(entry);
    
    // 获取必要的函数
    llvm::Function* exit_func = module_.getFunction("exit");
    llvm::Function* write_func = module_.getFunction("write");
    llvm::Function* strlen_func = module_.getFunction("strlen");
    
    // 输出 "UNIMPLEMENTED: "
    llvm::Value* prefix = builder.CreateGlobalString("UNIMPLEMENTED: ", "unimplemented_prefix");
    llvm::Value* prefix_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 15);
    llvm::Value* stderr_fd = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 2);
    builder.CreateCall(write_func, {stderr_fd, prefix, prefix_len});
    
    // 输出错误消息
    llvm::Value* msg_arg = unimplemented_func->arg_begin();
    llvm::Value* msg_len = builder.CreateCall(strlen_func, {msg_arg});
    builder.CreateCall(write_func, {stderr_fd, msg_arg, msg_len});
    
    // 输出换行
    llvm::Value* newline = builder.CreateGlobalString("\n", "newline");
    llvm::Value* newline_len = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1);
    builder.CreateCall(write_func, {stderr_fd, newline, newline_len});
    
    // 调用 exit(1)
    builder.CreateCall(exit_func, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
    builder.CreateUnreachable();
    
    builtins_["unimplemented"] = unimplemented_func;
}

void Builtins::declareDebug() {
    // debug()是编译器intrinsic，不创建实际函数
    // 只是标记它存在，实际在codegen_expr.cpp中内联展开
    
    // 创建一个dummy函数声明（不会被实际调用）
    llvm::FunctionType* debug_type = llvm::FunctionType::get(
        llvm::PointerType::get(context_, 0),  // 返回泛型指针
        {llvm::PointerType::get(context_, 0)}, // 接受泛型指针
        false
    );
    
    llvm::Function* debug_func = llvm::Function::Create(
        debug_type,
        llvm::Function::ExternalLinkage,
        "debug_intrinsic",
        &module_
    );
    
    builtins_["debug"] = debug_func;
}

void Builtins::declareAbs() {
    // abs()是编译器intrinsic
    llvm::FunctionType* abs_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function* abs_func = llvm::Function::Create(
        abs_type,
        llvm::Function::ExternalLinkage,
        "abs_intrinsic",
        &module_
    );
    
    builtins_["abs"] = abs_func;
}

void Builtins::declareMin() {
    // min()是编译器intrinsic
    llvm::FunctionType* min_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::Type::getInt64Ty(context_), llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function* min_func = llvm::Function::Create(
        min_type,
        llvm::Function::ExternalLinkage,
        "min_intrinsic",
        &module_
    );
    
    builtins_["min"] = min_func;
}

void Builtins::declareMax() {
    // max()是编译器intrinsic
    llvm::FunctionType* max_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::Type::getInt64Ty(context_), llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function* max_func = llvm::Function::Create(
        max_type,
        llvm::Function::ExternalLinkage,
        "max_intrinsic",
        &module_
    );
    
    builtins_["max"] = max_func;
}

void Builtins::declareClamp() {
    // clamp()是编译器intrinsic
    llvm::FunctionType* clamp_type = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(context_),
        {llvm::Type::getInt64Ty(context_), llvm::Type::getInt64Ty(context_), llvm::Type::getInt64Ty(context_)},
        false
    );
    
    llvm::Function* clamp_func = llvm::Function::Create(
        clamp_type,
        llvm::Function::ExternalLinkage,
        "clamp_intrinsic",
        &module_
    );
    
    builtins_["clamp"] = clamp_func;
}

void Builtins::declarePow() {
    // pow()是编译器intrinsic
    llvm::FunctionType* pow_type = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context_),
        {llvm::Type::getDoubleTy(context_), llvm::Type::getDoubleTy(context_)},
        false
    );
    
    llvm::Function* pow_func = llvm::Function::Create(
        pow_type,
        llvm::Function::ExternalLinkage,
        "pow_intrinsic",
        &module_
    );
    
    builtins_["pow"] = pow_func;
}

void Builtins::declareSqrt() {
    // sqrt()是编译器intrinsic
    llvm::FunctionType* sqrt_type = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context_),
        {llvm::Type::getDoubleTy(context_)},
        false
    );
    
    llvm::Function* sqrt_func = llvm::Function::Create(
        sqrt_type,
        llvm::Function::ExternalLinkage,
        "sqrt_intrinsic",
        &module_
    );
    
    builtins_["sqrt"] = sqrt_func;
}

void Builtins::declareFloor() {
    // floor()是编译器intrinsic
    llvm::FunctionType* floor_type = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context_),
        {llvm::Type::getDoubleTy(context_)},
        false
    );
    
    llvm::Function* floor_func = llvm::Function::Create(
        floor_type,
        llvm::Function::ExternalLinkage,
        "floor_intrinsic",
        &module_
    );
    
    builtins_["floor"] = floor_func;
}

void Builtins::declareCeil() {
    // ceil()是编译器intrinsic
    llvm::FunctionType* ceil_type = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context_),
        {llvm::Type::getDoubleTy(context_)},
        false
    );
    
    llvm::Function* ceil_func = llvm::Function::Create(
        ceil_type,
        llvm::Function::ExternalLinkage,
        "ceil_intrinsic",
        &module_
    );
    
    builtins_["ceil"] = ceil_func;
}

void Builtins::declareRound() {
    // round()是编译器intrinsic
    llvm::FunctionType* round_type = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context_),
        {llvm::Type::getDoubleTy(context_)},
        false
    );
    
    llvm::Function* round_func = llvm::Function::Create(
        round_type,
        llvm::Function::ExternalLinkage,
        "round_intrinsic",
        &module_
    );
    
    builtins_["round"] = round_func;
}

// ========== Intrinsic统一接口 ==========

llvm::Value* Builtins::generateIntrinsic(
    const std::string& name,
    llvm::IRBuilder<>& builder,
    const std::vector<llvm::Value*>& args,
    const std::vector<std::string>& arg_names,
    const std::map<std::string, llvm::Type*>& variable_types,
    const std::map<std::string, llvm::Value*>& named_values
) {
    // 统一分发到具体的intrinsic实现
    if (name == "len") {
        if (args.size() != 1) {
            std::cerr << "len() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        std::string arg_name = arg_names.empty() ? "" : arg_names[0];
        return generateLen(builder, args[0], arg_name, variable_types, named_values);
    }
    
    if (name == "is_empty") {
        if (args.size() != 1) {
            std::cerr << "is_empty() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        std::string arg_name = arg_names.empty() ? "" : arg_names[0];
        return generateIsEmpty(builder, args[0], arg_name, variable_types, named_values);
    }
    
    if (name == "debug") {
        if (args.size() != 1) {
            std::cerr << "debug() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        std::string arg_name = arg_names.empty() ? "" : arg_names[0];
        return generateDebug(builder, args[0], arg_name, variable_types, named_values);
    }
    
    if (name == "abs") {
        if (args.size() != 1) {
            std::cerr << "abs() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        return generateAbs(builder, args[0]);
    }
    
    if (name == "min") {
        if (args.size() != 2) {
            std::cerr << "min() requires exactly 2 arguments" << std::endl;
            return nullptr;
        }
        return generateMin(builder, args[0], args[1]);
    }
    
    if (name == "max") {
        if (args.size() != 2) {
            std::cerr << "max() requires exactly 2 arguments" << std::endl;
            return nullptr;
        }
        return generateMax(builder, args[0], args[1]);
    }
    
    if (name == "clamp") {
        if (args.size() != 3) {
            std::cerr << "clamp() requires exactly 3 arguments" << std::endl;
            return nullptr;
        }
        return generateClamp(builder, args[0], args[1], args[2]);
    }
    
    if (name == "pow") {
        if (args.size() != 2) {
            std::cerr << "pow() requires exactly 2 arguments" << std::endl;
            return nullptr;
        }
        return generatePow(builder, args[0], args[1]);
    }
    
    if (name == "sqrt") {
        if (args.size() != 1) {
            std::cerr << "sqrt() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        return generateSqrt(builder, args[0]);
    }
    
    if (name == "floor") {
        if (args.size() != 1) {
            std::cerr << "floor() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        return generateFloor(builder, args[0]);
    }
    
    if (name == "ceil") {
        if (args.size() != 1) {
            std::cerr << "ceil() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        return generateCeil(builder, args[0]);
    }
    
    if (name == "round") {
        if (args.size() != 1) {
            std::cerr << "round() requires exactly 1 argument" << std::endl;
            return nullptr;
        }
        return generateRound(builder, args[0]);
    }
    
    std::cerr << "Unknown intrinsic: " << name << std::endl;
    return nullptr;
}

// ========== 辅助函数 ==========

bool Builtins::isSliceType(llvm::Type* type) const {
    if (!type || !type->isStructTy()) return false;
    llvm::StructType* st = llvm::cast<llvm::StructType>(type);
    return st->getNumElements() == 2 && 
           st->getElementType(0)->isPointerTy() &&
           st->getElementType(1)->isIntegerTy(64);
}

// ========== Intrinsic实现 ==========

llvm::Value* Builtins::generateLen(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg,
    const std::string& arg_name,
    const std::map<std::string, llvm::Type*>& variable_types,
    const std::map<std::string, llvm::Value*>& named_values
) {
    // 策略1：如果有变量名，优先使用类型信息（支持编译期优化）
    if (!arg_name.empty()) {
        auto type_it = variable_types.find(arg_name);
        if (type_it != variable_types.end()) {
            llvm::Type* var_type = type_it->second;
            
            // 固定数组 → 编译期常量
            if (var_type->isArrayTy()) {
                llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(var_type);
                return llvm::ConstantInt::get(context_, llvm::APInt(64, arr_type->getNumElements()));
            }
            
            // 切片 → 提取len字段
            if (isSliceType(var_type)) {
                auto val_it = named_values.find(arg_name);
                if (val_it != named_values.end()) {
                    llvm::Value* slice_val = builder.CreateLoad(var_type, val_it->second, "slice_load");
                    return builder.CreateExtractValue(slice_val, 1, "slice_len");
                }
            }
            
            // 字符串 → 调用strlen
            if (var_type->isPointerTy()) {
                auto val_it = named_values.find(arg_name);
                if (val_it != named_values.end()) {
                    if (llvm::Function* strlen_func = module_.getFunction("strlen")) {
                        llvm::Value* str_ptr = builder.CreateLoad(
                            llvm::PointerType::get(context_, 0),
                            val_it->second,
                            "str_load"
                        );
                        return builder.CreateCall(strlen_func, {str_ptr}, "str_len");
                    }
                }
            }
        }
    }
    
    // 策略2：通过arg的类型判断（用于表达式结果）
    if (!arg) return nullptr;
    
    llvm::Type* arg_type = arg->getType();
    
    // 切片值 → 提取len字段
    if (isSliceType(arg_type)) {
        return builder.CreateExtractValue(arg, 1, "slice_len");
    }
    
    // 字符串值 → 调用strlen
    if (arg_type->isPointerTy()) {
        if (llvm::Function* strlen_func = module_.getFunction("strlen")) {
            return builder.CreateCall(strlen_func, {arg}, "str_len");
        }
    }
    
    std::cerr << "len() requires an array, slice, or string argument" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generateIsEmpty(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg,
    const std::string& arg_name,
    const std::map<std::string, llvm::Type*>& variable_types,
    const std::map<std::string, llvm::Value*>& named_values
) {
    llvm::Value* zero_i64 = llvm::ConstantInt::get(context_, llvm::APInt(64, 0));
    
    // 策略1：如果有变量名，优先使用类型信息（支持编译期优化）
    if (!arg_name.empty()) {
        auto type_it = variable_types.find(arg_name);
        if (type_it != variable_types.end()) {
            llvm::Type* var_type = type_it->second;
            
            // 固定数组 → 编译期常量
            if (var_type->isArrayTy()) {
                llvm::ArrayType* arr_type = llvm::cast<llvm::ArrayType>(var_type);
                bool is_empty = (arr_type->getNumElements() == 0);
                return llvm::ConstantInt::get(context_, llvm::APInt(1, is_empty ? 1 : 0));
            }
            
            // 切片 → 提取len并比较
            if (isSliceType(var_type)) {
                auto val_it = named_values.find(arg_name);
                if (val_it != named_values.end()) {
                    llvm::Value* slice_val = builder.CreateLoad(var_type, val_it->second, "slice_load");
                    llvm::Value* len = builder.CreateExtractValue(slice_val, 1, "slice_len");
                    return builder.CreateICmpEQ(len, zero_i64, "is_empty");
                }
            }
            
            // 字符串 → strlen并比较
            if (var_type->isPointerTy()) {
                auto val_it = named_values.find(arg_name);
                if (val_it != named_values.end()) {
                    if (llvm::Function* strlen_func = module_.getFunction("strlen")) {
                        llvm::Value* str_ptr = builder.CreateLoad(
                            llvm::PointerType::get(context_, 0),
                            val_it->second,
                            "str_load"
                        );
                        llvm::Value* len = builder.CreateCall(strlen_func, {str_ptr}, "str_len");
                        return builder.CreateICmpEQ(len, zero_i64, "is_empty");
                    }
                }
            }
        }
    }
    
    // 策略2：通过arg的类型判断（用于表达式结果）
    if (!arg) return nullptr;
    
    llvm::Type* arg_type = arg->getType();
    
    // 切片值 → 提取len并比较
    if (isSliceType(arg_type)) {
        llvm::Value* len = builder.CreateExtractValue(arg, 1, "slice_len");
        return builder.CreateICmpEQ(len, zero_i64, "is_empty");
    }
    
    // 字符串值 → strlen并比较
    if (arg_type->isPointerTy()) {
        if (llvm::Function* strlen_func = module_.getFunction("strlen")) {
            llvm::Value* len = builder.CreateCall(strlen_func, {arg}, "str_len");
            return builder.CreateICmpEQ(len, zero_i64, "is_empty");
        }
    }
    
    std::cerr << "is_empty() requires an array, slice, or string argument" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generateDebug(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg,
    const std::string& arg_name,
    const std::map<std::string, llvm::Type*>& variable_types,
    const std::map<std::string, llvm::Value*>& named_values
) {
    // debug(value) - 输出调试信息并返回原值
    // 根据类型不同打印不同格式
    
    if (!arg) return nullptr;
    
    llvm::Type* arg_type = arg->getType();
    llvm::Function* printf_func = module_.getFunction("printf");
    
    if (!printf_func) {
        std::cerr << "printf not found for debug()" << std::endl;
        return arg;  // 如果没有printf，直接返回原值
    }
    
    // 创建调试前缀
    llvm::Value* debug_prefix = builder.CreateGlobalString("DEBUG: ", "debug_prefix");
    builder.CreateCall(printf_func, {debug_prefix});
    
    // 根据类型打印值
    if (arg_type->isIntegerTy()) {
        unsigned bit_width = arg_type->getIntegerBitWidth();
        
        if (bit_width == 1) {
            // bool类型
            llvm::Value* format = builder.CreateGlobalString("%s\n", "debug_bool_fmt");
            llvm::Value* true_str = builder.CreateGlobalString("true", "true_str");
            llvm::Value* false_str = builder.CreateGlobalString("false", "false_str");
            llvm::Value* str = builder.CreateSelect(arg, true_str, false_str, "bool_str");
            builder.CreateCall(printf_func, {format, str});
        } else if (bit_width == 8) {
            // i8/u8 可能是char
            llvm::Value* format = builder.CreateGlobalString("%c (i8=%d)\n", "debug_i8_fmt");
            llvm::Value* ext_val = builder.CreateSExt(arg, llvm::Type::getInt32Ty(context_), "ext");
            builder.CreateCall(printf_func, {format, ext_val, ext_val});
        } else if (bit_width <= 32) {
            // i16, i32
            llvm::Value* format = builder.CreateGlobalString("%d\n", "debug_int_fmt");
            llvm::Value* ext_val = builder.CreateSExt(arg, llvm::Type::getInt32Ty(context_), "ext");
            builder.CreateCall(printf_func, {format, ext_val});
        } else {
            // i64, i128
            llvm::Value* format = builder.CreateGlobalString("%lld\n", "debug_i64_fmt");
            llvm::Value* ext_val = builder.CreateSExtOrTrunc(arg, llvm::Type::getInt64Ty(context_), "ext");
            builder.CreateCall(printf_func, {format, ext_val});
        }
    } else if (arg_type->isFloatingPointTy()) {
        // f32, f64
        llvm::Value* format = builder.CreateGlobalString("%f\n", "debug_float_fmt");
        llvm::Value* ext_val = arg;
        if (arg_type->isFloatTy()) {
            ext_val = builder.CreateFPExt(arg, llvm::Type::getDoubleTy(context_), "fpext");
        }
        builder.CreateCall(printf_func, {format, ext_val});
    } else if (arg_type->isPointerTy()) {
        // string (pointer)
        llvm::Value* format = builder.CreateGlobalString("\"%s\"\n", "debug_str_fmt");
        builder.CreateCall(printf_func, {format, arg});
    } else if (isSliceType(arg_type)) {
        // slice - 显示长度
        llvm::Value* len = builder.CreateExtractValue(arg, 1, "slice_len");
        llvm::Value* format = builder.CreateGlobalString("[slice len=%lld]\n", "debug_slice_fmt");
        builder.CreateCall(printf_func, {format, len});
    } else {
        // 其他类型 - 显示类型信息
        llvm::Value* format = builder.CreateGlobalString("[value of unknown type]\n", "debug_unknown_fmt");
        builder.CreateCall(printf_func, {format});
    }
    
    // 返回原值（debug是透明的）
    return arg;
}

llvm::Value* Builtins::generateAbs(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg
) {
    // abs(x) - 绝对值
    if (!arg) return nullptr;
    
    llvm::Type* arg_type = arg->getType();
    
    // 整数类型
    if (arg_type->isIntegerTy()) {
        // abs(x) = x < 0 ? -x : x
        llvm::Value* zero = llvm::ConstantInt::get(arg_type, 0);
        llvm::Value* is_negative = builder.CreateICmpSLT(arg, zero, "is_neg");
        llvm::Value* neg_val = builder.CreateNeg(arg, "neg");
        return builder.CreateSelect(is_negative, neg_val, arg, "abs");
    }
    
    // 浮点类型
    if (arg_type->isFloatingPointTy()) {
        // 使用LLVM的fabs intrinsic
        llvm::Type* types[] = {arg_type};
        llvm::Function* fabs_func = llvm::Intrinsic::getDeclaration(
            &module_,
            llvm::Intrinsic::fabs,
            types
        );
        return builder.CreateCall(fabs_func, {arg}, "fabs");
    }
    
    std::cerr << "abs() requires a numeric argument" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generateMin(
    llvm::IRBuilder<>& builder,
    llvm::Value* a,
    llvm::Value* b
) {
    // min(a, b) - 返回较小值
    if (!a || !b) return nullptr;
    
    llvm::Type* a_type = a->getType();
    llvm::Type* b_type = b->getType();
    
    // 类型必须相同
    if (a_type != b_type) {
        std::cerr << "min() requires both arguments to have the same type" << std::endl;
        return nullptr;
    }
    
    // 整数类型
    if (a_type->isIntegerTy()) {
        // min(a, b) = a < b ? a : b
        llvm::Value* cmp = builder.CreateICmpSLT(a, b, "cmp");
        return builder.CreateSelect(cmp, a, b, "min");
    }
    
    // 浮点类型
    if (a_type->isFloatingPointTy()) {
        // min(a, b) = a < b ? a : b
        llvm::Value* cmp = builder.CreateFCmpOLT(a, b, "fcmp");
        return builder.CreateSelect(cmp, a, b, "fmin");
    }
    
    std::cerr << "min() requires numeric arguments" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generateMax(
    llvm::IRBuilder<>& builder,
    llvm::Value* a,
    llvm::Value* b
) {
    // max(a, b) - 返回较大值
    if (!a || !b) return nullptr;
    
    llvm::Type* a_type = a->getType();
    llvm::Type* b_type = b->getType();
    
    // 类型必须相同
    if (a_type != b_type) {
        std::cerr << "max() requires both arguments to have the same type" << std::endl;
        return nullptr;
    }
    
    // 整数类型
    if (a_type->isIntegerTy()) {
        // max(a, b) = a > b ? a : b
        llvm::Value* cmp = builder.CreateICmpSGT(a, b, "cmp");
        return builder.CreateSelect(cmp, a, b, "max");
    }
    
    // 浮点类型
    if (a_type->isFloatingPointTy()) {
        // max(a, b) = a > b ? a : b
        llvm::Value* cmp = builder.CreateFCmpOGT(a, b, "fcmp");
        return builder.CreateSelect(cmp, a, b, "fmax");
    }
    
    std::cerr << "max() requires numeric arguments" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generateClamp(
    llvm::IRBuilder<>& builder,
    llvm::Value* val,
    llvm::Value* min_val,
    llvm::Value* max_val
) {
    // clamp(val, min, max) - 限制值在[min, max]范围内
    if (!val || !min_val || !max_val) return nullptr;
    
    llvm::Type* val_type = val->getType();
    
    // 类型必须相同
    if (val_type != min_val->getType() || val_type != max_val->getType()) {
        std::cerr << "clamp() requires all arguments to have the same type" << std::endl;
        return nullptr;
    }
    
    // clamp(val, min, max) = max(min, min(val, max))
    // 即：先限制上界，再限制下界
    
    // 整数类型
    if (val_type->isIntegerTy()) {
        // temp = val > max ? max : val
        llvm::Value* cmp_max = builder.CreateICmpSGT(val, max_val, "cmp_max");
        llvm::Value* temp = builder.CreateSelect(cmp_max, max_val, val, "clamp_upper");
        
        // result = temp < min ? min : temp
        llvm::Value* cmp_min = builder.CreateICmpSLT(temp, min_val, "cmp_min");
        return builder.CreateSelect(cmp_min, min_val, temp, "clamp");
    }
    
    // 浮点类型
    if (val_type->isFloatingPointTy()) {
        // temp = val > max ? max : val
        llvm::Value* cmp_max = builder.CreateFCmpOGT(val, max_val, "fcmp_max");
        llvm::Value* temp = builder.CreateSelect(cmp_max, max_val, val, "fclamp_upper");
        
        // result = temp < min ? min : temp
        llvm::Value* cmp_min = builder.CreateFCmpOLT(temp, min_val, "fcmp_min");
        return builder.CreateSelect(cmp_min, min_val, temp, "fclamp");
    }
    
    std::cerr << "clamp() requires numeric arguments" << std::endl;
    return nullptr;
}

llvm::Value* Builtins::generatePow(
    llvm::IRBuilder<>& builder,
    llvm::Value* base,
    llvm::Value* exp
) {
    // pow(base, exp) - 幂运算
    if (!base || !exp) return nullptr;
    
    // 支持浮点类型
    if (!base->getType()->isFloatingPointTy() || !exp->getType()->isFloatingPointTy()) {
        std::cerr << "pow() requires floating-point arguments" << std::endl;
        return nullptr;
    }
    
    // 类型统一为f64
    if (base->getType()->isFloatTy()) {
        base = builder.CreateFPExt(base, llvm::Type::getDoubleTy(context_), "base_ext");
    }
    if (exp->getType()->isFloatTy()) {
        exp = builder.CreateFPExt(exp, llvm::Type::getDoubleTy(context_), "exp_ext");
    }
    
    // 使用LLVM的pow intrinsic
    llvm::Type* types[] = {llvm::Type::getDoubleTy(context_)};
    llvm::Function* pow_func = llvm::Intrinsic::getDeclaration(
        &module_,
        llvm::Intrinsic::pow,
        types
    );
    
    return builder.CreateCall(pow_func, {base, exp}, "pow");
}

llvm::Value* Builtins::generateSqrt(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg
) {
    // sqrt(x) - 平方根
    if (!arg) return nullptr;
    
    if (!arg->getType()->isFloatingPointTy()) {
        std::cerr << "sqrt() requires a floating-point argument" << std::endl;
        return nullptr;
    }
    
    // 统一为f64
    llvm::Value* val = arg;
    if (arg->getType()->isFloatTy()) {
        val = builder.CreateFPExt(arg, llvm::Type::getDoubleTy(context_), "val_ext");
    }
    
    // 使用LLVM的sqrt intrinsic
    llvm::Type* types[] = {llvm::Type::getDoubleTy(context_)};
    llvm::Function* sqrt_func = llvm::Intrinsic::getDeclaration(
        &module_,
        llvm::Intrinsic::sqrt,
        types
    );
    
    return builder.CreateCall(sqrt_func, {val}, "sqrt");
}

llvm::Value* Builtins::generateFloor(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg
) {
    // floor(x) - 向下取整
    if (!arg) return nullptr;
    
    if (!arg->getType()->isFloatingPointTy()) {
        std::cerr << "floor() requires a floating-point argument" << std::endl;
        return nullptr;
    }
    
    // 统一为f64
    llvm::Value* val = arg;
    if (arg->getType()->isFloatTy()) {
        val = builder.CreateFPExt(arg, llvm::Type::getDoubleTy(context_), "val_ext");
    }
    
    // 使用LLVM的floor intrinsic
    llvm::Type* types[] = {llvm::Type::getDoubleTy(context_)};
    llvm::Function* floor_func = llvm::Intrinsic::getDeclaration(
        &module_,
        llvm::Intrinsic::floor,
        types
    );
    
    return builder.CreateCall(floor_func, {val}, "floor");
}

llvm::Value* Builtins::generateCeil(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg
) {
    // ceil(x) - 向上取整
    if (!arg) return nullptr;
    
    if (!arg->getType()->isFloatingPointTy()) {
        std::cerr << "ceil() requires a floating-point argument" << std::endl;
        return nullptr;
    }
    
    // 统一为f64
    llvm::Value* val = arg;
    if (arg->getType()->isFloatTy()) {
        val = builder.CreateFPExt(arg, llvm::Type::getDoubleTy(context_), "val_ext");
    }
    
    // 使用LLVM的ceil intrinsic
    llvm::Type* types[] = {llvm::Type::getDoubleTy(context_)};
    llvm::Function* ceil_func = llvm::Intrinsic::getDeclaration(
        &module_,
        llvm::Intrinsic::ceil,
        types
    );
    
    return builder.CreateCall(ceil_func, {val}, "ceil");
}

llvm::Value* Builtins::generateRound(
    llvm::IRBuilder<>& builder,
    llvm::Value* arg
) {
    // round(x) - 四舍五入
    if (!arg) return nullptr;
    
    if (!arg->getType()->isFloatingPointTy()) {
        std::cerr << "round() requires a floating-point argument" << std::endl;
        return nullptr;
    }
    
    // 统一为f64
    llvm::Value* val = arg;
    if (arg->getType()->isFloatTy()) {
        val = builder.CreateFPExt(arg, llvm::Type::getDoubleTy(context_), "val_ext");
    }
    
    // 使用LLVM的round intrinsic
    llvm::Type* types[] = {llvm::Type::getDoubleTy(context_)};
    llvm::Function* round_func = llvm::Intrinsic::getDeclaration(
        &module_,
        llvm::Intrinsic::round,
        types
    );
    
    return builder.CreateCall(round_func, {val}, "round");
}

} // namespace pawc

