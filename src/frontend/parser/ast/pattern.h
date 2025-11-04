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

/// EnumPattern - 枚举模式 (例如: Some(x), None, Option::Some(x), ok(v))
class EnumPattern : public Pattern {
public:
    // 简单构造器（向后兼容）
    EnumPattern(std::string variant_name, PatternPtr inner = nullptr)
        : type_name_(""),
          variant_name_(std::move(variant_name)),
          is_alias_(false) {
        if (inner) {
            inner_patterns_.push_back(std::move(inner));
        }
    }
    
    // 完整构造器
    EnumPattern(std::string type_name, std::string variant_name, 
                std::vector<PatternPtr> inner_patterns, bool is_alias = false)
        : type_name_(std::move(type_name)),
          variant_name_(std::move(variant_name)),
          inner_patterns_(std::move(inner_patterns)),
          is_alias_(is_alias) {}
    
    const std::string& getTypeName() const { return type_name_; }
    const std::string& getVariantName() const { return variant_name_; }
    bool isAlias() const { return is_alias_; }
    const std::vector<PatternPtr>& getInnerPatterns() const { return inner_patterns_; }
    
    // 向后兼容
    Pattern* getInner() const { 
        return inner_patterns_.empty() ? nullptr : inner_patterns_[0].get(); 
    }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;        // 类型名（可选，如 "Option"）
    std::string variant_name_;     // 变体名（如 "Some" 或 "ok"）
    std::vector<PatternPtr> inner_patterns_;  // 内部模式（支持多参数）
    bool is_alias_;                // 是否是小写别名（ok, err, some, none）
};

/// StructPattern - 结构体模式 (例如: Point { x, y }, Point { x: a, y: b })
class StructPattern : public Pattern {
public:
    /// 字段模式：字段名 -> 绑定的模式
    struct FieldPattern {
        std::string field_name;  // 字段名（如 "x"）
        PatternPtr pattern;      // 绑定的模式（通常是VariablePattern）
        
        FieldPattern(std::string name, PatternPtr pat)
            : field_name(std::move(name)), pattern(std::move(pat)) {}
    };
    
    StructPattern(std::string struct_name, std::vector<FieldPattern> fields)
        : struct_name_(std::move(struct_name)), fields_(std::move(fields)) {}
    
    const std::string& getStructName() const { return struct_name_; }
    const std::vector<FieldPattern>& getFields() const { return fields_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string struct_name_;              // 结构体名（如 "Point"）
    std::vector<FieldPattern> fields_;     // 字段模式列表
};

/// ArrayPattern - 数组解构模式 (例如: [a, b, c])
class ArrayPattern : public Pattern {
public:
    ArrayPattern(std::vector<PatternPtr> elements, size_t expected_size)
        : elements_(std::move(elements)), expected_size_(expected_size) {}
    
    const std::vector<PatternPtr>& getElements() const { return elements_; }
    size_t getExpectedSize() const { return expected_size_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> elements_;  // 元素模式
    size_t expected_size_;              // 期望的数组大小
};

/// SlicePattern - 切片解构模式 (例如: [first, ..rest], [first, .., last])
class SlicePattern : public Pattern {
public:
    SlicePattern(std::vector<PatternPtr> prefix, PatternPtr rest = nullptr, 
                 std::vector<PatternPtr> suffix = {})
        : prefix_(std::move(prefix)), rest_(std::move(rest)), suffix_(std::move(suffix)) {}
    
    const std::vector<PatternPtr>& getPrefix() const { return prefix_; }
    Pattern* getRest() const { return rest_.get(); }
    bool hasRest() const { return rest_ != nullptr; }
    const std::vector<PatternPtr>& getSuffix() const { return suffix_; }
    bool hasSuffix() const { return !suffix_.empty(); }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> prefix_;  // 前缀元素模式
    PatternPtr rest_;                 // 剩余部分 (可选, ..rest)
    std::vector<PatternPtr> suffix_;  // 后缀元素模式（用于 [a, .., z]）
};

/// RangePattern - 范围模式 (例如: 1..10, 'a'..'z', 1..=10)
class RangePattern : public Pattern {
public:
    RangePattern(PatternPtr start, PatternPtr end, bool inclusive)
        : start_(std::move(start)), end_(std::move(end)), inclusive_(inclusive) {}
    
    Pattern* getStart() const { return start_.get(); }
    Pattern* getEnd() const { return end_.get(); }
    bool isInclusive() const { return inclusive_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    PatternPtr start_;     // 起始值（通常是 LiteralPattern）
    PatternPtr end_;       // 结束值
    bool inclusive_;       // 是否包含结束值（.. 或 ..=）
};

/// OrPattern - OR模式 (例如: 1 | 2 | 3, Color::red | Color::blue)
class OrPattern : public Pattern {
public:
    explicit OrPattern(std::vector<PatternPtr> alternatives)
        : alternatives_(std::move(alternatives)) {}
    
    const std::vector<PatternPtr>& getAlternatives() const { return alternatives_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> alternatives_;  // 多个可选模式
};

} // namespace pawc

#endif // PAW_PATTERN_H
