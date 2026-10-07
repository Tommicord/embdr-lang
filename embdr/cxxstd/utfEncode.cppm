/* Copyright(c) 2026 Tommicord
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
#include <cstddef>
#include <cstdint>

export module embdr.cxxstd.utfEncode;
import embdr.cxxstd.stringView;

namespace embdr::cxxstd
{
    export enum class UtfError : uint8_t {
        NONE,
        TRUNCATED,
        UNEXPECTED_CONTINUATION,
        INVALID_LEAD,
        OVERLONG,
        SURROGATE,
        TOO_LARGE,
        INVALID_UTF16_PAIR,
        BUFFER_TO_SMALL,
    };

    export [[nodiscard]] constexpr SimpleStringView utf_error_name(const UtfError error) noexcept
    {
        switch (error)
            {
                case UtfError::NONE:
                    return "none";
                case UtfError::TRUNCATED:
                    return "truncated";
                case UtfError::UNEXPECTED_CONTINUATION:
                    return "unexpected continuation";
                case UtfError::INVALID_LEAD:
                    return "invalid lead";
                case UtfError::OVERLONG:
                    return "overlong";
                case UtfError::SURROGATE:
                    return "surrogate";
                case UtfError::TOO_LARGE:
                    return "too large";
                case UtfError::INVALID_UTF16_PAIR:
                    return "invalid utf16 pair";
                case UtfError::BUFFER_TO_SMALL:
                    return "buffer too small";
            }
        return "none";
    }

    export struct Utf8Decode
    {
        char32_t _M_code_point;
        uint32_t _M_consumed;
        UtfError _M_error;
    };

    export struct Utf16Decode
    {
        char32_t _M_code_point;
        uint32_t _M_consumed;
        UtfError _M_error;
    };

    export struct UtfValidation
    {
        UtfError _M_error;
        size_t _M_position;

        [[nodiscard]] [[nodiscard]]
        constexpr bool ok() const noexcept
        {
            return this->_M_error == UtfError::NONE;
        }
    };

    export struct UtfConvert
    {
        UtfError _M_error;
        size_t _M_written;
        size_t _M_error_position;
    };

    export [[nodiscard]] constexpr bool is_unicode_scalar(const char32_t cp) noexcept
    {
        return cp <= 0x10FFFFu && !(cp >= 0xD800u && cp <= 0xDFFFu);
    }

    export [[nodiscard]] constexpr bool is_high_surrogate(const char16_t c) noexcept
    {
        return c >= 0xD800u && c <= 0xDBFFu;
    }

    export [[nodiscard]] constexpr bool is_low_surrogate(const char16_t c) noexcept
    {
        return c >= 0xDC00u && c <= 0xDFFFu;
    }

    export [[nodiscard]] constexpr char32_t combine_surrogates(const char16_t hi, const char16_t lo) noexcept
    {
        return 0x10000u + ((static_cast<char32_t>(hi) - 0xD800u) << 10) + (static_cast<char32_t>(lo) - 0xDC00u);
    }

    export [[nodiscard]] constexpr bool is_utf8_continuation(const char c) noexcept
    {
        return (static_cast<unsigned char>(c) & 0xC0u) == 0x80u;
    }

    export [[nodiscard]] constexpr bool is_utf8_lead(const char c) noexcept
    {
        const auto b = static_cast<unsigned char>(c);
        return b >= 0xC2u && b <= 0xF4u;
    }

    export [[nodiscard]] constexpr uint32_t utf8_sequence_length(const char lead) noexcept
    {
        const auto b = static_cast<unsigned char>(lead);
        if (b < 0x80u)
            return 1;
        if (b < 0xC2u)
            return 0;
        if (b < 0xE0u)
            return 2;
        if (b < 0xF0u)
            return 3;
        if (b < 0xF5u)
            return 4;
        return 0;
    }

    export [[nodiscard]] constexpr size_t utf8_length_from_code_point(const char32_t cp) noexcept
    {
        if (!is_unicode_scalar(cp))
            return 0;
        if (cp < 0x80u)
            return 1;
        if (cp < 0x800u)
            return 2;
        if (cp < 0x10000u)
            return 3;
        return 4;
    }

    export [[nodiscard]] constexpr size_t utf16_length_from_code_point(const char32_t cp) noexcept
    {
        if (!is_unicode_scalar(cp))
            return 0;
        return cp < 0x10000u ? 1 : 2;
    }

    export [[nodiscard]] constexpr Utf8Decode decode_utf8(const char* p, const size_t len) noexcept
    {
        if (len == 0)
            return {0, 0, UtfError::TRUNCATED};
        const auto b0 = static_cast<unsigned char>(p[0]);
        if (b0 < 0x80u)
            return {static_cast<char32_t>(b0), 1, UtfError::NONE};
        if (b0 < 0xC0u)
            return {0, 1, UtfError::UNEXPECTED_CONTINUATION};
        if (b0 < 0xC2u)
            return {0, 0, UtfError::OVERLONG};
        if (b0 < 0xF5u)
            {
                uint32_t need = 0;
                char32_t cp = 0;
                char32_t minimum = 0;
                if (b0 < 0xE0u)
                    {
                        need = 2;
                        cp = static_cast<char32_t>(b0 & 0x1Fu);
                        minimum = 0x80u;
                    }
                else if (b0 < 0xF0u)
                    {
                        need = 3;
                        cp = static_cast<char32_t>(b0 & 0x0Fu);
                        minimum = 0x800u;
                    }
                else
                    {
                        need = 4;
                        cp = static_cast<char32_t>(b0 & 0x07u);
                        minimum = 0x10000u;
                    }
                for (uint32_t i = 1; i < need; ++i)
                    {
                        if (i >= len)
                            return {0, 0, UtfError::TRUNCATED};
                        const auto b = static_cast<unsigned char>(p[i]);
                        if ((b & 0xC0u) != 0x80u)
                            return {0, 0, UtfError::TRUNCATED};
                        cp = static_cast<char32_t>((cp << 6) | (b & 0x3Fu));
                    }
                if (cp < minimum)
                    return {0, 0, UtfError::OVERLONG};
                if (cp >= 0xD800u && cp <= 0xDFFFu)
                    return {0, 0, UtfError::SURROGATE};
                if (cp > 0x10FFFFu)
                    return {0, 0, UtfError::TOO_LARGE};
                return {cp, need, UtfError::NONE};
            }
        if (b0 < 0xF8u)
            return {0, 0, UtfError::TOO_LARGE};
        return {0, 0, UtfError::INVALID_LEAD};
    }

    export [[nodiscard]] constexpr Utf16Decode decode_utf16(const char16_t* p, const size_t len) noexcept
    {
        if (len == 0)
            return {0, 0, UtfError::TRUNCATED};
        const char16_t u0 = p[0];
        if (is_high_surrogate(u0))
            {
                if (len < 2)
                    return {0, 0, UtfError::TRUNCATED};
                if (!is_low_surrogate(p[1]))
                    return {0, 0, UtfError::INVALID_UTF16_PAIR};
                return {combine_surrogates(u0, p[1]), 2, UtfError::NONE};
            }
        if (is_low_surrogate(u0))
            return {0, 0, UtfError::INVALID_UTF16_PAIR};
        return {static_cast<char32_t>(u0), 1, UtfError::NONE};
    }

    export [[nodiscard]] constexpr Utf8Decode decode_utf32(const char32_t* p, const size_t len) noexcept
    {
        if (len == 0)
            return {0, 0, UtfError::TRUNCATED};
        const char32_t cp = p[0];
        if (cp >= 0xD800u && cp <= 0xDFFFu)
            return {0, 0, UtfError::SURROGATE};
        if (cp > 0x10FFFFu)
            return {0, 0, UtfError::TOO_LARGE};
        return {cp, 1, UtfError::NONE};
    }

    export [[nodiscard]] constexpr uint32_t encode_utf8(const char32_t cp, char* out, const size_t cap) noexcept
    {
        const size_t need = utf8_length_from_code_point(cp);
        if (need == 0 || cap < need)
            return 0;
        const auto b = static_cast<unsigned char>(cp < 0x80u ? 0 : cp < 0x800u ? 1 : cp < 0x10000u ? 2 : 3);
        if (b == 0)
            {
                out[0] = static_cast<char>(cp);
                return 1;
            }
        if (b == 1)
            {
                out[0] = static_cast<char>(0xC0u | (cp >> 6));
                out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 2;
            }
        if (b == 2)
            {
                out[0] = static_cast<char>(0xE0u | (cp >> 12));
                out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
                out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
                return 3;
            }
        out[0] = static_cast<char>(0xF0u | (cp >> 18));
        out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 4;
    }

    export [[nodiscard]] constexpr uint32_t encode_utf16(const char32_t cp, char16_t* out, const size_t cap) noexcept
    {
        const size_t need = utf16_length_from_code_point(cp);
        if (need == 0 || cap < need)
            return 0;
        if (need == 1)
            {
                out[0] = static_cast<char16_t>(cp);
                return 1;
            }
        const char32_t v = cp - 0x10000u;
        out[0] = static_cast<char16_t>(0xD800u + (v >> 10));
        out[1] = static_cast<char16_t>(0xDC00u + (v & 0x3FFu));
        return 2;
    }

    export [[nodiscard]] constexpr UtfValidation validate_utf8(const char* p, const size_t len) noexcept
    {
        size_t pos = 0;
        while (pos < len)
            {
                const Utf8Decode decoded = decode_utf8(p + pos, len - pos);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, pos};
                pos += decoded._M_consumed;
            }
        return {UtfError::NONE, len};
    }

    export [[nodiscard]] constexpr UtfValidation validate_utf16(const char16_t* p, const size_t len) noexcept
    {
        size_t pos = 0;
        while (pos < len)
            {
                const Utf16Decode decoded = decode_utf16(p + pos, len - pos);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, pos};
                pos += decoded._M_consumed;
            }
        return {UtfError::NONE, len};
    }

    export [[nodiscard]] constexpr UtfValidation validate_utf32(const char32_t* p, const size_t len) noexcept
    {
        size_t pos = 0;
        while (pos < len)
            {
                const Utf8Decode decoded = decode_utf32(p + pos, len - pos);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, pos};
                pos += decoded._M_consumed;
            }
        return {UtfError::NONE, len};
    }

    export [[nodiscard]] constexpr bool is_valid_utf8(const char* p, const size_t len) noexcept
    {
        return validate_utf8(p, len).ok();
    }

    export [[nodiscard]] constexpr bool is_valid_utf16(const char16_t* p, const size_t len) noexcept
    {
        return validate_utf16(p, len).ok();
    }

    export [[nodiscard]] constexpr bool is_ascii(const char* p, const size_t len) noexcept
    {
        for (size_t i = 0; i < len; ++i)
            {
                if (static_cast<unsigned char>(p[i]) >= 0x80u)
                    return false;
            }
        return true;
    }

    export [[nodiscard]] constexpr size_t utf8_length_from_utf16(const char16_t* p, const size_t len) noexcept
    {
        size_t total = 0;
        size_t i = 0;
        while (i < len)
            {
                const char16_t u = p[i];
                if (is_high_surrogate(u))
                    {
                        if (i + 1 >= len || !is_low_surrogate(p[i + 1]))
                            return 0;
                        total += 4;
                        i += 2;
                        continue;
                    }
                if (is_low_surrogate(u))
                    return 0;
                const size_t add = utf8_length_from_code_point(static_cast<char32_t>(u));
                if (add == 0)
                    return 0;
                total += add;
                ++i;
            }
        return total;
    }

    export [[nodiscard]] constexpr size_t utf16_length_from_utf8(const char* p, const size_t len) noexcept
    {
        size_t total = 0;
        size_t pos = 0;
        while (pos < len)
            {
                const Utf8Decode decoded = decode_utf8(p + pos, len - pos);
                if (decoded._M_error != UtfError::NONE)
                    return 0;
                const size_t add = utf16_length_from_code_point(decoded._M_code_point);
                if (add == 0)
                    return 0;
                total += add;
                pos += decoded._M_consumed;
            }
        return total;
    }

    export [[nodiscard]] constexpr size_t utf8_count_code_points(const char* p, const size_t len) noexcept
    {
        size_t count = 0;
        size_t pos = 0;
        while (pos < len)
            {
                const Utf8Decode decoded = decode_utf8(p + pos, len - pos);
                if (decoded._M_error != UtfError::NONE)
                    return 0;
                ++count;
                pos += decoded._M_consumed;
            }
        return count;
    }

    export [[nodiscard]] constexpr size_t utf16_count_code_points(const char16_t* p, const size_t len) noexcept
    {
        size_t count = 0;
        size_t i = 0;
        while (i < len)
            {
                const char16_t u = p[i];
                if (is_high_surrogate(u))
                    {
                        if (i + 1 >= len || !is_low_surrogate(p[i + 1]))
                            return 0;
                        i += 2;
                    }
                else if (is_low_surrogate(u))
                    {
                        return 0;
                    }
                else
                    {
                        ++i;
                    }
                ++count;
            }
        return count;
    }

    export [[nodiscard]] constexpr UtfConvert utf8_to_utf16(const char* src, const size_t slen, char16_t* dst,
                                                            const size_t dcap) noexcept
    {
        size_t si = 0;
        size_t written = 0;
        while (si < slen)
            {
                const Utf8Decode decoded = decode_utf8(src + si, slen - si);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, written, si};
                const size_t need = utf16_length_from_code_point(decoded._M_code_point);
                if (need == 0)
                    return {UtfError::SURROGATE, written, si};
                if (written + need > dcap)
                    return {UtfError::BUFFER_TO_SMALL, written, si};
                written += encode_utf16(decoded._M_code_point, dst + written, dcap - written);
                si += decoded._M_consumed;
            }
        return {UtfError::NONE, written, 0};
    }

    export [[nodiscard]] constexpr UtfConvert utf16_to_utf8(const char16_t* src, const size_t slen, char* dst,
                                                            const size_t dcap) noexcept
    {
        size_t si = 0;
        size_t written = 0;
        while (si < slen)
            {
                const Utf16Decode decoded = decode_utf16(src + si, slen - si);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, written, si};
                const size_t need = utf8_length_from_code_point(decoded._M_code_point);
                if (need == 0)
                    return {UtfError::TOO_LARGE, written, si};
                if (written + need > dcap)
                    return {UtfError::BUFFER_TO_SMALL, written, si};
                written += encode_utf8(decoded._M_code_point, dst + written, dcap - written);
                si += decoded._M_consumed;
            }
        return {UtfError::NONE, written, 0};
    }

    export [[nodiscard]] constexpr UtfConvert utf8_to_utf32(const char* src, const size_t slen, char32_t* dst,
                                                            const size_t dcap) noexcept
    {
        size_t si = 0;
        size_t written = 0;
        while (si < slen)
            {
                const Utf8Decode decoded = decode_utf8(src + si, slen - si);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, written, si};
                if (written + 1 > dcap)
                    return {UtfError::BUFFER_TO_SMALL, written, si};
                dst[written++] = decoded._M_code_point;
                si += decoded._M_consumed;
            }
        return {UtfError::NONE, written, 0};
    }

    export [[nodiscard]] constexpr UtfConvert utf32_to_utf8(const char32_t* src, const size_t slen, char* dst,
                                                            const size_t dcap) noexcept
    {
        size_t si = 0;
        size_t written = 0;
        while (si < slen)
            {
                const Utf8Decode decoded = decode_utf32(src + si, slen - si);
                if (decoded._M_error != UtfError::NONE)
                    return {decoded._M_error, written, si};
                const size_t need = utf8_length_from_code_point(decoded._M_code_point);
                if (need == 0)
                    return {decoded._M_error, written, si};
                if (written + need > dcap)
                    return {UtfError::BUFFER_TO_SMALL, written, si};
                written += encode_utf8(decoded._M_code_point, dst + written, dcap - written);
                si += decoded._M_consumed;
            }
        return {UtfError::NONE, written, 0};
    }
} // namespace embdr::cxxstd
