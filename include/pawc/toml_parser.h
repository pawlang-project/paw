#ifndef PAWC_TOML_PARSER_H
#define PAWC_TOML_PARSER_H

#include <string>
#include <map>
#include <vector>

namespace pawc {

// TOML值类型
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

// PawLang项目配置
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

// TOML解析器
class TomlParser {
public:
    TomlParser(const std::string& content);
    
    // 解析TOML内容
    bool parse();
    
    // 获取解析后的配置
    const PawConfig& getConfig() const { return config_; }
    
    // 获取错误信息
    const std::string& getError() const { return error_; }
    
private:
    std::string content_;
    size_t pos_;
    PawConfig config_;
    std::string error_;
    
    // 解析辅助函数
    void skipWhitespace();
    void skipComment();
    std::string parseKey();
    TomlValue parseValue();
    std::string parseString();
    int64_t parseInt();
    bool parseBool();
    std::vector<TomlValue> parseArray();
    std::map<std::string, TomlValue> parseTable();
    
    // 字符检查
    bool isAtEnd() const { return pos_ >= content_.size(); }
    char peek() const { return isAtEnd() ? '\0' : content_[pos_]; }
    char advance() { return content_[pos_++]; }
};

// 加载paw.toml文件
PawConfig loadPawConfig(const std::string& project_dir);

} // namespace pawc

#endif // PAWC_TOML_PARSER_H

