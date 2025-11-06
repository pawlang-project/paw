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

/// ObjectGenPass - objectfilegeneratePass
///
/// Compile LLVM IR to target platform object fileile(.o)
class ObjectGenPass : public PassBase<ObjectGenPass> {
public:
    static std::string name() { return "ObjectGenPass"; }
    
    PassResult runImpl(PassContext* context) {
        // Get LLVM module from context
        llvm::Module* module = nullptr;
        if (!context->getCachedResult("llvm_module", module) || !module) {
            return PassResult{false, "No LLVM module available for object generation"};
        }
        
        // initializeAlltargets
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();
        
        // gettarget triple
        auto target_triple_str = llvm::sys::getDefaultTargetTriple();
        llvm::Triple target_triple(target_triple_str);
        module->setTargetTriple(target_triple);
        
        // lookuptarget
        std::string error;
        auto target = llvm::TargetRegistry::lookupTarget(target_triple_str, error);
        if (!target) {
            return PassResult{false, "Failed to lookup target: " + error};
        }
        
        // Create TargetMachine - 🔧 Bug Fix: use unique_ptr to manage lifetimeiod
        llvm::StringRef cpu = "generic";
        llvm::StringRef features = "";
        llvm::TargetOptions opt;
        auto cm = llvm::CodeModel::Small;
        
        std::unique_ptr<llvm::TargetMachine> target_machine(
            target->createTargetMachine(target_triple, cpu, features, opt, std::nullopt, cm)
        );
        
        if (!target_machine) {
            return PassResult{false, "Failed to create TargetMachine"};
        }
        
        module->setDataLayout(target_machine->createDataLayout());
        
        // Generate object file path based on output file
        std::string output_file = context->getOutputFile();
        // Replace extension with .o, or append .o if no extension
        size_t last_dot = output_file.find_last_of('.');
        size_t last_slash = output_file.find_last_of("/\\");
        if (last_dot != std::string::npos && (last_slash == std::string::npos || last_dot > last_slash)) {
            output_file = output_file.substr(0, last_dot) + ".o";
        } else {
            output_file = output_file + ".o";
        }
        
        std::error_code ec;
        llvm::raw_fd_ostream dest(output_file, ec, llvm::sys::fs::OF_None);
        
        if (ec) {
            return PassResult{false, "Failed to open output file: " + output_file + " - " + ec.message()};
        }
        
        // Configure pass to generate object file
        llvm::legacy::PassManager pass;
        auto file_type = llvm::CodeGenFileType::ObjectFile;
        
        if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
            return PassResult{false, "TargetMachine can't emit object file"};
        }
        
        // runPass
        pass.run(*module);
        dest.flush();
        
        // target_machinewillin/atfunctionendtime/whenself/fromdynamicrelease
        
        // Cache object file path for linker use
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

