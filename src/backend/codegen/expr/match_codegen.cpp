//===--- match_codegen.cpp - Match Expression CodeGen -----------*- C++ -*-===//
/// @file match_codegen.cpp
/// @brief Implementation file
///
//
// Match expression code generation: pattern matching and branch generation
//
//===----------------------------------------------------------------------===//

#include "expr_codegen.h"
#include "../type/type_codegen.h"
#include "frontend/parser/ast/expr.h"
#include "frontend/parser/ast/pattern.h"
#include "middleend/types/composite_types.h"
#include "middleend/types/generic_types.h"
#include "middleend/types/type_system.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>

namespace pawc {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Match expression main implementation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::visit(MatchExpr* node) {
    // Match expression code generation - complete implementation
    
    auto& context = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    // 1. Evaluate the scrutinee value
    node->getScrutinee()->accept(this);
    llvm::Value* scrutinee_value = results_;
    
    if (!scrutinee_value) {
        results_ = nullptr;
        return;
    }
    
    // 2. Get the match expression's results type
    Type* results_type = node->getType();
    if (!results_type) {
        results_ = nullptr;
        return;
    }
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_results_type = context_->getLLVMType(results_type);
    
    // 3. Create results variable (to store each branch's results)
    llvm::AllocaInst* results_alloca = builder.CreateAlloca(
        llvm_results_type, nullptr, "match.results");
    
    // 4. Create basic blocks
    llvm::Function* current_fn = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(context, "match.end", current_fn);
    
    // 5. Generate code for each branch
    const auto& arms = node->getArms();
    
    for (size_t i = 0; i < arms.size(); ++i) {
        const auto& arm = arms[i];
        
        // Create basic blocks for the branch
        llvm::BasicBlock* arm_bb = llvm::BasicBlock::Create(
            context, "match.arm." + std::to_string(i), current_fn);
        llvm::BasicBlock* next_bb = (i < arms.size() - 1) 
            ? llvm::BasicBlock::Create(context, "match.next." + std::to_string(i), current_fn)
            : end_bb;
        
        // Generate pattern matching condition
        llvm::Value* match_cond = generatePatternMatch(
            arm.pattern.get(), 
            scrutinee_value, 
            node->getScrutinee()->getType()
        );
        
        // Generate code for guard condition (if present)
        //
        // Guard conditions add an extra filtering step after pattern matching.
        // Control flow with guard:
        //   1. Check pattern match -> if failed, goto next_bb
        //   2. If matched, goto guard_bb
        //   3. In guard_bb: bind variables, evaluate guard
        //   4. If guard true, goto arm_bb (execute body)
        //   5. If guard false, goto next_bb (try next arm)
        //
        // Without guard:
        //   1. Check pattern match
        //   2. If matched, goto arm_bb directly
        //   3. If not matched, goto next_bb
        //
        // Note: Variables are bound in the guard block, not the arm block,
        // because the guard needs access to them. This ensures proper scoping.
        if (arm.guard) {
            // Create a separate basic block for guard evaluation
            // This block sits between pattern matching and the arm body
            llvm::BasicBlock* guard_bb = llvm::BasicBlock::Create(
                context, "match.guard." + std::to_string(i), current_fn);
            
            if (!match_cond) {
                // Pattern always matches (wildcard or variable binding)
                // Jump directly to guard check
                builder.CreateBr(guard_bb);
            } else {
                // Pattern matching is conditional
                // Only check guard if pattern matched
                builder.CreateCondBr(match_cond, guard_bb, next_bb);
            }
            
            // Generate code in the guard block
            builder.SetInsertPoint(guard_bb);
            
            // Bind variables from the pattern first
            // The guard expression can now reference these variables
            bindPatternVariables(arm.pattern.get(), scrutinee_value, node->getScrutinee()->getType());
            
            // Evaluate the guard condition
            arm.guard->accept(this);
            llvm::Value* guard_cond = results_;
            
            if (guard_cond) {
                // Guard evaluated successfully
                // If true, execute arm body; if false, try next arm
                builder.CreateCondBr(guard_cond, arm_bb, next_bb);
            } else {
                // Guard evaluation failed (error)
                // Skip to next arm
                builder.CreateBr(next_bb);
            }
        } else {
            // No guard condition - standard pattern matching flow
            if (!match_cond) {
                // Pattern always matches (wildcard or variable binding)
                // Jump directly to arm body
                builder.CreateBr(arm_bb);
            } else {
                // Conditional pattern match
                // Jump to arm body if matched, next arm if not
                builder.CreateCondBr(match_cond, arm_bb, next_bb);
            }
        }
        
        // Generate code for the arm body
        builder.SetInsertPoint(arm_bb);
        
        // Bind pattern variables (if not already bound in guard block)
        //
        // Variable binding location:
        //   - With guard: variables bound in guard_bb (before guard evaluation)
        //   - Without guard: variables bound here (before body execution)
        //
        // This ensures variables are always in scope when needed:
        //   - Guard needs them for its condition
        //   - Body needs them for its expression
        if (!arm.guard) {
            bindPatternVariables(arm.pattern.get(), scrutinee_value, node->getScrutinee()->getType());
        }
        
        // Evaluate branch expression
        arm.expression->accept(this);
        llvm::Value* arm_value = results_;
        
        if (arm_value) {
            builder.CreateStore(arm_value, results_alloca);
        }
        
        builder.CreateBr(end_bb);
        
        // Move to next branch
        if (i < arms.size() - 1) {
            builder.SetInsertPoint(next_bb);
        }
    }
    
    // 6. Set to end block and load results
    builder.SetInsertPoint(end_bb);
    results_ = builder.CreateLoad(llvm_results_type, results_alloca, "match.value");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pattern matching condition generation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* ExprCodeGen::generatePatternMatch(Pattern* pattern, llvm::Value* scrutinee, Type* scrutinee_type) {
    auto& context = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    // LiteralPattern: literal matching
    if (auto* lit = dynamic_cast<LiteralPattern*>(pattern)) {
        switch (lit->getKind()) {
            case LiteralPattern::Kind::Int: {
                // Integer comparison
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    scrutinee->getType(),
                    std::stoll(lit->getValue())
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.int.eq");
            }
            case LiteralPattern::Kind::Bool: {
                // Boolean comparison
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    llvm::Type::getInt1Ty(context),
                    lit->getValue() == "true" ? 1 : 0
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.bool.eq");
            }
            case LiteralPattern::Kind::String: {
                // String comparison (call runtime strcmp)
                llvm::Function* strcmp_fn = context_->getRuntimeFunction("paw_strcmp");
                if (!strcmp_fn) {
                    return nullptr;
                }
                
                // Create string literal
                llvm::Value* pattern_str = builder.CreateGlobalString(lit->getValue(), "match.str.literal");
                
                // Call strcmp
                llvm::Value* cmp_results = builder.CreateCall(
                    strcmp_fn,
                    {scrutinee, pattern_str},
                    "strcmp.results"
                );
                
                // strcmp returns 0 for equality
                llvm::Value* zero = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(context),
                    0
                );
                return builder.CreateICmpEQ(cmp_results, zero, "match.str.eq");
            }
            case LiteralPattern::Kind::Char: {
                // Character comparison
                llvm::Value* pattern_val = llvm::ConstantInt::get(
                    llvm::Type::getInt8Ty(context),
                    lit->getValue()[0]
                );
                return builder.CreateICmpEQ(scrutinee, pattern_val, "match.char.eq");
            }
            case LiteralPattern::Kind::Float: {
                // Floating-point number comparison
                llvm::APFloat ap_float(std::stod(lit->getValue()));
                llvm::Value* pattern_val = llvm::ConstantFP::get(context, ap_float);
                return builder.CreateFCmpOEQ(scrutinee, pattern_val, "match.float.eq");
            }
            default:
                return nullptr;
        }
    }
    
    // WildcardPattern: always matches
    if (dynamic_cast<WildcardPattern*>(pattern)) {
        return nullptr;  // nullptr means always matches
    }
    
    // VariablePattern: always matches (binds variable)
    if (dynamic_cast<VariablePattern*>(pattern)) {
        return nullptr;  // nullptr means always matches
    }
    
    // TuplePattern: tuple matching
    if (auto* tuple = dynamic_cast<TuplePattern*>(pattern)) {
        if (!scrutinee_type || scrutinee_type->getKind() != Type::Kind::Tuple) {
            return nullptr;
        }
        
        auto* tuple_type = static_cast<TupleType*>(scrutinee_type);
        const auto& element_types = tuple_type->getElementTypes();
        const auto& patterns = tuple->getElements();
        
        if (patterns.size() != element_types.size()) {
            return nullptr;  // Tuple size mismatch
        }
        
        // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
        llvm::Type* llvm_tuple_type = context_->getLLVMType(tuple_type);
        
        // Ensure scrutinee is pointer type
        llvm::Value* tuple_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "tuple.tmp");
            builder.CreateStore(scrutinee, temp);
            tuple_ptr = temp;
        }
        
        // Generate conditions for all sub-patterns and combine with AND
        llvm::Value* results_cond = nullptr;
        
        for (size_t i = 0; i < patterns.size(); ++i) {
            // Extract tuple element
            llvm::Value* elem_ptr = builder.CreateStructGEP(
                llvm_tuple_type,
                tuple_ptr,
                i,
                "tuple.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = context_->getLLVMType(element_types[i]);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "tuple.elem.val"
            );
            
            // Recursively generate sub-pattern matching condition
            llvm::Value* sub_cond = generatePatternMatch(
                patterns[i].get(),
                elem_value,
                element_types[i]
            );
            
            if (sub_cond) {
                if (!results_cond) {
                    results_cond = sub_cond;
                } else {
                    results_cond = builder.CreateAnd(results_cond, sub_cond, "tuple.match.and");
                }
            }
        }
        
        return results_cond;  // nullptr means all sub-patterns are wildcard/variable
    }
    
    // ArrayPattern: array matching
    if (auto* array = dynamic_cast<ArrayPattern*>(pattern)) {
        if (!scrutinee_type || scrutinee_type->getKind() != Type::Kind::Array) {
            return nullptr;
        }
        
        auto* array_type = static_cast<ArrayType*>(scrutinee_type);
        Type* elem_type = array_type->getElementType();
        const auto& patterns = array->getElements();
        
        if (patterns.size() != array_type->getSize()) {
            return nullptr;  // Array size mismatch
        }
        
        llvm::Type* llvm_array_type = context_->getLLVMType(array_type);
        
        // Ensure scrutinee is pointer type
        llvm::Value* array_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "array.tmp");
            builder.CreateStore(scrutinee, temp);
            array_ptr = temp;
        }
        
        // Generate conditions for all sub-patterns and combine with AND
        llvm::Value* results_cond = nullptr;
        
        for (size_t i = 0; i < patterns.size(); ++i) {
            // Extract array element: GEP [array_type, array_ptr, 0, i]
            llvm::Value* indices[] = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
            };
            
            llvm::Value* elem_ptr = builder.CreateGEP(
                llvm_array_type,
                array_ptr,
                indices,
                "array.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "array.elem.val"
            );
            
            // Recursively generate sub-pattern matching condition
            llvm::Value* sub_cond = generatePatternMatch(
                patterns[i].get(),
                elem_value,
                elem_type
            );
            
            if (sub_cond) {
                if (!results_cond) {
                    results_cond = sub_cond;
                } else {
                    results_cond = builder.CreateAnd(results_cond, sub_cond, "array.match.and");
                }
            }
        }
        
        return results_cond;  // nullptr means all sub-patterns are wildcard/variable
    }
    
    // SlicePattern: slice matching (supports Array and Slice, including suffix)
    if (auto* slice_pat = dynamic_cast<SlicePattern*>(pattern)) {
        // Slice patternmaymatch Array or Slice types
        if (!scrutinee_type) {
            return nullptr;
        }
        
        Type* elem_type = nullptr;
        size_t array_size = 0;
        
        if (scrutinee_type->getKind() == Type::Kind::Array) {
            auto* array_type = static_cast<ArrayType*>(scrutinee_type);
            elem_type = array_type->getElementType();
            array_size = array_type->getSize();
            
            // Checkfront/beforefix+back/afterfixlength
            size_t min_size = slice_pat->getPrefix().size() + slice_pat->getSuffix().size();
            if (min_size > array_size) {
                return nullptr;
            }
        } else if (scrutinee_type->getKind() == Type::Kind::Slice) {
            auto* slice_type = static_cast<SliceType*>(scrutinee_type);
            elem_type = slice_type->getElementType();
        } else {
            return nullptr;  // Type mismatch
        }
        
        llvm::Type* llvm_scrutinee_type = context_->getLLVMType(scrutinee_type);
        
        llvm::Value* scrutinee_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "slice.tmp");
            builder.CreateStore(scrutinee, temp);
            scrutinee_ptr = temp;
        }
        
        // Generate matching conditions for prefix elements
        llvm::Value* results_cond = nullptr;
        
        // Match prefix
        for (size_t i = 0; i < slice_pat->getPrefix().size(); ++i) {
            llvm::Value* elem_value = nullptr;
            
            if (scrutinee_type->getKind() == Type::Kind::Array) {
                llvm::Value* indices[] = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
                };
                llvm::Value* elem_ptr = builder.CreateGEP(
                    llvm_scrutinee_type, scrutinee_ptr, indices,
                    "slice.prefix." + std::to_string(i)
                );
                llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
                elem_value = builder.CreateLoad(elem_llvm_type, elem_ptr, "slice.prefix.val");
            }
            
            if (elem_value) {
                llvm::Value* sub_cond = generatePatternMatch(
                    slice_pat->getPrefix()[i].get(),
                    elem_value,
                    elem_type
                );
                
                if (sub_cond) {
                    if (!results_cond) {
                        results_cond = sub_cond;
                    } else {
                        results_cond = builder.CreateAnd(results_cond, sub_cond, "slice.match.and");
                    }
                }
            }
        }
        
        // Match suffix ([a, .., z] means z)
        for (size_t i = 0; i < slice_pat->getSuffix().size(); ++i) {
            llvm::Value* elem_value = nullptr;
            
            if (scrutinee_type->getKind() == Type::Kind::Array) {
                // Suffix index counts from array end
                size_t index = array_size - slice_pat->getSuffix().size() + i;
                llvm::Value* indices[] = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), index)
                };
                llvm::Value* elem_ptr = builder.CreateGEP(
                    llvm_scrutinee_type, scrutinee_ptr, indices,
                    "slice.suffix." + std::to_string(i)
                );
                llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
                elem_value = builder.CreateLoad(elem_llvm_type, elem_ptr, "slice.suffix.val");
            }
            
            if (elem_value) {
                llvm::Value* sub_cond = generatePatternMatch(
                    slice_pat->getSuffix()[i].get(),
                    elem_value,
                    elem_type
                );
                
                if (sub_cond) {
                    if (!results_cond) {
                        results_cond = sub_cond;
                    } else {
                        results_cond = builder.CreateAnd(results_cond, sub_cond, "slice.match.and");
                    }
                }
            }
        }
        
        return results_cond;
    }
    
    // RangePattern: Generate range comparison code
    //
    // Generates efficient range checking with 2 comparisons:
    //   Exclusive (..):  (value >= start) && (value < end)
    //   Inclusive (..=): (value >= start) && (value <= end)
    //
    // Supported types:
    //   - Integers: i8-i128, u8-u128 (signed/unsigned comparison)
    //   - Characters: char (unsigned byte comparison)
    //
    // Code generation strategy:
    //   1. Extract literal values from start/end patterns
    //   2. Create LLVM constant integers for bounds
    //   3. Generate comparison instructions (ICmpSGE, ICmpSLE/SLT)
    //   4. Combine with AND operation
    //
    // Performance: O(1) constant time, 2 CPU comparisons
    if (auto* range_pat = dynamic_cast<RangePattern*>(pattern)) {
        // Extract start and end literal patterns
        auto* start_lit = dynamic_cast<LiteralPattern*>(range_pat->getStart());
        auto* end_lit = dynamic_cast<LiteralPattern*>(range_pat->getEnd());
        
        if (!start_lit || !end_lit) {
            return nullptr;  // Range bounds must be literals (enforced by parser/type checker)
        }
        
        // Generate LLVM constants for range bounds
        llvm::Value* start_val = nullptr;
        llvm::Value* end_val = nullptr;
        
        if (start_lit->getKind() == LiteralPattern::Kind::Int) {
            start_val = llvm::ConstantInt::get(
                scrutinee->getType(),
                std::stoll(start_lit->getValue())
            );
            end_val = llvm::ConstantInt::get(
                scrutinee->getType(),
                std::stoll(end_lit->getValue())
            );
            
            // value >= start
            llvm::Value* ge_cond = builder.CreateICmpSGE(scrutinee, start_val, "range.ge");
            
            // value <= end (inclusive) or value < end (exclusive)
            llvm::Value* le_cond;
            if (range_pat->isInclusive()) {
                le_cond = builder.CreateICmpSLE(scrutinee, end_val, "range.le");
            } else {
                le_cond = builder.CreateICmpSLT(scrutinee, end_val, "range.lt");
            }
            
            // Combine conditions
            return builder.CreateAnd(ge_cond, le_cond, "range.match");
            
        } else if (start_lit->getKind() == LiteralPattern::Kind::Char) {
            // Character range: 'a'..'z'
            start_val = llvm::ConstantInt::get(
                llvm::Type::getInt8Ty(context),
                static_cast<uint8_t>(start_lit->getValue()[0])
            );
            end_val = llvm::ConstantInt::get(
                llvm::Type::getInt8Ty(context),
                static_cast<uint8_t>(end_lit->getValue()[0])
            );
            
            llvm::Value* ge_cond = builder.CreateICmpUGE(scrutinee, start_val, "range.ge");
            llvm::Value* le_cond;
            if (range_pat->isInclusive()) {
                le_cond = builder.CreateICmpULE(scrutinee, end_val, "range.le");
            } else {
                le_cond = builder.CreateICmpULT(scrutinee, end_val, "range.lt");
            }
            
            return builder.CreateAnd(ge_cond, le_cond, "range.match");
        }
        
        return nullptr;
    }
    
    // OrPattern: Generate disjunctive pattern matching
    //
    // Generates code that matches if ANY alternative pattern matches:
    //   pattern1 | pattern2 | pattern3
    //   => cond1 || cond2 || cond3
    //
    // Code generation strategy:
    //   1. Recursively generate match conditions for each alternative
    //   2. Combine conditions with logical OR (short-circuit evaluation)
    //   3. Return combined condition
    //
    // Short-circuit behavior:
    //   - LLVM automatically optimizes OR with short-circuit evaluation
    //   - Evaluation stops at first true condition
    //   - Efficient for patterns like: 1 | 2 | 3 | ... | 1000
    //
    // Performance: O(k) where k is number of alternatives (worst case all checked)
    //
    // Note: Variables are bound by whichever alternative matches first.
    // Type checker ensures all alternatives bind compatible variables.
    if (auto* or_pat = dynamic_cast<OrPattern*>(pattern)) {
        // Generate match conditions for all alternatives
        llvm::Value* results_cond = nullptr;
        
        for (const auto& alt : or_pat->getAlternatives()) {
            // Recursively generate condition for this alternative
            llvm::Value* alt_cond = generatePatternMatch(
                alt.get(),
                scrutinee,
                scrutinee_type
            );
            
            if (alt_cond) {
                if (!results_cond) {
                    // First alternative
                    results_cond = alt_cond;
                } else {
                    // Combine with previous alternatives using OR
                    // LLVM will optimize this with short-circuit evaluation
                    results_cond = builder.CreateOr(results_cond, alt_cond, "or.pattern");
                }
            }
        }
        
        return results_cond;
    }
    
    // EnumPattern: enum matching
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        // Special handling for Result type
        if (scrutinee_type && scrutinee_type->getKind() == Type::Kind::Result) {
            return generateResultPatternMatch(enum_pat, scrutinee, static_cast<ResultType*>(scrutinee_type));
        }
        
        // Special handling for Optional type
        if (scrutinee_type && scrutinee_type->getKind() == Type::Kind::Optional) {
            return generateOptionalPatternMatch(enum_pat, scrutinee, static_cast<OptionalType*>(scrutinee_type));
        }
        
        // Enum matching: check if tag is equal
        if (!scrutinee_type || scrutinee_type->getKind() != Type::Kind::Enum) {
            return nullptr;
        }
        
        auto* enum_type = static_cast<EnumType*>(scrutinee_type);
        const auto& variants = enum_type->getVariants();
        
        // Look up variant index
        int variant_idx = -1;
        for (size_t i = 0; i < variants.size(); ++i) {
            if (variants[i].first == enum_pat->getVariantName()) {
                variant_idx = static_cast<int>(i);
                break;
            }
        }
        
        if (variant_idx < 0) {
            return nullptr;  // Variant does not exist
        }
        
        // Extract enum's tag field (0th field)
        // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
        llvm::Type* llvm_enum_type = context_->getLLVMType(enum_type);
        
        llvm::Value* enum_ptr = scrutinee;
        if (!scrutinee->getType()->isPointerTy()) {
            // If it's a value type, allocate on stack first
            llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "enum.tmp");
            builder.CreateStore(scrutinee, temp);
            enum_ptr = temp;
        }
        
        // Extract tag field
        llvm::Value* tag_ptr = builder.CreateStructGEP(
            llvm_enum_type,
            enum_ptr,
            0,
            "enum.tag.ptr"
        );
        llvm::Value* tag_value = builder.CreateLoad(
            llvm::Type::getInt32Ty(context),
            tag_ptr,
            "enum.tag"
        );
        
        // Compare tag
        llvm::Value* expected_tag = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context),
            variant_idx
        );
        
        return builder.CreateICmpEQ(tag_value, expected_tag, "match.enum.eq");
    }
    
    // StructPattern: struct matching (always matches, correctness guaranteed by TypeChecker)
    if (dynamic_cast<StructPattern*>(pattern)) {
        return nullptr;  // Struct pattern always matches, validated at type check stage
    }
    
    return nullptr;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pattern variable binding
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void ExprCodeGen::bindPatternVariables(Pattern* pattern, llvm::Value* value, Type* value_type) {
    auto& builder = context_->getBuilder();
    
    // VariablePattern: create variable binding
    if (auto* var = dynamic_cast<VariablePattern*>(pattern)) {
        // Create variable in current scope
        // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
        llvm::Type* var_type = context_->getLLVMType(value_type);
        
        // Create alloca and store value
        llvm::AllocaInst* var_alloca = builder.CreateAlloca(
            var_type, nullptr, var->getName());
        builder.CreateStore(value, var_alloca);
        
        // **Important**: Must register to context, otherwise it won't be found on subsequent use!
        context_->defineVariable(var->getName(), var_alloca);
        return;
    }
    
    // WildcardPattern: bind no variables
    if (dynamic_cast<WildcardPattern*>(pattern)) {
        return;
    }
    
    // LiteralPattern: bind no variables
    if (dynamic_cast<LiteralPattern*>(pattern)) {
        return;
    }
    
    // TuplePattern: recursively bind tuple elements
    if (auto* tuple = dynamic_cast<TuplePattern*>(pattern)) {
        if (!value_type || value_type->getKind() != Type::Kind::Tuple) {
            return;
        }
        
        auto* tuple_type = static_cast<TupleType*>(value_type);
        const auto& element_types = tuple_type->getElementTypes();
        const auto& patterns = tuple->getElements();
        
        // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
        llvm::Type* llvm_tuple_type = context_->getLLVMType(tuple_type);
        
        // Ensure value is pointer type
        llvm::Value* tuple_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "tuple.tmp");
            builder.CreateStore(value, temp);
            tuple_ptr = temp;
        }
        
        // Recursively bind each element
        for (size_t i = 0; i < patterns.size() && i < element_types.size(); ++i) {
            // Extract tuple element
            llvm::Value* elem_ptr = builder.CreateStructGEP(
                llvm_tuple_type,
                tuple_ptr,
                i,
                "tuple.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = context_->getLLVMType(element_types[i]);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "tuple.elem.val"
            );
            
            // Recursively bind
            bindPatternVariables(patterns[i].get(), elem_value, element_types[i]);
        }
        return;
    }
    
    // ArrayPattern: recursively bind array elements
    if (auto* array = dynamic_cast<ArrayPattern*>(pattern)) {
        if (!value_type || value_type->getKind() != Type::Kind::Array) {
            return;
        }
        
        auto* array_type = static_cast<ArrayType*>(value_type);
        Type* elem_type = array_type->getElementType();
        const auto& patterns = array->getElements();
        
        llvm::Type* llvm_array_type = context_->getLLVMType(array_type);
        
        // Ensure value is pointer type
        llvm::Value* array_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "array.tmp");
            builder.CreateStore(value, temp);
            array_ptr = temp;
        }
        
        // Recursively bind each element
        for (size_t i = 0; i < patterns.size() && i < array_type->getSize(); ++i) {
            // Extract array element
            llvm::Value* indices[] = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), i)
            };
            
            llvm::Value* elem_ptr = builder.CreateGEP(
                llvm_array_type,
                array_ptr,
                indices,
                "array.elem." + std::to_string(i)
            );
            
            llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
            llvm::Value* elem_value = builder.CreateLoad(
                elem_llvm_type,
                elem_ptr,
                "array.elem.val"
            );
            
            // Recursively bind
            bindPatternVariables(patterns[i].get(), elem_value, elem_type);
        }
        return;
    }
    
    // SlicePattern: recursively bind slice elements
    if (auto* slice_pat = dynamic_cast<SlicePattern*>(pattern)) {
        if (!value_type) {
            return;
        }
        
        Type* elem_type = nullptr;
        Type* rest_type = nullptr;
        
        if (value_type->getKind() == Type::Kind::Array) {
            auto* array_type = static_cast<ArrayType*>(value_type);
            elem_type = array_type->getElementType();
            // rest part is Slice type
            rest_type = new SliceType(elem_type);
        } else if (value_type->getKind() == Type::Kind::Slice) {
            auto* slice_type = static_cast<SliceType*>(value_type);
            elem_type = slice_type->getElementType();
            rest_type = slice_type;
        } else {
            return;
        }
        
        llvm::Type* llvm_value_type = context_->getLLVMType(value_type);
        
        // Ensure value is pointer type
        llvm::Value* value_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "slice.tmp");
            builder.CreateStore(value, temp);
            value_ptr = temp;
        }
        
        // Bind prefix elements
        for (size_t i = 0; i < slice_pat->getPrefix().size(); ++i) {
            if (value_type->getKind() == Type::Kind::Array) {
                // Array: use GEP to extract
                llvm::Value* indices[] = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), i)
                };
                
                llvm::Value* elem_ptr = builder.CreateGEP(
                    llvm_value_type, value_ptr, indices,
                    "slice.prefix.elem." + std::to_string(i)
                );
                
                llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
                llvm::Value* elem_value = builder.CreateLoad(
                    elem_llvm_type, elem_ptr,                     "slice.prefix.val"
                );
                
                // Recursively bind
                bindPatternVariables(slice_pat->getPrefix()[i].get(), elem_value, elem_type);
            }
        }
        
        // Bind suffix elements ([a, .., z] means z)
        if (value_type->getKind() == Type::Kind::Array) {
            auto* array_type = static_cast<ArrayType*>(value_type);
            size_t array_size = array_type->getSize();
            
            for (size_t i = 0; i < slice_pat->getSuffix().size(); ++i) {
                // Suffix index counts from array end
                size_t index = array_size - slice_pat->getSuffix().size() + i;
                llvm::Value* indices[] = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), index)
                };
                
                llvm::Value* elem_ptr = builder.CreateGEP(
                    llvm_value_type, value_ptr, indices,
                    "slice.suffix.elem." + std::to_string(i)
                );
                
                llvm::Type* elem_llvm_type = context_->getLLVMType(elem_type);
                llvm::Value* elem_value = builder.CreateLoad(
                    elem_llvm_type, elem_ptr,                     "slice.suffix.val"
                );
                
                // Recursively bind
                bindPatternVariables(slice_pat->getSuffix()[i].get(), elem_value, elem_type);
            }
        }
        
        // Bind rest part (if it has a name)
        if (slice_pat->hasRest() && slice_pat->getRest()) {
            // Create Slice struct {ptr, len} and bind to rest variable
            
            if (value_type->getKind() == Type::Kind::Array) {
                auto* array_type = static_cast<ArrayType*>(value_type);
                size_t prefix_size = slice_pat->getPrefix().size();
                size_t suffix_size = slice_pat->getSuffix().size();
                size_t rest_size = array_type->getSize() - prefix_size - suffix_size;
                
                // Create LLVM representation of Slice type: { ptr, i64 }
                llvm::StructType* slice_struct_type = llvm::StructType::get(
                    context_->getLLVMContext(),
                    {
                        llvm::PointerType::getUnqual(context_->getLLVMContext()),  // data pointer
                        llvm::Type::getInt64Ty(context_->getLLVMContext())         // length
                    }
                );
                
                // Calculate rest array's starting pointer
                llvm::Value* indices[] = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_->getLLVMContext()), prefix_size)
                };
                
                llvm::Value* rest_ptr = builder.CreateGEP(
                    llvm_value_type, value_ptr, indices, "slice.rest.ptr"
                );
                
                // Create Slice struct {ptr, len}
                llvm::AllocaInst* slice_alloca = builder.CreateAlloca(
                    slice_struct_type, nullptr, "slice.rest"
                );
                
                // Set data field (index 0)
                llvm::Value* data_field_ptr = builder.CreateStructGEP(
                    slice_struct_type, slice_alloca, 0, "slice.data.ptr"
                );
                builder.CreateStore(rest_ptr, data_field_ptr);
                
                // Set len field (index 1)
                llvm::Value* len_field_ptr = builder.CreateStructGEP(
                    slice_struct_type, slice_alloca, 1, "slice.len.ptr"
                );
                llvm::Value* len_value = llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(context_->getLLVMContext()), rest_size
                );
                builder.CreateStore(len_value, len_field_ptr);
                
                // Load Slice value and bind to rest variable
                llvm::Value* slice_value = builder.CreateLoad(
                    slice_struct_type, slice_alloca, "slice.rest.val"
                );
                
                bindPatternVariables(slice_pat->getRest(), slice_value, rest_type);
            } else if (value_type->getKind() == Type::Kind::Slice) {
                // For Slice type, need to dynamically calculate rest part
                // Extract data and len fields
                // TODO: implement Slice rest binding
                // Currently not implemented, requires runtime calculation
            }
        }
        
        return;
    }
    
    // EnumPattern: bind enum data
    if (auto* enum_pat = dynamic_cast<EnumPattern*>(pattern)) {
        // Special handling for Result type
        if (value_type && value_type->getKind() == Type::Kind::Result) {
            bindResultPatternVariables(enum_pat, value, static_cast<ResultType*>(value_type));
            return;
        }
        
        // Special handling for Optional type
        if (value_type && value_type->getKind() == Type::Kind::Optional) {
            bindOptionalPatternVariables(enum_pat, value, static_cast<OptionalType*>(value_type));
            return;
        }
        
        if (!value_type || value_type->getKind() != Type::Kind::Enum) {
            return;
        }
        
        auto* enum_type = static_cast<EnumType*>(value_type);
        const auto& variants = enum_type->getVariants();
        
        // Look up variant
        int variant_idx = -1;
        Type* data_type = nullptr;
        for (size_t i = 0; i < variants.size(); ++i) {
            if (variants[i].first == enum_pat->getVariantName()) {
                variant_idx = static_cast<int>(i);
                data_type = variants[i].second;
                break;
            }
        }
        
        if (variant_idx < 0 || !data_type) {
            return;  // Variant has no data, or variant does not exist
        }
        
        // Extract enum's data field (1st field)
        // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
        llvm::Type* llvm_enum_type = context_->getLLVMType(enum_type);
        
        llvm::Value* enum_ptr = value;
        if (!value->getType()->isPointerTy()) {
            llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "enum.tmp");
            builder.CreateStore(value, temp);
            enum_ptr = temp;
        }
        
        // Extract data field
        llvm::Value* data_ptr = builder.CreateStructGEP(
            llvm_enum_type,
            enum_ptr,
            1,
            "enum.data.ptr"
        );
        
        llvm::Type* data_llvm_type = context_->getLLVMType(data_type);
        llvm::Value* data_value = builder.CreateLoad(
            data_llvm_type,
            data_ptr,
            "enum.data"
        );
        
        // 🔧 Multi-parameter support: check if tuple unpacking is needed
        auto& inner_patterns = enum_pat->getInnerPatterns();
        
        if (inner_patterns.empty()) {
            return;  // No inner patterns
        }
        
        if (data_type->isTuple()) {
            // Multi-parameter: extract each element from tuple and bind
            TupleType* tuple_type = static_cast<TupleType*>(data_type);
            const auto& element_types = tuple_type->getElementTypes();
            
            // Store tuple value to temporary variable (for GEP)
            llvm::AllocaInst* tuple_tmp = builder.CreateAlloca(
                data_llvm_type, nullptr, "enum.tuple.tmp");
            builder.CreateStore(data_value, tuple_tmp);
            
            // Extract tuple element for each inner pattern and bind
            for (size_t i = 0; i < inner_patterns.size() && i < element_types.size(); ++i) {
                // Extract tuple element
                llvm::Value* elem_ptr = builder.CreateStructGEP(
                    data_llvm_type,
                    tuple_tmp,
                    i,
                    "tuple.elem." + std::to_string(i) + ".ptr"
                );
                
                llvm::Type* elem_llvm_type = context_->getLLVMType(element_types[i]);
                llvm::Value* elem_value = builder.CreateLoad(
                    elem_llvm_type,
                    elem_ptr,
                    "tuple.elem." + std::to_string(i)
                );
                
                // Recursively bind variable
                bindPatternVariables(inner_patterns[i].get(), elem_value, element_types[i]);
            }
        } else {
            // Single parameter: bind directly
            bindPatternVariables(inner_patterns[0].get(), data_value, data_type);
        }
        return;
    }
    
    // StructPattern: extract fields and bind variables
    if (auto* struct_pat = dynamic_cast<StructPattern*>(pattern)) {
        if (value_type && value_type->getKind() == Type::Kind::Struct) {
            bindStructPatternVariables(struct_pat, value, static_cast<StructType*>(value_type));
        }
        return;
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Result type pattern matching
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

llvm::Value* ExprCodeGen::generateResultPatternMatch(
    EnumPattern* pattern,
    llvm::Value* scrutinee,
    ResultType* results_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    
    // Result memory layout: { i1 is_ok, T value, i8* error }
    // Ok => is_ok == true
    // Err => is_ok == false
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_results_type = context_->getLLVMType(results_type);
    
    // If scrutinee is not a pointer, store on stack first
    llvm::Value* results_ptr = scrutinee;
    if (!scrutinee->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "results.tmp");
        builder.CreateStore(scrutinee, temp);
        results_ptr = temp;
    }
    
    // Extract is_ok field (0th field)
    llvm::Value* is_ok_ptr = builder.CreateStructGEP(
        llvm_results_type,
        results_ptr,
        0,
        "results.is_ok.ptr"
    );
    llvm::Value* is_ok = builder.CreateLoad(
        llvm::Type::getInt1Ty(ctx),
        is_ok_ptr,
        "results.is_ok"
    );
    
    if (variant == "Ok") {
        // Ok variant: is_ok == true
        return is_ok;
    } else if (variant == "Err") {
        // Err variant: is_ok == false
        return builder.CreateNot(is_ok, "results.is_err");
    }
    
    return nullptr;
}

llvm::Value* ExprCodeGen::generateOptionalPatternMatch(
    EnumPattern* pattern,
    llvm::Value* scrutinee,
    OptionalType* opt_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    
    // Optional memory layout: { i1 has_value, T value }
    // Some => has_value == true
    // None => has_value == false
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_opt_type = context_->getLLVMType(opt_type);
    
    llvm::Value* opt_ptr = scrutinee;
    if (!scrutinee->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(scrutinee->getType(), nullptr, "opt.tmp");
        builder.CreateStore(scrutinee, temp);
        opt_ptr = temp;
    }
    
    // Extract has_value field (0th field)
    llvm::Value* has_value_ptr = builder.CreateStructGEP(
        llvm_opt_type,
        opt_ptr,
        0,
        "opt.has_value.ptr"
    );
    llvm::Value* has_value = builder.CreateLoad(
        llvm::Type::getInt1Ty(ctx),
        has_value_ptr,
        "opt.has_value"
    );
    
    if (variant == "Some") {
        return has_value;
    } else if (variant == "None") {
        return builder.CreateNot(has_value, "opt.is_none");
    }
    
    return nullptr;
}

void ExprCodeGen::bindResultPatternVariables(
    EnumPattern* pattern,
    llvm::Value* value,
    ResultType* results_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    auto& inner_patterns = pattern->getInnerPatterns();
    
    if (inner_patterns.empty()) {
        return;  // No variables need binding
    }
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_results_type = context_->getLLVMType(results_type);
    
    llvm::Value* results_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "results.tmp");
        builder.CreateStore(value, temp);
        results_ptr = temp;
    }
    
    if (variant == "Ok") {
        // Extract value field (1st field)
        llvm::Value* value_ptr = builder.CreateStructGEP(
            llvm_results_type,
            results_ptr,
            1,
            "results.value.ptr"
        );
        
        llvm::Type* ok_llvm_type = context_->getLLVMType(results_type->getOkType());
        llvm::Value* ok_value = builder.CreateLoad(
            ok_llvm_type,
            value_ptr,
            "results.value"
        );
        
        // Recursively bind inner pattern variable
        bindPatternVariables(inner_patterns[0].get(), ok_value, results_type->getOkType());
    }
    else if (variant == "Err") {
        // Extract error field (2nd field)
        llvm::Value* error_ptr = builder.CreateStructGEP(
            llvm_results_type,
            results_ptr,
            2,
            "results.error.ptr"
        );
        
        llvm::Type* string_llvm_type = context_->getStringType();
        llvm::Value* error_value = builder.CreateLoad(
            string_llvm_type,
            error_ptr,
            "results.error"
        );
        
        // Recursively bind inner pattern variable (error is string type)
        bindPatternVariables(inner_patterns[0].get(), error_value, nullptr);
    }
}

void ExprCodeGen::bindOptionalPatternVariables(
    EnumPattern* pattern,
    llvm::Value* value,
    OptionalType* opt_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    std::string variant = pattern->getVariantName();
    auto& inner_patterns = pattern->getInnerPatterns();
    
    if (inner_patterns.empty() || variant == "None") {
        return;  // None has no value to bind
    }
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_opt_type = context_->getLLVMType(opt_type);
    
    llvm::Value* opt_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "opt.tmp");
        builder.CreateStore(value, temp);
        opt_ptr = temp;
    }
    
    if (variant == "Some") {
        // Extract value field (1st field)
        llvm::Value* value_ptr = builder.CreateStructGEP(
            llvm_opt_type,
            opt_ptr,
            1,
            "opt.value.ptr"
        );
        
        llvm::Type* inner_llvm_type = context_->getLLVMType(opt_type->getInnerType());
        llvm::Value* inner_value = builder.CreateLoad(
            inner_llvm_type,
            value_ptr,
            "opt.value"
        );
        
        // Recursively bind inner pattern variable
        bindPatternVariables(inner_patterns[0].get(), inner_value, opt_type->getInnerType());
    }
}

void ExprCodeGen::bindStructPatternVariables(
    StructPattern* pattern,
    llvm::Value* value,
    StructType* struct_type
) {
    auto& ctx = context_->getLLVMContext();
    auto& builder = context_->getBuilder();
    
    // TypeCodeGen uniformly uses CodeGenContext::getLLVMType()
    llvm::Type* llvm_struct_type = context_->getLLVMType(struct_type);
    
    // Ensure value is pointer type
    llvm::Value* struct_ptr = value;
    if (!value->getType()->isPointerTy()) {
        llvm::AllocaInst* temp = builder.CreateAlloca(value->getType(), nullptr, "struct.tmp");
        builder.CreateStore(value, temp);
        struct_ptr = temp;
    }
    
    // Traverse each field pattern
    for (const auto& field_pattern : pattern->getFields()) {
        const std::string& field_name = field_pattern.field_name;
        
        // Look up field index in struct
        const auto& fields = struct_type->getFields();
        int field_idx = -1;
        Type* field_type = nullptr;
        
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].first == field_name) {
                field_idx = static_cast<int>(i);
                field_type = fields[i].second;
                break;
            }
        }
        
        if (field_idx < 0 || !field_type) {
            continue;  // Field does not exist (TypeChecker should have reported error)
        }
        
        // Extract field value
        llvm::Value* field_ptr = builder.CreateStructGEP(
            llvm_struct_type,
            struct_ptr,
            field_idx,
            "struct." + field_name + ".ptr"
        );
        
        llvm::Type* field_llvm_type = context_->getLLVMType(field_type);
        llvm::Value* field_value = builder.CreateLoad(
            field_llvm_type,
            field_ptr,
            "struct." + field_name
        );
        
        // Recursively bind field pattern variable
        bindPatternVariables(field_pattern.pattern.get(), field_value, field_type);
    }
}

} // namespace pawc

