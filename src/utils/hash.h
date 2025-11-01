//===--- hash.h - Hash Utilities --------------------------------*- C++ -*-===//

#ifndef PAW_HASH_H
#define PAW_HASH_H

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace pawc {

/// Hash - SipHash实现
class Hash {
public:
    static uint64_t hash64(const void* data, size_t len) {
        // 简化的哈希实现（生产环境应使用SipHash）
        uint64_t hash = 0xcbf29ce484222325ULL; // FNV offset basis
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        
        for (size_t i = 0; i < len; ++i) {
            hash ^= bytes[i];
            hash *= 0x100000001b3ULL; // FNV prime
        }
        
        return hash;
    }
    
    static uint64_t hashForIntern(const char* str) {
        return hash64(str, std::strlen(str));
    }
};

} // namespace pawc

#endif // PAW_HASH_H
