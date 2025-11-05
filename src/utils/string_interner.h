//===--- string_interner.h - String Interning -------------------*- C++ -*-===//

#ifndef PAW_STRING_INTERNER_H
#define PAW_STRING_INTERNER_H

#include <string>
#include <unordered_set>

namespace pawc {

/// StringInterner - string interning, optimize string comparison
class StringInterner {
public:
    const std::string* intern(const std::string& str) {
        auto [it, inserted] = pool_.insert(str);
        return &(*it);
    }
    
private:
    std::unordered_set<std::string> pool_;
};

} // namespace pawc

#endif // PAW_STRING_INTERNER_H
