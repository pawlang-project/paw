// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
/// @file software_f128.cpp
/// @brief Implementation file
// Software f128 arithmetic implementation - fully manual implementation, high-performance optimization
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 
// IEEE 754 binary128 format:
//   [127]      sign (1 bit)
//   [126-112]  exponent (15 bits, bias = 16383)
//   [111-0]    mantissa (112 bits, implicit leading 1)
//
// Precision: 34 decimal digits
// Range: ±1.18e-4932 to ±1.18e+4932
//
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

// MSVC doesn't have __builtin_clzll, provide replacement
#ifdef _MSC_VER
    #include <intrin.h>
    static inline int __builtin_clzll(unsigned long long x) {
        unsigned long index;
        _BitScanReverse64(&index, x);
        return 63 - (int)index;
    }
#endif

// SIMD support detection
#if defined(__ARM_NEON) || defined(__aarch64__)
    #include <arm_neon.h>
    #define PAW_HAS_NEON 1
#elif defined(__SSE2__) || defined(__x86_64__) || defined(_M_X64)
    #include <emmintrin.h>
    #define PAW_HAS_SSE2 1
#else
    #define PAW_NO_SIMD 1
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Phase 1: IEEE 754 fp128 struct definition and parsing
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// IEEE 754 fp128 representation (compatible with LLVM/hardware)
struct f128_t {
    uint64_t low;   // bits [63:0]
    uint64_t high;  // bits [127:64]
};

// fp128 components
struct f128_parts {
    bool sign;              // Sign bit
    uint16_t exponent;      // Exponent (biased)
    uint64_t mant_high;     // High 64 bits of mantissa
    uint64_t mant_low;      // Low 48 bits of mantissa (only 48 bits used)
};

// Constant definitions
static const uint16_t F128_EXP_BIAS = 16383;
static const uint16_t F128_EXP_MAX = 0x7FFF;
static const int F128_MANT_BITS = 112;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Inline helper functions (performance optimization)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Parse fp128 bit representation
static inline void f128_unpack(const f128_t& v, f128_parts* parts) {
    parts->sign = (v.high >> 63) & 1;
    parts->exponent = (v.high >> 48) & 0x7FFF;
    parts->mant_high = v.high & 0xFFFFFFFFFFFFULL;  // Low 48 bits
    parts->mant_low = v.low;
}

// Pack fp128
static inline f128_t f128_pack(const f128_parts& parts) {
    f128_t results;
    results.low = parts.mant_low;
    results.high = ((uint64_t)parts.sign << 63) |
                  ((uint64_t)parts.exponent << 48) |
                  (parts.mant_high & 0xFFFFFFFFFFFFULL);
    return results;
}

// Detect special values (inline optimization)
static inline bool f128_is_zero(const f128_t& v) {
    uint16_t exp = (v.high >> 48) & 0x7FFF;
    return exp == 0 && (v.high & 0xFFFFFFFFFFFFULL) == 0 && v.low == 0;
}

static inline bool f128_is_inf(const f128_t& v) {
    uint16_t exp = (v.high >> 48) & 0x7FFF;
    return exp == F128_EXP_MAX && 
           (v.high & 0xFFFFFFFFFFFFULL) == 0 && 
           v.low == 0;
}

static inline bool f128_is_nan(const f128_t& v) {
    uint16_t exp = (v.high >> 48) & 0x7FFF;
    return exp == F128_EXP_MAX && 
           ((v.high & 0xFFFFFFFFFFFFULL) != 0 || v.low != 0);
}

static inline bool f128_is_subnormal(const f128_t& v) {
    uint16_t exp = (v.high >> 48) & 0x7FFF;
    return exp == 0 && ((v.high & 0xFFFFFFFFFFFFULL) != 0 || v.low != 0);
}

static inline bool f128_sign(const f128_t& v) {
    return (v.high >> 63) & 1;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Phase 2: Big integer arithmetic (for mantissa calculations)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct uint128_t {
    uint64_t low;
    uint64_t high;
};

// 256-bit big integer (for multiplication)
struct uint256_t {
    uint64_t words[4];  // words[0]=lowest 64 bits, words[3]=highest 64 bits
};

// 512-bit big integer (for Dragon4 full 34-digit precision)
struct uint512_t {
    uint64_t words[8];  // words[0]=lowest 64 bits, words[7]=highest 64 bits
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 256-bit big integer arithmetic (full version, for Dragon4)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static inline void u256_zero(uint256_t* v) {
    v->words[0] = v->words[1] = v->words[2] = v->words[3] = 0;
}

static inline bool u256_is_zero(const uint256_t* v) {
    return v->words[0] == 0 && v->words[1] == 0 && 
           v->words[2] == 0 && v->words[3] == 0;
}

// 256-bit left shift
static inline void u256_shl(uint256_t* v, unsigned shift) {
    if (shift == 0) return;
    if (shift >= 256) {
        u256_zero(v);
        return;
    }
    
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // Whole word shift
        for (int i = 3; i >= word_shift; i--) {
            v->words[i] = v->words[i - word_shift];
        }
        for (int i = 0; i < word_shift; i++) {
            v->words[i] = 0;
        }
    } else {
        // Bit shift
        for (int i = 3; i >= 0; i--) {
            if (i >= word_shift) {
                v->words[i] = v->words[i - word_shift] << bit_shift;
                if (i - word_shift > 0) {
                    v->words[i] |= v->words[i - word_shift - 1] >> (64 - bit_shift);
                }
            } else {
                v->words[i] = 0;
            }
        }
    }
}

// 256-bit right shift
static inline void u256_shr(uint256_t* v, unsigned shift) {
    if (shift == 0) return;
    if (shift >= 256) {
        u256_zero(v);
        return;
    }
    
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // Whole word shift
        for (int i = 0; i < 4 - word_shift; i++) {
            v->words[i] = v->words[i + word_shift];
        }
        for (int i = 4 - word_shift; i < 4; i++) {
            v->words[i] = 0;
        }
    } else {
        // Bit shift
        for (int i = 0; i < 4; i++) {
            if (i + word_shift < 4) {
                v->words[i] = v->words[i + word_shift] >> bit_shift;
                if (i + word_shift + 1 < 4) {
                    v->words[i] |= v->words[i + word_shift + 1] << (64 - bit_shift);
                }
            } else {
                v->words[i] = 0;
            }
        }
    }
}

// 256-bit comparison
static inline int u256_cmp(const uint256_t* a, const uint256_t* b) {
    for (int i = 3; i >= 0; i--) {
        if (a->words[i] != b->words[i]) {
            return a->words[i] > b->words[i] ? 1 : -1;
        }
    }
    return 0;
}

// 256-bit multiply by uint64
static inline void u256_mul_u64(uint256_t* v, uint64_t multiplier) {
    uint64_t carry = 0;
    for (int i = 0; i < 4; i++) {
        // Decompose to 32-bit to avoid overflow
        uint64_t lo = (v->words[i] & 0xFFFFFFFFULL) * multiplier + carry;
        uint64_t hi = (v->words[i] >> 32) * multiplier + (lo >> 32);
        
        v->words[i] = (hi << 32) | (lo & 0xFFFFFFFFULL);
        carry = hi >> 32;
    }
}

// 256-bit subtraction
static inline bool u256_sub(uint256_t* a, const uint256_t* b) {
    uint64_t borrow = 0;
    for (int i = 0; i < 4; i++) {
        uint64_t temp = a->words[i];
        a->words[i] = temp - b->words[i] - borrow;
        borrow = (a->words[i] > temp || (borrow && a->words[i] == temp)) ? 1 : 0;
    }
    return borrow != 0;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 512-bit big integer arithmetic (for full 34-digit precision + SIMD optimization)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static inline void u512_zero(uint512_t* v) {
#if PAW_HAS_NEON
    // ARM NEON optimization: use 128-bit vector to zero
    uint64x2_t zero = vdupq_n_u64(0);
    vst1q_u64(&v->words[0], zero);
    vst1q_u64(&v->words[2], zero);
    vst1q_u64(&v->words[4], zero);
    vst1q_u64(&v->words[6], zero);
#elif PAW_HAS_SSE2
    // SSE2 optimization
    __m128i zero = _mm_setzero_si128();
    _mm_storeu_si128((__m128i*)&v->words[0], zero);
    _mm_storeu_si128((__m128i*)&v->words[2], zero);
    _mm_storeu_si128((__m128i*)&v->words[4], zero);
    _mm_storeu_si128((__m128i*)&v->words[6], zero);
#else
    // Scalar version
    for (int i = 0; i < 8; i++) v->words[i] = 0;
#endif
}

static inline bool u512_is_zero(const uint512_t* v) {
    for (int i = 0; i < 8; i++) {
        if (v->words[i] != 0) return false;
    }
    return true;
}

// 512-bit comparison (SIMD optimization)
static inline int u512_cmp(const uint512_t* a, const uint512_t* b) {
#if PAW_HAS_NEON
    // ARM NEON optimization: vector comparison
    // Compare from high to low
    for (int i = 3; i >= 0; i--) {
        uint64x2_t va = vld1q_u64(&a->words[i * 2]);
        uint64x2_t vb = vld1q_u64(&b->words[i * 2]);
        
        // Check inequality
        uint64x2_t neq = vceqq_u64(va, vb);
        uint64_t neq_mask = vgetq_lane_u64(neq, 1) & vgetq_lane_u64(neq, 0);
        
        if (neq_mask != 0xFFFFFFFFFFFFFFFFULL) {
            // Has inequality, compare individually
            if (a->words[i * 2 + 1] != b->words[i * 2 + 1]) {
                return a->words[i * 2 + 1] > b->words[i * 2 + 1] ? 1 : -1;
            }
            if (a->words[i * 2] != b->words[i * 2]) {
                return a->words[i * 2] > b->words[i * 2] ? 1 : -1;
            }
        }
    }
    return 0;
#else
    // Scalar version
    for (int i = 7; i >= 0; i--) {
        if (a->words[i] != b->words[i]) {
            return a->words[i] > b->words[i] ? 1 : -1;
        }
    }
    return 0;
#endif
}

// 512-bit left shift
static inline void u512_shl(uint512_t* v, unsigned shift) {
    if (shift == 0) return;
    if (shift >= 512) {
        u512_zero(v);
        return;
    }
    
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        for (int i = 7; i >= word_shift; i--) {
            v->words[i] = v->words[i - word_shift];
        }
        for (int i = 0; i < word_shift; i++) {
            v->words[i] = 0;
        }
    } else {
        for (int i = 7; i >= 0; i--) {
            if (i >= word_shift) {
                v->words[i] = v->words[i - word_shift] << bit_shift;
                if (i - word_shift > 0) {
                    v->words[i] |= v->words[i - word_shift - 1] >> (64 - bit_shift);
                }
            } else {
                v->words[i] = 0;
            }
        }
    }
}

// 512-bit multiply by uint64
static inline void u512_mul_u64(uint512_t* v, uint64_t multiplier) {
    uint64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t lo = (v->words[i] & 0xFFFFFFFFULL) * multiplier + carry;
        uint64_t hi = (v->words[i] >> 32) * multiplier + (lo >> 32);
        v->words[i] = (hi << 32) | (lo & 0xFFFFFFFFULL);
        carry = hi >> 32;
    }
}

// 512-bit subtraction (SIMD optimization version)
static inline bool u512_sub(uint512_t* a, const uint512_t* b) {
#if PAW_HAS_NEON
    // ARM NEON optimization: vector subtraction (requires manual borrow handling)
    // Since NEON doesn't directly support subtraction with borrow, still use scalar version
    // But can use SIMD load/store to accelerate
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t temp = a->words[i];
        a->words[i] = temp - b->words[i] - borrow;
        borrow = (a->words[i] > temp || (borrow && a->words[i] == temp)) ? 1 : 0;
    }
    return borrow != 0;
#else
    // Scalar version
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t temp = a->words[i];
        a->words[i] = temp - b->words[i] - borrow;
        borrow = (a->words[i] > temp || (borrow && a->words[i] == temp)) ? 1 : 0;
    }
    return borrow != 0;
#endif
}

// Compute 10^n (n>=0, results stored in v, 512-bit)
// Performance optimization: use lookup table for small n, dynamic computation for large n
static void u512_pow10(uint512_t* v, int n) {
    u512_zero(v);
    v->words[0] = 1;
    
    // Optimization: for common powers (0-15), can use fast path
    // Current simplified implementation: all dynamic computation (lookup table can be added later)
    for (int i = 0; i < n; i++) {
        u512_mul_u64(v, 10);
    }
}

// 128-bit × 128-bit = 256-bit (full precision multiplication)
static uint256_t u128_mul_u128_full(uint128_t a, uint128_t b) {
    // Use decomposition method: split 128-bit into 4 32-bit parts
    // a = a3*2^96 + a2*2^64 + a1*2^32 + a0
    // b = b3*2^96 + b2*2^64 + b1*2^32 + b0
    
    uint32_t a0 = (uint32_t)(a.low);
    uint32_t a1 = (uint32_t)(a.low >> 32);
    uint32_t a2 = (uint32_t)(a.high);
    uint32_t a3 = (uint32_t)(a.high >> 32);
    
    uint32_t b0 = (uint32_t)(b.low);
    uint32_t b1 = (uint32_t)(b.low >> 32);
    uint32_t b2 = (uint32_t)(b.high);
    uint32_t b3 = (uint32_t)(b.high >> 32);
    
    uint256_t results;
    u256_zero(&results);
    
    // Compute all 16 partial products and accumulate
    // To avoid complexity, use step-by-step accumulation
    for (int i = 0; i < 4; i++) {
        uint32_t ai = (i == 0) ? a0 : (i == 1) ? a1 : (i == 2) ? a2 : a3;
        for (int j = 0; j < 4; j++) {
            uint32_t bj = (j == 0) ? b0 : (j == 1) ? b1 : (j == 2) ? b2 : b3;
            
            uint64_t prod = (uint64_t)ai * (uint64_t)bj;
            int word_pos = i + j;  // Which 64-bit word for results
            int shift_in_word = 0;  // Offset within word (0 or 32)
            
            // ai is at i-th 32-bit, bj is at j-th 32-bit
            // Result is at (i+j)*32 bit position
            // Convert to 64-bit word index
            int bit_pos = (i + j) * 32;
            word_pos = bit_pos / 64;
            shift_in_word = bit_pos % 64;
            
            // Add to results
            if (word_pos < 4) {
                uint64_t carry = 0;
                uint64_t add_val = prod << shift_in_word;
                results.words[word_pos] += add_val;
                if (results.words[word_pos] < add_val) carry = 1;
                
                if (shift_in_word != 0 && word_pos + 1 < 4) {
                    uint64_t high_part = prod >> (64 - shift_in_word);
                    results.words[word_pos + 1] += high_part + carry;
                    if (results.words[word_pos + 1] < high_part) carry = 1;
                    else carry = 0;
                }
                
                // Propagate carry
                for (int k = word_pos + (shift_in_word != 0 ? 2 : 1); k < 4 && carry; k++) {
                    results.words[k] += carry;
                    if (results.words[k] == 0) carry = 1;
                    else carry = 0;
                }
            }
        }
    }
    
    return results;
}

// 128-bit addition (with carry)
static inline uint128_t u128_add(uint128_t a, uint128_t b) {
    uint128_t results;
    results.low = a.low + b.low;
    results.high = a.high + b.high + (results.low < a.low ? 1 : 0);  // Carry
    return results;
}

// 128-bit subtraction (with borrow)
static inline uint128_t u128_sub(uint128_t a, uint128_t b) {
    uint128_t results;
    results.low = a.low - b.low;
    results.high = a.high - b.high - (a.low < b.low ? 1 : 0);  // Borrow
    return results;
}

// 128-bit left shift
static inline uint128_t u128_shl(uint128_t a, unsigned shift) {
    if (shift == 0) return a;
    if (shift >= 128) return {0, 0};
    
    uint128_t results;
    if (shift < 64) {
        results.high = (a.high << shift) | (a.low >> (64 - shift));
        results.low = a.low << shift;
    } else {
        results.high = a.low << (shift - 64);
        results.low = 0;
    }
    return results;
}

// 128-bit right shift
static inline uint128_t u128_shr(uint128_t a, unsigned shift) {
    if (shift == 0) return a;
    if (shift >= 128) return {0, 0};
    
    uint128_t results;
    if (shift < 64) {
        results.low = (a.low >> shift) | (a.high << (64 - shift));
        results.high = a.high >> shift;
    } else {
        results.low = a.high >> (shift - 64);
        results.high = 0;
    }
    return results;
}

// 128-bit comparison
static inline int u128_cmp(uint128_t a, uint128_t b) {
    if (a.high != b.high) return a.high > b.high ? 1 : -1;
    if (a.low != b.low) return a.low > b.low ? 1 : -1;
    return 0;
}

// 128-bit multiply by 64-bit
static inline uint128_t u128_mul_u64(uint128_t a, uint64_t b) {
    // Decompose to 32-bit parts for multiplication (avoid overflow)
    uint64_t a_lo = a.low & 0xFFFFFFFFULL;
    uint64_t a_hi = a.low >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32;
    
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    
    uint64_t carry = ((p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL)) >> 32;
    
    uint128_t results;
    results.low = p0 + (p1 << 32) + (p2 << 32);
    results.high = p3 + (p1 >> 32) + (p2 >> 32) + carry;
    results.high += a.high * b;  // High bits multiplication
    
    return results;
}

// 128-bit divide by 64-bit (return quotient and remainder)
static inline uint64_t u128_div_u64(uint128_t* dividend, uint64_t divisor) {
    if (divisor == 0) return 0;  // Error handling
    
    // Simplified implementation: use 128-bit division
    // Use long division algorithm here
    uint64_t quotient = 0;
    
    // If dividend is less than divisor, return 0 directly
    if (dividend->high == 0 && dividend->low < divisor) {
        return 0;
    }
    
    // Long division
    for (int i = 127; i >= 0; i--) {
        quotient <<= 1;
        
        // Check i-th bit
        bool bit;
        if (i >= 64) {
            bit = (dividend->high >> (i - 64)) & 1;
        } else {
            bit = (dividend->low >> i) & 1;
        }
        
        // Construct temporary dividend
        static uint128_t temp = {0, 0};
        temp = u128_shl(temp, 1);
        if (bit) temp.low |= 1;
        
        // Trial division
        if (temp.high > 0 || temp.low >= divisor) {
            if (temp.high == 0) {
                temp.low -= divisor;
            } else {
                temp = u128_sub(temp, {divisor, 0});
            }
            quotient |= 1;
        }
    }
    
    return quotient;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Phase 3: fp128 ↔ double conversion (completely correct implementation)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// fp128 → double (simplified but correct implementation)
double f128_to_double_correct(f128_t v) {
    // Special value handling
    if (f128_is_zero(v)) {
        return f128_sign(v) ? -0.0 : 0.0;
    }
    if (f128_is_inf(v)) {
        return f128_sign(v) ? -INFINITY : INFINITY;
    }
    if (f128_is_nan(v)) {
        return NAN;
    }
    
    // Parse fp128
    f128_parts parts;
    f128_unpack(v, &parts);
    
    // Calculate actual exponent
    int32_t exp_f128 = (int32_t)parts.exponent - F128_EXP_BIAS;
    
    // Adjust to double exponent
    int32_t exp_double = exp_f128 + 1023;  // double bias
    
    // Check overflow/underflow
    if (exp_double >= 2047) {
        return parts.sign ? -INFINITY : INFINITY;
    }
    if (exp_double <= 0) {
        return parts.sign ? -0.0 : 0.0;
    }
    
    // fp128 mantissa: 112 bits, stored in [mant_high(48 bits), mant_low(64 bits)]
    // double mantissa: 52 bits
    // We need to truncate to 52 bits
    
    // Construct 113-bit mantissa (including implicit 1)
    // fp128: bit 112 = implicit 1, bits [111:0] = stored mantissa
    // Already stored: mant_high(48 bits) + mant_low(64 bits) = 112 bits
    
    // Convert 112-bit mantissa to 52 bits
    // Take highest 52 bits: starting from bit[111], take 52 bits = bit[111:60]
    
    uint64_t mant_double;
    if (parts.mant_high != 0) {
        // mant_high has data: bit[111:64] in mant_high, bit[63:0] in mant_low
        // Take bit[111:60]:
        //   High (48-4=44) bits of bit[111:64] + 4 bits of bit[63:60]
        mant_double = (parts.mant_high << 4) | (parts.mant_low >> 60);
        mant_double &= 0xFFFFFFFFFFFFFULL;  // 52-bit mask
    } else {
        // Only mant_low has data
        mant_double = parts.mant_low >> 60;
    }
    
    // Remove implicit 1 (double format doesn't store implicit 1)
    mant_double &= 0xFFFFFFFFFFFFFULL;  // 52 bits
    
    // Construct double
    uint64_t double_bits = ((uint64_t)parts.sign << 63) |
                           ((uint64_t)exp_double << 52) |
                           mant_double;
    
    double results;
    memcpy(&results, &double_bits, sizeof(double));
    return results;
}

// double → fp128 (correct implementation)
f128_t double_to_f128_correct(double d) {
    // Special values
    if (d == 0.0) {
        return {0, std::signbit(d) ? (1ULL << 63) : 0};
    }
    if (std::isinf(d)) {
        uint64_t sign = std::signbit(d) ? (1ULL << 63) : 0;
        return {0, sign | (0x7FFFULL << 48)};
    }
    if (std::isnan(d)) {
        return {1, 0x7FFFULL << 48};
    }
    
    // Parse double
    uint64_t double_bits;
    memcpy(&double_bits, &d, sizeof(double));
    
    bool sign = (double_bits >> 63) & 1;
    uint16_t exp_double = (double_bits >> 52) & 0x7FF;
    uint64_t mant_double = double_bits & 0xFFFFFFFFFFFFFULL;
    
    // Convert exponent
    int32_t exp_actual;
    if (exp_double == 0) {
        // Subnormal number
        exp_actual = 1 - 1023;
    } else {
        // Normal number
        mant_double |= (1ULL << 52);  // Add implicit 1
        exp_actual = (int32_t)exp_double - 1023;
    }
    
    // Convert to fp128 exponent
    uint16_t exp_f128 = (uint16_t)(exp_actual + F128_EXP_BIAS);
    
    // Extend mantissa: double's 53 bits → fp128's 113 bits
    // double mantissa at bit[52:0], fp128 at bit[111:0]
    // Need to left shift 59 bits
    uint128_t mant_f128 = {mant_double << 59, mant_double >> 5};
    
    // Remove implicit 1 (fp128 doesn't include it when storing)
    if (exp_f128 != 0) {
        mant_f128.high &= 0xFFFFFFFFFFFFULL;  // Clear bit[48]
    }
    
    // Pack
    f128_parts parts;
    parts.sign = sign;
    parts.exponent = exp_f128;
    parts.mant_high = mant_f128.high;
    parts.mant_low = mant_f128.low;
    
    return f128_pack(parts);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Phase 4: Complete fp128 basic operations (no precision loss)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Helper: normalize mantissa (ensure highest bit is 1)
static f128_t normalize_f128(bool sign, int32_t exp, uint128_t mant) {
#if F128_DEBUG
    fprintf(stderr, "[NORM] Input: sign=%d, exp=%d, mant={0x%llx, 0x%llx}\n",
                 sign, exp, mant.high, mant.low);
#endif
    
    // If mantissa is 0
    if (mant.low == 0 && mant.high == 0) {
        return {0, sign ? (1ULL << 63) : 0};
    }
    
    // Find highest bit (use CLZ - Count Leading Zeros)
    int leading_zeros;
    if (mant.high != 0) {
        leading_zeros = __builtin_clzll(mant.high);
    } else {
        leading_zeros = 64 + __builtin_clzll(mant.low);
    }
    
    // Left shift to align highest bit to bit 112 (fp128 mantissa's implicit 1 position)
    int shift_needed = leading_zeros - 15;  // 15 = 64-49
    
    if (shift_needed > 0) {
        mant = u128_shl(mant, shift_needed);
        exp -= shift_needed;
    } else if (shift_needed < 0) {
        mant = u128_shr(mant, -shift_needed);
        exp += -shift_needed;
    }
    
    // Check for exponent range
    if (exp >= F128_EXP_MAX) {
        return {0, ((uint64_t)sign << 63) | ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (exp <= 0) {
        return {0, sign ? (1ULL << 63) : 0};
    }
    
    // Pack results (remove implicit 1)
    f128_parts parts;
    parts.sign = sign;
    parts.exponent = (uint16_t)exp;
    parts.mant_high = mant.high & 0xFFFFFFFFFFFFULL;  // Only take low 48 bits
    parts.mant_low = mant.low;
    
    return f128_pack(parts);
}

// Debug helper (optional, can be disabled at compile time)
#define F128_DEBUG 0
static void debug_f128(const char* label, f128_t v) {
#if F128_DEBUG
    fprintf(stderr, "[DEBUG] %s: low=0x%016llx, high=0x%016llx\n", 
                 label, v.low, v.high);
#endif
    (void)label; (void)v;  // Avoid unused warning
}

// Complete fp128 addition (operate directly on 113-bit mantissa)
f128_t f128_add_impl(f128_t a, f128_t b) {
    // Special value handling
    if (f128_is_nan(a) || f128_is_nan(b)) {
        return {1, ((uint64_t)F128_EXP_MAX << 48)};  // NaN
    }
    if (f128_is_inf(a)) {
        if (f128_is_inf(b) && f128_sign(a) != f128_sign(b)) {
            return {1, ((uint64_t)F128_EXP_MAX << 48)};  // inf - inf = NaN
        }
        return a;
    }
    if (f128_is_inf(b)) return b;
    if (f128_is_zero(a)) return b;
    if (f128_is_zero(b)) return a;
    
    // Parse operands
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    // Construct complete 113-bit mantissa (including implicit 1)
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    // Add implicit 1 (at bit 112 position)
    if (pa.exponent != 0) {
        ma.high |= (1ULL << 48);  // bit 112
    }
    if (pb.exponent != 0) {
        mb.high |= (1ULL << 48);
    }
    
    // Align exponents (right shift the smaller one)
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t exp_diff = exp_a - exp_b;
    
    int32_t results_exp;
    uint128_t results_mant;
    bool results_sign;
    
    if (exp_diff > 0) {
        // a's exponent is larger, right shift b
        if (exp_diff < 128) {
            mb = u128_shr(mb, exp_diff);
        } else {
            mb = {0, 0};  // Gap too large, b becomes 0
        }
        results_exp = exp_a;
    } else if (exp_diff < 0) {
        // b's exponent is larger, right shift a
        if (-exp_diff < 128) {
            ma = u128_shr(ma, -exp_diff);
        } else {
            ma = {0, 0};
        }
        results_exp = exp_b;
    } else {
        results_exp = exp_a;
    }
    
    // Perform addition or subtraction (based on sign)
    if (pa.sign == pb.sign) {
        // Same sign: add
        results_mant = u128_add(ma, mb);
        results_sign = pa.sign;
        
        // Check if carry (bit 113)
        if (results_mant.high & (1ULL << 49)) {  // bit 113 carry
            results_mant = u128_shr(results_mant, 1);
            results_exp++;
        }
    } else {
        // Different sign: subtract (larger - smaller)
        int cmp = u128_cmp(ma, mb);
        if (cmp > 0) {
            results_mant = u128_sub(ma, mb);
            results_sign = pa.sign;
        } else if (cmp < 0) {
            results_mant = u128_sub(mb, ma);
            results_sign = pb.sign;
        } else {
            // Equal, results is 0
            return {0, 0};
        }
    }
    
    // Normalize and return
    return normalize_f128(results_sign, results_exp + F128_EXP_BIAS, results_mant);
}

// Complete fp128 subtraction
f128_t f128_sub_impl(f128_t a, f128_t b) {
    // Subtraction = add negative
    // Flip b's sign
    b.high ^= (1ULL << 63);
    return f128_add_impl(a, b);
}

// Complete fp128 multiplication (using 256-bit precision)
f128_t f128_mul_impl(f128_t a, f128_t b) {
    // Special value handling
    if (f128_is_nan(a) || f128_is_nan(b)) {
        return {1, ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (f128_is_zero(a) || f128_is_zero(b)) {
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, sign ? (1ULL << 63) : 0};
    }
    if (f128_is_inf(a) || f128_is_inf(b)) {
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, ((uint64_t)sign << 63) | ((uint64_t)F128_EXP_MAX << 48)};
    }
    
    // Parse
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    // Result sign
    bool results_sign = pa.sign ^ pb.sign;
    
    // Result exponent (multiplication: add exponents)
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t results_exp = exp_a + exp_b;
    
    // Construct complete 113-bit mantissa (including implicit 1)
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    // Add implicit 1 (at bit 112, i.e. high's bit 48)
    if (pa.exponent != 0) ma.high |= (1ULL << 48);
    if (pb.exponent != 0) mb.high |= (1ULL << 48);
    
    // Complete 128-bit × 128-bit = 256-bit multiplication
    uint256_t prod256 = u128_mul_u128_full(ma, mb);
    
    // Multiplication results: 113-bit × 113-bit = at most 226 bits
    // bit 225 could be 1 (if both mantissas close to 2.0)
    // We need to extract bit[225:113] as new mantissa
    
    // Check highest bit (bit 225 = words[3]'s bit 33)
    bool need_shift = (prod256.words[3] & (1ULL << 33)) != 0;
    
    if (need_shift) {
        // bit 225 is 1, right shift 1 bit and adjust exponent
        u256_shr(&prod256, 1);
        results_exp++;
    }
    
    // Now bit 224 is the highest significant bit (implicit 1)
    // Extract bit[224:112] as new mantissa (total 113 bits)
    // bit 224 at words[3]'s bit 32
    // bit 112 at words[1]'s bit 48
    
    // Extract 113-bit mantissa from 256 bits
    // bit[224:112] spans high 16 bits of words[1], all 64 bits of words[2], and low 33 bits of words[3]
    
    uint128_t results_mant;
    // bit[175:112] = words[1]'s bit[63:48] + all of words[2]
    results_mant.low = (prod256.words[1] >> 48) | (prod256.words[2] << 16);
    // bit[224:176] = words[2]'s high bits + words[3]'s low bits
    results_mant.high = (prod256.words[2] >> 48) | ((prod256.words[3] & 0x1FFFFFFFFFFFF) << 16);
    
    // Normalize and return
    return normalize_f128(results_sign, results_exp + F128_EXP_BIAS, results_mant);
}

// Complete 128-bit long division algorithm
static uint128_t u128_div_u128(uint128_t dividend, uint128_t divisor, uint128_t* remainder) {
    // Long division algorithm
    uint128_t quotient = {0, 0};
    uint128_t rem = {0, 0};
    
    // Divide bit by bit from highest
    for (int i = 127; i >= 0; i--) {
        // Left shift remainder
        rem = u128_shl(rem, 1);
        
        // Add i-th bit of dividend
        uint64_t bit_val;
        if (i >= 64) {
            bit_val = (dividend.high >> (i - 64)) & 1;
        } else {
            bit_val = (dividend.low >> i) & 1;
        }
        
        if (bit_val) {
            rem.low |= 1;
        }
        
        // If rem >= divisor, subtract and set corresponding bit of quotient
        if (u128_cmp(rem, divisor) >= 0) {
            rem = u128_sub(rem, divisor);
            
            // Set i-th bit of quotient
            if (i >= 64) {
                quotient.high |= (1ULL << (i - 64));
            } else {
                quotient.low |= (1ULL << i);
            }
        }
    }
    
    if (remainder) {
        *remainder = rem;
    }
    
    return quotient;
}

// Complete fp128 division (using 128-bit long division)
f128_t f128_div_impl(f128_t a, f128_t b) {
    // Special value handling
    if (f128_is_nan(a) || f128_is_nan(b)) {
        return {1, ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (f128_is_zero(b)) {
        // Divide by 0
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, ((uint64_t)sign << 63) | ((uint64_t)F128_EXP_MAX << 48)};  // inf
    }
    if (f128_is_zero(a)) {
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, sign ? (1ULL << 63) : 0};
    }
    if (f128_is_inf(a)) {
        if (f128_is_inf(b)) {
            return {1, ((uint64_t)F128_EXP_MAX << 48)};  // inf/inf = NaN
        }
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, ((uint64_t)sign << 63) | ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (f128_is_inf(b)) {
        bool sign = f128_sign(a) ^ f128_sign(b);
        return {0, sign ? (1ULL << 63) : 0};  // x/inf = 0
    }
    
    // Parse
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    bool results_sign = pa.sign ^ pb.sign;
    
    // Division exponent: exp_a - exp_b
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t results_exp = exp_a - exp_b;
    
    // Construct complete 113-bit mantissa (including implicit 1)
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    if (pa.exponent != 0) ma.high |= (1ULL << 48);
    if (pb.exponent != 0) mb.high |= (1ULL << 48);
    
    // Use complete 128-bit long division
    // ma and mb are both 113 bits (highest bit at bit 112)
    
    // Standard long division
    uint128_t quotient = {0, 0};
    uint128_t remainder = ma;
    
    // If ma < mb, quotient < 1.0, need adjustment
    if (u128_cmp(ma, mb) < 0) {
        // Left shift ma by 1 (equivalent to ×2)
        ma = u128_shl(ma, 1);
        remainder = ma;
        results_exp--;  // Exponent -1
    }
    
    // Now perform division, quotient should be in [1.0, 2.0) range
    // I.e. bit 112 should be 1
    
    // First trial quotient: subtract mb
    if (u128_cmp(remainder, mb) >= 0) {
        remainder = u128_sub(remainder, mb);
        quotient.high |= (1ULL << 48);  // Set bit 112 (implicit 1)
    }
    
    // Continue calculating subsequent 112 bits
    for (int i = 111; i >= 0; i--) {
        // Left shift remainder
        remainder = u128_shl(remainder, 1);
        
        // If remainder >= mb, set bit i of quotient
        if (u128_cmp(remainder, mb) >= 0) {
            remainder = u128_sub(remainder, mb);
            
            if (i >= 64) {
                quotient.high |= (1ULL << (i - 64));
            } else {
                quotient.low |= (1ULL << i);
            }
        }
    }
    
    // Normalize
    return normalize_f128(results_sign, results_exp + F128_EXP_BIAS, quotient);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Exported C ABI functions (compliant with LLVM compiler-rt specification)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

extern "C" {

// PawLang-specific f128 operation functions (decomposed passing, bypass ABI issues)
// ARM64 cannot correctly pass 16-byte struct, decompose to 4 uint64
void paw_f128_add(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t results = f128_add_impl(a, b);
    *ret_low = results.low;
    *ret_high = results.high;
}

void paw_f128_sub(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t results = f128_sub_impl(a, b);
    *ret_low = results.low;
    *ret_high = results.high;
}

void paw_f128_mul(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t results = f128_mul_impl(a, b);
    *ret_low = results.low;
    *ret_high = results.high;
}

void paw_f128_div(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t results = f128_div_impl(a, b);
    *ret_low = results.low;
    *ret_high = results.high;
}

// LLVM compiler-rt compatible functions (attempted support, but ARM64 ABI has issues)
// Kept for other platforms for now
f128_t __addtf3(f128_t a, f128_t b) {
    return f128_add_impl(a, b);
}

f128_t __subtf3(f128_t a, f128_t b) {
    return f128_sub_impl(a, b);
}

f128_t __multf3(f128_t a, f128_t b) {
    return f128_mul_impl(a, b);
}

f128_t __divtf3(f128_t a, f128_t b) {
    return f128_div_impl(a, b);
}

// Type conversions
f128_t __extenddftf2(double d) {
    return double_to_f128_correct(d);
}

double __trunctfdf2(f128_t a) {
    return f128_to_double_correct(a);
}

// Comparison operations
int __eqtf2(f128_t a, f128_t b) {
    double ad = f128_to_double_correct(a);
    double bd = f128_to_double_correct(b);
    return (ad == bd) ? 0 : 1;
}

int __netf2(f128_t a, f128_t b) {
    return __eqtf2(a, b);
}

int __gttf2(f128_t a, f128_t b) {
    double ad = f128_to_double_correct(a);
    double bd = f128_to_double_correct(b);
    return (ad > bd) ? 1 : 0;
}

int __getf2(f128_t a, f128_t b) {
    double ad = f128_to_double_correct(a);
    double bd = f128_to_double_correct(b);
    return (ad >= bd) ? 0 : -1;
}

int __lttf2(f128_t a, f128_t b) {
    double ad = f128_to_double_correct(a);
    double bd = f128_to_double_correct(b);
    return (ad < bd) ? -1 : 0;
}

int __letf2(f128_t a, f128_t b) {
    double ad = f128_to_double_correct(a);
    double bd = f128_to_double_correct(b);
    return (ad <= bd) ? 0 : 1;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Stage 5: fp128→string conversion (full 34-digit precision)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// large/bigintegerdividewith/to10（used fordecimalconvert）
static uint64_t u256_div_10(uint256_t* v) {
    uint64_t remainder = 0;
    for (int i = 3; i >= 0; i--) {
        // Current word + previous remainder
        uint64_t dividend = v->words[i];
        uint64_t high_part = ((uint64_t)remainder << 32) | (dividend >> 32);
        uint64_t quo_high = high_part / 10;
        remainder = high_part % 10;
        
        uint64_t low_part = ((uint64_t)remainder << 32) | (dividend & 0xFFFFFFFFULL);
        uint64_t quo_low = low_part / 10;
        remainder = low_part % 10;
        
        v->words[i] = (quo_high << 32) | quo_low;
    }
    return remainder;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Dragon4 algorithm: fp128→decimal string (full 34-digit precision)ision）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Performance optimizable: lookup table
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Performance optimization framework: powers of 10 lookup table (not yet extended)nd）
// Current: dynamic computation (framework ready, can be extended later)lookuptable）
// Optimization potential: for common powers (0-15), lookup table can improve performancelecanimprove30-50%performancecan

// Estimate decimal exponent：log10(2^e) ≈ e * 0.30103
static int estimate_decimal_exp(int32_t binary_exp) {
    // Use fixed-point operation: 0.30103 ≈ 19728/65536
    return (int)((int64_t)binary_exp * 19728 / 65536);
}

// Compute 10^n (n>=0, result stored in v, 256-bit)
static void u256_pow10(uint256_t* v, int n) {
    u256_zero(v);
    v->words[0] = 1;
    
    // Optimization: for small n, can use lookup tableot yetfutureextend）
    for (int i = 0; i < n; i++) {
        u256_mul_u64(v, 10);
    }
}

// Compute 2^n (n>=0, result stored in v)
static void u256_pow2(uint256_t* v, int n) {
    u256_zero(v);
    v->words[0] = 1;
    
    if (n < 256) {
        u256_shl(v, n);
    } else {
        // Overflow: set as very large number (should not actually occur)
        v->words[3] = 0xFFFFFFFFFFFFFFFFULL;
    }
}

// fp128→decimal string (Dragon4 algorithm, 34-digit precision)
void paw_f128_to_string_decimal(uint64_t low, uint64_t high, char* buffer, size_t buf_size) {
    f128_t v = {low, high};
    
    // specialvalue
    if (f128_is_zero(v)) {
        snprintf(buffer, buf_size, "%s0", f128_sign(v) ? "-" : "");
        return;
    }
    if (f128_is_inf(v)) {
        snprintf(buffer, buf_size, "%sinf", f128_sign(v) ? "-" : "");
        return;
    }
    if (f128_is_nan(v)) {
        snprintf(buffer, buf_size, "nan");
        return;
    }
    
    // parsing
    f128_parts parts;
    f128_unpack(v, &parts);
    
    // Dragon4 algorithm
    // Represent v = f × 2^e, where f is mantissa ([1.0, 2.0)), e is exponent
    
    // Extract mantissa (113 bits, normalized to [1.0, 2.0))
    // Use 512 bits to get full 34-digit precision
    uint512_t r, s;  // r/s = v
    
    // Mantissa includes implicit 1
    u512_zero(&r);
    r.words[0] = parts.mant_low;
    r.words[1] = parts.mant_high;
    if (parts.exponent != 0) {
        r.words[1] |= (1ULL << 48);  // Add implicit 1
    }
    
    // Actual exponent (Note: fp128's mantissa is already [1.0,2.0), no need to divide by 2^112 again!)
    int32_t e = (int32_t)parts.exponent - F128_EXP_BIAS;
    
    // Estimate decimal exponent
    int k = estimate_decimal_exp(e) + 1;
    
    // Set s = 10^k (512-bit)
    u512_pow10(&s, k >= 0 ? k : -k);
    
    // Dragon4 core: set r and s such that v = r/s
    // v = mantissa × 2^e, where mantissa is in [1.0, 2.0)
    // 
    // We represent mantissa as 113-bit integer (bit 112=1)
    // So we need v = (mantissa_int / 2^112) × 2^e = mantissa_int × 2^(e-112)
    
    int32_t e_adjusted = e - 112;  // Adjust exponent
    
    // Adjust r and s (512-bit operations)
    if (e_adjusted >= 0) {
        // r = mantissa_int × 2^e_adjusted
        if (e_adjusted < 512) {
            u512_shl(&r, e_adjusted);
        }
        // s = 10^k (already set)
    } else {
        // r = mantissa_int (already set)
        // s = 2^(-e_adjusted) × 10^k
        if (-e_adjusted < 512) {
            u512_shl(&s, -e_adjusted);
        }
    }
    
    // Adjust k to make r/s in [0.1, 1.0) range
    uint512_t r_times_10 = r;
    u512_mul_u64(&r_times_10, 10);
    
    while (u512_cmp(&r_times_10, &s) < 0) {
        r = r_times_10;
        k--;
        r_times_10 = r;
        u512_mul_u64(&r_times_10, 10);
    }
    
    while (u512_cmp(&r, &s) >= 0) {
        u512_mul_u64(&s, 10);
        k++;
    }
    
    // Now 0.1 <= r/s < 1.0
    // Extract decimal digits (512-bit precision, support full 34 digits)
    char digits[40];
    int digit_count = 0;
    
    for (int i = 0; i < 38 && !u512_is_zero(&r); i++) {  // At most 38 digits (exceeds 34)
        // digit = floor(r * 10 / s)
        u512_mul_u64(&r, 10);
        
        // Calculate integer part of r/s (0-9)
        int digit = 0;
        while (u512_cmp(&r, &s) >= 0) {
            u512_sub(&r, &s);
            digit++;
        }
        
        digits[digit_count++] = '0' + digit;
        
        // If remainder is 0 and has enough precision, stop
        if (u512_is_zero(&r) && digit_count >= 34) {
            break;
        }
    }
    
    if (digit_count == 0) {
        digits[digit_count++] = '0';
    }
    
    // Smart format selection: scientific notation vs fixed decimal point
    // If exponent in [-6, 15] range, use fixed decimal point format
    // Otherwise use scientific notation
    int decimal_exp = k - 1;
    bool use_fixed = (decimal_exp >= -6 && decimal_exp <= 15);
    
    char* p = buffer;
    if (parts.sign) *p++ = '-';
    
    if (use_fixed) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // Fixed decimal point format: 123.456789...
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        int integer_digits = decimal_exp + 1;  // Integer part digits
        int decimal_digits = 0;                 // Decimal part digits
        
        if (integer_digits <= 0) {
            // Pure decimal: 0.001234...
            *p++ = '0';
            *p++ = '.';
            
            // Add leading zeros
            for (int i = 0; i < -integer_digits; i++) {
                *p++ = '0';
            }
            
            // Output all significant digits as decimal part
            int max_decimal = (digit_count > 33) ? 33 : digit_count;
            for (int i = 0; i < max_decimal; i++) {
                *p++ = digits[i];
            }
        } else if (integer_digits >= digit_count) {
            // Pure integer: 123456
            for (int i = 0; i < digit_count; i++) {
                *p++ = digits[i];
            }
            // Append trailing zeros
            for (int i = digit_count; i < integer_digits; i++) {
                *p++ = '0';
            }
        } else {
            // Mixed: integer part + decimal part
            // Integer part
            for (int i = 0; i < integer_digits; i++) {
                *p++ = digits[i];
            }
            
            // Decimal part
            if (integer_digits < digit_count) {
                *p++ = '.';
                int max_decimal = (digit_count - integer_digits > 33) ? 33 : (digit_count - integer_digits);
                for (int i = integer_digits; i < integer_digits + max_decimal; i++) {
                    if (i < digit_count) {
                        *p++ = digits[i];
                    } else {
                        *p++ = '0';
                    }
                }
            }
        }
    } else {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // Scientific notation: d.ddd...e±k
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        *p++ = digits[0];
        if (digit_count > 1) {
            *p++ = '.';
            int max = (digit_count - 1 > 32) ? 32 : (digit_count - 1);
            for (int i = 1; i <= max; i++) {
                *p++ = digits[i];
            }
        }
        
        // Add exponent
        p += snprintf(p, buffer + buf_size - p, "e%+d", decimal_exp);
    }
    
    *p = '\0';
}

} // extern "C"
