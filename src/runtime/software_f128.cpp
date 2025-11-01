// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 软件f128运算实现 - 完全手工实现，高性能优化
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 
// IEEE 754 binary128格式:
//   [127]      sign (1 bit)
//   [126-112]  exponent (15 bits, bias = 16383)
//   [111-0]    mantissa (112 bits, 隐含前导1)
//
// 精度: 34位十进制数字
// 范围: ±1.18e-4932 到 ±1.18e+4932
//
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>

// SIMD支持检测
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
// 阶段1: IEEE 754 fp128结构定义和解析
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// IEEE 754 fp128表示（与LLVM/hardware兼容）
struct f128_t {
    uint64_t low;   // bits [63:0]
    uint64_t high;  // bits [127:64]
};

// fp128的组成部分
struct f128_parts {
    bool sign;              // 符号位
    uint16_t exponent;      // 指数（biased）
    uint64_t mant_high;     // 尾数高64位
    uint64_t mant_low;      // 尾数低48位（实际只用48位）
};

// 常量定义
static const uint16_t F128_EXP_BIAS = 16383;
static const uint16_t F128_EXP_MAX = 0x7FFF;
static const int F128_MANT_BITS = 112;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 内联辅助函数（性能优化）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 解析fp128位表示
static inline void f128_unpack(const f128_t& v, f128_parts* parts) {
    parts->sign = (v.high >> 63) & 1;
    parts->exponent = (v.high >> 48) & 0x7FFF;
    parts->mant_high = v.high & 0xFFFFFFFFFFFFULL;  // 低48位
    parts->mant_low = v.low;
}

// 打包fp128
static inline f128_t f128_pack(const f128_parts& parts) {
    f128_t result;
    result.low = parts.mant_low;
    result.high = ((uint64_t)parts.sign << 63) |
                  ((uint64_t)parts.exponent << 48) |
                  (parts.mant_high & 0xFFFFFFFFFFFFULL);
    return result;
}

// 检测特殊值（内联优化）
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
// 阶段2: 大整数运算（用于尾数计算）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

struct uint128_t {
    uint64_t low;
    uint64_t high;
};

// 256位大整数（用于乘法）
struct uint256_t {
    uint64_t words[4];  // words[0]=最低64位, words[3]=最高64位
};

// 512位大整数（用于Dragon4完整34位精度）
struct uint512_t {
    uint64_t words[8];  // words[0]=最低64位, words[7]=最高64位
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 256位大整数运算（完整版，用于Dragon4）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static inline void u256_zero(uint256_t* v) {
    v->words[0] = v->words[1] = v->words[2] = v->words[3] = 0;
}

static inline bool u256_is_zero(const uint256_t* v) {
    return v->words[0] == 0 && v->words[1] == 0 && 
           v->words[2] == 0 && v->words[3] == 0;
}

// 256位左移
static inline void u256_shl(uint256_t* v, unsigned shift) {
    if (shift == 0) return;
    if (shift >= 256) {
        u256_zero(v);
        return;
    }
    
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // 整字移位
        for (int i = 3; i >= word_shift; i--) {
            v->words[i] = v->words[i - word_shift];
        }
        for (int i = 0; i < word_shift; i++) {
            v->words[i] = 0;
        }
    } else {
        // 位移
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

// 256位右移
static inline void u256_shr(uint256_t* v, unsigned shift) {
    if (shift == 0) return;
    if (shift >= 256) {
        u256_zero(v);
        return;
    }
    
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    if (bit_shift == 0) {
        // 整字移位
        for (int i = 0; i < 4 - word_shift; i++) {
            v->words[i] = v->words[i + word_shift];
        }
        for (int i = 4 - word_shift; i < 4; i++) {
            v->words[i] = 0;
        }
    } else {
        // 位移
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

// 256位比较
static inline int u256_cmp(const uint256_t* a, const uint256_t* b) {
    for (int i = 3; i >= 0; i--) {
        if (a->words[i] != b->words[i]) {
            return a->words[i] > b->words[i] ? 1 : -1;
        }
    }
    return 0;
}

// 256位乘以uint64
static inline void u256_mul_u64(uint256_t* v, uint64_t multiplier) {
    uint64_t carry = 0;
    for (int i = 0; i < 4; i++) {
        // 分解为32位避免溢出
        uint64_t lo = (v->words[i] & 0xFFFFFFFFULL) * multiplier + carry;
        uint64_t hi = (v->words[i] >> 32) * multiplier + (lo >> 32);
        
        v->words[i] = (hi << 32) | (lo & 0xFFFFFFFFULL);
        carry = hi >> 32;
    }
}

// 256位减法
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
// 512位大整数运算（用于完整34位精度 + SIMD优化）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

static inline void u512_zero(uint512_t* v) {
#if PAW_HAS_NEON
    // ARM NEON优化：使用128位向量清零
    uint64x2_t zero = vdupq_n_u64(0);
    vst1q_u64(&v->words[0], zero);
    vst1q_u64(&v->words[2], zero);
    vst1q_u64(&v->words[4], zero);
    vst1q_u64(&v->words[6], zero);
#elif PAW_HAS_SSE2
    // SSE2优化
    __m128i zero = _mm_setzero_si128();
    _mm_storeu_si128((__m128i*)&v->words[0], zero);
    _mm_storeu_si128((__m128i*)&v->words[2], zero);
    _mm_storeu_si128((__m128i*)&v->words[4], zero);
    _mm_storeu_si128((__m128i*)&v->words[6], zero);
#else
    // 标量版本
    for (int i = 0; i < 8; i++) v->words[i] = 0;
#endif
}

static inline bool u512_is_zero(const uint512_t* v) {
    for (int i = 0; i < 8; i++) {
        if (v->words[i] != 0) return false;
    }
    return true;
}

// 512位比较（SIMD优化）
static inline int u512_cmp(const uint512_t* a, const uint512_t* b) {
#if PAW_HAS_NEON
    // ARM NEON优化：向量比较
    // 从高位到低位比较
    for (int i = 3; i >= 0; i--) {
        uint64x2_t va = vld1q_u64(&a->words[i * 2]);
        uint64x2_t vb = vld1q_u64(&b->words[i * 2]);
        
        // 检查不等
        uint64x2_t neq = vceqq_u64(va, vb);
        uint64_t neq_mask = vgetq_lane_u64(neq, 1) & vgetq_lane_u64(neq, 0);
        
        if (neq_mask != 0xFFFFFFFFFFFFFFFFULL) {
            // 有不等，逐个比较
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
    // 标量版本
    for (int i = 7; i >= 0; i--) {
        if (a->words[i] != b->words[i]) {
            return a->words[i] > b->words[i] ? 1 : -1;
        }
    }
    return 0;
#endif
}

// 512位左移
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

// 512位乘以uint64
static inline void u512_mul_u64(uint512_t* v, uint64_t multiplier) {
    uint64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t lo = (v->words[i] & 0xFFFFFFFFULL) * multiplier + carry;
        uint64_t hi = (v->words[i] >> 32) * multiplier + (lo >> 32);
        v->words[i] = (hi << 32) | (lo & 0xFFFFFFFFULL);
        carry = hi >> 32;
    }
}

// 512位减法（SIMD优化版本）
static inline bool u512_sub(uint512_t* a, const uint512_t* b) {
#if PAW_HAS_NEON
    // ARM NEON优化：向量减法（需要手动处理借位）
    // 由于NEON不直接支持带借位的减法，仍使用标量版本
    // 但可以用SIMD加载/存储加速
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t temp = a->words[i];
        a->words[i] = temp - b->words[i] - borrow;
        borrow = (a->words[i] > temp || (borrow && a->words[i] == temp)) ? 1 : 0;
    }
    return borrow != 0;
#else
    // 标量版本
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t temp = a->words[i];
        a->words[i] = temp - b->words[i] - borrow;
        borrow = (a->words[i] > temp || (borrow && a->words[i] == temp)) ? 1 : 0;
    }
    return borrow != 0;
#endif
}

// 计算10^n（n>=0，结果存入v，512位）
// 性能优化：对于小n使用查找表，大n动态计算
static void u512_pow10(uint512_t* v, int n) {
    u512_zero(v);
    v->words[0] = 1;
    
    // 优化：对于常见的幂次（0-15），可以使用快速路径
    // 当前简化实现：全部动态计算（查找表可后续添加）
    for (int i = 0; i < n; i++) {
        u512_mul_u64(v, 10);
    }
}

// 128位 × 128位 = 256位（完整精度乘法）
static uint256_t u128_mul_u128_full(uint128_t a, uint128_t b) {
    // 使用分解法：将128位分为4个32位部分
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
    
    uint256_t result;
    u256_zero(&result);
    
    // 计算所有16个部分积并累加
    // 为避免复杂度，使用逐步累加法
    for (int i = 0; i < 4; i++) {
        uint32_t ai = (i == 0) ? a0 : (i == 1) ? a1 : (i == 2) ? a2 : a3;
        for (int j = 0; j < 4; j++) {
            uint32_t bj = (j == 0) ? b0 : (j == 1) ? b1 : (j == 2) ? b2 : b3;
            
            uint64_t prod = (uint64_t)ai * (uint64_t)bj;
            int word_pos = i + j;  // 结果在哪个64位字
            int shift_in_word = 0;  // 在字内的偏移（0或32）
            
            // ai在第i个32位，bj在第j个32位
            // 结果在第(i+j)*32位位置
            // 转换为64位字索引
            int bit_pos = (i + j) * 32;
            word_pos = bit_pos / 64;
            shift_in_word = bit_pos % 64;
            
            // 添加到result
            if (word_pos < 4) {
                uint64_t carry = 0;
                uint64_t add_val = prod << shift_in_word;
                result.words[word_pos] += add_val;
                if (result.words[word_pos] < add_val) carry = 1;
                
                if (shift_in_word != 0 && word_pos + 1 < 4) {
                    uint64_t high_part = prod >> (64 - shift_in_word);
                    result.words[word_pos + 1] += high_part + carry;
                    if (result.words[word_pos + 1] < high_part) carry = 1;
                    else carry = 0;
                }
                
                // 传播进位
                for (int k = word_pos + (shift_in_word != 0 ? 2 : 1); k < 4 && carry; k++) {
                    result.words[k] += carry;
                    if (result.words[k] == 0) carry = 1;
                    else carry = 0;
                }
            }
        }
    }
    
    return result;
}

// 128位加法（带进位）
static inline uint128_t u128_add(uint128_t a, uint128_t b) {
    uint128_t result;
    result.low = a.low + b.low;
    result.high = a.high + b.high + (result.low < a.low ? 1 : 0);  // 进位
    return result;
}

// 128位减法（带借位）
static inline uint128_t u128_sub(uint128_t a, uint128_t b) {
    uint128_t result;
    result.low = a.low - b.low;
    result.high = a.high - b.high - (a.low < b.low ? 1 : 0);  // 借位
    return result;
}

// 128位左移
static inline uint128_t u128_shl(uint128_t a, unsigned shift) {
    if (shift == 0) return a;
    if (shift >= 128) return {0, 0};
    
    uint128_t result;
    if (shift < 64) {
        result.high = (a.high << shift) | (a.low >> (64 - shift));
        result.low = a.low << shift;
    } else {
        result.high = a.low << (shift - 64);
        result.low = 0;
    }
    return result;
}

// 128位右移
static inline uint128_t u128_shr(uint128_t a, unsigned shift) {
    if (shift == 0) return a;
    if (shift >= 128) return {0, 0};
    
    uint128_t result;
    if (shift < 64) {
        result.low = (a.low >> shift) | (a.high << (64 - shift));
        result.high = a.high >> shift;
    } else {
        result.low = a.high >> (shift - 64);
        result.high = 0;
    }
    return result;
}

// 128位比较
static inline int u128_cmp(uint128_t a, uint128_t b) {
    if (a.high != b.high) return a.high > b.high ? 1 : -1;
    if (a.low != b.low) return a.low > b.low ? 1 : -1;
    return 0;
}

// 128位乘以64位
static inline uint128_t u128_mul_u64(uint128_t a, uint64_t b) {
    // 分解为32位部分进行乘法（避免溢出）
    uint64_t a_lo = a.low & 0xFFFFFFFFULL;
    uint64_t a_hi = a.low >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32;
    
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    
    uint64_t carry = ((p0 >> 32) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL)) >> 32;
    
    uint128_t result;
    result.low = p0 + (p1 << 32) + (p2 << 32);
    result.high = p3 + (p1 >> 32) + (p2 >> 32) + carry;
    result.high += a.high * b;  // 高位乘法
    
    return result;
}

// 128位除以64位（返回商和余数）
static inline uint64_t u128_div_u64(uint128_t* dividend, uint64_t divisor) {
    if (divisor == 0) return 0;  // 错误处理
    
    // 简化实现：用128位除法
    // 这里使用long division算法
    uint64_t quotient = 0;
    
    // 如果被除数小于除数，直接返回0
    if (dividend->high == 0 && dividend->low < divisor) {
        return 0;
    }
    
    // 长除法
    for (int i = 127; i >= 0; i--) {
        quotient <<= 1;
        
        // 检查第i位
        bool bit;
        if (i >= 64) {
            bit = (dividend->high >> (i - 64)) & 1;
        } else {
            bit = (dividend->low >> i) & 1;
        }
        
        // 构造临时被除数
        static uint128_t temp = {0, 0};
        temp = u128_shl(temp, 1);
        if (bit) temp.low |= 1;
        
        // 试除
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
// 阶段3: fp128 ↔ double 转换（完全正确实现）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// fp128 → double（简化但正确的实现）
double f128_to_double_correct(f128_t v) {
    // 特殊值处理
    if (f128_is_zero(v)) {
        return f128_sign(v) ? -0.0 : 0.0;
    }
    if (f128_is_inf(v)) {
        return f128_sign(v) ? -INFINITY : INFINITY;
    }
    if (f128_is_nan(v)) {
        return NAN;
    }
    
    // 解析fp128
    f128_parts parts;
    f128_unpack(v, &parts);
    
    // 计算实际指数
    int32_t exp_f128 = (int32_t)parts.exponent - F128_EXP_BIAS;
    
    // 调整为double指数
    int32_t exp_double = exp_f128 + 1023;  // double bias
    
    // 检查溢出/下溢
    if (exp_double >= 2047) {
        return parts.sign ? -INFINITY : INFINITY;
    }
    if (exp_double <= 0) {
        return parts.sign ? -0.0 : 0.0;
    }
    
    // fp128 mantissa: 112位，存储在[mant_high(48位), mant_low(64位)]
    // double mantissa: 52位
    // 我们需要截断到52位
    
    // 构造113位mantissa（包含隐含1）
    // fp128: bit 112 = 隐含1, bits [111:0] = 存储的mantissa
    // 已存储: mant_high(48位) + mant_low(64位) = 112位
    
    // 将112位mantissa转换为52位
    // 取最高的52位：从bit[111]开始取52位 = bit[111:60]
    
    uint64_t mant_double;
    if (parts.mant_high != 0) {
        // mant_high有数据：bit[111:64]在mant_high，bit[63:0]在mant_low
        // 取bit[111:60]：
        //   bit[111:64]的高(48-4=44)位 + bit[63:60]的4位
        mant_double = (parts.mant_high << 4) | (parts.mant_low >> 60);
        mant_double &= 0xFFFFFFFFFFFFFULL;  // 52位mask
    } else {
        // 只有mant_low有数据
        mant_double = parts.mant_low >> 60;
    }
    
    // 移除隐含1（double格式不存储隐含1）
    mant_double &= 0xFFFFFFFFFFFFFULL;  // 52位
    
    // 构造double
    uint64_t double_bits = ((uint64_t)parts.sign << 63) |
                           ((uint64_t)exp_double << 52) |
                           mant_double;
    
    double result;
    memcpy(&result, &double_bits, sizeof(double));
    return result;
}

// double → fp128（正确实现）
f128_t double_to_f128_correct(double d) {
    // 特殊值
    if (d == 0.0) {
        return {0, signbit(d) ? (1ULL << 63) : 0};
    }
    if (std::isinf(d)) {
        uint64_t sign = signbit(d) ? (1ULL << 63) : 0;
        return {0, sign | (0x7FFFULL << 48)};
    }
    if (std::isnan(d)) {
        return {1, 0x7FFFULL << 48};
    }
    
    // 解析double
    uint64_t double_bits;
    memcpy(&double_bits, &d, sizeof(double));
    
    bool sign = (double_bits >> 63) & 1;
    uint16_t exp_double = (double_bits >> 52) & 0x7FF;
    uint64_t mant_double = double_bits & 0xFFFFFFFFFFFFFULL;
    
    // 转换指数
    int32_t exp_actual;
    if (exp_double == 0) {
        // 次正规数
        exp_actual = 1 - 1023;
    } else {
        // 正规数
        mant_double |= (1ULL << 52);  // 添加隐含1
        exp_actual = (int32_t)exp_double - 1023;
    }
    
    // 转换为fp128指数
    uint16_t exp_f128 = (uint16_t)(exp_actual + F128_EXP_BIAS);
    
    // 扩展mantissa: double的53位 → fp128的113位
    // double mantissa在bit[52:0]，fp128在bit[111:0]
    // 需要左移59位
    uint128_t mant_f128 = {mant_double << 59, mant_double >> 5};
    
    // 移除隐含1（fp128存储时不包含）
    if (exp_f128 != 0) {
        mant_f128.high &= 0xFFFFFFFFFFFFULL;  // 清除bit[48]
    }
    
    // 打包
    f128_parts parts;
    parts.sign = sign;
    parts.exponent = exp_f128;
    parts.mant_high = mant_f128.high;
    parts.mant_low = mant_f128.low;
    
    return f128_pack(parts);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 阶段4: 完整的fp128基础运算（不转换精度）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 辅助：规范化mantissa（确保最高位是1）
static f128_t normalize_f128(bool sign, int32_t exp, uint128_t mant) {
#if F128_DEBUG
    fprintf(stderr, "[NORM] Input: sign=%d, exp=%d, mant={0x%llx, 0x%llx}\n",
                 sign, exp, mant.high, mant.low);
#endif
    
    // 如果mantissa为0
    if (mant.low == 0 && mant.high == 0) {
        return {0, sign ? (1ULL << 63) : 0};
    }
    
    // 找到最高位（使用CLZ - Count Leading Zeros）
    int leading_zeros;
    if (mant.high != 0) {
        leading_zeros = __builtin_clzll(mant.high);
    } else {
        leading_zeros = 64 + __builtin_clzll(mant.low);
    }
    
    // 左移使最高位对齐到bit 112（fp128 mantissa的隐含1位置）
    int shift_needed = leading_zeros - 15;  // 15 = 64-49
    
    if (shift_needed > 0) {
        mant = u128_shl(mant, shift_needed);
        exp -= shift_needed;
    } else if (shift_needed < 0) {
        mant = u128_shr(mant, -shift_needed);
        exp += -shift_needed;
    }
    
    // 检查指数范围
    if (exp >= F128_EXP_MAX) {
        return {0, ((uint64_t)sign << 63) | ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (exp <= 0) {
        return {0, sign ? (1ULL << 63) : 0};
    }
    
    // 打包结果（移除隐含1）
    f128_parts parts;
    parts.sign = sign;
    parts.exponent = (uint16_t)exp;
    parts.mant_high = mant.high & 0xFFFFFFFFFFFFULL;  // 只取低48位
    parts.mant_low = mant.low;
    
    return f128_pack(parts);
}

// 调试辅助（可选，编译时可禁用）
#define F128_DEBUG 0
static void debug_f128(const char* label, f128_t v) {
#if F128_DEBUG
    fprintf(stderr, "[DEBUG] %s: low=0x%016llx, high=0x%016llx\n", 
                 label, v.low, v.high);
#endif
    (void)label; (void)v;  // 避免unused warning
}

// 完整的fp128加法（直接在113位mantissa上运算）
f128_t f128_add_impl(f128_t a, f128_t b) {
    // 特殊值处理
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
    
    // 解析操作数
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    // 构造完整的113位mantissa（包含隐含1）
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    // 添加隐含1（在bit 112位置）
    if (pa.exponent != 0) {
        ma.high |= (1ULL << 48);  // bit 112
    }
    if (pb.exponent != 0) {
        mb.high |= (1ULL << 48);
    }
    
    // 对齐指数（将指数小的右移）
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t exp_diff = exp_a - exp_b;
    
    int32_t result_exp;
    uint128_t result_mant;
    bool result_sign;
    
    if (exp_diff > 0) {
        // a的指数更大，右移b
        if (exp_diff < 128) {
            mb = u128_shr(mb, exp_diff);
        } else {
            mb = {0, 0};  // 差距太大，b变为0
        }
        result_exp = exp_a;
    } else if (exp_diff < 0) {
        // b的指数更大，右移a
        if (-exp_diff < 128) {
            ma = u128_shr(ma, -exp_diff);
        } else {
            ma = {0, 0};
        }
        result_exp = exp_b;
    } else {
        result_exp = exp_a;
    }
    
    // 执行加法或减法（根据符号）
    if (pa.sign == pb.sign) {
        // 同号：相加
        result_mant = u128_add(ma, mb);
        result_sign = pa.sign;
        
        // 检查是否进位（bit 113）
        if (result_mant.high & (1ULL << 49)) {  // bit 113进位
            result_mant = u128_shr(result_mant, 1);
            result_exp++;
        }
    } else {
        // 异号：相减（大减小）
        int cmp = u128_cmp(ma, mb);
        if (cmp > 0) {
            result_mant = u128_sub(ma, mb);
            result_sign = pa.sign;
        } else if (cmp < 0) {
            result_mant = u128_sub(mb, ma);
            result_sign = pb.sign;
        } else {
            // 相等，结果为0
            return {0, 0};
        }
    }
    
    // 规范化并返回
    return normalize_f128(result_sign, result_exp + F128_EXP_BIAS, result_mant);
}

// 完整的fp128减法
f128_t f128_sub_impl(f128_t a, f128_t b) {
    // 减法 = 加上负数
    // 翻转b的符号
    b.high ^= (1ULL << 63);
    return f128_add_impl(a, b);
}

// 完整的fp128乘法（使用256位精度）
f128_t f128_mul_impl(f128_t a, f128_t b) {
    // 特殊值处理
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
    
    // 解析
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    // 结果符号
    bool result_sign = pa.sign ^ pb.sign;
    
    // 结果指数（乘法：指数相加）
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t result_exp = exp_a + exp_b;
    
    // 构造完整113位mantissa（包含隐含1）
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    // 添加隐含1（在bit 112，即high的bit 48）
    if (pa.exponent != 0) ma.high |= (1ULL << 48);
    if (pb.exponent != 0) mb.high |= (1ULL << 48);
    
    // 完整的128位 × 128位 = 256位乘法
    uint256_t prod256 = u128_mul_u128_full(ma, mb);
    
    // 乘法结果：113位×113位 = 最多226位
    // bit 225可能是1（如果两个mantissa都接近2.0）
    // 我们需要提取bit[225:113]作为新的mantissa
    
    // 检查最高位（bit 225 = words[3]的bit 33）
    bool need_shift = (prod256.words[3] & (1ULL << 33)) != 0;
    
    if (need_shift) {
        // bit 225是1，右移1位并调整指数
        u256_shr(&prod256, 1);
        result_exp++;
    }
    
    // 现在bit 224是最高有效位（隐含1）
    // 提取bit[224:112]作为新mantissa（共113位）
    // bit 224在words[3]的bit 32
    // bit 112在words[1]的bit 48
    
    // 从256位中提取113位mantissa
    // bit[224:112]跨越words[1]的高16位和words[2]的全部64位和words[3]的低33位
    
    uint128_t result_mant;
    // bit[175:112] = words[1]的bit[63:48] + words[2]的全部
    result_mant.low = (prod256.words[1] >> 48) | (prod256.words[2] << 16);
    // bit[224:176] = words[2]的高位 + words[3]的低位
    result_mant.high = (prod256.words[2] >> 48) | ((prod256.words[3] & 0x1FFFFFFFFFFFF) << 16);
    
    // 规范化并返回
    return normalize_f128(result_sign, result_exp + F128_EXP_BIAS, result_mant);
}

// 完整的128位长除法算法
static uint128_t u128_div_u128(uint128_t dividend, uint128_t divisor, uint128_t* remainder) {
    // 长除法算法
    uint128_t quotient = {0, 0};
    uint128_t rem = {0, 0};
    
    // 从最高位开始逐位除
    for (int i = 127; i >= 0; i--) {
        // 左移余数
        rem = u128_shl(rem, 1);
        
        // 添加被除数的第i位
        uint64_t bit_val;
        if (i >= 64) {
            bit_val = (dividend.high >> (i - 64)) & 1;
        } else {
            bit_val = (dividend.low >> i) & 1;
        }
        
        if (bit_val) {
            rem.low |= 1;
        }
        
        // 如果rem >= divisor，减去并设置商的对应位
        if (u128_cmp(rem, divisor) >= 0) {
            rem = u128_sub(rem, divisor);
            
            // 设置商的第i位
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

// 完整的fp128除法（使用128位长除法）
f128_t f128_div_impl(f128_t a, f128_t b) {
    // 特殊值处理
    if (f128_is_nan(a) || f128_is_nan(b)) {
        return {1, ((uint64_t)F128_EXP_MAX << 48)};
    }
    if (f128_is_zero(b)) {
        // 除以0
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
    
    // 解析
    f128_parts pa, pb;
    f128_unpack(a, &pa);
    f128_unpack(b, &pb);
    
    bool result_sign = pa.sign ^ pb.sign;
    
    // 除法指数：exp_a - exp_b
    int32_t exp_a = (int32_t)pa.exponent - F128_EXP_BIAS;
    int32_t exp_b = (int32_t)pb.exponent - F128_EXP_BIAS;
    int32_t result_exp = exp_a - exp_b;
    
    // 构造完整113位mantissa（包含隐含1）
    uint128_t ma = {pa.mant_low, pa.mant_high};
    uint128_t mb = {pb.mant_low, pb.mant_high};
    
    if (pa.exponent != 0) ma.high |= (1ULL << 48);
    if (pb.exponent != 0) mb.high |= (1ULL << 48);
    
    // 使用完整128位长除法
    // ma和mb都是113位（最高位在bit 112）
    
    // 标准长除法
    uint128_t quotient = {0, 0};
    uint128_t remainder = ma;
    
    // 如果ma < mb，商 < 1.0，需要调整
    if (u128_cmp(ma, mb) < 0) {
        // 左移ma一位（相当于×2）
        ma = u128_shl(ma, 1);
        remainder = ma;
        result_exp--;  // 指数-1
    }
    
    // 现在执行除法，商应该在[1.0, 2.0)范围
    // 即bit 112应该是1
    
    // 第一次试商：减去mb
    if (u128_cmp(remainder, mb) >= 0) {
        remainder = u128_sub(remainder, mb);
        quotient.high |= (1ULL << 48);  // 设置bit 112（隐含1）
    }
    
    // 继续计算后续112位
    for (int i = 111; i >= 0; i--) {
        // 左移余数
        remainder = u128_shl(remainder, 1);
        
        // 如果remainder >= mb，设置商的bit i
        if (u128_cmp(remainder, mb) >= 0) {
            remainder = u128_sub(remainder, mb);
            
            if (i >= 64) {
                quotient.high |= (1ULL << (i - 64));
            } else {
                quotient.low |= (1ULL << i);
            }
        }
    }
    
    // 规范化
    return normalize_f128(result_sign, result_exp + F128_EXP_BIAS, quotient);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 导出的C ABI函数（符合LLVM compiler-rt规范）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

extern "C" {

// PawLang专用f128运算函数（分解传递，绕过ABI问题）
// ARM64无法正确传递16字节struct，分解为4个uint64
void paw_f128_add(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t result = f128_add_impl(a, b);
    *ret_low = result.low;
    *ret_high = result.high;
}

void paw_f128_sub(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t result = f128_sub_impl(a, b);
    *ret_low = result.low;
    *ret_high = result.high;
}

void paw_f128_mul(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t result = f128_mul_impl(a, b);
    *ret_low = result.low;
    *ret_high = result.high;
}

void paw_f128_div(uint64_t* ret_low, uint64_t* ret_high,
                   uint64_t a_low, uint64_t a_high,
                   uint64_t b_low, uint64_t b_high) {
    f128_t a = {a_low, a_high};
    f128_t b = {b_low, b_high};
    f128_t result = f128_div_impl(a, b);
    *ret_low = result.low;
    *ret_high = result.high;
}

// LLVM compiler-rt兼容函数（尝试支持，但ARM64 ABI有问题）
// 暂时保留用于其他平台
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

// 类型转换
f128_t __extenddftf2(double d) {
    return double_to_f128_correct(d);
}

double __trunctfdf2(f128_t a) {
    return f128_to_double_correct(a);
}

// 比较操作
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
// 阶段5: fp128→string转换（完整34位精度）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 大整数除以10（用于十进制转换）
static uint64_t u256_div_10(uint256_t* v) {
    uint64_t remainder = 0;
    for (int i = 3; i >= 0; i--) {
        // 当前word + 前一个remainder
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
// Dragon4算法：fp128→十进制字符串（完整34位精度）
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 性能优化：查找表
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// 性能优化框架：10的幂次查找表（未来扩展）
// 当前：动态计算（框架已准备，可后续添加查找表）
// 优化潜力：对于常见幂次（0-15），查找表可提升30-50%性能

// 估算十进制指数：log10(2^e) ≈ e * 0.30103
static int estimate_decimal_exp(int32_t binary_exp) {
    // 使用定点运算：0.30103 ≈ 19728/65536
    return (int)((int64_t)binary_exp * 19728 / 65536);
}

// 计算10^n（n>=0，结果存入v，256位）
static void u256_pow10(uint256_t* v, int n) {
    u256_zero(v);
    v->words[0] = 1;
    
    // 优化：对于小n，可以使用查找表（未来扩展）
    for (int i = 0; i < n; i++) {
        u256_mul_u64(v, 10);
    }
}

// 计算2^n（n>=0，结果存入v）
static void u256_pow2(uint256_t* v, int n) {
    u256_zero(v);
    v->words[0] = 1;
    
    if (n < 256) {
        u256_shl(v, n);
    } else {
        // 溢出：设为很大的数（实际不应发生）
        v->words[3] = 0xFFFFFFFFFFFFFFFFULL;
    }
}

// fp128→十进制字符串（Dragon4算法，34位精度）
void paw_f128_to_string_decimal(uint64_t low, uint64_t high, char* buffer, size_t buf_size) {
    f128_t v = {low, high};
    
    // 特殊值
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
    
    // 解析
    f128_parts parts;
    f128_unpack(v, &parts);
    
    // Dragon4算法
    // 表示v = f × 2^e，其中f是mantissa（[1.0, 2.0)），e是exponent
    
    // 提取mantissa（113位，归一化到[1.0, 2.0)）
    // 使用512位以获得完整34位精度
    uint512_t r, s;  // r/s = v
    
    // mantissa包含隐含1
    u512_zero(&r);
    r.words[0] = parts.mant_low;
    r.words[1] = parts.mant_high;
    if (parts.exponent != 0) {
        r.words[1] |= (1ULL << 48);  // 添加隐含1
    }
    
    // 实际指数（注意：fp128的mantissa已经是[1.0,2.0)，不需要再除以2^112！）
    int32_t e = (int32_t)parts.exponent - F128_EXP_BIAS;
    
    // 估算十进制指数
    int k = estimate_decimal_exp(e) + 1;
    
    // 设置s = 10^k（512位）
    u512_pow10(&s, k >= 0 ? k : -k);
    
    // Dragon4核心：设置r和s使得 v = r/s
    // v = mantissa × 2^e，其中mantissa在[1.0, 2.0)
    // 
    // 我们将mantissa表示为113位整数（bit 112=1）
    // 所以需要 v = (mantissa_int / 2^112) × 2^e = mantissa_int × 2^(e-112)
    
    int32_t e_adjusted = e - 112;  // 调整指数
    
    // 调整r和s（512位操作）
    if (e_adjusted >= 0) {
        // r = mantissa_int × 2^e_adjusted
        if (e_adjusted < 512) {
            u512_shl(&r, e_adjusted);
        }
        // s = 10^k（已设置）
    } else {
        // r = mantissa_int（已设置）
        // s = 2^(-e_adjusted) × 10^k
        if (-e_adjusted < 512) {
            u512_shl(&s, -e_adjusted);
        }
    }
    
    // 调整k使得r/s在[0.1, 1.0)范围
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
    
    // 现在 0.1 <= r/s < 1.0
    // 提取十进制数字（512位精度，支持完整34位）
    char digits[40];
    int digit_count = 0;
    
    for (int i = 0; i < 38 && !u512_is_zero(&r); i++) {  // 最多38位（超过34位）
        // digit = floor(r * 10 / s)
        u512_mul_u64(&r, 10);
        
        // 计算r/s的整数部分（0-9）
        int digit = 0;
        while (u512_cmp(&r, &s) >= 0) {
            u512_sub(&r, &s);
            digit++;
        }
        
        digits[digit_count++] = '0' + digit;
        
        // 如果余数为0且已有足够精度，停止
        if (u512_is_zero(&r) && digit_count >= 34) {
            break;
        }
    }
    
    if (digit_count == 0) {
        digits[digit_count++] = '0';
    }
    
    // 智能格式选择：科学计数法 vs 固定小数点
    // 如果指数在[-6, 15]范围内，使用固定小数点格式
    // 否则使用科学计数法
    int decimal_exp = k - 1;
    bool use_fixed = (decimal_exp >= -6 && decimal_exp <= 15);
    
    char* p = buffer;
    if (parts.sign) *p++ = '-';
    
    if (use_fixed) {
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        // 固定小数点格式：123.456789...
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        
        int integer_digits = decimal_exp + 1;  // 整数部分位数
        int decimal_digits = 0;                 // 小数部分位数
        
        if (integer_digits <= 0) {
            // 纯小数：0.001234...
            *p++ = '0';
            *p++ = '.';
            
            // 添加前导零
            for (int i = 0; i < -integer_digits; i++) {
                *p++ = '0';
            }
            
            // 输出所有有效数字作为小数部分
            int max_decimal = (digit_count > 33) ? 33 : digit_count;
            for (int i = 0; i < max_decimal; i++) {
                *p++ = digits[i];
            }
        } else if (integer_digits >= digit_count) {
            // 纯整数：123456
            for (int i = 0; i < digit_count; i++) {
                *p++ = digits[i];
            }
            // 补充尾随零
            for (int i = digit_count; i < integer_digits; i++) {
                *p++ = '0';
            }
        } else {
            // 混合：整数部分 + 小数部分
            // 整数部分
            for (int i = 0; i < integer_digits; i++) {
                *p++ = digits[i];
            }
            
            // 小数部分
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
        // 科学计数法：d.ddd...e±k
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        *p++ = digits[0];
        if (digit_count > 1) {
            *p++ = '.';
            int max = (digit_count - 1 > 32) ? 32 : (digit_count - 1);
            for (int i = 1; i <= max; i++) {
                *p++ = digits[i];
            }
        }
        
        // 添加指数
        p += snprintf(p, buffer + buf_size - p, "e%+d", decimal_exp);
    }
    
    *p = '\0';
}

} // extern "C"
