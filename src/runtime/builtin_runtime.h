//===--- builtin_runtime.h - Builtin Runtime Functions -----------*- C++ -*-===//
/// @file builtin_runtime.h
/// @brief Built-in functions and runtime support
///
//
// PawLang Compiler - Runtime Library (No GC)
//
//===----------------------------------------------------------------------===//

#ifndef PAW_BUILTIN_RUNTIME_H
#define PAW_BUILTIN_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Exit Handlers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// C standard atexit function (MSVC CRT provides this, so we only declare it for non-MSVC)
#ifndef _MSC_VER
int atexit(void (*func)(void));
#endif

// Call all registered atexit handlers (called automatically before main returns)
void paw_call_atexit_handlers(void);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Windows/MinGW low-level functions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#ifdef _WIN32
// Stack checking function (used by MinGW)
void ___chkstk_ms(void);
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Memory Management (No GC)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void* paw_malloc(size_t size);
void paw_free(void* ptr);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Safety Checks
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_bounds_check(size_t index, size_t length);
int32_t paw_strcmp(const char* s1, const char* s2);
void paw_null_check(void* ptr);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Panic and Error Handling
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_panic(const char* message);
void paw_unreachable(const char* file, int line);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Debug and Assertions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void paw_assert(int condition, const char* message, const char* file, int line);
void paw_debug_assert(int condition, const char* message, const char* file, int line);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// I/O Functions (18 type overloads)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// MSVC doesn't support __int128, use struct for i128/u128
#ifdef _MSC_VER
struct paw_i128 {
    uint64_t low;
    int64_t high;
};
struct paw_u128 {
    uint64_t low;
    uint64_t high;
};
#endif

// Signed integers (5)
void paw_print_i8(int8_t value);
void paw_print_i16(int16_t value);
void paw_print_i32(int32_t value);
void paw_print_i64(int64_t value);
#ifndef _MSC_VER
void paw_print_i128(__int128 value);
#else
void paw_print_i128(struct paw_i128 value);
#endif

// Unsigned integers (5)
void paw_print_u8(uint8_t value);
void paw_print_u16(uint16_t value);
void paw_print_u32(uint32_t value);
void paw_print_u64(uint64_t value);
#ifndef _MSC_VER
void paw_print_u128(unsigned __int128 value);
#else
void paw_print_u128(struct paw_u128 value);
#endif

// Floating-point numbers (5) - completeprecision
// Note：in/atLLVMmiddle/center，f8/f16usespecialof/thefloating-pointtypes（bfloat/half），needconvert
#ifdef __clang__
typedef _Float16 half_t;
typedef __bf16 bfloat_t;
#else
typedef uint16_t half_t;
typedef uint16_t bfloat_t;
#endif

void paw_print_f8(bfloat_t value);     // bfloat16
void paw_print_f16(half_t value);      // half
void paw_print_f32(float value);
void paw_print_f64(double value);

// f128: software implementation (100% cross-platform)
// through/viastructpassfp128data（avoidABImismatch）
struct paw_f128_data {
    uint64_t low;   // Low 64 bits
    uint64_t high;  // High 64 bits
};

void paw_print_f128(struct paw_f128_data value);

// Other types (3)
void paw_print_bool(int value);
void paw_print_char(char value);
void paw_print_string(const char* value);
void paw_print_newline(void);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// to_string Functions (18 types)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

char* paw_i8_to_string(int8_t value);
char* paw_i16_to_string(int16_t value);
char* paw_i32_to_string(int32_t value);
char* paw_i64_to_string(int64_t value);
#ifndef _MSC_VER
char* paw_i128_to_string(__int128 value);
#else
char* paw_i128_to_string(struct paw_i128 value);
#endif

char* paw_u8_to_string(uint8_t value);
char* paw_u16_to_string(uint16_t value);
char* paw_u32_to_string(uint32_t value);
char* paw_u64_to_string(uint64_t value);
#ifndef _MSC_VER
char* paw_u128_to_string(unsigned __int128 value);
#else
char* paw_u128_to_string(struct paw_u128 value);
#endif

char* paw_f8_to_string(uint8_t value);
char* paw_f16_to_string(uint16_t value);
char* paw_f32_to_string(float value);
char* paw_f64_to_string(double value);
char* paw_f128_to_string(long double value);

char* paw_bool_to_string(int value);
char* paw_char_to_string(char value);
char* paw_string_to_string(const char* value);

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Length Operations (supportAllcaniterationtypes)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// lenfunction - according totypesreturnlength
size_t paw_string_len(const char* str);    // stringlength
size_t paw_array_len(void* array);         // Array length (placeholder)ompile timeconstant，hereplaceholder)
size_t paw_slice_len(void* slice);         // slicelength (runtime/whenget)

// String Operations
char* paw_string_concat(const char* a, const char* b);

#ifdef __cplusplus
}
#endif

#endif // PAW_BUILTIN_RUNTIME_H
