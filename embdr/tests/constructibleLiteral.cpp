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

#include <catch2/catch_all.hpp>

#include <cstdint>

import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.literal;

using embdr::cxxstd::Maybe;
using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

namespace
{
    Maybe<NumericLiteralInfo, NumericError> parse(const SimpleStringView text) { return parse_numeric_literal(text); }

    constexpr bool parses_at_compile_time() noexcept
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse_numeric_literal("0xFFint16");
        return parsed.has_value() && parsed.value().radix == LiteralRadix::HEX &&
               parsed.value().suffix == LiteralSuffixKind::INT_TYPE && !parsed.value().is_float;
    }

    constexpr uint64_t fold_at_compile_time() noexcept
    {
        const Maybe<uint64_t, NumericError> value = parse_int_value("0xFF");
        return value.has_value() ? value.value() : 0;
    }
} // namespace

static_assert(parses_at_compile_time(), "numeric literal parsing must work in constant evaluation");
static_assert(fold_at_compile_time() == 255, "integer literals must fold at compile time");

TEST_CASE("Numeric error messages cover every error kind", "[literal]")
{
    REQUIRE(as_str(NumericErrorKind::EMPTY_BODY) == "number has no digits");
    REQUIRE(as_str(NumericErrorKind::DIGIT_OUT_OF_BASE) == "digit is out of range for this base");
    REQUIRE(as_str(NumericErrorKind::LEADING_SEPARATOR) == "separator '_' before the first digit");
    REQUIRE(as_str(NumericErrorKind::TRAILING_SEPARATOR) == "separator '_' does not separate two digits");
    REQUIRE(as_str(NumericErrorKind::FLOAT_IN_NON_DECIMAL) == "fraction on a non-decimal literal");
    REQUIRE(as_str(NumericErrorKind::DANGLING_EXPONENT) == "exponent has no digits");
    REQUIRE(as_str(NumericErrorKind::INVALID_SUFFIX) == "invalid literal suffix");
    REQUIRE(as_str(NumericErrorKind::SUFFIX_MISMATCH) == "suffix does not match the literal form");
    REQUIRE(as_str(NumericErrorKind::TRAILING_CHAR) == "unexpected trailing character");
    REQUIRE(as_str(NumericErrorKind::OVERFLOW) == "integer literal overflows");
    REQUIRE(as_str(NumericErrorKind::NOT_AN_INTEGER) == "expected an integer literal");
}

TEST_CASE("radix_value maps every radix", "[literal]")
{
    REQUIRE(radix_value(LiteralRadix::BINARY) == 2);
    REQUIRE(radix_value(LiteralRadix::OCTAL) == 8);
    REQUIRE(radix_value(LiteralRadix::DECIMAL) == 10);
    REQUIRE(radix_value(LiteralRadix::HEX) == 16);
}

TEST_CASE("literal suffix classification", "[literal]")
{
    REQUIRE(literal_suffix_kind("int32") == LiteralSuffixKind::INT_TYPE);
    REQUIRE(literal_suffix_kind("uint64") == LiteralSuffixKind::INT_TYPE);
    REQUIRE(literal_suffix_kind("int512") == LiteralSuffixKind::INT_TYPE);
    REQUIRE(literal_suffix_kind("float16") == LiteralSuffixKind::FLOAT_TYPE);
    REQUIRE(literal_suffix_kind("float512") == LiteralSuffixKind::FLOAT_TYPE);
    // u8 is a string prefix, never a numeric type
    REQUIRE(literal_suffix_kind("u8") == LiteralSuffixKind::NONE);
    REQUIRE(literal_suffix_kind("i32") == LiteralSuffixKind::NONE);
    REQUIRE(literal_suffix_kind("") == LiteralSuffixKind::NONE);
}

TEST_CASE("well-formed literals parse with the right shape", "[literal]")
{
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("42");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().radix == LiteralRadix::DECIMAL);
        REQUIRE(!parsed.value().is_float);
        REQUIRE(!parsed.value().has_radix_prefix);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("0xFF");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().radix == LiteralRadix::HEX);
        REQUIRE(parsed.value().has_radix_prefix);
        REQUIRE(parsed.value().digit_begin == 2);
        REQUIRE(parsed.value().digit_end == 4);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("0o777");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().radix == LiteralRadix::OCTAL);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("0b1010");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().radix == LiteralRadix::BINARY);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("1_000_000");
        REQUIRE(parsed.has_value());
        REQUIRE(!parsed.value().is_float);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("3.14");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().is_float);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("6.02e23");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().is_float);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("1e-5");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().is_float);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("100uint32");
        REQUIRE(parsed.has_value());
        REQUIRE(!parsed.value().is_float);
        REQUIRE(parsed.value().suffix == LiteralSuffixKind::INT_TYPE);
        REQUIRE(parsed.value().suffix_text == "uint32");
        REQUIRE(parsed.value().suffix_offset == 3);
    }
    {
        const Maybe<NumericLiteralInfo, NumericError> parsed = parse("3.14float64");
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value().is_float);
        REQUIRE(parsed.value().suffix == LiteralSuffixKind::FLOAT_TYPE);
    }
}

TEST_CASE("malformed literals report kind and offset", "[literal]")
{
    struct ErrorCase
    {
        const char* text;
        NumericErrorKind kind;
        uint32_t offset;
    };
    constexpr ErrorCase cases[] = {
        {"0x", NumericErrorKind::EMPTY_BODY, 2},
        {"0b", NumericErrorKind::EMPTY_BODY, 2},
        {"0o", NumericErrorKind::EMPTY_BODY, 2},
        {"0b12", NumericErrorKind::DIGIT_OUT_OF_BASE, 3},
        {"0o8", NumericErrorKind::EMPTY_BODY, 2},
        {"0x_1", NumericErrorKind::LEADING_SEPARATOR, 2},
        {"1_", NumericErrorKind::TRAILING_SEPARATOR, 1},
        {"1__0", NumericErrorKind::TRAILING_SEPARATOR, 1},
        {"1_ 0", NumericErrorKind::TRAILING_SEPARATOR, 1},
        {"0x1.5", NumericErrorKind::FLOAT_IN_NON_DECIMAL, 3},
        {"1e", NumericErrorKind::DANGLING_EXPONENT, 1},
        {"1e+", NumericErrorKind::DANGLING_EXPONENT, 1},
        {"1e_", NumericErrorKind::DANGLING_EXPONENT, 1},
        {"123i32", NumericErrorKind::INVALID_SUFFIX, 3},
        {"1u8", NumericErrorKind::INVALID_SUFFIX, 1},
        {"0b10float64", NumericErrorKind::SUFFIX_MISMATCH, 4},
        {"3.14int32", NumericErrorKind::SUFFIX_MISMATCH, 4},
        {"0x1!", NumericErrorKind::TRAILING_CHAR, 3},
    };
    for (const ErrorCase& entry : cases)
        {
            const Maybe<NumericLiteralInfo, NumericError> parsed = parse(entry.text);
            REQUIRE(!parsed.has_value());
            REQUIRE(parsed.error().kind == entry.kind);
            REQUIRE(parsed.error().offset == entry.offset);
        }
}

TEST_CASE("parse_int_value folds every radix and separators", "[literal]")
{
    struct ValueCase
    {
        const char* text;
        uint64_t value;
    };
    constexpr ValueCase cases[] = {
        {"0", 0},
        {"42", 42},
        {"0xFF", 255},
        {"0xff", 255},
        {"0o777", 511},
        {"0b1010", 10},
        {"1_000_000", 1000000},
        {"0xF_F", 255},
        {"18446744073709551615", 18446744073709551615ULL},
    };
    for (const ValueCase& entry : cases)
        {
            const Maybe<uint64_t, NumericError> value = parse_int_value(entry.text);
            REQUIRE(value.has_value());
            REQUIRE(value.value() == entry.value);
        }
}

TEST_CASE("parse_int_value reports overflow and rejects floats", "[literal]")
{
    const Maybe<uint64_t, NumericError> overflow = parse_int_value("18446744073709551616");
    REQUIRE(!overflow.has_value());
    REQUIRE(overflow.error().kind == NumericErrorKind::OVERFLOW);

    const Maybe<uint64_t, NumericError> hex_overflow = parse_int_value("0x1FFFFFFFFFFFFFFFF");
    REQUIRE(!hex_overflow.has_value());
    REQUIRE(hex_overflow.error().kind == NumericErrorKind::OVERFLOW);

    const Maybe<uint64_t, NumericError> not_int = parse_int_value("3.14");
    REQUIRE(!not_int.has_value());
    REQUIRE(not_int.error().kind == NumericErrorKind::NOT_AN_INTEGER);

    // parser errors propagate unchanged
    const Maybe<uint64_t, NumericError> bad = parse_int_value("0b12");
    REQUIRE(!bad.has_value());
    REQUIRE(bad.error().kind == NumericErrorKind::DIGIT_OUT_OF_BASE);
}

TEST_CASE("scan_escape decodes every supported form", "[literal]")
{
    {
        const EscapeResult esc = scan_escape("\\n", 0);
        REQUIRE(esc.ok);
        REQUIRE(esc.next == 2);
        REQUIRE(esc.code_point == '\n');
    }
    {
        const EscapeResult esc = scan_escape("\\x41", 0);
        REQUIRE(esc.ok);
        REQUIRE(esc.next == 4);
        REQUIRE(esc.code_point == 'A');
    }
    {
        const EscapeResult esc = scan_escape("\\u{1F600}", 0);
        REQUIRE(esc.ok);
        REQUIRE(esc.next == 9);
        REQUIRE(esc.code_point == 0x1F600);
    }
    {
        const EscapeResult esc = scan_escape("\\\\", 0);
        REQUIRE(esc.ok);
        REQUIRE(esc.code_point == '\\');
    }
    {
        const EscapeResult esc = scan_escape("\\'", 0);
        REQUIRE(esc.ok);
        REQUIRE(esc.code_point == '\'');
    }
}

TEST_CASE("scan_escape rejects malformed sequences", "[literal]")
{
    REQUIRE(!scan_escape("\\q", 0).ok); // unknown letter
    REQUIRE(!scan_escape("\\", 0).ok); // dangling backslash
    REQUIRE(!scan_escape("\\xZZ", 0).ok); // non-hex digits
    REQUIRE(!scan_escape("\\x4", 0).ok); // too short
    REQUIRE(!scan_escape("\\u0041", 0).ok); // no braces
    REQUIRE(!scan_escape("\\u{}", 0).ok); // no digits
    REQUIRE(!scan_escape("\\u{D800}", 0).ok); // surrogate is not a scalar
    REQUIRE(!scan_escape("\\u{110000}", 0).ok); // above U+10FFFF
    // more than six hex digits
    REQUIRE(!scan_escape("\\u{0000041}", 0).ok);
}

TEST_CASE("decode_string_body decodes escapes into the caller buffer", "[literal]")
{
    char buffer[64] = {};

    const Maybe<StringDecode, StringError> plain = decode_string_body("hi", false, buffer, sizeof(buffer));
    REQUIRE(plain.has_value());
    REQUIRE(plain.value().written == 2);
    REQUIRE(plain.value().code_points == 2);
    REQUIRE(SimpleStringView{buffer, plain.value().written} == "hi");

    const Maybe<StringDecode, StringError> escapes = decode_string_body("a\\nb\\x41", false, buffer, sizeof(buffer));
    REQUIRE(escapes.has_value());
    REQUIRE(SimpleStringView{buffer, escapes.value().written} == "a\nbA");

    // \xFF is U+00FF, encoded as two UTF-8 bytes — strings stay valid UTF-8
    const Maybe<StringDecode, StringError> high = decode_string_body("\\xFF", false, buffer, sizeof(buffer));
    REQUIRE(high.has_value());
    REQUIRE(high.value().written == 2);
    REQUIRE(static_cast<unsigned char>(buffer[0]) == 0xC3);
    REQUIRE(static_cast<unsigned char>(buffer[1]) == 0xBF);

    // raw bodies keep backslashes verbatim
    const Maybe<StringDecode, StringError> raw = decode_string_body("a\\nb", true, buffer, sizeof(buffer));
    REQUIRE(raw.has_value());
    REQUIRE(SimpleStringView{buffer, raw.value().written} == "a\\nb");

    // the empty body decodes to nothing
    const Maybe<StringDecode, StringError> empty = decode_string_body("", false, buffer, sizeof(buffer));
    REQUIRE(empty.has_value());
    REQUIRE(empty.value().written == 0);
}

TEST_CASE("decode_string_body reports failures", "[literal]")
{
    char buffer[4] = {};

    const Maybe<StringDecode, StringError> bad_escape = decode_string_body("a\\qb", false, buffer, sizeof(buffer));
    REQUIRE(!bad_escape.has_value());
    REQUIRE(bad_escape.error().kind == StringErrorKind::INVALID_ESCAPE);
    REQUIRE(bad_escape.error().offset == 1);

    // 0xC3 demands a continuation byte; '(' is not one
    const Maybe<StringDecode, StringError> bad_utf8 = decode_string_body("a\xC3(b", false, buffer, sizeof(buffer));
    REQUIRE(!bad_utf8.has_value());
    REQUIRE(bad_utf8.error().kind == StringErrorKind::INVALID_UTF8);
    REQUIRE(bad_utf8.error().offset == 1);

    // "abcd" does not fit a 4-byte buffer once "ab" already filled it — three
    // bytes need more room than remains
    const Maybe<StringDecode, StringError> too_small = decode_string_body("abcd", false, buffer, 3);
    REQUIRE(!too_small.has_value());
    REQUIRE(too_small.error().kind == StringErrorKind::BUFFER_TOO_SMALL);

    // a two-byte scalar does not fit a one-byte remainder
    const Maybe<StringDecode, StringError> utf8_too_small = decode_string_body("\\u{1F600}", false, buffer, 3);
    REQUIRE(!utf8_too_small.has_value());
    REQUIRE(utf8_too_small.error().kind == StringErrorKind::BUFFER_TOO_SMALL);
}

TEST_CASE("String error messages cover every error kind", "[literal]")
{
    REQUIRE(as_str(StringErrorKind::INVALID_ESCAPE) == "invalid escape sequence");
    REQUIRE(as_str(StringErrorKind::INVALID_UTF8) == "invalid UTF-8 sequence");
    REQUIRE(as_str(StringErrorKind::BUFFER_TOO_SMALL) == "decoded string does not fit the buffer");
}

TEST_CASE("decode_char_literal folds a single scalar", "[literal]")
{
    const Maybe<char32_t, CharError> plain = decode_char_literal("'a'");
    REQUIRE(plain.has_value());
    REQUIRE(plain.value() == 'a');

    const Maybe<char32_t, CharError> escaped = decode_char_literal("'\\n'");
    REQUIRE(escaped.has_value());
    REQUIRE(escaped.value() == '\n');

    const Maybe<char32_t, CharError> hex = decode_char_literal("'\\x41'");
    REQUIRE(hex.has_value());
    REQUIRE(hex.value() == 'A');

    const Maybe<char32_t, CharError> unicode = decode_char_literal("'\\u{1F600}'");
    REQUIRE(unicode.has_value());
    REQUIRE(unicode.value() == 0x1F600);

    const Maybe<char32_t, CharError> utf8 = decode_char_literal("'é'");
    REQUIRE(utf8.has_value());
    REQUIRE(utf8.value() == 0xE9);
}

TEST_CASE("decode_char_literal reports failures", "[literal]")
{
    const Maybe<char32_t, CharError> unterminated = decode_char_literal("'a");
    REQUIRE(!unterminated.has_value());
    REQUIRE(unterminated.error().kind == CharErrorKind::UNTERMINATED);

    const Maybe<char32_t, CharError> empty = decode_char_literal("''");
    REQUIRE(!empty.has_value());
    REQUIRE(empty.error().kind == CharErrorKind::EMPTY);

    const Maybe<char32_t, CharError> too_long = decode_char_literal("'ab'");
    REQUIRE(!too_long.has_value());
    REQUIRE(too_long.error().kind == CharErrorKind::TOO_LONG);

    const Maybe<char32_t, CharError> bad_escape = decode_char_literal("'\\q'");
    REQUIRE(!bad_escape.has_value());
    REQUIRE(bad_escape.error().kind == CharErrorKind::INVALID_ESCAPE);

    // 0xC3 demands a continuation byte; ')' is not one
    const Maybe<char32_t, CharError> bad_utf8 = decode_char_literal("'\xC3('");
    REQUIRE(!bad_utf8.has_value());
    REQUIRE(bad_utf8.error().kind == CharErrorKind::INVALID_UTF8);
}

TEST_CASE("Char error messages cover every error kind", "[literal]")
{
    REQUIRE(as_str(CharErrorKind::UNTERMINATED) == "unterminated character literal");
    REQUIRE(as_str(CharErrorKind::EMPTY) == "empty character literal");
    REQUIRE(as_str(CharErrorKind::TOO_LONG) == "character literal holds more than one scalar");
    REQUIRE(as_str(CharErrorKind::INVALID_ESCAPE) == "invalid escape sequence");
    REQUIRE(as_str(CharErrorKind::INVALID_UTF8) == "invalid UTF-8 sequence");
}
