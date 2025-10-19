#ifndef PAWC_TOML_PARSER_H
#define PAWC_TOML_PARSER_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace pawc {

// TOML value types
struct TomlValue {
    enum class Type {
        String,
        Integer,
        Boolean,
        Array,
        Table
    };
    
    Type type;
    std::string string_value;
    int64_t int_value;
    bool bool_value;
    std::vector<TomlValue> array_value;
    std::map<std::string, TomlValue> table_value;
    
    TomlValue() : type(Type::String), int_value(0), bool_value(false) {}
};

// PawLang project configuration
struct PawConfig {
    // [package]
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string edition;
    std::string description;
    
    // [lib]
    std::string lib_type;  // "bin" or "lib"
    
    // [dependencies]
    std::map<std::string, std::string> dependencies;
    
    // [build]
    std::string target;
    int opt_level;
    bool debug;
    
    // [features]
    std::vector<std::string> default_features;
    std::map<std::string, std::vector<std::string>> features;
};

// TOML parser
class TomlParser {
public:
    TomlParser(const std::string& content);
    
    // Parse TOML content
    bool parse();
    
    // Get parsed configuration
    const PawConfig& getConfig() const { return config_; }
    
    // Get error message
    const std::string& getError() const { return error_; }
    
private:
    std::string content_;
    size_t pos_;
    PawConfig config_;
    std::string error_;
    
    // Parsing helper functions
    void skipWhitespace();
    void skipComment();
    std::string parseKey();
    TomlValue parseValue();
    std::string parseString();
    int64_t parseInt();
    bool parseBool();
    std::vector<TomlValue> parseArray();
    std::map<std::string, TomlValue> parseTable();
    
    // Character checking
    bool isAtEnd() const { return pos_ >= content_.size(); }
    char peek() const { return isAtEnd() ? '\0' : content_[pos_]; }
    char advance() { return content_[pos_++]; }
};

// Load paw.toml file
PawConfig loadPawConfig(const std::string& project_dir);

} // namespace pawc

#endif // PAWC_TOML_PARSER_H

