#include <iostream>
#include <vector>
#include "src/lexer/lexer.h"

int main() {
    std::string source = R"(f"Test 2: F-string without interpolation")";
    pawc::Lexer lexer(source, "test.paw", nullptr);
    auto tokens = lexer.tokenize();
    
    for (const auto& tok : tokens) {
        std::cout << "Type: " << static_cast<int>(tok.type) 
                  << ", Value: [" << tok.value << "]" << std::endl;
    }
    
    return 0;
}

