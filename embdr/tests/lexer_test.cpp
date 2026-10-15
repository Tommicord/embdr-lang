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
#include <limits>
#include <vector>

import embdr.cxxstd.stringView;
import embdr.lex.lexer;
import embdr.lex.token;

using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

namespace
{
    /* Collects every token including the final END_OF_FILE; fails the test on any lex error. */
    std::vector<Token> tokenize(const SimpleStringView source)
    {
        Lexer lexer{source};
        std::vector<Token> tokens;
        while (true)
            {
                LexResult result = lexer.next();
                REQUIRE(result.has_value());
                tokens.push_back(result.value());
                if (result.value().kind == TokenKind::END_OF_FILE)
                    return tokens;
            }
    }

    std::vector<TokenKind> kinds_of(const SimpleStringView source)
    {
        std::vector<TokenKind> kinds;
        for (const Token& token : tokenize(source))
            kinds.push_back(token.kind);
        return kinds;
    }

    constexpr bool lex_let_at_compile_time() noexcept
    {
        Lexer lexer{"let"};
        const LexResult result = lexer.next();
        return result.has_value() && result.value().kind == TokenKind::KW_LET && result.value().lexeme == "let" &&
               result.value().offset == 0;
    }

    constexpr bool lex_number_at_compile_time() noexcept
    {
        Lexer lexer{"0xFFint16"};
        const LexResult result = lexer.next();
        return result.has_value() && result.value().kind == TokenKind::INT_LITERAL &&
               result.value().lexeme == "0xFFint16" && result.value().offset == 0;
    }

    constexpr bool lex_prefixed_string_at_compile_time() noexcept
    {
        Lexer lexer{"u16\"hi\""};
        const LexResult result = lexer.next();
        return result.has_value() && result.value().kind == TokenKind::STRING_LITERAL_U16 &&
               result.value().lexeme == "u16\"hi\"" && result.value().offset == 0;
    }
} // namespace

static_assert(lex_let_at_compile_time(), "the lexer must be usable in constant evaluation");
static_assert(lex_number_at_compile_time(), "number lexing must be usable in constant evaluation");
static_assert(lex_prefixed_string_at_compile_time(), "string lexing must be usable in constant evaluation");

TEST_CASE("empty input yields a single end-of-file token", "[lexer]")
{
    const std::vector<Token> tokens = tokenize("");
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].kind == TokenKind::END_OF_FILE);
    REQUIRE(tokens[0].lexeme.empty());
    REQUIRE(tokens[0].offset == 0);
}

TEST_CASE("every reserved keyword is classified", "[lexer]")
{
    struct KeywordCase
    {
        const char* text;
        TokenKind kind;
    };
    constexpr KeywordCase cases[] = {
        {"as", TokenKind::KW_AS},
        {"break", TokenKind::KW_BREAK},
        {"cexpr", TokenKind::KW_CEXPR},
        {"continue", TokenKind::KW_CONTINUE},
        {"defer", TokenKind::KW_DEFER},
        {"else", TokenKind::KW_ELSE},
        {"embed", TokenKind::KW_EMBED},
        {"enum", TokenKind::KW_ENUM},
        {"extern", TokenKind::KW_EXTERN},
        {"false", TokenKind::KW_FALSE},
        {"fn", TokenKind::KW_FN},
        {"for", TokenKind::KW_FOR},
        {"function", TokenKind::KW_FUNCTION},
        {"if", TokenKind::KW_IF},
        {"in", TokenKind::KW_IN},
        {"interrupt", TokenKind::KW_INTERRUPT},
        {"is", TokenKind::KW_IS},
        {"let", TokenKind::KW_LET},
        {"loop", TokenKind::KW_LOOP},
        {"macro", TokenKind::KW_MACRO},
        {"match", TokenKind::KW_MATCH},
        {"module", TokenKind::KW_MODULE},
        {"packed", TokenKind::KW_PACKED},
        {"pub", TokenKind::KW_PUB},
        {"ref", TokenKind::KW_REF},
        {"ret", TokenKind::KW_RET},
        {"self", TokenKind::KW_SELF},
        {"static", TokenKind::KW_STATIC},
        {"struct", TokenKind::KW_STRUCT},
        {"true", TokenKind::KW_TRUE},
        {"use", TokenKind::KW_USE},
        {"volatile", TokenKind::KW_VOLATILE},
        {"while", TokenKind::KW_WHILE},
    };
    for (const KeywordCase& entry : cases)
        {
            const std::vector<Token> tokens = tokenize(entry.text);
            REQUIRE(tokens.size() == 2);
            REQUIRE(tokens[0].kind == entry.kind);
            REQUIRE(tokens[0].lexeme == entry.text);
            REQUIRE(tokens[0].offset == 0);
            REQUIRE(tokens[1].kind == TokenKind::END_OF_FILE);
        }
}

TEST_CASE("contextual words remain plain identifiers", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::IDENT, TokenKind::IDENT,
                                             TokenKind::IDENT, TokenKind::IDENT, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("default main u8 u16 i32") == expected);
}

TEST_CASE("keywords and identifiers interleave like source text", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::KW_LET, TokenKind::IDENT,      TokenKind::EQ,
                                             TokenKind::IDENT,  TokenKind::SEMICOLON,  TokenKind::KW_RET,
                                             TokenKind::IDENT,  TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("let x = _9;\nret x") == expected);
}

TEST_CASE("token offsets resolve to line and column", "[lexer]")
{
    const SimpleStringView source = "let\n  x";
    const std::vector<Token> tokens = tokenize(source);
    REQUIRE(tokens.size() == 3);

    REQUIRE(tokens[0].kind == TokenKind::KW_LET);
    REQUIRE(tokens[0].offset == 0);
    const LineCol let_pos = line_col_of(source, tokens[0].offset);
    REQUIRE(let_pos.line == 1);
    REQUIRE(let_pos.col == 1);

    REQUIRE(tokens[1].kind == TokenKind::IDENT);
    REQUIRE(tokens[1].lexeme == "x");
    REQUIRE(tokens[1].offset == 6);
    const LineCol x_pos = line_col_of(source, tokens[1].offset);
    REQUIRE(x_pos.line == 2);
    REQUIRE(x_pos.col == 3);
}

TEST_CASE("punctuation uses maximal munch", "[lexer]")
{
    struct PunctCase
    {
        const char* text;
        TokenKind kind;
    };
    constexpr PunctCase cases[] = {
        {"<<=", TokenKind::LSHIFT_ASSIGN},
        {">>=", TokenKind::RSHIFT_ASSIGN},
        {"..=", TokenKind::DOT_DOT_ASSIGN},
        {"...", TokenKind::ELLIPSIS},
        {"|>", TokenKind::PIPE_GT},
        {"::", TokenKind::COLON_COLON},
        {"->", TokenKind::ARROW},
        {"..", TokenKind::DOT_DOT},
        {"&&", TokenKind::AMP_AMP},
        {"||", TokenKind::PIPE_PIPE},
        {"==", TokenKind::EQ_EQ},
        {"!=", TokenKind::BANG_EQ},
        {"<=", TokenKind::LT_EQ},
        {">=", TokenKind::GT_EQ},
        {"<<", TokenKind::LT_LT},
        {">>", TokenKind::GT_GT},
        {"+=", TokenKind::PLUS_EQ},
        {"-=", TokenKind::MINUS_EQ},
        {"*=", TokenKind::STAR_EQ},
        {"/=", TokenKind::SLASH_EQ},
        {"%=", TokenKind::PERCENT_EQ},
        {"&=", TokenKind::AMP_EQ},
        {"|=", TokenKind::PIPE_EQ},
        {"^=", TokenKind::CARET_EQ},
        {"??", TokenKind::QUESTION_QUESTION},
        {"?", TokenKind::QUESTION},
        {"!", TokenKind::BANG},
        {"~", TokenKind::TILDE},
        {"+", TokenKind::PLUS},
        {"-", TokenKind::MINUS},
        {"*", TokenKind::STAR},
        {"/", TokenKind::SLASH},
        {"%", TokenKind::PERCENT},
        {"^", TokenKind::CARET},
        {"&", TokenKind::AMP},
        {"|", TokenKind::PIPE},
        {"<", TokenKind::LT},
        {">", TokenKind::GT},
        {"=", TokenKind::EQ},
        {"(", TokenKind::LPAREN},
        {")", TokenKind::RPAREN},
        {"{", TokenKind::LBRACE},
        {"}", TokenKind::RBRACE},
        {"[", TokenKind::LBRACKET},
        {"]", TokenKind::RBRACKET},
        {";", TokenKind::SEMICOLON},
        {",", TokenKind::COMMA},
        {".", TokenKind::DOT},
        {":", TokenKind::COLON},
        {"#", TokenKind::HASH},
        {"$", TokenKind::DOLLAR},
    };
    for (const PunctCase& entry : cases)
        {
            const std::vector<Token> tokens = tokenize(entry.text);
            REQUIRE(tokens.size() == 2);
            REQUIRE(tokens[0].kind == entry.kind);
            REQUIRE(tokens[0].lexeme == entry.text);
            REQUIRE(tokens[0].offset == 0);
            REQUIRE(tokens[1].kind == TokenKind::END_OF_FILE);
        }
}

TEST_CASE("adjacent operators do not steal characters", "[lexer]")
{
    {
        const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::LSHIFT_ASSIGN, TokenKind::IDENT,
                                                 TokenKind::END_OF_FILE};
        REQUIRE(kinds_of("a<<=b") == expected);
    }
    {
        const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::DOT_DOT_ASSIGN, TokenKind::IDENT,
                                                 TokenKind::END_OF_FILE};
        REQUIRE(kinds_of("a..=b") == expected);
    }
    {
        const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::ELLIPSIS, TokenKind::IDENT,
                                                 TokenKind::END_OF_FILE};
        REQUIRE(kinds_of("a...b") == expected);
    }
    {
        const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::COLON_COLON, TokenKind::LT,
                                                 TokenKind::IDENT, TokenKind::GT,          TokenKind::END_OF_FILE};
        REQUIRE(kinds_of("std::<println>") == expected);
    }
    {
        const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::DOT,   TokenKind::IDENT,
                                                 TokenKind::ARROW, TokenKind::IDENT, TokenKind::END_OF_FILE};
        REQUIRE(kinds_of("a.b->c") == expected);
    }
}

TEST_CASE("comments are skipped, including nested block comments", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::KW_LET, TokenKind::IDENT, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("// line comment\nlet /* a /* nested */ still */ x") == expected);
    REQUIRE(kinds_of("/// doc comment\nret") == (std::vector<TokenKind>{TokenKind::KW_RET, TokenKind::END_OF_FILE}));
    REQUIRE(kinds_of("/* only */") == (std::vector<TokenKind>{TokenKind::END_OF_FILE}));

    const std::vector<Token> tokens = tokenize("/* a\nb */ let");
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].kind == TokenKind::KW_LET);
    REQUIRE(tokens[0].offset == 10);
    const LineCol pos = line_col_of("/* a\nb */ let", tokens[0].offset);
    REQUIRE(pos.line == 2);
    REQUIRE(pos.col == 6);
}

TEST_CASE("unexpected characters are reported with their offset", "[lexer]")
{
    LexResult result = Lexer{"@"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNEXPECTED_CHAR);
    REQUIRE(result.error().offset == 0);

    Lexer lexer{"=@"};
    REQUIRE(lexer.next().has_value());
    const LexResult bad = lexer.next();
    REQUIRE(!bad.has_value());
    REQUIRE(bad.error().kind == LexError::Kind::UNEXPECTED_CHAR);
    REQUIRE(bad.error().offset == 1);

    // The offending byte was consumed, so the next call reaches end of file
    // instead of looping on the same position.
    const LexResult after = lexer.next();
    REQUIRE(after.has_value());
    REQUIRE(after.value().kind == TokenKind::END_OF_FILE);
    REQUIRE(after.value().offset == 2);
}

TEST_CASE("unterminated block comments report where the comment opened", "[lexer]")
{
    Lexer lexer{"let /* oops"};
    REQUIRE(lexer.next().has_value());
    const LexResult bad = lexer.next();
    REQUIRE(!bad.has_value());
    REQUIRE(bad.error().kind == LexError::Kind::UNTERMINATED_COMMENT);
    REQUIRE(bad.error().offset == 4);

    const LineCol pos = line_col_of("let /* oops", bad.error().offset);
    REQUIRE(pos.line == 1);
    REQUIRE(pos.col == 5);
}

TEST_CASE("sources beyond the 32-bit offset range are rejected", "[lexer]")
{
    if constexpr (sizeof(size_t) > sizeof(uint32_t))
        {
            // A view larger than 4 GiB over a tiny buffer: next() rejects on
            // size alone, before ever reading from the buffer.
            char dummy[4] = {};
            const SimpleStringView huge{dummy, static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1};
            const LexResult result = Lexer{huge}.next();
            REQUIRE(!result.has_value());
            REQUIRE(result.error().kind == LexError::Kind::SOURCE_TOO_LARGE);
            REQUIRE(result.error().offset == 0);
        }
}

TEST_CASE("integer literals in every base and separator form", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::INT_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("42") == expected);
    REQUIRE(kinds_of("0xFF") == expected);
    REQUIRE(kinds_of("0o777") == expected);
    REQUIRE(kinds_of("0b1010") == expected);
    REQUIRE(kinds_of("1_000_000") == expected);
    REQUIRE(kinds_of("100int128") == expected);
    REQUIRE(kinds_of("0x1Fint8") == expected);

    const std::vector<Token> tokens = tokenize("1_000_000 0xFFint16 100int128");
    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].lexeme == "1_000_000");
    REQUIRE(tokens[0].offset == 0);
    REQUIRE(tokens[1].lexeme == "0xFFint16");
    REQUIRE(tokens[1].offset == 10);
    REQUIRE(tokens[2].lexeme == "100int128");
    REQUIRE(tokens[2].offset == 20);
}

TEST_CASE("float literals accept exponents and float suffixes", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::FLOAT_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("3.14") == expected);
    REQUIRE(kinds_of("6.02e23") == expected);
    REQUIRE(kinds_of("1e-5") == expected);
    REQUIRE(kinds_of("3.14float64") == expected);
    REQUIRE(kinds_of("100float64") == expected);
    REQUIRE(kinds_of("1.2float16") == expected);

    const std::vector<Token> tokens = tokenize("3.14float64");
    REQUIRE(tokens[0].lexeme == "3.14float64");
}

TEST_CASE("dots after integers are ranges and member access, not floats", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::INT_LITERAL, TokenKind::DOT_DOT, TokenKind::INT_LITERAL,
                                             TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("1..2") == expected);

    const std::vector<TokenKind> hex_range = {TokenKind::INT_LITERAL, TokenKind::DOT_DOT, TokenKind::INT_LITERAL,
                                              TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("0x1..0x2") == hex_range);
}

TEST_CASE("malformed number literals report precise offsets", "[lexer]")
{
    // digits that the chosen base does not have, or an empty base body
    LexResult result = Lexer{"0b12"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 3);

    result = Lexer{"0x"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 2);

    result = Lexer{"0o8"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 2);

    // misplaced separators
    result = Lexer{"1_"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 1);

    result = Lexer{"1__0"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 1);

    result = Lexer{"0x_1"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 2);

    // dangling exponent
    result = Lexer{"1e"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 1);

    // fraction dot on a hex literal (".." stays a range, see above)
    result = Lexer{"0x1.5"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(result.error().offset == 3);
}

TEST_CASE("suffix mistakes are rejected", "[lexer]")
{
    // u8 is a string prefix, never a type
    LexResult result = Lexer{"1u8"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_SUFFIX);
    REQUIRE(result.error().offset == 1);

    result = Lexer{"123i32"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_SUFFIX);
    REQUIRE(result.error().offset == 3);

    // float suffix on a non-decimal literal
    result = Lexer{"0b10float64"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_SUFFIX);
    REQUIRE(result.error().offset == 4);

    // integer suffix on a fractional literal
    result = Lexer{"3.14int32"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_SUFFIX);
    REQUIRE(result.error().offset == 4);

    const std::vector<Token> ok = tokenize("100uint32");
    REQUIRE(ok.size() == 2);
    REQUIRE(ok[0].kind == TokenKind::INT_LITERAL);
    REQUIRE(ok[0].lexeme == "100uint32");
}

TEST_CASE("string literal prefixes select the token kind", "[lexer]")
{
    const std::vector<Token> tokens = tokenize("\"hi\" u8\"hi\" u16\"hi\" u32\"hi\" r\"raw\\n\" p\"pat\"");
    REQUIRE(tokens.size() == 7);
    REQUIRE(tokens[0].kind == TokenKind::STRING_LITERAL);
    REQUIRE(tokens[0].lexeme == "\"hi\"");
    REQUIRE(tokens[0].offset == 0);
    REQUIRE(tokens[1].kind == TokenKind::STRING_LITERAL);
    REQUIRE(tokens[1].lexeme == "u8\"hi\"");
    REQUIRE(tokens[1].offset == 5);
    REQUIRE(tokens[2].kind == TokenKind::STRING_LITERAL_U16);
    REQUIRE(tokens[2].lexeme == "u16\"hi\"");
    REQUIRE(tokens[3].kind == TokenKind::STRING_LITERAL_U32);
    REQUIRE(tokens[4].kind == TokenKind::RAW_STRING_LITERAL);
    REQUIRE(tokens[4].lexeme == "r\"raw\\n\"");
    REQUIRE(tokens[5].kind == TokenKind::PATTERN_LITERAL);
    REQUIRE(tokens[5].lexeme == "p\"pat\"");

    const std::vector<TokenKind> empty = {TokenKind::STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("\"\"") == empty);
}

TEST_CASE("a space defeats the string prefix", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("u8 \"hi\"") == expected);

    const std::vector<TokenKind> ident = {TokenKind::IDENT, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("u8") == ident);
    REQUIRE(kinds_of("u16") == ident);
}

TEST_CASE("string escapes are validated", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("\"a\\nb\"") == expected);
    REQUIRE(kinds_of("\"\\\\\"") == expected);
    REQUIRE(kinds_of("\"\\\"\"") == expected);
    REQUIRE(kinds_of("\"\\x41\\u{1F600}\"") == expected);

    LexResult result = Lexer{"\"a\\qb\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"\"\\xZZ\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);

    // \u needs braces around a valid Unicode scalar
    result = Lexer{"\"\\u0041\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);

    result = Lexer{"\"\\u{D800}\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);

    result = Lexer{"\"\\u{}"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);
}

TEST_CASE("unterminated strings report their opening quote", "[lexer]")
{
    LexResult result = Lexer{"\"abc"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNTERMINATED_STRING);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"\"abc\ndef\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNTERMINATED_STRING);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"r\"abc"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNTERMINATED_STRING);
    REQUIRE(result.error().offset == 0);
}

TEST_CASE("raw and pattern strings keep their content verbatim", "[lexer]")
{
    // raw strings may span lines and treat the backslash as a plain byte
    const std::vector<TokenKind> raw = {TokenKind::RAW_STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("r\"a\nb\\n\"") == raw);

    // a regex pattern is raw too, so "\." never reaches the escape scanner
    const std::vector<TokenKind> pattern = {TokenKind::PATTERN_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("p\"^[0-9]{1,3}\\.[0-9]{1,3}$\"") == pattern);
}

TEST_CASE("malformed prefix spellings are rejected", "[lexer]")
{
    LexResult result = Lexer{"u64\"x\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_PREFIX);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"u\"x\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_PREFIX);
    REQUIRE(result.error().offset == 0);

    // unrelated identifiers are not prefix attempts: identifier + string
    const std::vector<TokenKind> expected = {TokenKind::IDENT, TokenKind::STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("foo\"x\"") == expected);
}

TEST_CASE("character literals accept escapes and Unicode scalars", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::CHAR_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("'a'") == expected);
    REQUIRE(kinds_of("'\\n'") == expected);
    REQUIRE(kinds_of("'\\x41'") == expected);
    REQUIRE(kinds_of("'\\u{1F600}'") == expected);
    REQUIRE(kinds_of("'\\''") == expected);

    const std::vector<Token> tokens = tokenize("'\\u{1F600}'");
    REQUIRE(tokens[0].lexeme == "'\\u{1F600}'");
    REQUIRE(tokens[0].offset == 0);
}

TEST_CASE("malformed character literals report precise errors", "[lexer]")
{
    // two scalars in one literal
    LexResult result = Lexer{"'ab'"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_CHAR);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"''"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_CHAR);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"'\\q'"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_ESCAPE);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"'a"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNTERMINATED_CHAR);
    REQUIRE(result.error().offset == 0);

    result = Lexer{"'a\n"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::UNTERMINATED_CHAR);
    REQUIRE(result.error().offset == 0);
}

TEST_CASE("UTF-8 content is decoded, broken sequences are rejected", "[lexer]")
{
    const std::vector<TokenKind> expected = {TokenKind::STRING_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("\"h\u00E9llo\"") == expected);

    const std::vector<TokenKind> u16k = {TokenKind::STRING_LITERAL_U16, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("u16\"h\u00E9\"") == u16k);

    const std::vector<TokenKind> ch = {TokenKind::CHAR_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("'\u00E9'") == ch);

    // 0xC3 demands a continuation byte; '(' is not one
    LexResult result = Lexer{"\"\xC3(\"\""}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_UTF8);
    REQUIRE(result.error().offset == 1);

    result = Lexer{"'\xC3('"}.next();
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == LexError::Kind::INVALID_UTF8);
    REQUIRE(result.error().offset == 1);
}

TEST_CASE("save and restore rewind the cursor exactly", "[lexer]")
{
    Lexer lexer{"let x = 42"};
    REQUIRE(lexer.next().has_value()); // let

    const Lexer::State state = lexer.save_pos();
    const LexResult x = lexer.next();
    REQUIRE(x.has_value());
    REQUIRE(x.value().lexeme == "x");

    lexer.restore_pos(state);
    const LexResult x_again = lexer.next();
    REQUIRE(x_again.has_value());
    REQUIRE(x_again.value().lexeme == "x");
    REQUIRE(x_again.value().offset == x.value().offset);

    // a rewind across a line break restores line bookkeeping too
    Lexer multiline{"a\nb"};
    REQUIRE(multiline.next().has_value());
    const Lexer::State before_newline = multiline.save_pos();
    REQUIRE(multiline.next().has_value()); // b
    REQUIRE(multiline.line_col().line == 2);
    multiline.restore_pos(before_newline);
    const LineCol rewound = multiline.line_col();
    REQUIRE(rewound.line == 1);
    REQUIRE(rewound.col == 2);
}

TEST_CASE("peek lexes without consuming", "[lexer]")
{
    Lexer lexer{"let x"};
    const LexResult peeked = lexer.peek();
    REQUIRE(peeked.has_value());
    REQUIRE(peeked.value().kind == TokenKind::KW_LET);
    REQUIRE(peeked.value().lexeme == "let");

    // the cursor did not move: next() yields the same token
    const LexResult actual = lexer.next();
    REQUIRE(actual.has_value());
    REQUIRE(actual.value().kind == TokenKind::KW_LET);
    REQUIRE(actual.value().offset == peeked.value().offset);

    // peek at end of file is stable
    Lexer eof{""};
    REQUIRE(eof.peek().value().kind == TokenKind::END_OF_FILE);
    REQUIRE(eof.peek().value().kind == TokenKind::END_OF_FILE);

    // a peeked error leaves no trace either
    Lexer bad{"@"};
    REQUIRE(!bad.peek().has_value());
    REQUIRE(bad.offset() == 0);
    REQUIRE(!bad.next().has_value());
}

TEST_CASE("keep_comments surfaces comments as tokens", "[lexer]")
{
    Lexer lexer{"/// doc\nlet // note\n/* block */ x"};
    lexer.set_keep_comments(true);
    REQUIRE(lexer.keep_comments());

    const std::vector<TokenKind> expected = {TokenKind::DOC_COMMENT,   TokenKind::KW_LET, TokenKind::LINE_COMMENT,
                                             TokenKind::BLOCK_COMMENT, TokenKind::IDENT,  TokenKind::END_OF_FILE};
    std::vector<TokenKind> kinds;
    while (true)
        {
            const LexResult result = lexer.next();
            REQUIRE(result.has_value());
            kinds.push_back(result.value().kind);
            if (result.value().kind == TokenKind::END_OF_FILE)
                break;
        }
    REQUIRE(kinds == expected);

    // comments are off by default
    Lexer plain{"let // note"};
    REQUIRE(!plain.keep_comments());
    const std::vector<TokenKind> skipped = {TokenKind::KW_LET, TokenKind::END_OF_FILE};
    std::vector<TokenKind> plain_kinds;
    while (true)
        {
            const LexResult result = plain.next();
            REQUIRE(result.has_value());
            plain_kinds.push_back(result.value().kind);
            if (result.value().kind == TokenKind::END_OF_FILE)
                break;
        }
    REQUIRE(plain_kinds == skipped);
}

TEST_CASE("a kept comment carries the flags of its position", "[lexer]")
{
    Lexer lexer{"// lead\nlet /* mid */ x"};
    lexer.set_keep_comments(true);

    const LexResult comment = lexer.next();
    REQUIRE(comment.has_value());
    REQUIRE(comment.value().kind == TokenKind::LINE_COMMENT);
    REQUIRE(any(comment.value().flags & TokenFlags::START_OF_LINE));

    const LexResult let = lexer.next();
    REQUIRE(let.has_value());
    REQUIRE(let.value().kind == TokenKind::KW_LET);
    REQUIRE(any(let.value().flags & TokenFlags::START_OF_LINE));

    const LexResult block = lexer.next();
    REQUIRE(block.has_value());
    REQUIRE(block.value().kind == TokenKind::BLOCK_COMMENT);
    // the block comment sits mid-line after a space, not at a line start
    REQUIRE(!any(block.value().flags & TokenFlags::START_OF_LINE));
    REQUIRE(any(block.value().flags & TokenFlags::LEADING_SPACE));

    const LexResult x = lexer.next();
    REQUIRE(x.has_value());
    REQUIRE(x.value().kind == TokenKind::IDENT);
    REQUIRE(!any(x.value().flags & TokenFlags::START_OF_LINE));
    REQUIRE(any(x.value().flags & TokenFlags::LEADING_SPACE));
}

TEST_CASE("unterminated block comments fail in kept mode too", "[lexer]")
{
    Lexer lexer{"let /* oops"};
    lexer.set_keep_comments(true);
    REQUIRE(lexer.next().has_value());
    const LexResult bad = lexer.next();
    REQUIRE(!bad.has_value());
    REQUIRE(bad.error().kind == LexError::Kind::UNTERMINATED_COMMENT);
    REQUIRE(bad.error().offset == 4);
    // and the next call reaches EOF instead of repeating the error
    REQUIRE(lexer.next().value().kind == TokenKind::END_OF_FILE);
}

TEST_CASE("start-of-line tracking follows the physical lines", "[lexer]")
{
    Lexer lexer{"let\n  x"};
    REQUIRE(lexer.is_at_start_of_line()); // before any token
    REQUIRE(lexer.next().has_value()); // let at line 1
    REQUIRE(!lexer.is_at_start_of_line());
    REQUIRE(lexer.next().has_value()); // x at line 2
    REQUIRE(!lexer.is_at_start_of_line());
    REQUIRE(lexer.peek().value().kind == TokenKind::END_OF_FILE);
}

TEST_CASE("token flags carry start-of-line and leading-space information", "[lexer]")
{
    const std::vector<Token> tokens = tokenize("let x");
    REQUIRE(any(tokens[0].flags & TokenFlags::START_OF_LINE));
    REQUIRE(!any(tokens[0].flags & TokenFlags::LEADING_SPACE));
    REQUIRE(!any(tokens[1].flags & TokenFlags::START_OF_LINE));
    REQUIRE(any(tokens[1].flags & TokenFlags::LEADING_SPACE));

    const std::vector<Token> second_line = tokenize("a\nb");
    REQUIRE(any(second_line[1].flags & TokenFlags::START_OF_LINE));
    REQUIRE(!any(second_line[1].flags & TokenFlags::LEADING_SPACE));

    // a comment before the token counts as leading space
    Lexer commented{"/*c*/ x"};
    commented.set_keep_comments(true);
    REQUIRE(commented.next().has_value()); // the comment token itself
    const LexResult x = commented.next();
    REQUIRE(x.has_value());
    REQUIRE(any(x.value().flags & TokenFlags::LEADING_SPACE));
}

TEST_CASE("CRLF and lone CR both count as one line break", "[lexer]")
{
    {
        const SimpleStringView source = "let\r\n  x";
        Lexer lexer{source};
        REQUIRE(lexer.next().has_value());
        const LexResult x = lexer.next();
        REQUIRE(x.has_value());
        REQUIRE(x.value().lexeme == "x");
        const LineCol pos = line_col_of(source, x.value().offset);
        REQUIRE(pos.line == 2);
        REQUIRE(pos.col == 3);
    }
    {
        const SimpleStringView source = "let\rx";
        Lexer lexer{source};
        REQUIRE(lexer.next().has_value());
        const LexResult x = lexer.next();
        REQUIRE(x.has_value());
        REQUIRE(x.value().lexeme == "x");
        const LineCol pos = line_col_of(source, x.value().offset);
        REQUIRE(pos.line == 2);
        REQUIRE(pos.col == 1);
    }
    // the lexer's own incremental tracking agrees with line_col_of; the
    // cursor sits at the end of "b", so the '\r' after it is not consumed yet
    {
        Lexer lexer{"a\r\nb\rc"};
        REQUIRE(lexer.next().has_value());
        REQUIRE(lexer.next().value().lexeme == "b");
        REQUIRE(lexer.line_col().line == 2);
        REQUIRE(lexer.next().value().lexeme == "c");
        REQUIRE(lexer.line_col().line == 3);
    }
}

TEST_CASE("a UTF-8 BOM is skipped and occupies no column", "[lexer]")
{
    const SimpleStringView source = "\xEF\xBB\xBFlet";
    Lexer lexer{source};
    const LexResult result = lexer.next();
    REQUIRE(result.has_value());
    REQUIRE(result.value().kind == TokenKind::KW_LET);
    REQUIRE(result.value().offset == 3);
    REQUIRE(result.value().lexeme == "let");
    const LineCol pos = lexer.line_col();
    REQUIRE(pos.line == 1);
    REQUIRE(pos.col == 4); // right after "let", the BOM added no column
}

TEST_CASE("a malformed number is consumed whole so lexing resynchronizes", "[lexer]")
{
    Lexer lexer{"0b12 let"};
    const LexResult bad = lexer.next();
    REQUIRE(!bad.has_value());
    REQUIRE(bad.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(bad.error().offset == 3);
    // the trailing '2' is NOT re-lexed as a separate token: the whole glued
    // extent was consumed, so the next token is already the keyword
    const LexResult next = lexer.next();
    REQUIRE(next.has_value());
    REQUIRE(next.value().kind == TokenKind::KW_LET);
    REQUIRE(next.value().offset == 5);
}

TEST_CASE("exponent signs glue to the literal only when they belong", "[lexer]")
{
    const std::vector<TokenKind> exponent = {TokenKind::FLOAT_LITERAL, TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("1e-5") == exponent);
    REQUIRE(kinds_of("1e+5") == exponent);

    // a binary minus never joins the literal
    const std::vector<TokenKind> minus = {TokenKind::INT_LITERAL, TokenKind::MINUS, TokenKind::INT_LITERAL,
                                          TokenKind::END_OF_FILE};
    REQUIRE(kinds_of("1-5") == minus);
}

TEST_CASE("raw_token_length measures the token at an offset", "[lexer]")
{
    const SimpleStringView source = "let /*c*/ \"s\" @ x";
    REQUIRE(Lexer::raw_token_length(source, 0) == 3); // let
    REQUIRE(Lexer::raw_token_length(source, 4) == 5); // /*c*/
    REQUIRE(Lexer::raw_token_length(source, 10) == 3); // "s"
    REQUIRE(Lexer::raw_token_length(source, 14) == 1); // the rejected byte
    REQUIRE(Lexer::raw_token_length(source, 16) == 1); // x
    REQUIRE(Lexer::raw_token_length(source, 17) == 0); // at the end
    REQUIRE(Lexer::raw_token_length(source, 99) == 0); // past the end
}
