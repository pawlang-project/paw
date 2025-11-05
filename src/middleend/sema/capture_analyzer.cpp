//===--- capture_analyzer.cpp - Closure Capture Analysis -------*- C++ -*-===//
/// @file capture_analyzer.cpp
/// @brief Type system and semantic analysis implementation

#include "capture_analyzer.h"
#include "frontend/parser/ast/pattern.h"
#include <iostream>

namespace pawc {

std::vector<ClosureExpr::CapturedVar> CaptureAnalyzer::analyze(
    Expr* body, 
    const std::vector<ClosureExpr::Param>& params) {
    
    // clearemptybeforeof/theanalysisresults
    local_vars_stack_.clear();
    captured_vars_.clear();
    closure_depth_ = 0;
    
    // createtheone/alayer/levelscope
    enterScope();
    
    // 1. willclosureparameteraddtocurrentscope（they/themnotyescapturevariable）
    for (const auto& param : params) {
        local_vars_stack_.back().insert(param.name);
    }
    
    // 2. traverseclosurebody/struct
    if (body) {
        body->accept(this);
    }
    
    // exitscope
    exitScope();
    
    // 3. build/constructcapturevariablelist
    std::vector<ClosureExpr::CapturedVar> results;
    for (const auto& var_name : captured_vars_) {
        // fromsymboltablelookuptypes
        VariableSymbol* var = symbols_->lookupVariable(var_name);
        if (var) {
            results.emplace_back(var_name, var->getType(), false);  // defaultvaluecapture
        }
    }
    
    return results;
}

void CaptureAnalyzer::enterScope() {
    local_vars_stack_.push_back(std::set<std::string>());
}

void CaptureAnalyzer::exitScope() {
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.pop_back();
    }
}

bool CaptureAnalyzer::isLocalVariable(const std::string& name) const {
    // CheckAllscopestack
    for (const auto& scope : local_vars_stack_) {
        if (scope.find(name) != scope.end()) {
            return true;
        }
    }
    return false;
}

void CaptureAnalyzer::visit(IdentifierExpr* node) {
    const std::string& name = node->getName();
    
    // ifnotyeslocalvariable，otherwiseyescapturevariable
    if (!isLocalVariable(name)) {
        // Checkyesnoin/atexternal/outsidescopeexists
        if (symbols_->lookupVariable(name)) {
            captured_vars_.insert(name);
            std::cerr << "[CaptureAnalyzer] Captured variable: " << name << std::endl;
        }
    }
}

void CaptureAnalyzer::visit(BinaryExpr* node) {
    node->getLeft()->accept(this);
    node->getRight()->accept(this);
}

void CaptureAnalyzer::visit(UnaryExpr* node) {
    node->getOperand()->accept(this);
}

void CaptureAnalyzer::visit(CallExpr* node) {
    node->getCallee()->accept(this);
    for (const auto& arg : node->getArgs()) {
        arg->accept(this);
    }
}

void CaptureAnalyzer::visit(MemberExpr* node) {
    node->getObject()->accept(this);
}

void CaptureAnalyzer::visit(IndexExpr* node) {
    node->getObject()->accept(this);
    node->getIndex()->accept(this);
}

void CaptureAnalyzer::visit(IfExpr* node) {
    node->getCondition()->accept(this);
    node->getThenExpr()->accept(this);
    if (node->getElseExpr()) {
        node->getElseExpr()->accept(this);
    }
}

void CaptureAnalyzer::visit(BlockExpr* node) {
    // 🔧 enternewscope
    enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    
    // 🔧 exitscope（localvariablenotno longer/againvisible）
    exitScope();
}

void CaptureAnalyzer::visit(ArrayLiteral* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(TupleExpr* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(RangeExpr* node) {
    node->getStart()->accept(this);
    node->getEnd()->accept(this);
}

void CaptureAnalyzer::visit(StructLiteral* node) {
    for (const auto& field : node->getFields()) {
        field.value->accept(this);
    }
}

void CaptureAnalyzer::visit(MatchExpr* node) {
    node->getScrutinee()->accept(this);
    for (const auto& arm : node->getArms()) {
        // 🔧 each/everymatch armallyesone/aindividual/piecenewscope
        enterScope();
        
        // visitpattern（definitionnewvariable）
        arm.pattern->accept(this);
        
        // visitarmof/theexpression
        arm.expression->accept(this);
        
        // 🔧 exitarmscope
        exitScope();
    }
}

void CaptureAnalyzer::visit(ClosureExpr* node) {
    // 🔧 nestedclosure：recursionanalysisitscapture
    // nestedclosurepossiblycaptureoutsidelayer/levelclosureof/theparameterandcapturevariable
    
    closure_depth_++;
    
    if (closure_depth_ > 10) {
        // preventtoodeepnestedcause/lead toproblem/issue
        std::cerr << "[CaptureAnalyzer] Warning: Nested closure depth > 10" << std::endl;
        closure_depth_--;
        return;
    }
    
    // createnewscope
    enterScope();
    
    // addnestedclosureof/theparameter
    for (const auto& param : node->getParams()) {
        local_vars_stack_.back().insert(param.name);
    }
    
    // analysisnestedclosureof/thebody
    if (node->getBody()) {
        node->getBody()->accept(this);
    }
    
    // exitnestedclosurescope
    exitScope();
    
    closure_depth_--;
}

void CaptureAnalyzer::visit(TryExpr* node) {
    node->getExpr()->accept(this);
}

void CaptureAnalyzer::visit(ExprStmt* node) {
    if (node->getExpr()) {
        node->getExpr()->accept(this);
    }
}

void CaptureAnalyzer::visit(VarDecl* node) {
    // Firstcheckinitialvalue（possiblyreferenceexternal/outsidevariable）
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // thenback/afteraddtocurrentscopeof/thelocalvariable
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.back().insert(node->getName());
    }
}

void CaptureAnalyzer::visit(DestructuringDecl* node) {
    // Firstcheckinitialvalue
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // thenback/afteraddtocurrentscopeof/thelocalvariable
    if (!local_vars_stack_.empty()) {
        for (const auto& name : node->getNames()) {
            local_vars_stack_.back().insert(name);
        }
    }
}

void CaptureAnalyzer::visit(StructDestructuringDecl* node) {
    // Firstcheckinitialvalue
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // thenback/afteraddtocurrentscopeof/thelocalvariable
    if (!local_vars_stack_.empty()) {
        for (const auto& name : node->getFieldNames()) {
            local_vars_stack_.back().insert(name);
        }
    }
}

void CaptureAnalyzer::visit(ReturnStmt* node) {
    if (node->getValue()) {
        node->getValue()->accept(this);
    }
}

void CaptureAnalyzer::visit(IfStmt* node) {
    node->getCondition()->accept(this);
    node->getThenStmt()->accept(this);
    if (node->getElseStmt()) {
        node->getElseStmt()->accept(this);
    }
}

void CaptureAnalyzer::visit(LoopStmt* node) {
    // simplifyimplementation：onlytraversebody
    node->getBody()->accept(this);
}

void CaptureAnalyzer::visit(WhileStmt* node) {
    node->getCondition()->accept(this);
    node->getBody()->accept(this);
}

void CaptureAnalyzer::visit(BlockStmt* node) {
    // 🔧 enternewscope
    enterScope();
    
    for (const auto& stmt : node->getStmts()) {
        stmt->accept(this);
    }
    
    // 🔧 exitscope（localvariablenotno longer/againvisible）
    exitScope();
}

void CaptureAnalyzer::visit(ForStmt* node) {
    // ForStmttemporarilytime/whennotuse，simplifyimplementation
    // PawLang uses unified loop keyword
}

void CaptureAnalyzer::visit(VariablePattern* node) {
    // Patternbindof/thevariableyescurrentscopeof/thelocalvariable
    if (!local_vars_stack_.empty()) {
        local_vars_stack_.back().insert(node->getName());
    }
}

void CaptureAnalyzer::visit(TuplePattern* node) {
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(EnumPattern* node) {
    // EnumPatternpossiblyhasnestedpattern
    const auto& inner_patterns = node->getInnerPatterns();
    for (const auto& pattern : inner_patterns) {
        pattern->accept(this);
    }
}

void CaptureAnalyzer::visit(StructPattern* node) {
    // structbody/structpatternbindfieldvariable
    for (const auto& field : node->getFields()) {
        field.pattern->accept(this);
    }
}

void CaptureAnalyzer::visit(ArrayPattern* node) {
    // arraypatternbindelementvariable
    for (const auto& elem : node->getElements()) {
        elem->accept(this);
    }
}

void CaptureAnalyzer::visit(SlicePattern* node) {
    // Slicepatternbindvariable
    for (const auto& prefix : node->getPrefix()) {
        prefix->accept(this);
    }
    for (const auto& suffix : node->getSuffix()) {
        suffix->accept(this);
    }
    if (node->hasRest()) {
        node->getRest()->accept(this);
    }
}

void CaptureAnalyzer::visit(OrPattern* node) {
    // ORpattern：traverseAllbranch
    for (const auto& alt : node->getAlternatives()) {
        alt->accept(this);
    }
}

} // namespace pawc

