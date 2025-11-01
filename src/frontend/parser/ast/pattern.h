//===--- pattern.h - Pattern AST Nodes ---------------------------*- C++ -*-===//
//
// Pattern节点用于match表达式的模式匹配
//
//===----------------------------------------------------------------------===//

#ifndef PAW_PATTERN_H
#define PAW_PATTERN_H

#include "ast_base.h"
#include <string>
#include <memory>
#include <vector>

namespace pawc {

class ASTVisitor;

/// LiteralPattern - 字面量模式 (例如: 0, 1, "hello")
class LiteralPattern : public Pattern {
public:
    enum class Kind {
        Int,
        Float,
        Bool,
        Char,
        String
    };
    
    LiteralPattern(Kind kind, std::string value)
        : kind_(kind), value_(std::move(value)) {}
    
    Kind getKind() const { return kind_; }
    const std::string& getValue() const { return value_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    Kind kind_;
    std::string value_;
};

/// WildcardPattern - 通配符模式 (_)
class WildcardPattern : public Pattern {
public:
    WildcardPattern() = default;
    void accept(ASTVisitor* visitor) override;
};

/// VariablePattern - 变量绑定模式 (例如: value, x)
class VariablePattern : public Pattern {
public:
    explicit VariablePattern(std::string name)
        : name_(std::move(name)) {}
    
    const std::string& getName() const { return name_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
};

/// TuplePattern - 元组模式 (例如: (x, y), (0, _))
class TuplePattern : public Pattern {
public:
    explicit TuplePattern(std::vector<PatternPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<PatternPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> elements_;
};

/// EnumPattern - 枚举模式 (例如: Some(x), None)
class EnumPattern : public Pattern {
public:
    EnumPattern(std::string variant_name, PatternPtr inner = nullptr)
        : variant_name_(std::move(variant_name)),
          inner_(std::move(inner)) {}
    
    const std::string& getVariantName() const { return variant_name_; }
    Pattern* getInner() const { return inner_.get(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string variant_name_;
    PatternPtr inner_;  // 可选的内部模式
};

} // namespace pawc

#endif // PAW_PATTERN_H
