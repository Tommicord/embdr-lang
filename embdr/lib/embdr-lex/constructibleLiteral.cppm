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
#include <limits>

export module embdr.lex.literal;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.utfEncode;
import embdr.lex.charinfo;

namespace embdr::lex
{
  using charinfo::base_digit_value;
  using charinfo::is_base_digit;
  using charinfo::is_digit;
  using charinfo::is_identifier_body;
  using charinfo::is_identifier_start;
  using cxxstd::decode_utf8;
  using cxxstd::encode_utf8;
  using cxxstd::is_unicode_scalar;
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;
  using cxxstd::Utf8Decode;
  using cxxstd::UtfError;

  // Literal parsing and decoding — the role clang splits between
  // clang/lib/Lex/LiteralSupport.cpp (NumericLiteralParser / StringLiteralParser
  // validate a token the lexer already bounded) and Sema: the lexer consumes the
  // glued extent of a literal; this module classifies and validates it, and
  // computes constant values for the compiler's constexpr engine.
  //
  // Splitting it out from the lexer buys three things:
  //   - the parsers are usable standalone (tests, cexpr constant folding);
  //   - error offsets are relative to the literal, so diagnostics can point
  //     into the middle of "0b10float64" without re-lexing;
  //   - the lexer keeps exactly one job: token boundaries.
  //
  // Everything is constexpr and allocates nothing (CODE_QUALITY §1.3); decoded
  // strings go into a caller-provided buffer, never an internal one.

  export enum class LiteralRadix : uint8_t {
    BINARY,
    OCTAL,
    DECIMAL,
    HEX,
  };

  export enum class LiteralSuffixKind : uint8_t {
    NONE, // no suffix, or a spelling that is not a valid type suffix
    INT_TYPE, // int8..int512, uint8..uint512
    FLOAT_TYPE // float16..float512
  };

  export enum class NumericErrorKind : uint8_t {
    EMPTY_BODY, // "0x" — no digits after the radix prefix
    DIGIT_OUT_OF_BASE, // "0b12" — a decimal digit the radix does not have
    LEADING_SEPARATOR, // "0x_1" — '_' before the first digit
    TRAILING_SEPARATOR, // "1_"  — '_' not followed by a digit
    FLOAT_IN_NON_DECIMAL, // "0x1.5" — fraction on a non-decimal literal
    DANGLING_EXPONENT, // "1e" / "1e+" — exponent without digits
    INVALID_SUFFIX, // "123i32" — suffix is not a numeric type name
    SUFFIX_MISMATCH, // "0b10float64" / "3.14int32" — suffix vs. form
    TRAILING_CHAR, // bytes left over after the literal
    OVERFLOW, // value does not fit in uint64_t
    NOT_AN_INTEGER, // parse_int_value() fed a float literal
  };

  export struct NumericError
  {
    NumericErrorKind kind = NumericErrorKind::EMPTY_BODY;
    uint32_t offset = 0; // byte offset relative to the literal start
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const NumericErrorKind kind) noexcept
  {
    switch (kind)
      {
        case NumericErrorKind::EMPTY_BODY:
          return "number has no digits";
        case NumericErrorKind::DIGIT_OUT_OF_BASE:
          return "digit is out of range for this base";
        case NumericErrorKind::LEADING_SEPARATOR:
          return "separator '_' before the first digit";
        case NumericErrorKind::TRAILING_SEPARATOR:
          return "separator '_' does not separate two digits";
        case NumericErrorKind::FLOAT_IN_NON_DECIMAL:
          return "fraction on a non-decimal literal";
        case NumericErrorKind::DANGLING_EXPONENT:
          return "exponent has no digits";
        case NumericErrorKind::INVALID_SUFFIX:
          return "invalid literal suffix";
        case NumericErrorKind::SUFFIX_MISMATCH:
          return "suffix does not match the literal form";
        case NumericErrorKind::TRAILING_CHAR:
          return "unexpected trailing character";
        case NumericErrorKind::OVERFLOW:
          return "integer literal overflows";
        case NumericErrorKind::NOT_AN_INTEGER:
          return "expected an integer literal";
      }
    return "unknown numeric literal error";
  }

  // Classification of a successfully bounded literal. Offsets are relative to
  // the parsed view; suffix_text points into the same buffer (valid as long as
  // the literal text itself).
  export struct NumericLiteralInfo
  {
    LiteralRadix radix = LiteralRadix::DECIMAL;
    LiteralSuffixKind suffix = LiteralSuffixKind::NONE;
    bool is_float = false;
    bool has_radix_prefix = false;
    uint32_t digit_begin = 0; // first body digit (past "0x" and friends)
    uint32_t digit_end = 0; // one past the last body digit
    SimpleStringView suffix_text{};
    uint32_t suffix_offset = 0;
  };

  export [[nodiscard]] constexpr uint32_t
  radix_value (const LiteralRadix radix) noexcept
  {
    switch (radix)
      {
        case LiteralRadix::BINARY:
          return 2;
        case LiteralRadix::OCTAL:
          return 8;
        case LiteralRadix::HEX:
          return 16;
        case LiteralRadix::DECIMAL:
          break;
      }
    return 10;
  }

  // Classifies a glued literal suffix. "u8" is a *string* prefix only, never
  // a numeric type, so it is NONE here (a "1u8" is a diagnostic, not uint8).
  export [[nodiscard]] constexpr LiteralSuffixKind
  literal_suffix_kind (const SimpleStringView text) noexcept
  {
    if (text == "int8" || text == "int16" || text == "int32" || text == "int64" || text == "int128" || text == "int256"
        || text == "int512" || text == "uint8" || text == "uint16" || text == "uint32" || text == "uint64"
        || text == "uint128" || text == "uint256" || text == "uint512")
      {
        return LiteralSuffixKind::INT_TYPE;
      }
    if (text == "float16" || text == "float32" || text == "float64" || text == "float128" || text == "float256"
        || text == "float512")
      {
        return LiteralSuffixKind::FLOAT_TYPE;
      }
    return LiteralSuffixKind::NONE;
  }

  // Scans a run of `radix` digits starting at `pos`, where '_' may appear
  // only between two digits. Advances `pos` past the run (a trailing
  // separator is an error, so `pos` never stops on '_'). The value tells
  // whether at least one digit was seen (callers that guaranteed a digit
  // up front can ignore it).
  [[nodiscard]] constexpr Maybe<bool, NumericError>
  scan_digits (const SimpleStringView literal, size_t& pos, const uint32_t radix) noexcept
  {
    bool saw_digit = false;
    while (pos < literal.size ())
      {
        const char c = literal[pos];
        if (c == '_')
          {
            if (!saw_digit)
              return NumericError{NumericErrorKind::LEADING_SEPARATOR, static_cast<uint32_t> (pos)};
            if (pos + 1 >= literal.size () || !is_base_digit (literal[pos + 1], radix))
              return NumericError{NumericErrorKind::TRAILING_SEPARATOR, static_cast<uint32_t> (pos)};
            ++pos;
            continue;
          }
        if (is_base_digit (c, radix))
          {
            saw_digit = true;
            ++pos;
            continue;
          }
        break;
      }
    return saw_digit;
  }

  // Validates one bounded numeric literal: radix prefix, body digits, '_' as
  // a separator only *between* digits, decimal fraction and exponent, glued
  // type suffix. Mirrors the checks clang's NumericLiteralParser performs on
  // the token LexNumericConstant already delimited.
  export [[nodiscard]] constexpr Maybe<NumericLiteralInfo, NumericError>
  parse_numeric_literal (const SimpleStringView literal) noexcept
  {
    NumericLiteralInfo info{};
    const size_t size = literal.size ();
    size_t i = 0;

    uint32_t radix = 10;
    if (size >= 2 && literal[0] == '0')
      {
        const char marker = literal[1];
        if (marker == 'x' || marker == 'X')
          radix = 16;
        else if (marker == 'o' || marker == 'O')
          radix = 8;
        else if (marker == 'b' || marker == 'B')
          radix = 2;
        if (radix != 10)
          {
            info.has_radix_prefix = true;
            i = 2;
          }
      }
    info.radix = radix == 16  ? LiteralRadix::HEX
                 : radix == 8 ? LiteralRadix::OCTAL
                 : radix == 2 ? LiteralRadix::BINARY
                              : LiteralRadix::DECIMAL;
    info.digit_begin = static_cast<uint32_t> (i);

    // Body: base digits with '_' only between digits.
    const Maybe<bool, NumericError> body = scan_digits (literal, i, radix);
    if (body.has_error ())
      return body.error ();
    if (!body.value ())
      return NumericError{NumericErrorKind::EMPTY_BODY, info.digit_begin};
    info.digit_end = static_cast<uint32_t> (i);

    // A decimal digit the chosen base does not have ("0b12").
    if (radix != 10 && i < size && is_digit (literal[i]))
      return NumericError{NumericErrorKind::DIGIT_OUT_OF_BASE, static_cast<uint32_t> (i)};

    bool is_float = false;
    if (i < size && literal[i] == '.' && !(i + 1 < size && literal[i + 1] == '.'))
      {
        // A ".." is the range operator glued to the number, never a
        // fraction (syntax.txt ranges: "1..5"); stop before it.
        if (radix != 10)
          return NumericError{NumericErrorKind::FLOAT_IN_NON_DECIMAL, static_cast<uint32_t> (i)};
        if (i + 1 < size && is_digit (literal[i + 1]))
          {
            is_float = true;
            ++i; // '.'
            // The look-ahead above guarantees one digit, so this
            // scan cannot fail on separators.
            const Maybe<bool, NumericError> fraction = scan_digits (literal, i, 10);
            if (fraction.has_error ())
              return fraction.error ();
            info.digit_end = static_cast<uint32_t> (i);
          }
      }

    // Exponent (decimal only): 'e'/'E', optional sign, >= 1 digit.
    if (radix == 10 && i < size && (literal[i] == 'e' || literal[i] == 'E'))
      {
        const size_t e_pos = i;
        size_t j = i + 1;
        if (j < size && (literal[j] == '+' || literal[j] == '-'))
          {
            ++j;
          }
        if (j >= size || !is_digit (literal[j]))
          return NumericError{NumericErrorKind::DANGLING_EXPONENT, static_cast<uint32_t> (e_pos)};
        is_float = true;
        i = j;
        const Maybe<bool, NumericError> exponent = scan_digits (literal, i, 10);
        if (exponent.has_error ())
          return exponent.error ();
      }

    // Optional type suffix, glued to the literal.
    if (i < size && is_identifier_start (literal[i]))
      {
        const size_t suffix_begin = i;
        while (i < size && is_identifier_body (literal[i]))
          {
            ++i;
          }
        const SimpleStringView text = literal.substr (suffix_begin, i - suffix_begin);
        const LiteralSuffixKind kind = literal_suffix_kind (text);
        if (kind == LiteralSuffixKind::NONE)
          return NumericError{NumericErrorKind::INVALID_SUFFIX, static_cast<uint32_t> (suffix_begin)};
        if (kind == LiteralSuffixKind::FLOAT_TYPE && radix != 10)
          return NumericError{NumericErrorKind::SUFFIX_MISMATCH, static_cast<uint32_t> (suffix_begin)};
        if (kind == LiteralSuffixKind::INT_TYPE && is_float)
          return NumericError{NumericErrorKind::SUFFIX_MISMATCH, static_cast<uint32_t> (suffix_begin)};
        info.suffix = kind;
        info.suffix_text = text;
        info.suffix_offset = static_cast<uint32_t> (suffix_begin);
        if (kind == LiteralSuffixKind::FLOAT_TYPE)
          {
            is_float = true;
          }
      }

    if (i != size)
      return NumericError{NumericErrorKind::TRAILING_CHAR, static_cast<uint32_t> (i)};

    info.is_float = is_float;
    return info;
  }

  // Constant-folds the value of an integer literal (any radix, '_' separators
  // stripped) — the entry point the cexpr engine needs for compile-time
  // evaluation. Overflow is an error value, never wrapped silently.
  export [[nodiscard]] constexpr Maybe<uint64_t, NumericError>
  parse_int_value (const SimpleStringView literal) noexcept
  {
    const Maybe<NumericLiteralInfo, NumericError> parsed = parse_numeric_literal (literal);
    if (!parsed.has_value ())
      return parsed.error ();
    const NumericLiteralInfo& info = parsed.value ();
    if (info.is_float)
      return NumericError{NumericErrorKind::NOT_AN_INTEGER, 0};

    const uint64_t radix = radix_value (info.radix);
    const uint64_t max = (std::numeric_limits<uint64_t>::max) ();
    uint64_t value = 0;
    for (uint32_t i = info.digit_begin; i < info.digit_end; ++i)
      {
        const char c = literal[i];
        if (c == '_')
          {
            continue; // separators were validated by the parser
          }
        const uint64_t digit = base_digit_value (c);
        if (value > (max - digit) / radix)
          return NumericError{NumericErrorKind::OVERFLOW, i};
        value = value * radix + digit;
      }
    return value;
  }

  // Result of scanning one escape sequence at source[pos] == '\\'.
  export struct EscapeResult
  {
    bool ok = false;
    uint32_t next = 0; // index one past the escape
    char32_t code_point = 0; // decoded scalar (simple escapes fill the ASCII value)
  };

  // Scans one escape at source[pos] == '\\' and returns the index after it.
  // Grammar: simple escapes (\n \r \t \0 \\ \' \" \a \b \f \v), \xHH with
  // exactly two hex digits, and \u{H..H} (1..6 hex digits, a valid Unicode
  // scalar). Anything else — including an unknown letter like \q — is an
  // error at `pos`, so diagnostics always blame the backslash.
  export [[nodiscard]] constexpr EscapeResult
  scan_escape (const SimpleStringView source, const size_t pos) noexcept
  {
    if (pos + 1 >= source.size ())
      {
        return EscapeResult{false, static_cast<uint32_t> (pos + 1), 0};
      }
    const char e = source[pos + 1];
    switch (e)
      {
        case '0':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), 0};
        case 'a':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\a'};
        case 'b':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\b'};
        case 'f':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\f'};
        case 'n':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\n'};
        case 'r':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\r'};
        case 't':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\t'};
        case 'v':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\v'};
        case '\\':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\\'};
        case '\'':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '\''};
        case '"':
          return EscapeResult{true, static_cast<uint32_t> (pos + 2), '"'};
        case 'x':
          {
            if (pos + 3 >= source.size ())
              {
                return EscapeResult{false, static_cast<uint32_t> (pos + 2), 0};
              }
            if (!is_base_digit (source[pos + 2], 16) || !is_base_digit (source[pos + 3], 16))
              {
                return EscapeResult{false, static_cast<uint32_t> (pos + 2), 0};
              }
            const uint32_t value = base_digit_value (source[pos + 2]) * 16U + base_digit_value (source[pos + 3]);
            return EscapeResult{true, static_cast<uint32_t> (pos + 4), static_cast<char32_t> (value)};
          }
        case 'u':
          {
            if (pos + 3 >= source.size () || source[pos + 2] != '{')
              {
                return EscapeResult{false, static_cast<uint32_t> (pos + 1), 0};
              }
            size_t i = pos + 3;
            uint32_t value = 0;
            uint32_t digits = 0;
            while (i < source.size () && source[i] != '}')
              {
                if (!is_base_digit (source[i], 16) || digits >= 6)
                  {
                    return EscapeResult{false, static_cast<uint32_t> (pos + 1), 0};
                  }
                value = value * 16U + base_digit_value (source[i]);
                ++digits;
                ++i;
              }
            if (i >= source.size () || source[i] != '}' || digits == 0 || !is_unicode_scalar (value))
              {
                return EscapeResult{false, static_cast<uint32_t> (pos + 1), 0};
              }
            return EscapeResult{true, static_cast<uint32_t> (i + 1), static_cast<char32_t> (value)};
          }
        default:
          return EscapeResult{false, static_cast<uint32_t> (pos + 1), 0};
      }
  }

  export enum class StringErrorKind : uint8_t {
    INVALID_ESCAPE, // bad escape sequence at the reported offset
    INVALID_UTF8, // malformed UTF-8 at the reported offset
    BUFFER_TOO_SMALL, // caller buffer could not hold the decoded bytes
  };

  export struct StringError
  {
    StringErrorKind kind = StringErrorKind::INVALID_ESCAPE;
    uint32_t offset = 0; // byte offset relative to the body start
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const StringErrorKind kind) noexcept
  {
    switch (kind)
      {
        case StringErrorKind::INVALID_ESCAPE:
          return "invalid escape sequence";
        case StringErrorKind::INVALID_UTF8:
          return "invalid UTF-8 sequence";
        case StringErrorKind::BUFFER_TOO_SMALL:
          return "decoded string does not fit the buffer";
      }
    return "unknown string literal error";
  }

  export struct StringDecode
  {
    uint32_t written = 0; // bytes written to the output buffer
    uint32_t code_points = 0; // Unicode scalars produced
  };

  // Decodes the *body* of a string literal (between the quotes) into a
  // caller-provided buffer — clang's StringLiteralParser without the
  // allocation: it appends into scratch memory we cannot afford
  // (CODE_QUALITY §1.3).
  //
  // Escapes are decoded to Unicode scalars and re-encoded as UTF-8, so
  // "\xFF" is U+00FF (bytes C3 BF), not the raw byte 0xFF: strings stay valid
  // UTF-8 by construction. Raw bodies (r"..." / p"...") are copied verbatim
  // but still UTF-8 validated.
  export [[nodiscard]] constexpr Maybe<StringDecode, StringError>
  decode_string_body (const SimpleStringView body, const bool raw, char* const out, const size_t capacity) noexcept
  {
    StringDecode result{};
    size_t pos = 0;
    while (pos < body.size ())
      {
        const char c = body[pos];
        if (!raw && c == '\\')
          {
            const EscapeResult esc = scan_escape (body, pos);
            if (!esc.ok)
              return StringError{StringErrorKind::INVALID_ESCAPE, static_cast<uint32_t> (pos)};
            if (!is_unicode_scalar (esc.code_point))
              return StringError{StringErrorKind::INVALID_ESCAPE, static_cast<uint32_t> (pos)};
            const size_t need = cxxstd::utf8_length_from_code_point (esc.code_point);
            if (result.written + need > capacity)
              return StringError{StringErrorKind::BUFFER_TOO_SMALL, static_cast<uint32_t> (pos)};
            result.written += encode_utf8 (esc.code_point, out + result.written, capacity - result.written);
            ++result.code_points;
            pos = esc.next;
            continue;
          }
        const Utf8Decode decoded = decode_utf8 (body.data () + pos, body.size () - pos);
        if (decoded._M_error != UtfError::NONE)
          return StringError{StringErrorKind::INVALID_UTF8, static_cast<uint32_t> (pos)};
        const size_t need = cxxstd::utf8_length_from_code_point (decoded._M_code_point);
        if (result.written + need > capacity)
          return StringError{StringErrorKind::BUFFER_TOO_SMALL, static_cast<uint32_t> (pos)};
        result.written += encode_utf8 (decoded._M_code_point, out + result.written, capacity - result.written);
        ++result.code_points;
        pos += decoded._M_consumed;
      }
    return result;
  }

  export enum class CharErrorKind : uint8_t {
    UNTERMINATED, // no closing quote
    EMPTY, // '' — nothing between the quotes
    TOO_LONG, // more than one scalar ('ab')
    INVALID_ESCAPE, // bad escape sequence
    INVALID_UTF8, // malformed UTF-8 scalar
  };

  export struct CharError
  {
    CharErrorKind kind = CharErrorKind::EMPTY;
    uint32_t offset = 0; // byte offset relative to the literal start
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const CharErrorKind kind) noexcept
  {
    switch (kind)
      {
        case CharErrorKind::UNTERMINATED:
          return "unterminated character literal";
        case CharErrorKind::EMPTY:
          return "empty character literal";
        case CharErrorKind::TOO_LONG:
          return "character literal holds more than one scalar";
        case CharErrorKind::INVALID_ESCAPE:
          return "invalid escape sequence";
        case CharErrorKind::INVALID_UTF8:
          return "invalid UTF-8 sequence";
      }
    return "unknown character literal error";
  }

  // Constant-folds a complete '...' lexeme to its single Unicode scalar —
  // the compile-time counterpart of the lexer's char-literal validation.
  export [[nodiscard]] constexpr Maybe<char32_t, CharError>
  decode_char_literal (const SimpleStringView lexeme) noexcept
  {
    if (lexeme.size () < 2 || lexeme.front () != '\'')
      return CharError{CharErrorKind::UNTERMINATED, 0};
    if (lexeme.back () != '\'')
      return CharError{CharErrorKind::UNTERMINATED, 0};
    const SimpleStringView body = lexeme.substr (1, lexeme.size () - 2);
    if (body.empty ())
      return CharError{CharErrorKind::EMPTY, 1};

    char32_t scalar = 0;
    size_t next = 0;
    if (body[0] == '\\')
      {
        const EscapeResult esc = scan_escape (body, 0);
        if (!esc.ok)
          return CharError{CharErrorKind::INVALID_ESCAPE, 1};
        scalar = esc.code_point;
        next = esc.next;
      }
    else
      {
        const Utf8Decode decoded = decode_utf8 (body.data (), body.size ());
        if (decoded._M_error != UtfError::NONE)
          return CharError{CharErrorKind::INVALID_UTF8, 1};
        scalar = decoded._M_code_point;
        next = decoded._M_consumed;
      }
    if (next != body.size ())
      return CharError{CharErrorKind::TOO_LONG, 1};
    return scalar;
  }
} // namespace embdr::lex
