/**
 * Simple test to verify LLVM C API works with our local installation
 * 
 * Compile: clang examples/test_llvm_simple.c -I llvm/install/include -L llvm/install/lib -lLLVMCore -lLLVMSupport -lLLVMTargetParser -lLLVMBinaryFormat -lLLVMRemarks -lLLVMBitstreamReader -lLLVMDemangle -lc++ -o test_llvm_simple
 */

#include <stdio.h>
#include <llvm-c/Core.h>

int main() {
    printf("🚀 Testing LLVM C API\n\n");

    // Create context
    printf("Creating LLVM context...\n");
    LLVMContextRef context = LLVMContextCreate();
    printf("✅ Context created\n\n");

    // Create module
    printf("Creating module...\n");
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test_module", context);
    printf("✅ Module created\n\n");

    // Create function type: i32 add(i32, i32)
    printf("Creating function type...\n");
    LLVMTypeRef i32_type = LLVMInt32TypeInContext(context);
    LLVMTypeRef param_types[] = { i32_type, i32_type };
    LLVMTypeRef func_type = LLVMFunctionType(i32_type, param_types, 2, 0);
    printf("✅ Function type created\n\n");

    // Add function
    printf("Adding function to module...\n");
    LLVMValueRef func = LLVMAddFunction(module, "add", func_type);
    printf("✅ Function added\n\n");

    // Create basic block
    printf("Creating basic block...\n");
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, func, "entry");
    printf("✅ Basic block created\n\n");

    // Create builder
    printf("Creating IR builder...\n");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    printf("✅ Builder positioned\n\n");

    // Build instructions
    printf("Building instructions...\n");
    LLVMValueRef a = LLVMGetParam(func, 0);
    LLVMValueRef b = LLVMGetParam(func, 1);
    LLVMValueRef result = LLVMBuildAdd(builder, a, b, "result");
    LLVMBuildRet(builder, result);
    printf("✅ Instructions built\n\n");

    // Print IR
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           Generated LLVM IR                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    char *ir = LLVMPrintModuleToString(module);
    printf("%s\n", ir);
    LLVMDisposeMessage(ir);

    // Cleanup
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);

    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           ✅ Success! LLVM C API is working!                ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    return 0;
}

