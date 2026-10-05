/*
 * Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the “Software”), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

module;
#include <cstdint>
#include <cstddef>
#include <cinttypes>
#include <type_traits>
#include <utility>

export module embdr.cxxstd.stringFormat;
import embdr.cxxstd.stringView;
import embdr.cxxstd.stringFloat;
import embdr.cxxstd.stringNumber;

namespace embdr::cxxstd {
        export template <typename T>
        struct FormatArg {
                T v;
                constexpr FormatArg(T val) : v(val) {}
        };
        template <unsigned int N, typename CharTy>
        struct FormatArg<const CharTy[N]> {
                using value_type = CharTy*;
                BasicTemplatedStringView<N, CharTy> v;
                constexpr FormatArg(const CharTy (&str)[N]) noexcept : v(str) {}
                constexpr operator value_type() const noexcept { return v.data(); }
        };
        export template <typename T>
        FormatArg(T) -> FormatArg<T>;
        export template <unsigned int N, typename CharTy>
        FormatArg(const CharTy (&)[N]) -> FormatArg<const CharTy[N]>;

        enum class SpecAlign : unsigned char { none, left, right, center };
        enum class SpecSign : unsigned char { minus, plus, space };
        enum class SpecFlag : unsigned char { none = 0, alt = 1, zero_pad = 2 };
        enum class FloatKind : unsigned char { number, infinity, not_a_number };

        constexpr SpecFlag operator|(const SpecFlag lhs, const SpecFlag rhs) noexcept {
                return static_cast<SpecFlag>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
        }

        constexpr SpecFlag operator&(const SpecFlag lhs, const SpecFlag rhs) noexcept {
                return static_cast<SpecFlag>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
        }

        constexpr bool has_flag(const SpecFlag flags, const SpecFlag flag) noexcept {
                return (flags & flag) != SpecFlag::none;
        }

        template <auto...>
        inline constexpr bool format_argument_error = false;

        template <typename CharTy>
        struct FormatSpec {
                CharTy fill = static_cast<CharTy>(' ');
                CharTy type = static_cast<CharTy>('\0');
                SpecAlign align = SpecAlign::none;
                SpecSign sign = SpecSign::minus;
                SpecFlag flags = SpecFlag::none;
                unsigned short width = 0;
                short precision = -1;
        };

        template <typename CharTy>
        struct FormatSink {
                CharTy* dst = nullptr;
                unsigned int capacity = 0;
                unsigned int pos = 0;
                bool stopped = false;

                constexpr void put(const CharTy ch) noexcept {
                        if (this->stopped)
                                return;
                        if (this->dst != nullptr && this->pos < this->capacity)
                                this->dst[this->pos] = ch;
                        ++this->pos;
                }

                constexpr void fill(unsigned int count, const CharTy ch) noexcept {
                        while (count-- > 0)
                                this->put(ch);
                }
        };

        template <typename CharTy>
        constexpr void put_ascii(FormatSink<CharTy>& sink, const char* text, const unsigned int len) noexcept {
                for (unsigned int i = 0; i < len; ++i)
                        sink.put(static_cast<CharTy>(text[i]));
        }

        template <typename CharTy>
        constexpr unsigned int text_length(const CharTy* text) noexcept {
                unsigned int len = 0;
                while (text[len] != static_cast<CharTy>('\0'))
                        ++len;
                return len;
        }

        template <typename CharTy>
        constexpr CharTy pick_sign(const bool negative, const FormatSpec<CharTy>& spec) noexcept {
                if (negative)
                        return static_cast<CharTy>('-');
                if (spec.sign == SpecSign::plus)
                        return static_cast<CharTy>('+');
                if (spec.sign == SpecSign::space)
                        return static_cast<CharTy>(' ');
                return static_cast<CharTy>('\0');
        }

        template <typename CharTy>
        constexpr void text_spec(FormatSpec<CharTy>& spec) noexcept {
                if (has_flag(spec.flags, SpecFlag::zero_pad) && spec.align == SpecAlign::none) {
                        spec.fill = static_cast<CharTy>('0');
                        spec.align = SpecAlign::right;
                }
        }

        template <typename CharTy, typename EmitPrefix, typename EmitBody>
        constexpr void emit_padded(FormatSink<CharTy>& sink, const FormatSpec<CharTy>& spec,
                                   const SpecAlign default_align, const CharTy sign_ch, const unsigned int prefix_len,
                                   const unsigned int body_len, EmitPrefix&& emit_prefix,
                                   EmitBody&& emit_body) noexcept {
                const unsigned int sign_len = sign_ch != static_cast<CharTy>('\0') ? 1u : 0u;
                const unsigned int total = sign_len + prefix_len + body_len;
                const unsigned int pad = spec.width > total ? static_cast<unsigned int>(spec.width) - total : 0u;
                const auto emit_sign = [&]() noexcept {
                        if (sign_len != 0)
                                sink.put(sign_ch);
                };
                if (has_flag(spec.flags, SpecFlag::zero_pad) && spec.align == SpecAlign::none) {
                        emit_sign();
                        emit_prefix();
                        sink.fill(pad, static_cast<CharTy>('0'));
                        emit_body();
                        return;
                }
                const SpecAlign align = spec.align != SpecAlign::none ? spec.align : default_align;
                if (align == SpecAlign::left) {
                        emit_sign();
                        emit_prefix();
                        emit_body();
                        sink.fill(pad, spec.fill);
                } else if (align == SpecAlign::center) {
                        const unsigned int left_pad = pad / 2;
                        sink.fill(left_pad, spec.fill);
                        emit_sign();
                        emit_prefix();
                        emit_body();
                        sink.fill(pad - left_pad, spec.fill);
                } else {
                        sink.fill(pad, spec.fill);
                        emit_sign();
                        emit_prefix();
                        emit_body();
                }
        }

        template <typename CharTy, typename EmitBody>
        constexpr void emit_padded(FormatSink<CharTy>& sink, const FormatSpec<CharTy>& spec,
                                   const SpecAlign default_align, const CharTy sign_ch, const unsigned int body_len,
                                   EmitBody&& emit_body) noexcept {
                emit_padded(sink, spec, default_align, sign_ch, 0u, body_len, []() noexcept {}, emit_body);
        }

        template <typename CharTy>
        constexpr bool is_integer_type(const CharTy type) noexcept {
                return type == static_cast<CharTy>('\0') || type == static_cast<CharTy>('d') ||
                       type == static_cast<CharTy>('u') || type == static_cast<CharTy>('x') ||
                       type == static_cast<CharTy>('X') || type == static_cast<CharTy>('b') ||
                       type == static_cast<CharTy>('B') || type == static_cast<CharTy>('o') ||
                       type == static_cast<CharTy>('c');
        }

        template <typename CharTy>
        constexpr bool is_floating_type(const CharTy type) noexcept {
                return type == static_cast<CharTy>('\0') || type == static_cast<CharTy>('f') ||
                       type == static_cast<CharTy>('F') || type == static_cast<CharTy>('e') ||
                       type == static_cast<CharTy>('E') || type == static_cast<CharTy>('g') ||
                       type == static_cast<CharTy>('G');
        }

        template <typename CharTy, typename Reader>
        constexpr bool parse_format_spec(const Reader& rd, const unsigned int begin, const unsigned int end,
                                         FormatSpec<CharTy>& spec) noexcept {
                const auto is_digit = [](const CharTy c) noexcept {
                        return c >= static_cast<CharTy>('0') && c <= static_cast<CharTy>('9');
                };
                const auto is_align = [](const CharTy c) noexcept {
                        return c == static_cast<CharTy>('<') || c == static_cast<CharTy>('>') ||
                               c == static_cast<CharTy>('^');
                };
                const auto align_of = [](const CharTy c) noexcept {
                        if (c == static_cast<CharTy>('<'))
                                return SpecAlign::left;
                        if (c == static_cast<CharTy>('>'))
                                return SpecAlign::right;
                        return SpecAlign::center;
                };
                if (begin == end)
                        return true;
                if (rd[begin] != static_cast<CharTy>(':'))
                        return false;
                unsigned int i = begin + 1;
                if (i == end)
                        return true;
                if (i + 1 < end && is_align(rd[i + 1])) {
                        spec.fill = rd[i];
                        spec.align = align_of(rd[i + 1]);
                        i += 2;
                } else if (is_align(rd[i])) {
                        spec.align = align_of(rd[i]);
                        i += 1;
                }
                if (i < end && (rd[i] == static_cast<CharTy>('+') || rd[i] == static_cast<CharTy>(' '))) {
                        spec.sign = rd[i] == static_cast<CharTy>('+') ? SpecSign::plus : SpecSign::space;
                        i += 1;
                }
                if (i < end && rd[i] == static_cast<CharTy>('#')) {
                        spec.flags = spec.flags | SpecFlag::alt;
                        i += 1;
                }
                if (i < end && rd[i] == static_cast<CharTy>('0')) {
                        spec.flags = spec.flags | SpecFlag::zero_pad;
                        i += 1;
                }
                if (i < end && is_digit(rd[i])) {
                        unsigned int width = 0;
                        while (i < end && is_digit(rd[i])) {
                                width = width * 10u + static_cast<unsigned int>(rd[i] - static_cast<CharTy>('0'));
                                if (width > 65535u)
                                        width = 65535u;
                                i += 1;
                        }
                        spec.width = static_cast<unsigned short>(width);
                }
                if (i < end && rd[i] == static_cast<CharTy>('.')) {
                        i += 1;
                        if (i >= end || !is_digit(rd[i]))
                                return false;
                        int precision = 0;
                        while (i < end && is_digit(rd[i])) {
                                if (precision < 32767)
                                        precision = precision * 10 + static_cast<int>(rd[i] - static_cast<CharTy>('0'));
                                if (precision > 32767)
                                        precision = 32767;
                                i += 1;
                        }
                        spec.precision = static_cast<short>(precision);
                }
                if (i < end) {
                        spec.type = rd[i];
                        i += 1;
                }
                return i == end;
        }

        template <typename CharTy>
        constexpr void emit_text(FormatSink<CharTy>& sink, const char* text, FormatSpec<CharTy> spec,
                                 const SpecAlign default_align) noexcept {
                text_spec(spec);
                const unsigned int len = text_length(text);
                emit_padded(sink, spec, default_align, static_cast<CharTy>('\0'), len, [&]() noexcept {
                        put_ascii(sink, text, len);
                });
        }

        template <typename CharTy>
        constexpr void emit_char(FormatSink<CharTy>& sink, const CharTy ch, FormatSpec<CharTy> spec) noexcept {
                text_spec(spec);
                emit_padded(sink, spec, SpecAlign::left, static_cast<CharTy>('\0'), 1u, [&]() noexcept {
                        sink.put(ch);
                });
        }

        template <typename CharTy>
        constexpr void emit_text_content(FormatSink<CharTy>& sink, const BasicStringView<CharTy> text,
                                         FormatSpec<CharTy> spec) noexcept {
                unsigned int len = static_cast<unsigned int>(text.size());
                if (len != 0 && text[len - 1] == static_cast<CharTy>('\0'))
                        --len;
                if (spec.precision >= 0 && static_cast<unsigned int>(spec.precision) < len)
                        len = static_cast<unsigned int>(spec.precision);
                text_spec(spec);
                emit_padded(sink, spec, SpecAlign::left, static_cast<CharTy>('\0'), len, [&]() noexcept {
                        for (unsigned int i = 0; i < len; ++i)
                                sink.put(static_cast<CharTy>(text[i]));
                });
        }

        struct FloatRepr {
                char digits[64] = {};
                unsigned int ndigits = 0;
                int exp10 = 0;
                bool negative = false;
                FloatKind kind = FloatKind::number;
        };

        constexpr FloatRepr parse_float_repr(const char* text, const unsigned int len) noexcept {
                FloatRepr repr;
                unsigned int i = 0;
                if (i < len && (text[i] == '-' || text[i] == '+')) {
                        repr.negative = text[i] == '-';
                        i += 1;
                }
                if (i < len && (text[i] == 'i' || text[i] == 'I')) {
                        repr.kind = FloatKind::infinity;
                        return repr;
                }
                if (i < len && (text[i] == 'n' || text[i] == 'N')) {
                        repr.kind = FloatKind::not_a_number;
                        return repr;
                }
                int exp = 0;
                bool has_exp = false;
                unsigned int pre_dot = 0;
                bool seen_dot = false;
                unsigned int nd = 0;
                for (; i < len; ++i) {
                        const char c = text[i];
                        if (c >= '0' && c <= '9') {
                                if (!seen_dot)
                                        pre_dot += 1;
                                if (nd < 63) {
                                        repr.digits[nd] = c;
                                        nd += 1;
                                }
                        } else if (c == '.') {
                                seen_dot = true;
                        } else if (c == 'e' || c == 'E') {
                                has_exp = true;
                                i += 1;
                                bool neg = false;
                                if (i < len && (text[i] == '-' || text[i] == '+')) {
                                        neg = text[i] == '-';
                                        i += 1;
                                }
                                int e = 0;
                                for (; i < len && text[i] >= '0' && text[i] <= '9'; ++i) {
                                        if (e < 100000)
                                                e = e * 10 + (text[i] - '0');
                                }
                                exp = neg ? -e : e;
                                break;
                        }
                }
                repr.ndigits = nd;
                repr.exp10 = (has_exp ? exp : 0) + static_cast<int>(pre_dot) - 1;
                unsigned int start = 0;
                while (start + 1 < repr.ndigits && repr.digits[start] == '0')
                        start += 1;
                if (start != 0) {
                        repr.exp10 -= static_cast<int>(start);
                        for (unsigned int k = 0; k + start < repr.ndigits; ++k)
                                repr.digits[k] = repr.digits[k + start];
                        repr.ndigits -= start;
                }
                bool all_zero = true;
                for (unsigned int k = 0; k < repr.ndigits; ++k) {
                        if (repr.digits[k] != '0')
                                all_zero = false;
                }
                if (repr.ndigits == 0 || all_zero) {
                        repr.digits[0] = '0';
                        repr.ndigits = 1;
                        repr.exp10 = 0;
                }
                return repr;
        }

        constexpr void round_float_repr(FloatRepr& repr, const int keep) noexcept {
                if (keep < 0) {
                        repr.digits[0] = '0';
                        repr.ndigits = 1;
                        repr.exp10 = 0;
                        return;
                }
                if (keep == 0) {
                        if (repr.digits[0] >= '5') {
                                repr.digits[0] = '1';
                                repr.exp10 += 1;
                        } else {
                                repr.digits[0] = '0';
                                repr.exp10 = 0;
                        }
                        repr.ndigits = 1;
                        return;
                }
                if (static_cast<int>(repr.ndigits) <= keep)
                        return;
                const unsigned int k = static_cast<unsigned int>(keep);
                if (repr.digits[k] >= '5') {
                        int i = static_cast<int>(k) - 1;
                        for (; i >= 0; --i) {
                                if (repr.digits[i] != '9') {
                                        repr.digits[i] = static_cast<char>(repr.digits[i] + 1);
                                        break;
                                }
                                repr.digits[i] = '0';
                        }
                        if (i < 0) {
                                repr.digits[0] = '1';
                                repr.exp10 += 1;
                        }
                }
                repr.ndigits = k;
        }

        constexpr void pad_float_repr(FloatRepr& repr, const unsigned int count) noexcept {
                while (repr.ndigits < count && repr.ndigits < 63) {
                        repr.digits[repr.ndigits] = '0';
                        repr.ndigits += 1;
                }
        }

        constexpr void strip_float_repr(FloatRepr& repr) noexcept {
                while (repr.ndigits > 1 && repr.digits[repr.ndigits - 1] == '0')
                        repr.ndigits -= 1;
        }

        template <typename CharTy>
        constexpr void emit_float_fixed(FormatSink<CharTy>& sink, const FormatSpec<CharTy>& spec, const CharTy sign_ch,
                                        const FloatRepr& repr, const unsigned int frac_target,
                                        const bool alt) noexcept {
                const int e = repr.exp10;
                const unsigned int int_len = e >= 0 ? static_cast<unsigned int>(e) + 1u : 1u;
                const unsigned int frac_len = (frac_target > 0u || alt) ? frac_target + 1u : 0u;
                const unsigned int body_len = int_len + frac_len;
                emit_padded(sink, spec, SpecAlign::right, sign_ch, body_len, [&]() noexcept {
                        if (e >= 0) {
                                const unsigned int int_digits = static_cast<unsigned int>(e) + 1u;
                                for (unsigned int i = 0; i < int_digits; ++i)
                                        sink.put(i < repr.ndigits ? static_cast<CharTy>(repr.digits[i])
                                                                  : static_cast<CharTy>('0'));
                        } else {
                                sink.put(static_cast<CharTy>('0'));
                        }
                        if (frac_len != 0u) {
                                sink.put(static_cast<CharTy>('.'));
                                for (unsigned int k = 1u; k <= frac_target; ++k) {
                                        const int idx = e + static_cast<int>(k);
                                        const bool inside = idx >= 0 && static_cast<unsigned int>(idx) < repr.ndigits;
                                        sink.put(inside ? static_cast<CharTy>(repr.digits[idx])
                                                        : static_cast<CharTy>('0'));
                                }
                        }
                });
        }

        template <typename CharTy>
        constexpr void emit_float_sci(FormatSink<CharTy>& sink, const FormatSpec<CharTy>& spec, const CharTy sign_ch,
                                      const FloatRepr& repr, const unsigned int frac_count, const bool alt,
                                      const CharTy exp_char) noexcept {
                char exp_digits[8];
                unsigned int exp_len = 0;
                const bool exp_negative = repr.exp10 < 0;
                unsigned int exp_abs =
                    exp_negative ? static_cast<unsigned int>(-repr.exp10) : static_cast<unsigned int>(repr.exp10);
                do {
                        exp_digits[exp_len] = static_cast<char>('0' + static_cast<int>(exp_abs % 10u));
                        exp_len += 1;
                        exp_abs /= 10u;
                } while (exp_abs != 0u && exp_len < 8u);
                while (exp_len < 2u) {
                        exp_digits[exp_len] = '0';
                        exp_len += 1;
                }
                const unsigned int frac_len = (frac_count > 0u || alt) ? frac_count + 1u : 0u;
                const unsigned int body_len = 1u + frac_len + 2u + exp_len;
                emit_padded(sink, spec, SpecAlign::right, sign_ch, body_len, [&]() noexcept {
                        sink.put(repr.ndigits != 0u ? static_cast<CharTy>(repr.digits[0]) : static_cast<CharTy>('0'));
                        if (frac_len != 0u) {
                                sink.put(static_cast<CharTy>('.'));
                                for (unsigned int k = 1u; k <= frac_count; ++k)
                                        sink.put(k < repr.ndigits ? static_cast<CharTy>(repr.digits[k])
                                                                  : static_cast<CharTy>('0'));
                        }
                        sink.put(exp_char);
                        sink.put(exp_negative ? static_cast<CharTy>('-') : static_cast<CharTy>('+'));
                        for (unsigned int i = exp_len; i > 0u; --i)
                                sink.put(static_cast<CharTy>(exp_digits[i - 1u]));
                });
        }

        template <typename CharTy, typename P>
        constexpr void format_pointer(FormatSink<CharTy>& sink, const P ptr, const FormatSpec<CharTy>& spec) noexcept {
                if (ptr == nullptr) {
                        emit_text(sink, "nullptr", spec, SpecAlign::right);
                        return;
                }
                if !consteval {
                        if constexpr (std::is_pointer_v<P>) {
                                char raw[24];
                                const unsigned long long raw_len =
                                    uitoa(raw, static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(ptr)),
                                          NumberBase::hexadecimal, false);
                                emit_padded(sink, spec, SpecAlign::right, static_cast<CharTy>('\0'), 2u,
                                            static_cast<unsigned int>(raw_len), [&]() noexcept {
                                                    put_ascii(sink, "0x", 2u);
                                            },
                                            [&]() noexcept { put_ascii(sink, raw, static_cast<unsigned int>(raw_len)); });
                        } else {
                                sink.stopped = true;
                        }
                } else {
                        sink.stopped = true;
                }
        }

        template <typename CharTy, typename T>
        constexpr void format_integer(FormatSink<CharTy>& sink, const T value,
                                      const FormatSpec<CharTy>& spec) noexcept {
                const CharTy type = spec.type;
                if (type == static_cast<CharTy>('c')) {
                        emit_char(sink, static_cast<CharTy>(value), spec);
                        return;
                }
                auto base = NumberBase::decimal;
                bool upper = false;
                if (type == static_cast<CharTy>('x')) {
                        base = NumberBase::hexadecimal;
                } else if (type == static_cast<CharTy>('X')) {
                        base = NumberBase::hexadecimal;
                        upper = true;
                } else if (type == static_cast<CharTy>('o')) {
                        base = NumberBase::octal;
                } else if (type == static_cast<CharTy>('b')) {
                        base = NumberBase::binary;
                } else if (type == static_cast<CharTy>('B')) {
                        base = NumberBase::binary;
                        upper = true;
                } else if (type != static_cast<CharTy>('\0') && type != static_cast<CharTy>('d') &&
                           type != static_cast<CharTy>('u')) {
                        sink.stopped = true;
                        return;
                }
                bool negative = false;
                unsigned long long magnitude = 0;
                if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
                        if (value < 0) {
                                negative = true;
                                magnitude = base != NumberBase::decimal
                                                ? static_cast<unsigned long long>(value)
                                                : 0ull - static_cast<unsigned long long>(value);
                        } else {
                                magnitude = static_cast<unsigned long long>(value);
                        }
                } else {
                        magnitude = static_cast<unsigned long long>(value);
                }
                const bool allow_sign =
                    type == static_cast<CharTy>('\0') || type == static_cast<CharTy>('d');
                const CharTy sign_ch = allow_sign ? pick_sign(negative, spec) : static_cast<CharTy>('\0');
                char raw[72];
                const unsigned int digits_len = static_cast<unsigned int>(uitoa(raw, magnitude, base, upper));
                unsigned int body_len = digits_len;
                if (spec.precision >= 0 && static_cast<unsigned int>(spec.precision) > digits_len)
                        body_len = static_cast<unsigned int>(spec.precision);
                CharTy prefix[2];
                unsigned int prefix_len = 0;
                const bool with_prefix = has_flag(spec.flags, SpecFlag::alt) || type == static_cast<CharTy>('B');
                if (with_prefix) {
                        if (base == NumberBase::hexadecimal) {
                                prefix[0] = static_cast<CharTy>('0');
                                prefix[1] = upper ? static_cast<CharTy>('X') : static_cast<CharTy>('x');
                                prefix_len = 2;
                        } else if (base == NumberBase::binary) {
                                prefix[0] = static_cast<CharTy>('0');
                                prefix[1] = upper ? static_cast<CharTy>('B') : static_cast<CharTy>('b');
                                prefix_len = 2;
                        } else if (base == NumberBase::octal) {
                                const bool has_zero = body_len > digits_len || (digits_len != 0 && raw[0] == '0');
                                if (!has_zero) {
                                        prefix[0] = static_cast<CharTy>('0');
                                        prefix_len = 1;
                                }
                        }
                }
                emit_padded(
                    sink, spec, SpecAlign::right, sign_ch, prefix_len, body_len,
                    [&]() noexcept {
                            for (unsigned int i = 0; i < prefix_len; ++i)
                                    sink.put(prefix[i]);
                    },
                    [&]() noexcept {
                            if (body_len > digits_len)
                                    sink.fill(body_len - digits_len, static_cast<CharTy>('0'));
                            put_ascii(sink, raw, digits_len);
                    });
        }

        template <typename CharTy, typename T>
        constexpr void format_floating(FormatSink<CharTy>& sink, const T value,
                                       const FormatSpec<CharTy>& spec) noexcept {
                const CharTy type = spec.type;
                if (!is_floating_type(type)) {
                        sink.stopped = true;
                        return;
                }
                const bool upper = type == static_cast<CharTy>('F') || type == static_cast<CharTy>('E') ||
                                   type == static_cast<CharTy>('G');
                char raw[96];
                unsigned long long raw_len = 0;
                if constexpr (std::is_same_v<T, float>)
                        raw_len = ftoa(raw, value);
                else
                        raw_len = dtoa(raw, static_cast<double>(value));
                const FloatRepr repr = parse_float_repr(raw, static_cast<unsigned int>(raw_len));
                const CharTy sign_ch = pick_sign(repr.negative, spec);
                const bool alt = has_flag(spec.flags, SpecFlag::alt);
                if (repr.kind != FloatKind::number) {
                        const char* text =
                            repr.kind == FloatKind::infinity ? (upper ? "INF" : "inf") : (upper ? "NAN" : "nan");
                        const unsigned int text_len = text_length(text);
                        emit_padded(sink, spec, SpecAlign::right, sign_ch, text_len, [&]() noexcept {
                                put_ascii(sink, text, text_len);
                        });
                        return;
                }
                CharTy t = type;
                if (t == static_cast<CharTy>('\0') && spec.precision < 0) {
                        const unsigned int body_len = static_cast<unsigned int>(raw_len);
                        bool has_dot = false;
                        for (unsigned int i = 0; i < body_len; ++i) {
                                if (raw[i] == '.')
                                        has_dot = true;
                        }
                        const bool add_dot = alt && !has_dot;
                        emit_padded(sink, spec, SpecAlign::right, sign_ch, body_len + (add_dot ? 2u : 0u),
                                    [&]() noexcept {
                                            for (unsigned int i = 0; i < body_len; ++i) {
                                                    const char c = raw[i];
                                                    sink.put(upper && c >= 'a' && c <= 'z'
                                                                 ? static_cast<CharTy>(c - 'a' + 'A')
                                                                 : static_cast<CharTy>(c));
                                            }
                                            if (add_dot) {
                                                    sink.put(static_cast<CharTy>('.'));
                                                    sink.put(static_cast<CharTy>('0'));
                                            }
                                    });
                        return;
                }
                if (t == static_cast<CharTy>('\0'))
                        t = static_cast<CharTy>('g');
                if (t == static_cast<CharTy>('f') || t == static_cast<CharTy>('F')) {
                        const unsigned int p = spec.precision >= 0 ? static_cast<unsigned int>(spec.precision) : 6u;
                        FloatRepr rounded = repr;
                        round_float_repr(rounded, rounded.exp10 + static_cast<int>(p) + 1);
                        emit_float_fixed(sink, spec, sign_ch, rounded, p, alt);
                } else if (t == static_cast<CharTy>('e') || t == static_cast<CharTy>('E')) {
                        const unsigned int p = spec.precision >= 0 ? static_cast<unsigned int>(spec.precision) : 6u;
                        FloatRepr rounded = repr;
                        round_float_repr(rounded, static_cast<int>(p) + 1);
                        pad_float_repr(rounded, p + 1u);
                        emit_float_sci(sink, spec, sign_ch, rounded, p, alt,
                                       upper ? static_cast<CharTy>('E') : static_cast<CharTy>('e'));
                } else {
                        unsigned int p = spec.precision >= 0 ? static_cast<unsigned int>(spec.precision) : 6u;
                        if (p == 0u)
                                p = 1u;
                        FloatRepr rounded = repr;
                        round_float_repr(rounded, static_cast<int>(p));
                        if (alt)
                                pad_float_repr(rounded, p);
                        else
                                strip_float_repr(rounded);
                        if (rounded.exp10 < -4 || rounded.exp10 >= static_cast<int>(p)) {
                                emit_float_sci(sink, spec, sign_ch, rounded, rounded.ndigits - 1u, alt,
                                               upper ? static_cast<CharTy>('E') : static_cast<CharTy>('e'));
                        } else {
                                const int frac_i = static_cast<int>(rounded.ndigits) - rounded.exp10 - 1;
                                const unsigned int frac = frac_i > 0 ? static_cast<unsigned int>(frac_i) : 0u;
                                emit_float_fixed(sink, spec, sign_ch, rounded, frac, alt);
                        }
                }
        }

        template <typename CharTy, typename T>
        constexpr void format_value_impl(FormatSink<CharTy>& sink, const T& value,
                                         const FormatSpec<CharTy>& spec) noexcept;

        template <typename CharTy, typename T>
        constexpr void format_value_impl(FormatSink<CharTy>& sink, const T& value,
                                         const FormatSpec<CharTy>& spec) noexcept {
                using U = std::remove_cvref_t<T>;
                if constexpr (std::is_same_v<U, bool>) {
                        const CharTy type = spec.type;
                        if (type == static_cast<CharTy>('\0') || type == static_cast<CharTy>('s')) {
                                emit_text(sink, value ? "true" : "false", spec, SpecAlign::left);
                        } else if (type == static_cast<CharTy>('c')) {
                                emit_char(sink, value ? static_cast<CharTy>('1') : static_cast<CharTy>('0'), spec);
                        } else if (is_integer_type(type)) {
                                format_integer(sink, value, spec);
                        } else {
                                sink.stopped = true;
                        }
                } else if constexpr (std::is_enum_v<U>) {
                        format_value_impl(sink, static_cast<std::underlying_type_t<U>>(value), spec);
                } else if constexpr (std::is_floating_point_v<U>) {
                        format_floating(sink, value, spec);
                } else if constexpr (std::is_same_v<U, std::nullptr_t>) {
                        format_pointer(sink, value, spec);
                } else if constexpr (std::is_same_v<U, CharTy>) {
                        const CharTy type = spec.type;
                        if (type == static_cast<CharTy>('\0') || type == static_cast<CharTy>('s') ||
                            type == static_cast<CharTy>('c')) {
                                emit_char(sink, value, spec);
                        } else if (is_integer_type(type)) {
                                format_integer(sink, value, spec);
                        } else {
                                sink.stopped = true;
                        }
                } else if constexpr (std::is_integral_v<U>) {
                        if (is_integer_type(spec.type))
                                format_integer(sink, value, spec);
                        else
                                sink.stopped = true;
                } else if constexpr (requires { BasicStringView<CharTy>{value}; }) {
                        if (spec.type == static_cast<CharTy>('\0') || spec.type == static_cast<CharTy>('s')) {
                                if constexpr (std::is_pointer_v<U>) {
                                        if (value == nullptr) {
                                                emit_text(sink, "nullptr", spec, SpecAlign::right);
                                                return;
                                        }
                                }
                                emit_text_content(sink, BasicStringView<CharTy>{value}, spec);
                        } else {
                                sink.stopped = true;
                        }
                } else if constexpr (std::is_pointer_v<U>) {
                        if (spec.type == static_cast<CharTy>('\0') || spec.type == static_cast<CharTy>('p'))
                                format_pointer(sink, value, spec);
                        else
                                sink.stopped = true;
                } else {
                        sink.stopped = true;
                }
        }

        template <typename CharTy>
        constexpr void emit_format_text(FormatSink<CharTy>& sink, const CharTy*& fmt) noexcept {
                while (*fmt != static_cast<CharTy>('\0')) {
                        if (*fmt == static_cast<CharTy>('{')) {
                                if (*(fmt + 1) == static_cast<CharTy>('{')) {
                                        sink.put(static_cast<CharTy>('{'));
                                        fmt += 2;
                                        continue;
                                }
                                return;
                        }
                        if (*fmt == static_cast<CharTy>('}')) {
                                if (*(fmt + 1) == static_cast<CharTy>('}')) {
                                        sink.put(static_cast<CharTy>('}'));
                                        fmt += 2;
                                        continue;
                                }
                                sink.stopped = true;
                                return;
                        }
                        sink.put(*fmt);
                        ++fmt;
                }
        }

        template <typename CharTy>
        constexpr void parse_impl(FormatSink<CharTy>& sink, const CharTy* fmt) noexcept {
                emit_format_text(sink, fmt);
                if (*fmt != static_cast<CharTy>('\0'))
                        sink.stopped = true;
        }

        template <typename CharTy, typename First, typename... Rest>
        constexpr void parse_impl(FormatSink<CharTy>& sink, const CharTy* fmt, const First& first,
                                  const Rest&... rest) noexcept {
                emit_format_text(sink, fmt);
                if (sink.stopped)
                        return;
                if (*fmt == static_cast<CharTy>('\0'))
                        return;
                if (*fmt != static_cast<CharTy>('{')) {
                        sink.stopped = true;
                        return;
                }
                const CharTy* close = fmt + 1;
                while (*close != static_cast<CharTy>('\0') && *close != static_cast<CharTy>('}'))
                        ++close;
                if (*close != static_cast<CharTy>('}')) {
                        sink.stopped = true;
                        return;
                }
                FormatSpec<CharTy> spec{};
                const BasicStringView<CharTy> field(fmt + 1, static_cast<size_t>(close - (fmt + 1)));
                if (!parse_format_spec(field, 0u, static_cast<unsigned int>(field.size()), spec)) {
                        sink.stopped = true;
                        return;
                }
                format_value_impl(sink, first, spec);
                if (sink.stopped)
                        return;
                parse_impl(sink, close + 1, rest...);
        }

        template <unsigned int DstSize, typename CharTy, typename... Args>
        constexpr unsigned int format_into(CharTy (&dst)[DstSize], const CharTy* fmt, const Args&... args) noexcept {
                FormatSink<CharTy> sink{};
                sink.dst = dst;
                sink.capacity = DstSize - 1u;
                parse_impl(sink, fmt, args...);
                const unsigned int end = sink.pos < DstSize - 1u ? sink.pos : DstSize - 1u;
                dst[end] = static_cast<CharTy>('\0');
                return sink.pos;
        }

        template <unsigned int Pos, unsigned int ArgIdx, const BasicTemplatedStringView Fmt, FormatArg... Args>
        struct FormatJob {
                using FmtTy = std::remove_cv_t<decltype(Fmt)>;
                using FmtChar = typename FmtTy::value_type;

                static constexpr FmtChar at(const unsigned int index) noexcept {
                        return index < Fmt.size() ? Fmt[index] : static_cast<FmtChar>('\0');
                }

                static constexpr unsigned int close_pos() noexcept {
                        unsigned int i = Pos + 1;
                        while (i < Fmt.size() && Fmt[i] != static_cast<FmtChar>('}') &&
                               Fmt[i] != static_cast<FmtChar>('\0'))
                                ++i;
                        return i;
                }

                static constexpr unsigned int literal_run() noexcept {
                        unsigned int i = Pos;
                        while (i < Fmt.size()) {
                                const FmtChar c = Fmt[i];
                                if (c == static_cast<FmtChar>('\0') || c == static_cast<FmtChar>('{') ||
                                    c == static_cast<FmtChar>('}'))
                                        break;
                                ++i;
                        }
                        return i - Pos;
                }

                template <unsigned int I, typename First, typename... Rest>
                static constexpr decltype(auto) nth(const First& first, const Rest&... rest) noexcept {
                        if constexpr (I == 0)
                                return first;
                        else
                                return nth<I - 1>(rest...);
                }

                static constexpr auto current_arg() noexcept {
                        return nth<ArgIdx>(Args...).v;
                }

                static constexpr unsigned int field_close = close_pos();
                static constexpr bool field_has_args = ArgIdx < sizeof...(Args);
                static constexpr bool field_closed =
                    field_close < Fmt.size() && at(field_close) == static_cast<FmtChar>('}');
                static constexpr bool field_open = field_has_args && field_closed;

                static constexpr bool field_valid() noexcept {
                        if (!field_open)
                                return false;
                        FormatSpec<FmtChar> spec{};
                        if (!parse_format_spec(Fmt, Pos + 1, field_close, spec))
                                return false;
                        FormatSink<FmtChar> sink{};
                        format_value_impl(sink, current_arg(), spec);
                        return !sink.stopped;
                }

                static constexpr unsigned int field_size() noexcept {
                        FormatSpec<FmtChar> spec{};
                        parse_format_spec(Fmt, Pos + 1, field_close, spec);
                        FormatSink<FmtChar> sink{};
                        format_value_impl(sink, current_arg(), spec);
                        return sink.pos;
                }

                static constexpr unsigned int compute_size() noexcept {
                        if constexpr (at(Pos) == static_cast<FmtChar>('\0')) {
                                return 0;
                        } else if constexpr (at(Pos) == static_cast<FmtChar>('{')) {
                                if constexpr (at(Pos + 1) == static_cast<FmtChar>('{')) {
                                        return 1u + FormatJob<Pos + 2, ArgIdx, Fmt, Args...>::size;
                                } else if constexpr (!field_has_args) {
                                        static_assert(format_argument_error<Fmt, Pos, ArgIdx>,
                                                      "not enough arguments for format string");
                                        return 0;
                                } else if constexpr (!field_closed) {
                                        static_assert(format_argument_error<Fmt, Pos, ArgIdx>,
                                                      "unterminated format replacement field");
                                        return 0;
                                } else if constexpr (!field_valid()) {
                                        static_assert(format_argument_error<Fmt, Pos, ArgIdx>,
                                                      "invalid format specification or unsupported argument");
                                        return 0;
                                } else {
                                        return field_size() +
                                               FormatJob<field_close + 1, ArgIdx + 1, Fmt, Args...>::size;
                                }
                        } else if constexpr (at(Pos) == static_cast<FmtChar>('}')) {
                                if constexpr (at(Pos + 1) == static_cast<FmtChar>('}')) {
                                        return 1u + FormatJob<Pos + 2, ArgIdx, Fmt, Args...>::size;
                                } else {
                                        static_assert(format_argument_error<Fmt, Pos, ArgIdx>,
                                                      "unmatched '}' in format string");
                                        return 0;
                                }
                        } else {
                                return literal_run() + FormatJob<Pos + literal_run(), ArgIdx, Fmt, Args...>::size;
                        }
                }

                static constexpr void write_into(FormatSink<FmtChar>& sink) noexcept {
                        if constexpr (at(Pos) == static_cast<FmtChar>('\0')) {
                                return;
                        } else if constexpr (at(Pos) == static_cast<FmtChar>('{')) {
                                if constexpr (at(Pos + 1) == static_cast<FmtChar>('{')) {
                                        sink.put(static_cast<FmtChar>('{'));
                                        FormatJob<Pos + 2, ArgIdx, Fmt, Args...>::write_into(sink);
                        } else if constexpr (!field_open) {
                                return;
                        } else if constexpr (!field_valid()) {
                                return;
                        } else {
                                FormatSpec<FmtChar> spec{};
                                parse_format_spec(Fmt, Pos + 1, field_close, spec);
                                format_value_impl(sink, current_arg(), spec);
                                FormatJob<field_close + 1, ArgIdx + 1, Fmt, Args...>::write_into(sink);
                        }
                        } else if constexpr (at(Pos) == static_cast<FmtChar>('}')) {
                                if constexpr (at(Pos + 1) == static_cast<FmtChar>('}')) {
                                        sink.put(static_cast<FmtChar>('}'));
                                        FormatJob<Pos + 2, ArgIdx, Fmt, Args...>::write_into(sink);
                                }
                        } else {
                                const unsigned int run = literal_run();
                                for (unsigned int i = 0; i < run; ++i)
                                        sink.put(Fmt[Pos + i]);
                                FormatJob<Pos + run, ArgIdx, Fmt, Args...>::write_into(sink);
                        }
                }

                static constexpr unsigned int size = compute_size();
        };

        export template <const BasicTemplatedStringView Fmt, FormatArg... Args>
        consteval auto format() {
                using FmtChar = typename std::remove_cv_t<decltype(Fmt)>::value_type;
                constexpr unsigned int length = FormatJob<0, 0, Fmt, Args...>::size;
                if constexpr (length == 0) {
                        return BasicTemplatedStringView<0, FmtChar>{};
                } else {
                        FmtChar buffer[length]{};
                        FormatSink<FmtChar> sink{};
                        sink.dst = buffer;
                        sink.capacity = length;
                        FormatJob<0, 0, Fmt, Args...>::write_into(sink);
                        return BasicTemplatedStringView<length, FmtChar>{buffer};
                }
        }

        export template <unsigned int DstSize, typename CharTy, typename First, typename... Rest>
        constexpr unsigned int format(CharTy (&dst)[DstSize], const CharTy* fmt, First&& first,
                                      Rest&&... rest) noexcept {
                return format_into(dst, fmt, first, rest...);
        }

        export template <unsigned int DstSize, typename CharTy, typename... Args>
        constexpr unsigned int parse(CharTy (&dst)[DstSize], const CharTy* fmt, Args&&... args) noexcept {
                return format_into(dst, fmt, args...);
        }

        export template <unsigned int DstSize, typename CharTy>
        constexpr unsigned int parse(CharTy (&dst)[DstSize], const CharTy* fmt) noexcept {
                return format_into(dst, fmt);
        }
} // namespace embdr::cxxstd
