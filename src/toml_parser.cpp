#include "pawc/toml_parser.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <filesystem>

namespace pawc {

TomlParser::TomlParser(const std::string& content)
    : content_(content), pos_(0) {}

bool TomlParser::parse() {
    // 简化的TOML解析器
    // 只支持基本的key = value格式和[section]
    
    std::string current_section;
    
    while (!isAtEnd()) {
        skipWhitespace();
        skipComment();
        
        if (isAtEnd()) break;
        
        char c = peek();
        
        // Section header: [package]
        if (c == '[') {
            advance(); // skip [
            std::string section_name;
            while (!isAtEnd() && peek() != ']') {
                section_name += advance();
            }
            if (peek() != ']') {
                error_ = "Expected ']' after section name";
                return false;
            }
            advance(); // skip ]
            current_section = section_name;
            continue;
        }
        
        // Key = Value
        if (std::isalpha(c) || c == '_') {
            std::string key = parseKey();
            skipWhitespace();
            
            if (peek() != '=') {
                error_ = "Expected '=' after key";
                return false;
            }
            advance(); // skip =
            skipWhitespace();
            
            TomlValue value = parseValue();
            
            // 根据section和key设置配置
            if (current_section == "package") {
                if (key == "name") config_.name = value.string_value;
                else if (key == "version") config_.version = value.string_value;
                else if (key == "edition") config_.edition = value.string_value;
                else if (key == "description") config_.description = value.string_value;
            } else if (current_section == "lib") {
                if (key == "type") config_.lib_type = value.string_value;
            } else if (current_section == "build") {
                if (key == "target") config_.target = value.string_value;
                else if (key == "opt_level") config_.opt_level = static_cast<int>(value.int_value);
                else if (key == "debug") config_.debug = value.bool_value;
            }
            
            continue;
        }
        
        advance(); // skip unknown char
    }
    
    return true;
}

void TomlParser::skipWhitespace() {
    while (!isAtEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) {
        advance();
    }
}

void TomlParser::skipComment() {
    if (peek() == '#') {
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
    }
}

std::string TomlParser::parseKey() {
    std::string key;
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_' || peek() == '-')) {
        key += advance();
    }
    return key;
}

TomlValue TomlParser::parseValue() {
    skipWhitespace();
    
    TomlValue value;
    
    // 字符串: "value"
    if (peek() == '"') {
        value.type = TomlValue::Type::String;
        value.string_value = parseString();
    }
    // 布尔值: true/false
    else if (peek() == 't' || peek() == 'f') {
        value.type = TomlValue::Type::Boolean;
        value.bool_value = parseBool();
    }
    // 数字
    else if (std::isdigit(peek()) || peek() == '-') {
        value.type = TomlValue::Type::Integer;
        value.int_value = parseInt();
    }
    // 数组: [...]
    else if (peek() == '[') {
        value.type = TomlValue::Type::Array;
        value.array_value = parseArray();
    }
    
    // 跳过行尾
    while (!isAtEnd() && peek() != '\n') {
        if (peek() == '#') {
            skipComment();
            break;
        }
        if (!std::isspace(peek())) break;
        advance();
    }
    
    return value;
}

std::string TomlParser::parseString() {
    advance(); // skip opening "
    std::string str;
    while (!isAtEnd() && peek() != '"') {
        str += advance();
    }
    if (peek() == '"') advance(); // skip closing "
    return str;
}

int64_t TomlParser::parseInt() {
    std::string num_str;
    if (peek() == '-') num_str += advance();
    while (!isAtEnd() && std::isdigit(peek())) {
        num_str += advance();
    }
    return std::stoll(num_str);
}

bool TomlParser::parseBool() {
    std::string bool_str;
    while (!isAtEnd() && std::isalpha(peek())) {
        bool_str += advance();
    }
    return bool_str == "true";
}

std::vector<TomlValue> TomlParser::parseArray() {
    advance(); // skip [
    std::vector<TomlValue> arr;
    skipWhitespace();
    
    while (!isAtEnd() && peek() != ']') {
        arr.push_back(parseValue());
        skipWhitespace();
        if (peek() == ',') {
            advance();
            skipWhitespace();
        }
    }
    
    if (peek() == ']') advance(); // skip ]
    return arr;
}

std::map<std::string, TomlValue> TomlParser::parseTable() {
    // TODO: 实现嵌套表
    return {};
}

// 加载paw.toml文件
PawConfig loadPawConfig(const std::string& project_dir) {
    std::string toml_path = project_dir + "/paw.toml";
    
    std::ifstream file(toml_path);
    if (!file.is_open()) {
        // 返回默认配置
        PawConfig config;
        config.name = "unnamed";
        config.version = "0.1.0";
        config.lib_type = "bin";
        config.target = "native";
        config.opt_level = 2;
        config.debug = false;
        return config;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    TomlParser parser(content);
    if (!parser.parse()) {
        // 解析失败，返回默认配置
        PawConfig config;
        config.name = "unnamed";
        config.version = "0.1.0";
        return config;
    }
    
    return parser.getConfig();
}

} // namespace pawc

