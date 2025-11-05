//===--- arena.h - Arena Allocator --------------------------------*- C++ -*-===//
/// @file arena.h
/// @brief Compiler infrastructure
///

#ifndef PAW_ARENA_H
#define PAW_ARENA_H

#include <vector>
#include <memory>
#include <cstddef>

namespace pawc {

class Arena {
    static constexpr size_t BLOCK_SIZE = 64 * 1024;
    
    std::vector<std::unique_ptr<char[]>> blocks_;
    size_t current_offset_ = 0;
    
public:
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        void* mem = allocateRaw(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    void* allocateRaw(size_t size, size_t alignment) {
        // Align current offset
        size_t offset = (current_offset_ + alignment - 1) & ~(alignment - 1);
        
        if (blocks_.empty() || offset + size > BLOCK_SIZE) {
            blocks_.push_back(std::make_unique<char[]>(BLOCK_SIZE));
            current_offset_ = 0;
            offset = 0;
        }
        
        void* result = &blocks_.back()[offset];
        current_offset_ = offset + size;
        return result;
    }
    
    void reset() {
        current_offset_ = 0;
        // Keep blocks for reuse
    }
};

} // namespace pawc

#endif // PAW_ARENA_H
