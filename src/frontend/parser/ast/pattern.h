//===--- pattern.h - Pattern AST Nodes ---------------------------*- C++ -*-===//
//
// Patternnodeused formatchexpressionof/thepattern matching
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

/// LiteralPattern - literal pattern (e.g.: 0, 1, "hello")
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

/// WildcardPattern - wildcardpattern (_)
class WildcardPattern : public Pattern {
public:
    WildcardPattern() = default;
    void accept(ASTVisitor* visitor) override;
};

/// VariablePattern - variable binding pattern (e.g.: value, x)
class VariablePattern : public Pattern {
public:
    explicit VariablePattern(std::string name)
        : name_(std::move(name)) {}
    
    const std::string& getName() const { return name_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string name_;
};

/// TuplePattern - tuple pattern (e.g.: (x, y), (0, _))
class TuplePattern : public Pattern {
public:
    explicit TuplePattern(std::vector<PatternPtr> elements)
        : elements_(std::move(elements)) {}
    
    const std::vector<PatternPtr>& getElements() const { return elements_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> elements_;
};

/// EnumPattern - enum pattern (e.g.: Some(x), None, Option::Some(x))me(x), ok(v))
class EnumPattern : public Pattern {
public:
    // Simple single constructor (backward compatible)
    EnumPattern(std::string variant_name, PatternPtr inner = nullptr)
        : type_name_(""),
          variant_name_(std::move(variant_name)),
          is_alias_(false) {
        if (inner) {
            inner_patterns_.push_back(std::move(inner));
        }
    }
    
    // completeconstructor
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
    
    // Backward compatible
    Pattern* getInner() const { 
        return inner_patterns_.empty() ? nullptr : inner_patterns_[0].get(); 
    }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string type_name_;        // Type name (optional, e.g. "Option")n"）
    std::string variant_name_;     // Variant name (e.g. "Some") or "ok"）
    std::vector<PatternPtr> inner_patterns_;  // internal/insidepattern（supportmany/muchparameter）
    bool is_alias_;                // Is lowercase alias (ok, err, some, none)r, some, none）
};

/// StructPattern - struct pattern (e.g.: Point { x, y }, Point { x: a, y: b })t { x: a, y: b })
class StructPattern : public Pattern {
public:
    /// fieldpattern：fieldname -> bindof/thepattern
    struct FieldPattern {
        std::string field_name;  // Field name (e.g. "x")
        PatternPtr pattern;      // bindof/thepattern（usually/normallyyesVariablePattern）
        
        FieldPattern(std::string name, PatternPtr pat)
            : field_name(std::move(name)), pattern(std::move(pat)) {}
    };
    
    StructPattern(std::string struct_name, std::vector<FieldPattern> fields)
        : struct_name_(std::move(struct_name)), fields_(std::move(fields)) {}
    
    const std::string& getStructName() const { return struct_name_; }
    const std::vector<FieldPattern>& getFields() const { return fields_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::string struct_name_;              // Struct type name (e.g. "Point") "Point"）
    std::vector<FieldPattern> fields_;     // fieldpatternlist
};

/// ArrayPattern - array destructuring pattern (e.g.: [a, b, c])
class ArrayPattern : public Pattern {
public:
    ArrayPattern(std::vector<PatternPtr> elements, size_t expected_size)
        : elements_(std::move(elements)), expected_size_(expected_size) {}
    
    const std::vector<PatternPtr>& getElements() const { return elements_; }
    size_t getExpectedSize() const { return expected_size_; }
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> elements_;  // elementpattern
    size_t expected_size_;              // expectedof/thearraysize
};

/// SlicePattern - Slice destructuring pattern with middle rest support
///
/// Represents patterns that match arrays or slices with optional rest elements.
/// Supports three parts: prefix, rest (middle), and suffix.
///
/// Examples:
///   [first, .., last]      - Match first and last, ignore middle (anonymous rest)
///   [first, ..rest, last]  - Match first and last, bind middle to 'rest'
///   [a, b, .., y, z]       - Match first 2 and last 2 elements
///   [.., last]             - Match only the last element
///
/// The rest element (`..` or `..name`) must appear at most once.
/// Type checking ensures prefix.size() + suffix.size() <= array.size().
///
/// Code generation:
///   - Prefix elements: accessed from index 0
///   - Suffix elements: accessed from (array.size() - suffix.size())
///   - Rest: slice from prefix.size() to (array.size() - suffix.size())
class SlicePattern : public Pattern {
public:
    /// Constructs a SlicePattern
    /// @param prefix Pattern elements before the rest (may be empty)
    /// @param rest Optional rest pattern (nullptr for anonymous rest `..`)
    /// @param suffix Pattern elements after the rest (may be empty)
    SlicePattern(std::vector<PatternPtr> prefix, PatternPtr rest = nullptr, 
                 std::vector<PatternPtr> suffix = {})
        : prefix_(std::move(prefix)), rest_(std::move(rest)), suffix_(std::move(suffix)) {}
    
    /// Get the prefix patterns (elements before rest)
    const std::vector<PatternPtr>& getPrefix() const { return prefix_; }
    
    /// Get the rest pattern (may be nullptr for anonymous rest)
    Pattern* getRest() const { return rest_.get(); }
    
    /// Check if this pattern has a rest element
    bool hasRest() const { return rest_ != nullptr; }
    
    /// Get the suffix patterns (elements after rest)
    const std::vector<PatternPtr>& getSuffix() const { return suffix_; }
    
    /// Check if this pattern has suffix elements
    bool hasSuffix() const { return !suffix_.empty(); }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> prefix_;  ///< Pattern elements before rest
    PatternPtr rest_;                 ///< Optional rest pattern (nullptr for anonymous `..`)
    std::vector<PatternPtr> suffix_;  ///< Pattern elements after rest (for [a, .., z])
};

/// RangePattern - Range pattern matching
///
/// Matches values within a specified range. Supports both exclusive and inclusive ranges.
///
/// Syntax:
///   start..end    - Exclusive range (start <= value < end)
///   start..=end   - Inclusive range (start <= value <= end)
///
/// Examples:
///   90..100       - Matches 90, 91, ..., 99 (not 100)
///   90..=100      - Matches 90, 91, ..., 100 (includes 100)
///   'a'..'z'      - Matches lowercase letters a-y (not z)
///   'a'..='z'     - Matches lowercase letters a-z (includes z)
///
/// Constraints:
///   - Only supports integer types (i8, i16, i32, i64, u8, u16, u32, u64) and char
///   - Start and end must be literal patterns of the same type
///   - Type checked at compile time
///
/// Code generation:
///   Generates efficient comparison: (value >= start) && (value < end)
///   For inclusive: (value >= start) && (value <= end)
///   Optimized to 2 comparisons at runtime
class RangePattern : public Pattern {
public:
    /// Constructs a RangePattern
    /// @param start Starting value of the range (inclusive)
    /// @param end Ending value of the range
    /// @param inclusive Whether the end value is included in the range
    RangePattern(PatternPtr start, PatternPtr end, bool inclusive)
        : start_(std::move(start)), end_(std::move(end)), inclusive_(inclusive) {}
    
    /// Get the start pattern (typically a LiteralPattern)
    Pattern* getStart() const { return start_.get(); }
    
    /// Get the end pattern (typically a LiteralPattern)
    Pattern* getEnd() const { return end_.get(); }
    
    /// Check if this is an inclusive range (..=) or exclusive (..)
    bool isInclusive() const { return inclusive_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    PatternPtr start_;     ///< Start value (usually LiteralPattern)
    PatternPtr end_;       ///< End value (usually LiteralPattern)
    bool inclusive_;       ///< true for `..=` (inclusive), false for `..` (exclusive)
};

/// OrPattern - Alternative pattern matching (OR pattern)
///
/// Matches if any of the alternative patterns match. Uses pipe (|) separator.
///
/// Syntax:
///   pattern1 | pattern2 | pattern3
///
/// Examples:
///   1 | 2 | 3                    - Match any of these integers
///   "red" | "green" | "blue"     - Match any of these strings
///   'a' | 'e' | 'i' | 'o' | 'u'  - Match vowels
///   Some(x) | None               - Match any Optional variant
///
/// Constraints:
///   - All alternative patterns must have compatible types
///   - Type checked at compile time
///   - Can be combined with other patterns (e.g., (1 | 2 | 3) if x > 0)
///
/// Code generation:
///   Generates disjunction: cond1 || cond2 || cond3
///   Short-circuits on first match (optimized evaluation)
///   Time complexity: O(k) where k is the number of alternatives
///
/// Variable binding:
///   - All alternatives must bind the same variables
///   - Variables bound by the first matching alternative
class OrPattern : public Pattern {
public:
    /// Constructs an OrPattern
    /// @param alternatives Vector of alternative patterns to try matching
    explicit OrPattern(std::vector<PatternPtr> alternatives)
        : alternatives_(std::move(alternatives)) {}
    
    /// Get all alternative patterns
    const std::vector<PatternPtr>& getAlternatives() const { return alternatives_; }
    
    void accept(ASTVisitor* visitor) override;
    
private:
    std::vector<PatternPtr> alternatives_;  ///< Alternative patterns (at least 2)
};

} // namespace pawc

#endif // PAW_PATTERN_H
