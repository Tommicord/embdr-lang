/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


module;
#include <cinttypes>
#include <cstring>
#include <bit>

#if defined(__aarch64__) || defined(__ARM64)
#        define EMBDR_IS_AARCH64 1
#        define EMBDR_IS_X64     0
#elif defined(__x86_64__) || defined(__X64)
#        define EMBDR_IS_AARCH64 0
#        define EMBDR_IS_X64     1
#else
#        define EMBDR_IS_AARCH64 0
#        define EMBDR_IS_X64     0
#endif

#ifndef EMBDR_USE_NEON
#        if EMBDR_IS_AARCH64 && (defined(__ARM_NEON__) || defined(__ARM_NEON))
#                include <arm_neon.h>
#                define EMBDR_USE_NEON 1
#        else
#                define EMBDR_USE_NEON 0
#        endif
#endif

#ifndef EMBDR_USE_SSE2
#        if defined(__SSE2__) && __SSE2__
#                include <immintrin.h>
#                define EMBDR_USE_SSE2 1
#        else
#                define EMBDR_USE_SSE2 0
#        endif
#endif

#if EMBDR_USE_SSE2 || EMBDR_USE_NEON
#        define EMBDR_USE_NEON_OR_SSE2 1
#else
#        define EMBDR_USE_NEON_OR_SSE2 0
#endif

#ifndef EMBDR_USE_SSE3
#        if EMBDR_USE_SSE2 && defined(__SSE3__) && __SSE3__
#                define EMBDR_USE_SSE3 1
#        else
#                define EMBDR_USE_SSE3 0
#        endif
#endif

#ifndef EMBDR_USE_SSE4_1
#        if EMBDR_USE_SSE3 && defined(__SSE4_1__) && __SSE4_1__
#                define EMBDR_USE_SSE4_1 1
#        else
#                define EMBDR_USE_SSE4_1 0
#        endif
#endif

#ifndef EMBDR_NO_MEMMOVE
#        if EMBDR_USE_SSE2 && (defined(__AVX512F__) && defined(__AVX512IFMA__) && defined(__AVX512VBMI__))
#                define EMBDR_NO_MEMMOVE 1
#        elif EMBDR_USE_SSE2 && (EMBDR_USE_SSE4_1 || (defined(__SSSE3__) && __SSSE3__))
#                define EMBDR_NO_MEMMOVE 1
#        elif EMBDR_USE_NEON && defined(__APPLE__) && defined(__arm64__)
#                define EMBDR_NO_MEMMOVE 0
#        elif EMBDR_USE_NEON
#                define EMBDR_NO_MEMMOVE 1
#        else
#                define EMBDR_NO_MEMMOVE 0
#        endif
#endif

#if EMBDR_NO_MEMMOVE
#        define EMBDR_SHUFFLER_PARAM , move_shuffler
#        define EMBDR_SHUFFLER_DECL  , const uint8_t* move_shuffler
#else
#        define EMBDR_SHUFFLER_PARAM
#        define EMBDR_SHUFFLER_DECL
#endif

#if EMBDR_NO_MEMMOVE || EMBDR_USE_NEON
#        define EMBDR_NOT_REMOVE_FIRST_ZERO 1
#else
#        define EMBDR_NOT_REMOVE_FIRST_ZERO 0
#endif

export module embdr.cxxstd.stringFloat;
import embdr.cxxstd.stringView;

template <typename>
struct FloatingStringWidth {};

// Floating-point types: based on __FLT/DBL/LDBL macros
template <>
struct FloatingStringWidth<float> {
        // Maximum decimal string length for float
        // Format: sign + max significand digits + decimal point + exponent marker + exponent
        // __FLT_MANT_DIG__ = 24 (bits), so ~7 decimal digits
        // __FLT_MAX_10_EXP__ = 38
        // Worst case: "-1.234567e+38" = 1 + 7 + 1 + 1 + 2 + 2 = 14
        // Using a conservative value to handle various formats
        static constexpr unsigned int size = 32;
};
template <>
struct FloatingStringWidth<double> {
        // Maximum decimal string length for double
        // __DBL_MANT_DIG__ = 53 (bits), so ~15-16 decimal digits
        // __DBL_MAX_10_EXP__ = 308
        // Worst case: "-1.234567890123456e+308" = 1 + 16 + 1 + 1 + 3 + 3 = 25
        // Using a conservative value to handle various formats
        static constexpr unsigned int size = 64;
};
template <>
struct FloatingStringWidth<long double> {
        // Maximum decimal string length for long double
        // __LDBL_MANT_DIG__ = 64 (bits on x86), so ~19-20 decimal digits
        // __LDBL_MAX_10_EXP__ = 4932 (on x86 extended precision)
        // Worst case: "-1.234567890123456789e+4932" = 1 + 19 + 1 + 1 + 4 + 4 = 30
        // Using a conservative value to handle various formats
        static constexpr unsigned int size = 128;
};
#if defined(__SIZEOF_INT128__)
using uint128_t = __uint128_t;
#endif
constexpr int u64_lz_bits(uint64_t x) {
#if defined(__has_builtin) && __has_builtin(__builtin_clzll)
        if !consteval {
                return __builtin_clzll(x);
        }
#endif
        int n = 64;
        for (; x > 0; x >>= 1)
                --n;
        return n;
}

constexpr int u32_lz_bits(uint32_t x) {
#if defined(__has_builtin) && __has_builtin(__builtin_clz)
        if !consteval {
                return __builtin_clz(x);
        }
#endif
        int n = 32;
        for (; x > 0; x >>= 1)
                --n;
        return n;
}

constexpr int u64_tz_bits(uint64_t x) {
#if defined(__has_builtin) && __has_builtin(__builtin_ctzll)
        if !consteval {
                return __builtin_ctzll(x);
        }
#endif
        int n = 64;
        for (; x > 0; x <<= 1)
                --n;
        return n;
}

constexpr uint64_t umul128_hi64_fallback(uint64_t x, uint64_t y) {
        uint64_t a = x >> 32;
        uint64_t b = static_cast<uint32_t>(x);
        uint64_t c = y >> 32;
        uint64_t d = static_cast<uint32_t>(y);

        uint64_t ac = a * c;
        uint64_t bc = b * c;
        uint64_t ad = a * d;
        uint64_t bd = b * d;

        uint64_t cs = (bd >> 32) + static_cast<uint32_t>(ad) + static_cast<uint32_t>(bc);
        return ac + (ad >> 32) + (bc >> 32) + (cs >> 32);
}

constexpr void umul128_hi64_lo64_fallback(uint64_t x, uint64_t y, uint64_t& hi, uint64_t& lo) {
        uint64_t a = x >> 32;
        uint64_t b = static_cast<uint32_t>(x);
        uint64_t c = y >> 32;
        uint64_t d = static_cast<uint32_t>(y);

        uint64_t ac = a * c;
        uint64_t bc = b * c;
        uint64_t ad = a * d;
        uint64_t bd = b * d;

        uint64_t cs = (bd >> 32) + static_cast<uint32_t>(ad) + static_cast<uint32_t>(bc);
        hi = ac + (ad >> 32) + (bc >> 32) + (cs >> 32);
        lo = (cs << 32) + static_cast<uint32_t>(bd);
}

constexpr void umul128_u64_hi128(uint64_t a_high, uint64_t a_low, uint64_t b, uint64_t* result_high,
                                 uint64_t* result_mid) {
        uint64_t low_high, low_low;
        uint64_t high_high, high_low;
        if !consteval {
#if defined(_MSC_VER) && defined(__X64)
                low_low = _umul128(a_low, b, &low_high);
                high_low = _umul128(a_high, b, &high_high);
                unsigned char carry1 = _addcarry_u64(0, low_high, high_low, result_mid);
                _addcarry_u64(carry1, high_high, 0, result_high);
#elif defined(_MSC_VER) && defined(__ARM64)
                low_high = __umulh(a_low, b);
                high_high = __umulh(a_high, b);
                high_low = a_high * b;
                *result_mid = low_high + high_low;
                *result_high = high_high + (*result_mid < low_high);
#elif defined(__SIZEOF_INT128__)
                uint128_t hi128 = (static_cast<uint128_t>(b) * a_high + ((static_cast<uint128_t>(b) * a_low) >> 64));
                *result_high = hi128 >> 64;
                *result_mid = static_cast<uint64_t>(hi128);
#else
                umul128_hi64_lo64_fallback(a_low, b, low_high, low_low);
                umul128_hi64_lo64_fallback(a_high, b, high_high, high_low);
                *result_mid = low_high + high_low;
                *result_high = high_high + (*result_mid < low_high);
#endif
        } else {
                umul128_hi64_lo64_fallback(a_low, b, low_high, low_low);
                umul128_hi64_lo64_fallback(a_high, b, high_high, high_low);
                *result_mid = low_high + high_low;
                *result_high = high_high + (*result_mid < low_high);
        }
}

constexpr uint64_t umul128_hi64_xjb(uint64_t a, uint64_t b) {
        if !consteval {
#if defined(_MSC_VER) && (defined(__X64) || defined(__ARM64))
                return __umulh(a, b);
#elif defined(__SIZEOF_INT128__)
                return (static_cast<uint128_t>(a) * b) >> 64;
#endif
        }
        return umul128_hi64_fallback(a, b);
}

constexpr uint64_t u128_madd_hi64(uint64_t a, uint64_t b, uint64_t c) {
        if !consteval {
#if defined(_MSC_VER) && defined(__X64)
                uint64_t hi, lo, null_value;
                lo = _umul128(a, b, &hi);
                unsigned char carry1 = _addcarry_u64(0, lo, c, &null_value);
                return hi + carry1;
#elif defined(_MSC_VER) && defined(__ARM64)
                uint64_t hi = __umulh(a, b);
                uint64_t lo = a * b;
                return hi + ((lo + c) < c);
#elif defined(__SIZEOF_INT128__)
                return (static_cast<uint128_t>(a) * b + c) >> 64;
#endif
        }
        uint64_t hi, lo;
        umul128_hi64_lo64_fallback(a, b, hi, lo);
        return hi + ((lo + c) < c);
}

constexpr uint64_t byteswap64_xjb(uint64_t x) {
        if !consteval {
#if defined(__has_builtin) && __has_builtin(__builtin_bswap64)
                return __builtin_bswap64(x);
#elif defined(_MSC_VER)
                return _byteswap_uint64(x);
#endif
        }
        return ((x & 0xff00000000000000) >> 56) | ((x & 0x00ff000000000000) >> 40) | ((x & 0x0000ff0000000000) >> 24) |
               ((x & 0x000000ff00000000) >> 8) | ((x & 0x00000000ff000000) << 8) | ((x & 0x0000000000ff0000) << 24) |
               ((x & 0x000000000000ff00) << 40) | ((x & 0x00000000000000ff) << 56);
}

constexpr uint64_t is_little_endian() {
        if !consteval {
                const int n = 1;
                return *reinterpret_cast<const char*>(&n) == 1;
        } else {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                return 0;
#else
                return 1;
#endif
        }
}

constexpr uint64_t cmov_branchless(const uint64_t condition, const uint64_t true_value, const uint64_t false_value) {
#if !XJB_IS_REAL_GCC || !defined(__amd64__)
        return condition ? true_value : false_value;
#else
        asm volatile("test %2, %2\n\t"
                     "cmovne %1, %0\n\t"
                     : "+r"(false_value)
                     : "r"(true_value), "r"(condition)
                     : "cc");
        return false_value;
#endif
}

template <typename T>
constexpr T* compile_time_memcpy(T* dest, const T* src, size_t count) {
        for (size_t i = 0; i < count; ++i)
                dest[i] = src[i];
        return dest;
}

struct ShortestAscii16 {
#if EMBDR_USE_NEON
        uint64x2_t ascii16;
#elif EMBDR_USE_SSE2
        __m128i ascii16;
#else
        uint64_t hi;
        uint64_t lo;
#endif
        uint64_t dec_sig_len_sub1;
#if EMBDR_NO_MEMMOVE
        int32_t trailing_byte;
#endif
};
struct ShortestAscii {
        uint64_t ascii;
        uint64_t dec_sig_len_sub1;
};

struct DoubleTable {
        static constexpr int e10_DN = -4;
        static constexpr int e10_UP = 15;
        static constexpr int max_dec_sig_len = 17;
        static constexpr int num_pow10 = 323 - (-293) + 1;
        static constexpr int max_first_sig_pos = 5;
        static constexpr int max_one_offset = 17;
        static constexpr int max_buffer_requirement = EMBDR_NO_MEMMOVE ? 33 : 34;
        static constexpr int max_valid_output_len = 24;

        uint64_t pow10_double[num_pow10 * 2];
        uint64_t exp_result_double[324 + 308 + 1];
        alignas(64) unsigned char e10_variable_data[e10_UP - e10_DN + 1 + 1][EMBDR_NO_MEMMOVE ? 64 : 32];
        unsigned char h7[2048];

        consteval DoubleTable() : pow10_double{}, exp_result_double{}, e10_variable_data{}, h7{} {
                struct uint192 {
                        uint64_t w0, w1, w2;
                };
                uint192 current = {0xb2e28cedd086d011, 0x1e53ed49a96272c8, 0xcc5fc196fefd7d0c};
                constexpr uint64_t ten = 0xa000000000000000;

                for (int i = 0; i < num_pow10; ++i) {
                        int e10 = i - 293;
                        this->pow10_double[(num_pow10 - 1 - i) * 2 + 0] =
                            e10 == 0 ? 1ull << 63 : current.w2 + (e10 >= 0 && e10 <= 27);
                        this->pow10_double[(num_pow10 - 1 - i) * 2 + 1] = current.w1 + 1;

                        uint64_t h0 = umul128_hi64_fallback(current.w0, ten);
                        uint64_t h1 = umul128_hi64_fallback(current.w1, ten);
                        uint64_t c0 = h0 + current.w1 * ten;
                        uint64_t c1 = (c0 < h0) + h1 + current.w2 * ten;
                        uint64_t c2 = (c1 < h1) + umul128_hi64_fallback(current.w2, ten);
                        if (c2 >> 63)
                                current = {c0, c1, c2};
                        else
                                current = {c0 << 1, c1 << 1 | c0 >> 63, c2 << 1 | c1 >> 63};
                }

                for (int e10 = -324; e10 <= 308; ++e10) {
                        uint64_t e = e10 < 0 ? ('e' + '-' * 256) : 'e' + '+' * 256;
                        uint64_t e10_abs = e10 < 0 ? -e10 : e10;
                        uint64_t a = e10_abs / 100;
                        uint64_t bc = e10_abs - a * 100;
                        uint64_t b = bc / 10;
                        uint64_t c = bc - b * 10;
                        uint64_t exp_len = 4 + (e10_abs >= 100);
                        uint64_t e10_abs_ascii = (e10_abs >= 100) ? (a + '0') + ((b + '0') << 8) + ((c + '0') << 16)
                                                                  : (b + '0') + ((c + '0') << 8);
                        uint64_t exp_res = e + (e10_abs_ascii << 16) + (exp_len << 56);
                        exp_res = (e10 > e10_DN && e10 < e10_UP) ? 0 : exp_res;
                        this->exp_result_double[e10 + 324] = exp_res;
                }

                for (int e10 = e10_DN; e10 <= e10_UP + 1; e10++) {
                        const int tmp_data_ofs = e10 - e10_DN;
                        auto& current_line = this->e10_variable_data[tmp_data_ofs];
                        uint64_t first_sig_pos = (e10_DN <= e10 && e10 <= -1) ? 1 - e10 : 0;
                        uint64_t dot_pos = (0 <= e10 && e10 <= e10_UP) ? 1 + e10 : 1;
                        uint64_t move_pos = dot_pos + (0 <= e10 || e10 < e10_DN);
                        current_line[max_dec_sig_len + 0] = static_cast<unsigned char>(first_sig_pos);
                        current_line[max_dec_sig_len + 1] = static_cast<unsigned char>(dot_pos);
                        current_line[max_dec_sig_len + 2] = static_cast<unsigned char>(move_pos);

                        for (uint8_t D17 = 0; D17 <= 1; D17++) {
                                const unsigned char one_offset = 15 + D17 + (move_pos > dot_pos && dot_pos <= 15 + D17);
                                current_line[max_dec_sig_len + 3 + D17] = one_offset;
                        }

                        for (int dec_sig_len = 1; dec_sig_len <= max_dec_sig_len; dec_sig_len++) {
                                uint64_t exp_pos = (e10_DN <= e10 && e10 <= -1)
                                                       ? dec_sig_len
                                                       : (0 <= e10 && e10 <= e10_UP
                                                              ? (e10 + 3 > dec_sig_len + 1 ? e10 + 3 : dec_sig_len + 1)
                                                              : (dec_sig_len + 1 - (dec_sig_len == 1)));
                                current_line[dec_sig_len - 1] = static_cast<unsigned char>(exp_pos);
                        }
#if EMBDR_NO_MEMMOVE
                        uint8_t v = 0xf;
                        for (uint64_t j = 0; j < 0x10; ++j)
                                current_line[0x20 + 0x10 + j] = v--;
                        if (move_pos > dot_pos) {
                                for (uint64_t j = 0xf; j > dot_pos && j > 0; --j)
                                        current_line[j + 0x20 + 0x10] = current_line[j + 0x20 + 0x10 - 1];
                        }
                        for (uint64_t j = 0; j < 0x10; ++j) {
                                auto v = current_line[j + 0x20 + 0x10];
                                current_line[j + 0x20] = v ? (v - 1) : 0xf;
                        }
#endif
                }
                for (int exp = 0; exp < 2048; ++exp) {
                        constexpr int offset = 9;
                        int q = exp - 1075 + (exp == 0);
                        int k = (q * 78913) >> 18;
                        int h = q + (((-k - 1) * 217707) >> 16);
                        h7[exp] = static_cast<unsigned char>(h + 1 + offset);
                }
        }
};

struct FloatTable {
        static constexpr int e10_DN = -3;
        static constexpr int e10_UP = 6;
        static constexpr int max_dec_sig_len = 9;
        static constexpr int max_first_sig_pos = 4;
        static constexpr int max_valid_output_len = 15;
        static constexpr int max_buffer_requirement = 21;
        static constexpr int num_pow10 = 44 - (-32) + 1;

        uint64_t pow10_float_reverse[44 - (-32) + 1];
        uint32_t exp_result_float[45 + 38 + 1];
        unsigned char e10_variable_data[e10_UP - (e10_DN) + 1 + 1][16];
        unsigned char h37[256];

        struct const_value_float {
#if EMBDR_IS_AARCH64
                uint64_t c1 =
                    ((static_cast<uint64_t>('0' + '0' * 256) << (36)) + ((static_cast<uint64_t>(1) << (36 - 1)) - 7));
#else
                uint64_t c1 = ((static_cast<uint64_t>('0' + '0' * 256) << (36 - 1)) +
                               ((static_cast<uint64_t>(1) << (36 - 2)) - 7));
#endif
                uint64_t div10000 = 1844674407370956;
                uint64_t m = (1ull << 32) - 10000;
                uint32_t e7 = 10000000;
                uint32_t e6 = 1000000;
#if EMBDR_USE_NEON
                int32x4_t m32_4 = {0x147b000, -100 + 0x10000, 0xce0, -10 + 0x100};
#else
                int32_t m32_4[4] = {0x147b000, -100 + 0x10000, 0xce0, -10 + 0x100};
#endif
        };

        const_value_float constants_float;

        consteval FloatTable() :
            pow10_float_reverse{}, exp_result_float{}, e10_variable_data{}, h37{}, constants_float{} {
                struct uint128_xjb {
                        uint64_t w0, w1;
                };
                uint128_xjb current = {0x67de18eda5814af3, 0xcfb11ead453994ba};
                const uint64_t ten = 0xa000000000000000;

                for (int i = 0; i < num_pow10; ++i) {
                        int e10 = i - 32;
                        pow10_float_reverse[num_pow10 - i - 1] = e10 == 0 ? 1ull << 63 : current.w1 + 1;
                        uint64_t h0 = umul128_hi64_fallback(current.w0, ten);
                        uint64_t c0 = h0 + current.w1 * ten;
                        uint64_t c1 = (c0 < h0) + umul128_hi64_fallback(current.w1, ten);
                        if (c1 >> 63)
                                current = {c0, c1};
                        else
                                current = {c0 << 1, c1 << 1 | c0 >> 63};
                }

                for (int e10 = -45; e10 <= 38; e10++) {
                        uint64_t e = e10 < 0 ? ('e' + '-' * 256) : ('e' + '+' * 256);
                        uint64_t e10_abs = e10 < 0 ? -e10 : e10;
                        uint64_t a = e10_abs / 10;
                        uint64_t b = e10_abs - a * 10;
                        uint64_t e10_abs_ascii = (a + '0') + ((b + '0') << 8);
                        uint64_t exp_res = e + (e10_abs_ascii << 16);
                        exp_res = (e10 >= e10_DN && e10 <= e10_UP) ? 0 : exp_res;
                        exp_result_float[e10 + 45] = static_cast<uint32_t>(exp_res);
                }

                for (int e10 = e10_DN; e10 <= e10_UP + 1; e10++) {
                        int tmp_data_ofs = e10 - e10_DN;
                        uint64_t first_sig_pos = (e10_DN <= e10 && e10 <= -1) ? 1 - e10 : 0;
                        uint64_t dot_pos = (0 <= e10 && e10 <= e10_UP) ? 1 + e10 : 1;
                        uint64_t move_pos = dot_pos + ((0 <= e10 || e10 < e10_DN));
                        auto& current_line = e10_variable_data[tmp_data_ofs];
                        current_line[max_dec_sig_len + 0] = static_cast<unsigned char>(first_sig_pos);
                        current_line[max_dec_sig_len + 1] = static_cast<unsigned char>(dot_pos);
                        current_line[max_dec_sig_len + 2] = static_cast<unsigned char>(move_pos);

                        for (int dec_sig_len = 1; dec_sig_len <= max_dec_sig_len; dec_sig_len++) {
                                const uint64_t exp_pos =
                                    (e10_DN <= e10 && e10 <= -1)
                                        ? dec_sig_len
                                        : (0 <= e10 && e10 <= e10_UP
                                               ? (e10 + 3 > dec_sig_len + 1 ? e10 + 3 : dec_sig_len + 1)
                                               : (dec_sig_len + 1 - (dec_sig_len == 1)));
                                current_line[dec_sig_len - 1] = static_cast<unsigned char>(exp_pos);
                        }
                }

                for (int exp = 0; exp < 256; exp++) {
                        const int exp_bin = exp - 150 + (exp == 0);
                        int k = (exp_bin * 1233) >> 12;
                        int h37_precalc = (36 + 1) + exp_bin + ((k * -1701 + (-1701)) >> 9);
                        h37[exp] = static_cast<unsigned char>(h37_precalc);
                }
        }
};
struct ConstValueDouble {
        static constexpr int e10_DN = DoubleTable::e10_DN;
        static constexpr int e10_UP = DoubleTable::e10_UP;
        static constexpr int max_dec_sig_len = DoubleTable::max_dec_sig_len;
        static constexpr int num_pow10 = DoubleTable::num_pow10;

        uint64_t c1 = 78913ull << (64 - 18);
        uint64_t c2 = static_cast<uint64_t>(-217707);
        uint64_t c3 = static_cast<uint64_t>(1e15) - 1;
        uint64_t c4 = (1ull << 63) + 6;
        uint64_t c5 = static_cast<uint64_t>(-131072);
        uint64_t c6 = (1 << 9) - 1;
        uint64_t mul_const = 0xabcc77118461cefd;
        int64_t hundred_million = -100000000;
        uint64_t div10000 = 1844674407370956;
        uint64_t div10000_m = 0x100000000 - 10000;
        double div10000_2_d = static_cast<double>(-10000 + 0x100000000);
        int64_t div10000_2 = 0xd1b7176000;
#if EMBDR_USE_NEON
        int32x4_t multipliers32 = {0x68db8bb, -10000 + 0x10000, 0x147b000, -100 + 0x10000};
        int16x8_t multipliers16 = {0xce0, -10 + 0x100, '0' + '0' * 256};
#else
        int32_t multipliers32[4] = {0x68db8bb, -10000 + 0x10000, 0x147b000, -100 + 0x10000};
        int16_t multipliers16[8] = {0xce0, -10 + 0x100, '0' + '0' * 256};
#endif
#if EMBDR_USE_NEON
        uint8_t shuffle_table_neon[32] = {
            6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
        };
        uint8_t shuffle_table_memmove[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0,
                                             0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  10, 11, 12, 13, 14, 15};
#endif
#if EMBDR_USE_NEON && EMBDR_NO_MEMMOVE
        uint8_t reverse_shuffle_table[17] = {0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
#endif
#if EMBDR_USE_NEON
        uint8_t shuffle_table_neon[32] = {
            6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
        };
        uint8_t shuffle_table_memmove[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0,
                                             0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  10, 11, 12, 13, 14, 15};
#endif
#if (EMBDR_NO_MEMMOVE || EMBDR_USE_NEON) && (defined(__SSSE3__) && __SSSE3__)
        uint8_t shuffle_table[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0};
#endif
};
constexpr ConstValueDouble constants_double;
alignas(64) constexpr DoubleTable double_table;
alignas(64) constexpr FloatTable float_table;

constexpr uint64_t compute_double_dec_sig_len(uint64_t up_down, int tz, uint64_t D17) {
        uint64_t len = (EMBDR_NOT_REMOVE_FIRST_ZERO ? 14 + D17 : 15) - static_cast<uint64_t>(tz);
        const uint64_t ret = cmov_branchless(up_down, len, 15 + D17);
        return ret;
}

constexpr uint64_t compute_double_dec_sig_len_sse2(uint64_t up_down, int tz, uint64_t D17) {
        uint64_t len = (0 ? 14 + D17 : 15) + 48 - static_cast<uint64_t>(tz);
        const uint64_t ret = cmov_branchless(up_down, len, 15 + D17);
        return ret;
}

constexpr uint64_t compute_float_dec_sig_len(uint64_t up_down, int tz, uint64_t lz) {
        const uint64_t ret = cmov_branchless(up_down, (7 - lz) - tz, 8 - lz);
        return ret;
}

constexpr ShortestAscii to_ascii8_final(uint64_t abcdefgh_BCD, uint32_t lz, uint32_t up_down) {
        const uint64_t ZERO = 0x3030303030303030;
        uint32_t tz = u64_lz_bits(abcdefgh_BCD) >> 3;
        abcdefgh_BCD = abcdefgh_BCD >> (lz << 3);
        abcdefgh_BCD = is_little_endian() ? abcdefgh_BCD : byteswap64_xjb(abcdefgh_BCD);
        return {abcdefgh_BCD | ZERO, cmov_branchless(up_down, (7 ^ lz) - tz, 8 - lz)};
}

constexpr ShortestAscii to_ascii8_buffer(const uint64_t m, const uint32_t up_down, uint32_t& lz,
                                         const FloatTable::const_value_float* c) {
        uint64_t abcdefgh_BCD = 0;
        if !consteval {
#if EMBDR_USE_NEON
                int32x2_t tenthousands =
                    vreinterpret_s32_u64(vcreate_u64(m + c->m * ((m * (uint128_t)c->div10000) >> 64)));
                int16x4_t hundreds = vreinterpret_s16_s32(
                    vmla_n_s32(tenthousands, vqdmulh_s32(tenthousands, vdup_n_s32(c->m32_4[0])), c->m32_4[1]));
                int16x4_t BCD_big_endian =
                    vmla_n_s16(hundreds, vqdmulh_s16(hundreds, vdup_n_s16(c->m32_4[2])), c->m32_4[3]);
                uint64_t hgfedcba_BCD = vget_lane_u64(vreinterpret_u64_s16(BCD_big_endian), 0);
                uint64_t abcdefgh_BCD = byteswap64_xjb(hgfedcba_BCD);
#elif EMBDR_USE_SSE2
                uint64_t aabb_ccdd_merge =
                    (m << 32) + (1 - (10000ull << 32)) * ((m * static_cast<uint128_t>(1844674407370956)) >> 64);
                __m128i y = _mm_set1_epi64x(aabb_ccdd_merge);
                __m128i y_div_100 = _mm_srli_epi16(_mm_mulhi_epu16(y, _mm_set1_epi16(0x147b)), 3);
                __m128i y_mod_100 = _mm_sub_epi16(y, _mm_mullo_epi16(y_div_100, _mm_set1_epi16(100)));
                __m128i z = _mm_or_si128(y_div_100, _mm_slli_epi32(y_mod_100, 16));
                __m128i z_div_10 = _mm_mulhi_epi16(z, _mm_set1_epi16(0x199a));
                __m128i tmp = _mm_sub_epi16(_mm_slli_epi16(z, 8), _mm_mullo_epi16(_mm_set1_epi16(2559), z_div_10));
                abcdefgh_BCD = _mm_cvtsi128_si64(tmp);
#endif
        } else {
                int64_t aabb_ccdd_merge = (m << 32) + (1 - (10000ull << 32)) * ((m * 109951163) >> 40);
                int64_t aa_bb_cc_dd_merge =
                    (aabb_ccdd_merge << 16) +
                    (1 - (100ull << 16)) * (((aabb_ccdd_merge * 10486) >> 20) & ((0x7Full << 32) | 0x7Full));
                abcdefgh_BCD = (aa_bb_cc_dd_merge << 8) +
                               (1 - (10ull << 8)) * (((aa_bb_cc_dd_merge * 103) >> 10) &
                                                     ((0xFull << 48) | (0xFull << 32) | (0xFull << 16) | 0xFull));
        }
        return to_ascii8_final(abcdefgh_BCD, lz, up_down);
}

constexpr ShortestAscii16 to_ascii16_buffer(char* buf, const uint64_t m, const uint64_t up_down, const uint64_t D17,
                                            const ConstValueDouble* cv EMBDR_SHUFFLER_DECL) {
        constexpr uint64_t ZERO = 0x3030303030303030;
        uint32_t abcdefgh = umul128_hi64_xjb(m, cv->mul_const) >> (90 - 64);
        int64_t hundred_million = cv->hundred_million;
        uint32_t ijklmnop = m + abcdefgh * hundred_million;

        if !consteval {
#if EMBDR_USE_NEON
#        if EMBDR_NO_MEMMOVE
                uint64x1_t hundredmillions = {ijklmnop | ((uint64_t)abcdefgh << 32)};
#        else
                uint64x1_t hundredmillions = {abcdefgh | ((uint64_t)ijklmnop << 32)};
#        endif
                int32x2_t hundredmillions_s32 = vreinterpret_s32_u64(hundredmillions);
                        int32x2_t high_10000 = vreinterpret_s32_u32( vshr_n_u32( vreinterpret_u32_s32( vqdmulh_s32( hundredmillions_s32, vdup_n_s32( cv->multipliers32[0] ) ), 9 ) );
                        int32x2_t tenthousands = vmla_s32( hundredmillions_s32, high_10000, vdup_n_s32( cv->multipliers32[1] ) );
                        int32x4_t extended = vreinterpretq_s32_u32( vshll_n_u16( vreinterpret_u16_s32( tenthousands ), 0 ) );
                        int32x4_t high_100 = vqdmulhq_s32( extended, vdupq_n_s32( cv->multipliers32[2] ) );
                        int16x8_t hundreds = vreinterpretq_s16_s32( vmlaq_s32( extended, high_100, vdupq_n_s32( cv->multipliers32[3] ) ) );
                        int16x8_t high_10 = vqdmulhq_s16( hundreds, vdupq_n_s16( cv->multipliers16[0] ) );
                        int16x8_t BCD_big_endian = vmlaq_s16( hundreds, high_10, vdupq_n_s16( cv->multipliers16[1] ) );
#        if EMBDR_NO_MEMMOVE
                        int8x16_t ascii16_swapped = vorrq_s8( vreinterpretq_s8_s16( BCD_big_endian ), vdupq_n_s8( '0' ) );
                        uint16x8_t is_not_zero = vreinterpretq_u16_u8( vcgtzq_s8( vreinterpretq_s8_s16( BCD_big_endian ) ) );
                        int8x16_t ascii16 = vreinterpretq_s8_u8( vqtbl1q_u8( vreinterpretq_u8_s8( ascii16_swapped ), vld1q_u8( move_shuffler ) ) );
                        uint64_t zeroes = vget_lane_u64( vreinterpret_u64_u8( vshrn_n_u16( is_not_zero, 4 ) ), 0 );
                        int tz = u64_tz_bits( zeroes ) >> 2;
                        uint64_t dec_sig_len = up_down ? (EMBDR_NOT_REMOVE_FIRST_ZERO ? (14 + D17) - tz : 15 - tz) : 15 + D17;
                        return {vreinterpretq_u64_s8( ascii16 ), dec_sig_len, vgetq_lane_s32( vreinterpretq_s32_s8( ascii16_swapped ), 0 )};
#        else
                        int8x16_t BCD_little_endian = vreinterpretq_s8_u8( vqtbl1q_u8( vreinterpretq_u8_s16( BCD_big_endian ),
                                                                  vld1q_u8( &cv->shuffle_table_neon[(15 + D17) & 16] ) ) );
                        int8x16_t ascii16 = vorrq_s8( BCD_little_endian, vdupq_n_s8( '0' ) );
                        vst1q_s8( (int8_t*)buf, vdupq_n_s8( '0' ) );
                        vst1q_s8( (int8_t*)(buf + 16), vdupq_n_s8( '0' ) );
                        uint16x8_t is_not_zero = vreinterpretq_u16_u8( vcgtzq_s8( BCD_little_endian ) );
                        uint64_t zeroes = vget_lane_u64( vreinterpret_u64_u8( vshrn_n_u16( is_not_zero, 4 ) ), 0 );
                        uint32_t tz = u64_lz_bits( zeroes ) >> 2;
                        uint32_t dec_sig_len = up_down ? ((0) ? (14 + D17) - tz : 15 - tz) : 15 + D17;
                        return {vreinterpretq_u64_s8( ascii16 ), dec_sig_len};
#        endif
#elif EMBDR_USE_SSE2
                __m128i x = _mm_unpacklo_epi64(_mm_cvtsi32_si128(ijklmnop), _mm_cvtsi32_si128(abcdefgh));
                __m128i y =
                    _mm_add_epi64(x, _mm_mul_epu32(_mm_set1_epi64x((1ull << 32) - 10000),
                                                   _mm_srli_epi64(_mm_mul_epu32(x, _mm_set1_epi64x(109951163)), 40)));
                __m128i y_div_100 = _mm_srli_epi16(_mm_mulhi_epu16(y, _mm_set1_epi16(0x147b)), 3);
                __m128i y_mod_100 = _mm_sub_epi16(y, _mm_mullo_epi16(y_div_100, _mm_set1_epi16(100)));
                __m128i z = _mm_or_si128(y_div_100, _mm_slli_epi32(y_mod_100, 16));
                __m128i z_div_10 = _mm_mulhi_epi16(z, _mm_set1_epi16(0x199a));
                __m128i bcd_swapped =
                    _mm_sub_epi16(_mm_slli_epi16(z, 8), _mm_mullo_epi16(_mm_set1_epi16(2559), z_div_10));
                __m128i little_endian_bcd = _mm_shuffle_epi32(bcd_swapped, _MM_SHUFFLE(0, 1, 2, 3));

#        if EMBDR_NOT_REMOVE_FIRST_ZERO
                __m128i move_mask;
                memcpy(&move_mask, &(cv->shuffle_table[D17 ? 0 : 1]), 16);
                little_endian_bcd = _mm_shuffle_epi8(little_endian_bcd, move_mask);
#        endif

                int mask = _mm_movemask_epi8(_mm_cmpgt_epi8(little_endian_bcd, _mm_setzero_si128()));
                int tz = u64_lz_bits(mask);

                __m128i ascii16 = _mm_add_epi8(little_endian_bcd, _mm_set1_epi8('0'));
                memset(buf, '0', 32);
                return {ascii16, compute_double_dec_sig_len_sse2(up_down, tz, D17)};
#endif
        } else {
                uint64_t abcd_efgh = abcdefgh + (0x100000000 - 10000) * ((abcdefgh * 0x68db8bbull) >> 40);
                uint64_t ijkl_mnop = ijklmnop + (0x100000000 - 10000) * ((ijklmnop * 0x68db8bbull) >> 40);
                uint64_t ab_cd_ef_gh = abcd_efgh + (0x10000 - 100) * (((abcd_efgh * 0x147b) >> 19) & 0x7f0000007f);
                uint64_t ij_kl_mn_op = ijkl_mnop + (0x10000 - 100) * (((ijkl_mnop * 0x147b) >> 19) & 0x7f0000007f);
                uint64_t a_b_c_d_e_f_g_h =
                    ab_cd_ef_gh + (0x100 - 10) * (((ab_cd_ef_gh * 0x67) >> 10) & 0xf000f000f000f);
                uint64_t i_j_k_l_m_n_o_p =
                    ij_kl_mn_op + (0x100 - 10) * (((ij_kl_mn_op * 0x67) >> 10) & 0xf000f000f000f);
                int abcdefgh_tz = u64_tz_bits(a_b_c_d_e_f_g_h);
                int ijklmnop_tz = u64_tz_bits(i_j_k_l_m_n_o_p);
                uint64_t abcdefgh_bcd = is_little_endian() ? byteswap64_xjb(a_b_c_d_e_f_g_h) : a_b_c_d_e_f_g_h;
                uint64_t ijklmnop_bcd = is_little_endian() ? byteswap64_xjb(i_j_k_l_m_n_o_p) : i_j_k_l_m_n_o_p;
                int tz = (ijklmnop == 0) ? 64 + abcdefgh_tz : ijklmnop_tz;
                tz = tz / 8;
                for (int i = 0; i < 32; ++i)
                        buf[i] = '0';
                ShortestAscii16 result{};
#if EMBDR_USE_NEON || EMBDR_USE_SSE2
                struct M128IUnion {
                        uint64_t lo;
                        uint64_t hi;
                };
                M128IUnion m_union;
                m_union.hi = abcdefgh_bcd | ZERO;
                m_union.lo = ijklmnop_bcd | ZERO;
                result.ascii16 = std::bit_cast<__m128i>(m_union);
                result.dec_sig_len_sub1 = compute_double_dec_sig_len(up_down, tz, D17);
#else
                result.hi = abcdefgh_bcd | ZERO;
                result.lo = ijklmnop_bcd | ZERO;
                result.dec_sig_len_sub1 = compute_double_dec_sig_len(up_down, tz, D17);
#endif
                return result;
        }
}

constexpr char* xjb64(char* buff, const double v) {
        const ConstValueDouble* cv = &constants_double;
        constexpr auto t = &double_table;
#if EMBDR_IS_AARCH64 && (defined(__clang__) || defined(__GNUC__))
        asm("" : "+r"(cv));
#endif
        uint64_t vi;
        if !consteval {
                memcpy(&vi, &v, sizeof(v));
        } else {
                vi = std::bit_cast<uint64_t>(v);
        }
        *buff = '-';
        buff += vi >> 63;
        uint64_t sig = vi & ((1ull << 52) - 1);
        uint64_t exp = (vi << 1) >> 53;
        int64_t q = static_cast<int64_t>(exp) - 1075;
        uint64_t c = sig | (1ull << 52);
#if EMBDR_IS_X64
        if (((exp + 1) & 2047) <= 1) [[unlikely]]
#endif
        {
                if (exp == 0) {
                        if (sig <= 1) {
                                const char* str = sig ? "5e-324\0" : "0.0\0\0\0\0";
                                if !consteval {
                                        return static_cast<char*>(memcpy(buff, str, 8)) + (sig ? 6 : 3);
                                } else {
                                        compile_time_memcpy(buff, str, sig ? 6 : 3);
                                        return buff + (sig ? 6 : 3);
                                }
                        }
                        c = sig;
                        q = 1 - 1075;
                }
                if (exp == 2047) {
                        if !consteval {
                                return static_cast<char*>(memcpy(buff, sig ? "nan" : "inf", 4)) + 3;
                        } else {
                                compile_time_memcpy(buff, sig ? "nan" : "inf", 4);
                                return buff + 3;
                        }
                }
        }
        unsigned char h7_precalc = t->h7[exp];
        constexpr int offset = 9;
        bool irregular = sig == 0;
        int64_t k = 0;
        uint64_t m_up = 0;
        uint32_t one = 0;
        uint32_t up_down = 0;
        {
                uint64_t hi64 = 0, lo64 = 0, pow10_hi = 0, pow10_lo = 0;
#if defined(__SIZEOF_INT128__) && EMBDR_IS_AARCH64
                k = ((int64_t)q * (uint128_t)(78913ull << (64 - 18))) >> 64;
#else
                k = (q * 78913) >> 18;
#endif

                const uint64_t* pow10_ptr = t->pow10_double + 323 * 2 + 2 + k * 2;
                pow10_hi = pow10_ptr[0];
                pow10_lo = pow10_ptr[1];
                umul128_u64_hi128(pow10_hi, pow10_lo, c << h7_precalc, &hi64, &lo64);
                uint64_t dot_one = (hi64 << (64 - offset)) | (lo64 >> offset);
                uint64_t half_ulp = (pow10_hi >> ((1 + offset) - h7_precalc)) + ((c + 1) & 1);
                bool up = half_ulp > ~0 - dot_one;
                bool down = half_ulp > dot_one;
                m_up = (hi64 >> offset) + up;
                up_down = up + down;
                uint64_t half = (dot_one == (1ull << 62)) ? 0 : cv->c4;
                one = static_cast<uint32_t>(u128_madd_hi64(dot_one, 10, half));
        }
        if (irregular) [[unlikely]] {
                k = (q * 315653 - 131072) >> 20;
                int64_t h = q + ((k * -217707 - 217707) >> 16);
                uint64_t pow10_hi = t->pow10_double[323 * 2 + 2 + k * 2];
                uint64_t half_ulp = pow10_hi >> (-h);
                uint64_t dot_one = (pow10_hi << (53 + h));
                uint64_t up = half_ulp > ~0 - dot_one;
                uint64_t down = (half_ulp >> 1) > dot_one;
                m_up = (pow10_hi >> (11 - h)) + up;
                up_down = up + down;
                one = ((dot_one >> (53 + h)) * 5 + (1 << (9 - h))) >> (10 - h);
                if ((((dot_one >> 54) * 5) & ((1 << 9) - 1)) > ((half_ulp >> 55) * 5))
                        one = (((dot_one >> 54) * 5) >> 9) + 1;
                if (dot_one == (1ull << 62))
                        one = 2;
        }
        uint64_t D17 = m_up > static_cast<uint64_t>(cv->c3);
        uint64_t mr = D17 ? m_up : m_up * 10;

        int64_t e10 = k + (15 + D17);
        constexpr int64_t e10_DN = t->e10_DN;
        constexpr int64_t e10_UP = t->e10_UP;
        constexpr uint64_t interval = e10_UP - e10_DN + 1;
#if EMBDR_IS_AARCH64
        uint32_t e10_3 = static_cast<int32_t>(e10) + static_cast<int32_t>(-e10_DN);
        uint32_t e10_data_ofs = e10_3 < interval ? e10_3 : interval;
#else
        uint64_t e10_3 = e10 + (-e10_DN);
        uint64_t e10_data_ofs = e10_3 < interval ? e10_3 : interval;
#endif

#if EMBDR_NO_MEMMOVE
        if !consteval {
                memset(buff, '0', 8);
        } else {
                for (int i = 0; i < 8; ++i)
                        buff[i] = '0';
        }
        const uint8_t* move_shuffler = &(t->e10_variable_data[e10_data_ofs][32 + D17 * 16]);
#else
        if !consteval {
                memset(buff, '0', 32);
        } else {
                for (int i = 0; i < 32; ++i)
                        buff[i] = '0';
        }
#endif
        ShortestAscii16 s =
            to_ascii16_buffer(buff, EMBDR_NOT_REMOVE_FIRST_ZERO ? m_up : mr, up_down, D17, cv EMBDR_SHUFFLER_PARAM);
        uint64_t first_sig_pos = t->e10_variable_data[e10_data_ofs][17 + 0];
        uint64_t dot_pos = t->e10_variable_data[e10_data_ofs][17 + 1];
        uint64_t one_offset = t->e10_variable_data[e10_data_ofs][17 + 3 + D17];
        uint64_t move_pos = t->e10_variable_data[e10_data_ofs][17 + 2];
        uint64_t exp_pos = t->e10_variable_data[e10_data_ofs][s.dec_sig_len_sub1];
        char* buf_origin = buff;
        buff += first_sig_pos;
#if EMBDR_USE_NEON_OR_SSE2
        if !consteval {
                memcpy(buff, &(s.ascii16), 16);
        } else {
                struct M128IUnion {
                        uint64_t lo;
                        uint64_t hi;
                };
                auto m = std::bit_cast<M128IUnion>(s.ascii16);
                struct uint64_bytes {
                        char b[sizeof(uint64_t)];
                };
                auto hi_bytes = std::bit_cast<uint64_bytes>(m.hi);
                auto lo_bytes = std::bit_cast<uint64_bytes>(m.lo);
                for (int i = 0; i < 8; ++i)
                        buff[i] = hi_bytes.b[i];
                for (int i = 0; i < 8; ++i)
                        buff[8 + i] = lo_bytes.b[i];
        }
#else
        if !consteval {
                memcpy(buff + 0, &(s.hi), 8);
                memcpy(buff + 8, &(s.lo), 8);
        } else {
                struct Uint64Bytes {
                        char b[sizeof(uint64_t)];
                };
                auto hi_bytes = std::bit_cast<Uint64Bytes>(s.hi);
                auto lo_bytes = std::bit_cast<Uint64Bytes>(s.lo);
                for (int i = 0; i < 8; ++i)
                        buff[i] = hi_bytes.b[i];
                for (int i = 0; i < 8; ++i)
                        buff[8 + i] = lo_bytes.b[i];
        }
#endif

        one |= 0x30303030;
        if !consteval {
                memcpy(&buff[15 + D17], &one, 4);
                memmove(&buff[move_pos], &buff[dot_pos], 16);
        } else {
                struct char_uint32_bytes {
                        char b[sizeof(uint32_t)];
                };
                auto one_bytes = std::bit_cast<char_uint32_bytes>(one);
                for (int i = 0; i < 4; ++i)
                        buff[15 + D17 + i] = one_bytes.b[i];
                if (move_pos < dot_pos) {
                        for (int i = 0; i < 16; ++i)
                                buff[move_pos + i] = buff[dot_pos + i];
                } else if (move_pos > dot_pos) {
                        for (int i = 16; i > 0; --i)
                                buff[move_pos + i - 1] = buff[dot_pos + i - 1];
                }
        }
        buf_origin[dot_pos] = '.';

#if EMBDR_IS_AARCH64
        if (exp == 0) [[unlikely]]
#endif
                if (m_up < static_cast<uint64_t>(1e14)) [[unlikely]] {
                        uint64_t lz = 0;
                        while (buff[2 + lz] == '0')
                                lz++;
                        lz += 2;
                        e10 -= lz - 1;
                        buff[0] = buff[lz];
                        if !consteval {
                                memmove(&buff[2], &buff[lz + 1], 16);
                        } else {
                                if (2 < lz + 1) {
                                        for (int i = 0; i < 16; ++i)
                                                buff[2 + i] = buff[lz + 1 + i];
                                } else if (2 > lz + 1) {
                                        for (int i = 16; i > 0; --i)
                                                buff[2 + i - 1] = buff[lz + 1 + i - 1];
                                }
                        }
                        exp_pos = exp_pos - lz + (exp_pos - lz != 1);
                }

        uint64_t exp_result = t->exp_result_double[e10 + 324];
        buff += exp_pos;
        if !consteval {
                memcpy(buff, &exp_result, 8);
        } else {
                struct CharUint64Bytes {
                        char b[sizeof(uint64_t)];
                };
                auto exp_result_bytes = std::bit_cast<CharUint64Bytes>(exp_result);
                for (int i = 0; i < 8; ++i)
                        buff[i] = exp_result_bytes.b[i];
        }
        const uint64_t exp_len = exp_result >> 56;
        return buff + exp_len;
}

constexpr char* xjb32(char* buff, const float v) {
        using namespace embdr::cxxstd;
        constexpr auto t = &float_table;
        const FloatTable::const_value_float* c = &t->constants_float;

        uint32_t vi;
        if !consteval {
                memcpy(&vi, &v, 4);
        } else {
                vi = std::bit_cast<uint32_t>(v);
        }
        buff[0] = '-';
        buff += vi >> 31;
        const uint32_t sig = vi & ((1 << 23) - 1);
        const uint64_t exp = (vi << 1) >> 24;
        uint64_t sig_bin = sig | (1 << 23);
        int64_t exp_bin = static_cast<int64_t>(exp) - 150;
        if (exp == 0) {
                if (sig == 0) {
                        constexpr SimpleStringView zero = "0.0";
                        if !consteval {
                                return static_cast<char*>(memcpy(buff, zero.data(), 4)) + zero.size();
                        } else {
                                compile_time_memcpy(buff, zero.data(), zero.size());
                                return buff + zero.size();
                        }
                }
                exp_bin = 1 - 150;
                sig_bin = sig;
        }
        if (exp == 255) {
                constexpr SimpleStringView nan = "nan";
                constexpr SimpleStringView inf = "inf";
                if !consteval {
                        return static_cast<char*>(memcpy(buff, sig ? nan.data() : inf.data(), 4)) + 3;
                } else {
                        compile_time_memcpy(buff, sig ? nan.data() : inf.data(), 4);
                        return buff + 3;
                }
        }
        uint32_t h37_precalc = t->h37[exp];
        bool irregular = sig == 0;
        constexpr int BIT = 36;

        int64_t k = (exp_bin * 1233) >> 12;
        if (irregular) {
                k = (exp_bin * 1233 - 512) >> 12;
                h37_precalc = (BIT + 1) + exp_bin + ((k * -1701 + (-1701)) >> 9);
        }
        uint64_t pow10_hi = t->pow10_float_reverse[45 + k];
        uint64_t cb = sig_bin << h37_precalc;
        uint64_t hi64 = umul128_hi64_xjb(cb, pow10_hi);
        uint64_t half_ulp = (pow10_hi >> (65 - h37_precalc)) + ((sig + 1) & 1);
        uint64_t dot_one_36bit = hi64 & ((static_cast<uint64_t>(1) << BIT) - 1);
        uint32_t m_up = static_cast<uint32_t>((hi64 + half_ulp) >> BIT);
        uint32_t up_down = m_up > static_cast<uint32_t>((hi64 - (half_ulp >> 0)) >> BIT);
#if EMBDR_IS_AARCH64
        uint32_t one = (dot_one_36bit * 10 + c->c1 + (dot_one_36bit >> (BIT - 4))) >> (BIT);
#else
        uint32_t one = (dot_one_36bit * 5 + c->c1 + (dot_one_36bit >> (BIT - 4))) >> (BIT - 1);
#endif

        if (irregular) {
                if ((exp_bin == 31 - 150) | (exp_bin == 214 - 150) | (exp_bin == 217 - 150))
                        ++one;
                up_down = m_up > static_cast<uint32_t>((hi64 - (half_ulp >> 1)) >> BIT);
        }
        uint32_t lz = (m_up < c->e7) + (m_up < c->e6);
        if !consteval {
                memset(buff, '0', 16);
        } else {
                for (int i = 0; i < 16; ++i)
                        buff[i] = '0';
        }
        ShortestAscii s = to_ascii8_buffer(m_up, up_down, lz, c);
        int32_t e10 = static_cast<int32_t>(k) + (8 - lz);
        constexpr int64_t e10_DN = t->e10_DN;
        constexpr int64_t e10_UP = t->e10_UP;
        constexpr uint64_t interval = e10_UP - e10_DN + 1;
        uint32_t e10_3 = e10 + (-e10_DN);
        uint32_t e10_data_ofs = e10_3 < interval ? e10_3 : interval;
        uint64_t first_sig_pos = t->e10_variable_data[e10_data_ofs][9 + 0];
        uint64_t dot_pos = t->e10_variable_data[e10_data_ofs][9 + 1];
        uint64_t move_pos = t->e10_variable_data[e10_data_ofs][9 + 2];
        uint64_t exp_pos = t->e10_variable_data[e10_data_ofs][s.dec_sig_len_sub1];
        char* buf_origin = buff;
        buff += first_sig_pos;
        if !consteval {
                memcpy(buff, &(s), 8);
                memcpy(&buff[8 - lz], &one, 4);
                memmove(&buff[move_pos], &buff[dot_pos], 8);
        } else {
                struct TmpAscii {
                        ShortestAscii ascii;
                };
                const TmpAscii values{s};
                struct CharTmpAsciiBytes {
                        char b[sizeof(TmpAscii)];
                };
                struct CharUin32Bytes {
                        char b[sizeof(uint32_t)];
                };
                auto s_bytes = std::bit_cast<CharTmpAsciiBytes>(values.ascii);
                for (int i = 0; i < 8; ++i)
                        buff[i] = s_bytes.b[i];
                auto one_bytes = std::bit_cast<CharUin32Bytes>(one);
                for (int i = 0; i < 4; ++i)
                        buff[8 - lz + i] = one_bytes.b[i];
                if (move_pos < dot_pos) {
                        for (int i = 0; i < 8; ++i)
                                buff[move_pos + i] = buff[dot_pos + i];
                } else if (move_pos > dot_pos) {
                        for (int i = 8; i > 0; --i)
                                buff[move_pos + i - 1] = buff[dot_pos + i - 1];
                }
        }
        buf_origin[dot_pos] = '.';

#if EMBDR_IS_AARCH64
        if (exp == 0)
#endif
                if (m_up < 100000) {
                        uint64_t lz2 = 0;
                        uint64_t u;
                        if !consteval {
                                memcpy(&u, &buff[2], 8);
                        } else {
                                for (int i = 0; i < 8; ++i)
                                        reinterpret_cast<char*>(&u)[i] = buff[2 + i];
                        }
                        u = is_little_endian() ? u : byteswap64_xjb(u);
                        lz2 = u64_tz_bits(u & 0x0f0f0f0f0f0f0f0f) / 8;
                        lz2 += 2;
                        e10 -= lz2 - 1;
                        buff[0] = buff[lz2];
                        if !consteval {
                                memmove(&buff[2], &buff[lz2 + 1], 8);
                        } else {
                                if (2 < lz2 + 1) {
                                        for (int i = 0; i < 8; ++i)
                                                buff[2 + i] = buff[lz2 + 1 + i];
                                } else if (2 > lz2 + 1) {
                                        for (int i = 8; i > 0; --i)
                                                buff[2 + i - 1] = buff[lz2 + 1 + i - 1];
                                }
                        }
                        exp_pos = exp_pos - lz2 + (exp_pos - lz2 != 1);
                }
        uint32_t exp_result_u32 = t->exp_result_float[45 + e10];
        if constexpr (!is_little_endian())
                exp_result_u32 = ((exp_result_u32 & 0xff000000) >> 24) | ((exp_result_u32 & 0x00ff0000) >> 8) |
                                 ((exp_result_u32 & 0x0000ff00) << 8) | ((exp_result_u32 & 0x0000000ff) << 24);
        const uint64_t exp_result_u64 =
            is_little_endian() ? exp_result_u32 : static_cast<uint64_t>(exp_result_u32) << 32;
        buff += exp_pos;
        if !consteval {
                memcpy(buff, &exp_result_u64, 8);
        } else {
                struct char_uint64_bytes {
                        char b[sizeof(uint64_t)];
                };
                auto exp_result_bytes = std::bit_cast<char_uint64_bytes>(exp_result_u64);
                for (int i = 0; i < 8; ++i)
                        buff[i] = exp_result_bytes.b[i];
        }
        return buff + (exp_result_u64 & 4);
}

template <typename ValFn>
constexpr unsigned long long cxxdtoa(char* buff, const double value, ValFn&& fn) {
        if (buff == nullptr)
                return 0;
        char* result = fn(buff, value);
        unsigned long long len = static_cast<size_t>(result - buff);
        buff[len] = '\0';
        return len;
}
template <typename ValFn>
constexpr unsigned long long cxxdtoa(char* buff, const double value, const int precision, ValFn&& fn) {
        if (buff == nullptr)
                return 0;
        if (precision < 0)
                return cxxdtoa(buff, value, fn);
        unsigned long long len = cxxdtoa(buff, value, fn);
        char* dot_pos;
        if !consteval {
                dot_pos = static_cast<char*>(memchr(buff, '.', len));
        } else {
                dot_pos = nullptr;
                for (size_t i = 0; i < len; ++i)
                        if (buff[i] == '.') {
                                dot_pos = buff + i;
                                break;
                        }
        }
        if (dot_pos != nullptr) {
                const int decimal_places = static_cast<int>(len - (dot_pos - buff + 1));
                if (decimal_places > precision)
                        len = (dot_pos - buff + 1) + precision;
                else if (decimal_places < precision) {
                        if (len + (precision - decimal_places) < 64) {
                                if !consteval {
                                        memset(buff + len, '0', precision - decimal_places);
                                } else {
                                        for (int i = 0; i < precision - decimal_places; ++i)
                                                buff[len + i] = '0';
                                }
                                len += precision - decimal_places;
                        }
                }
        } else {
                if (len + precision + 1 < 64) {
                        buff[len] = '.';
                        ++len;
                        if !consteval {
                                memset(buff + len, '0', precision);
                        } else {
                                for (int i = 0; i < precision; ++i)
                                        buff[len + i] = '0';
                        }
                        len += precision;
                }
        }
        buff[len] = '\0';
        return len;
}
template <typename ValFn>
constexpr unsigned long long cxxftoa(char* buff, const float value, ValFn&& fn) {
        if (buff == nullptr)
                return 0;
        const char* result = fn(buff, value);
        unsigned long long len = static_cast<size_t>(result - buff);
        buff[len] = '\0';
        return len;
}
template <typename ValFn>
constexpr unsigned long long cxxftoa(char* buff, const float value, const int precision, ValFn&& fn) {
        if (precision < 0)
                return cxxftoa(buff, value, fn);
        else
                return cxxdtoa(buff, value, precision, fn);
}

namespace embdr::cxxstd {
        export constexpr unsigned long long dtoa(char* buff, const double value) {
                return cxxdtoa(buff, value, &xjb64);
        }
        export constexpr unsigned long long dtoa(char* buff, const double value, const int precision) {
                return cxxdtoa(buff, value, precision, &xjb64);
        }
        export constexpr unsigned long long ftoa(char* buff, const float value) { return cxxftoa(buff, value, &xjb32); }
        export constexpr unsigned long long ftoa(char* buff, const float value, const int precision) {
                return cxxftoa(buff, value, precision, &xjb32);
        }
}; // namespace embdr::cxxstd
