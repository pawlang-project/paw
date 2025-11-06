//===--- builtin_runtime.cpp - Runtime Implementation ------------*- C++ -*-===//
/// @file builtin_runtime.cpp
/// @brief Implementation file

#include "builtin_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <mutex>

extern "C" {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// atexit implementation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Maximum number of atexit handlers (C standard requires at least 32)
#define ATEXIT_MAX_HANDLERS 128

// Static storage for atexit handlers
static std::vector<void (*)(void)> atexit_handlers;
static std::mutex atexit_mutex;
static bool atexit_initialized = false;

// Initialize atexit system
static void init_atexit() {
    if (!atexit_initialized) {
        atexit_handlers.reserve(ATEXIT_MAX_HANDLERS);
        atexit_initialized = true;
    }
}

// Register a function to be called on program exit
// Functions are called in reverse order of registration (LIFO)
// Note: This is only used when linking user programs with /ENTRY:main
// When compiling the compiler itself, MinGW's C runtime provides atexit
#ifdef __GNUC__
__attribute__((weak))
#endif
int atexit(void (*func)(void)) {
    if (!func) {
        return 1;  // Invalid function pointer
    }
    
    std::lock_guard<std::mutex> lock(atexit_mutex);
    init_atexit();
    
    if (atexit_handlers.size() >= ATEXIT_MAX_HANDLERS) {
        return 1;  // Too many handlers
    }
    
    atexit_handlers.push_back(func);
    return 0;  // Success
}

// Call all registered atexit handlers (in reverse order)
// This should be called before main returns
void paw_call_atexit_handlers() {
    std::lock_guard<std::mutex> lock(atexit_mutex);
    
    // Call handlers in reverse order (LIFO)
    for (auto it = atexit_handlers.rbegin(); it != atexit_handlers.rend(); ++it) {
        if (*it) {
            (*it)();
        }
    }
    
    // Clear handlers after calling
    atexit_handlers.clear();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Windows/MinGW low-level function stubs
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#ifdef _WIN32
// ___chkstk_ms - Stack checking function used by MinGW
// This is called by functions that allocate large stack frames
// We provide a minimal implementation that does nothing
// (stack checking is not critical for basic functionality)
extern "C" void ___chkstk_ms(void) {
    // Minimal stub - do nothing
    // In a full implementation, this would check stack overflow
}
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Memory Management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void* paw_malloc(size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        paw_panic("Out of memory");
    }
    return ptr;
}

void paw_free(void* ptr) {
    std::free(ptr);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Safety Checks
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_bounds_check(size_t index, size_t length) {
    if (index >= length) {
        paw_panic("Index out of bounds");
    }
}

// String comparison (used for match expression)
extern "C" int32_t paw_strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }
    return strcmp(s1, s2);
}

void paw_null_check(void* ptr) {
    if (!ptr) {
        paw_panic("Null pointer dereference");
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Panic and Error Handling
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_panic(const char* message) {
    std::fprintf(stderr, "\n\033[1;31mPANIC\033[0m: %s\n", message);
    std::abort();
}

void paw_unreachable(const char* file, int line) {
    std::fprintf(stderr, "\n\033[1;31mUNREACHABLE CODE\033[0m reached at %s:%d\n", file, line);
    std::abort();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Debug and Assertions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_assert(int condition, const char* message, const char* file, int line) {
    if (!condition) {
        std::fprintf(stderr, "\n\033[1;31mASSERTION FAILED\033[0m at %s:%d\n", file, line);
        std::fprintf(stderr, "Message: %s\n", message);
        std::abort();
    }
}

void paw_debug_assert(int condition, const char* message, const char* file, int line) {
#ifndef NDEBUG
    paw_assert(condition, message, file, line);
#else
    (void)condition;
    (void)message;
    (void)file;
    (void)line;
#endif
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Print Functions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_print_i8(int8_t value) { std::printf("%d", value); }
void paw_print_i16(int16_t value) { std::printf("%d", value); }
void paw_print_i32(int32_t value) { std::printf("%d", value); }
void paw_print_i64(int64_t value) { std::printf("%lld", (long long)value); }
void paw_print_i128(__int128 value) {
    // completeof/thei128printimplementation
    char buf[64];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    if (value == 0) {
        std::printf("0");
        return;
    }
    
    bool negative = value < 0;
    // Process negative numbers when converting to unsigned to avoid overflow
    unsigned __int128 v = negative ? (unsigned __int128)(-value) : (unsigned __int128)value;
    
    // Fill digits from back to front
    while (v > 0) {
        *--p = '0' + (v % 10);
        v /= 10;
    }
    
    if (negative) {
        *--p = '-';
    }
    
    std::printf("%s", p);
}

void paw_print_u8(uint8_t value) { std::printf("%u", value); }
void paw_print_u16(uint16_t value) { std::printf("%u", value); }
void paw_print_u32(uint32_t value) { std::printf("%u", value); }
void paw_print_u64(uint64_t value) { std::printf("%llu", (unsigned long long)value); }
void paw_print_u128(unsigned __int128 value) {
    char buf[64];
    int i = 63;
    buf[i] = '\0';
    if (value == 0) {
        std::printf("0");
        return;
    }
    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }
    std::printf("%s", &buf[i]);
}

// f8 (bfloat16): 1+8+7 bit format
// bfloat16 precision: ~2.4 decimal digits
void paw_print_f8(bfloat_t value) {
    // bfloat16 converts directly to float (maintains precision)
    float f_value = (float)value;
    std::printf("%.3g", f_value);  // 3 significant digits (bfloat16 precision)sion）
}

// f16 (IEEE 754 half): 1+5+10 bit format
// Half precision: ~3.31 decimal digits
void paw_print_f16(half_t value) {
    // IEEE half converts to float (maintains precision)
    float f_value = (float)value;
    std::printf("%.4g", f_value);  // 4 significant digits (half precision)）
}

void paw_print_f32(float value) { std::printf("%g", value); }
void paw_print_f64(double value) { std::printf("%.15g", value); }  // 15 digitse); }  // 15bit/digitprecision

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Software f128 print implementation - high performance optimizable
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 256-bit big integer (used for conversion)
struct big_int256 {
    uint64_t words[4];
};

// IEEE 754 fp128parse
static void parse_f128_bits(struct paw_f128_data bits,
                              bool* sign, uint16_t* exponent,
                              uint64_t* mant_high, uint64_t* mant_low) {
    *sign = (bits.high >> 63) & 1;
    *exponent = (bits.high >> 48) & 0x7FFF;
    *mant_high = bits.high & 0xFFFFFFFFFFFFULL;
    *mant_low = bits.low;
}

// Check special value
static int check_f128_special(bool sign, uint16_t exp,
                                uint64_t mh, uint64_t ml, char* buf) {
    if (exp == 0x7FFF && mh == 0 && ml == 0) {
        std::snprintf(buf, 64, "%s", sign ? "-inf" : "inf");
        return 1;
    }
    if (exp == 0x7FFF) {
        std::snprintf(buf, 64, "nan");
        return 1;
    }
    if (exp == 0 && mh == 0 && ml == 0) {
        std::snprintf(buf, 64, "%s0", sign ? "-" : "");
        return 1;
    }
    return 0;
}

// Big integer operations (optimized version)
static inline bool big_int256_is_zero(const struct big_int256* v) {
    return v->words[0] == 0 && v->words[1] == 0 && 
           v->words[2] == 0 && v->words[3] == 0;
}

static uint64_t big_int256_div_10(struct big_int256* v) {
    uint64_t rem = 0;
    for (int i = 3; i >= 0; i--) {
        uint64_t div_h = (rem << 32) | (v->words[i] >> 32);
        uint64_t quo_h = div_h / 10;
        rem = div_h % 10;
        
        uint64_t div_l = (rem << 32) | (v->words[i] & 0xFFFFFFFFULL);
        uint64_t quo_l = div_l / 10;
        rem = div_l % 10;
        
        v->words[i] = (quo_h << 32) | quo_l;
    }
    return rem;
}

// declaresoftware_f128.cppinconvertfunction
extern "C" void paw_f128_to_string_decimal(uint64_t low, uint64_t high, char* buffer, size_t buf_size);

void paw_print_f128(struct paw_f128_data value) {
    char buffer[128];
    paw_f128_to_string_decimal(value.low, value.high, buffer, sizeof(buffer));
    std::printf("%s", buffer);
}

void paw_print_bool(int value) { std::printf("%s", value ? "true" : "false"); }
void paw_print_char(char value) { std::printf("%c", value); }
void paw_print_string(const char* value) { std::printf("%s", value); }
void paw_print_newline() { std::printf("\n"); }

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// to_string Functions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

char* paw_i32_to_string(int32_t value) {
    char* buf = (char*)paw_malloc(32);
    std::snprintf(buf, 32, "%d", value);
    return buf;
}

char* paw_i64_to_string(int64_t value) {
    char* buf = (char*)paw_malloc(32);
    std::snprintf(buf, 32, "%lld", (long long)value);
    return buf;
}

char* paw_f64_to_string(double value) {
    char* buf = (char*)paw_malloc(32);
    std::snprintf(buf, 32, "%g", value);
    return buf;
}

char* paw_bool_to_string(int value) {
    const char* str = value ? "true" : "false";
    size_t len = std::strlen(str);
    char* buf = (char*)paw_malloc(len + 1);
    std::strcpy(buf, str);
    return buf;
}

// Other type to_string implementations omitted...

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Length Operations (supportAllcaniterationtypes)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

size_t paw_string_len(const char* str) {
    return std::strlen(str);
}

size_t paw_array_len(void* array) {
    // Arraylengthyescompile timeconstant，actualup/abovenotneedruntimefunction
    // Placeholder only, actual implementation will be inlined by compiler directlysubstitutionis/asconstant
    return 0;
}

size_t paw_slice_len(void* slice) {
    // Slicestruct: { ptr: *T, len: u64 }
    // Read len field (second field)
    struct Slice {
        void* ptr;
        size_t len;
    };
    Slice* s = (Slice*)slice;
    return s->len;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// String Operations
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

char* paw_string_concat(const char* a, const char* b) {
    size_t len_a = std::strlen(a);
    size_t len_b = std::strlen(b);
    char* results = (char*)paw_malloc(len_a + len_b + 1);
    std::strcpy(results, a);
    std::strcat(results, b);
    return results;
}

} // extern "C"
