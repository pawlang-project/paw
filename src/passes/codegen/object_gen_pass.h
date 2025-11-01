//===--- object_gen_pass.h - Object File Generation Pass --------*- C++ -*-===//

#ifndef PAW_OBJECT_GEN_PASS_H
#define PAW_OBJECT_GEN_PASS_H

#include "pass/pass.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>

namespace pawc {

/// ObjectGenPass - 对象文件生成Pass
///
/// 将LLVM IR编译为目标平台的对象文件(.o)
class ObjectGenPass : public PassBase<ObjectGenPass> {
public:
    static std::string name() { return "ObjectGenPass"; }
    
    PassResult runImpl(PassContext* context) {
        // 从context获取LLVM模块
        llvm::Module* module = nullptr;
        if (!context->getCachedResult("llvm_module", module) || !module) {
            return PassResult{false, "No LLVM module available for object generation"};
        }
        
        // 初始化所有targets
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();
        
        // 获取target triple
        auto target_triple_str = llvm::sys::getDefaultTargetTriple();
        llvm::Triple target_triple(target_triple_str);
        module->setTargetTriple(target_triple);
        
        // 查找target
        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
        if (!target) {
            return PassResult{false, "Failed to lookup target: " + error};
        }
        
        // 创建TargetMachine
        llvm::StringRef cpu = "generic";
        llvm::StringRef features = "";
        llvm::TargetOptions opt;
        auto cm = llvm::CodeModel::Small;
        
        auto* target_machine = target->createTargetMachine(
            target_triple, cpu, features, opt, std::nullopt, cm);
        
        if (!target_machine) {
            return PassResult{false, "Failed to create TargetMachine"};
        }
        
        module->setDataLayout(target_machine->createDataLayout());
        
        // 生成对象文件（使用临时文件）
        std::string output_file = "/tmp/paw_output.o";
        std::error_code ec;
        llvm::raw_fd_ostream dest(output_file, ec, llvm::sys::fs::OF_None);
        
        if (ec) {
            delete target_machine;
            return PassResult{false, "Failed to open output file: " + ec.message()};
        }
        
        // 设置Pass来生成对象文件
        llvm::legacy::PassManager pass;
        auto file_type = llvm::CodeGenFileType::ObjectFile;
        
        if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
            delete target_machine;
            return PassResult{false, "TargetMachine can't emit object file"};
        }
        
        // 运行Pass
        pass.run(*module);
        dest.flush();
        
        delete target_machine;
        
        // 缓存对象文件路径供链接器使用
        context->cacheAnalysisResult("object_file", output_file);
        
        if (context->isVerbose()) {
            std::string msg = "Object file generated: " + output_file + 
                             " (target: " + target_triple_str + ")";
            return PassResult{true, msg};
        }
        
        return PassResult{true, "Object file generated successfully"};
    }
};

} // namespace pawc

#endif // PAW_OBJECT_GEN_PASS_H

