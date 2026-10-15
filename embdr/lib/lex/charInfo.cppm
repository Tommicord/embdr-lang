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
#include <cstdint>

export module embdr.lex.charinfo;

namespace embdr::lex::charinfo
{
    // Character classification for the lexer — the shape of clang's
    // clang/lib/Lex/CharInfo.h, where every predicate a lexer needs is
    // centralized and constexpr.
    //
    // Deliberate difference: clang packs the classes into a 256-byte .rodata
    // bitmask table for O(1) lookup. We use range comparisons instead. On the
    // embedded targets this compiler serves, two compares are smaller than a
    // 256-byte table (binary size outranks everything in CODE_QUALITY §1), the
    // predicates fold away under constant evaluation, and GCC compiles the
    // range chains to the same branchless form the table would produce anyway.

    export [[nodiscard]] constexpr bool is_ascii(const char c) noexcept
    {
        return static_cast<unsigned char>(c) < 0x80u;
    }

    export [[nodiscard]] constexpr bool is_alpha(const char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    export [[nodiscard]] constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

    export [[nodiscard]] constexpr bool is_alnum(const char c) noexcept { return is_alpha(c) || is_digit(c); }

    export [[nodiscard]] constexpr bool is_hex_digit(const char c) noexcept
    {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    export [[nodiscard]] constexpr bool is_octal_digit(const char c) noexcept { return c >= '0' && c <= '7'; }

    // Source files are UTF-8, but identifiers stay ASCII: a non-ASCII byte in
    // an identifier would need normalization tables that embedded targets
    // cannot spare. Bytes >= 0x80 may appear only inside literals, where the
    // lexer validates them through embdr.cxxstd.utfEncode.
    export [[nodiscard]] constexpr bool is_identifier_start(const char c) noexcept { return is_alpha(c) || c == '_'; }

    export [[nodiscard]] constexpr bool is_identifier_body(const char c) noexcept
    {
        return is_identifier_start(c) || is_digit(c);
    }

    // ' ', '\t', '\v' (vertical tab) and '\f' (form feed) end no line.
    export [[nodiscard]] constexpr bool is_horizontal_space(const char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\v' || c == '\f';
    }

    // '\r' is a line terminator too (pre-CRLF systems and embedded test
    // fixtures use it); the lexer folds "\r\n" into a single line break.
    export [[nodiscard]] constexpr bool is_newline(const char c) noexcept { return c == '\n' || c == '\r'; }

    export [[nodiscard]] constexpr bool is_space(const char c) noexcept
    {
        return is_horizontal_space(c) || is_newline(c);
    }

    // Printable ASCII excluding space, letters and digits — the true
    // punctuation set (0x21..0x7E minus [0-9A-Za-z]).
    export [[nodiscard]] constexpr bool is_punctuation(const char c) noexcept
    {
        const auto b = static_cast<unsigned char>(c);
        return (b >= 0x21u && b <= 0x7Eu) && !is_alnum(c);
    }

    // Numeric value of a hex digit.
    // Precondition: is_hex_digit(c) — violating it terminates (CODE_QUALITY §2).
    export [[nodiscard]] constexpr uint32_t base_digit_value(const char c) noexcept
    {
        if (is_digit(c))
            {
                return static_cast<uint32_t>(c - '0');
            }
        if (c >= 'a' && c <= 'f')
            {
                return static_cast<uint32_t>(c - 'a') + 10U;
            }
        return static_cast<uint32_t>(c - 'A') + 10U;
    }

    // True when c is a digit of the given base (2, 8, 10 or 16). Any other
    // base value classifies nothing.
    export [[nodiscard]] constexpr bool is_base_digit(const char c, const uint32_t base) noexcept
    {
        switch (base)
            {
                case 2:
                    return c == '0' || c == '1';
                case 8:
                    return is_octal_digit(c);
                case 10:
                    return is_digit(c);
                case 16:
                    return is_hex_digit(c);
                default:
                    return false;
            }
    }
} // namespace embdr::lex::charinfo
