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

import embdr.cxxstd.stringView;
import embdr.lex.token;

using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

TEST_CASE("LexError messages cover every error kind", "[token]")
{
    REQUIRE(as_str(LexError::Kind::UNEXPECTED_CHAR) == "unexpected character");
    REQUIRE(as_str(LexError::Kind::UNTERMINATED_COMMENT) == "unterminated block comment");
    REQUIRE(as_str(LexError::Kind::UNTERMINATED_STRING) == "unterminated string literal");
    REQUIRE(as_str(LexError::Kind::UNTERMINATED_CHAR) == "unterminated character literal");
    REQUIRE(as_str(LexError::Kind::INVALID_ESCAPE) == "invalid escape sequence");
    REQUIRE(as_str(LexError::Kind::INVALID_NUMBER) == "invalid number literal");
    REQUIRE(as_str(LexError::Kind::INVALID_SUFFIX) == "invalid literal suffix");
    REQUIRE(as_str(LexError::Kind::INVALID_PREFIX) == "invalid literal prefix");
    REQUIRE(as_str(LexError::Kind::INVALID_CHAR) == "invalid character literal");
    REQUIRE(as_str(LexError::Kind::INVALID_UTF8) == "invalid UTF-8 sequence");
    REQUIRE(as_str(LexError::Kind::SOURCE_TOO_LARGE) == "source file is too large");
}

TEST_CASE("Token defaults to an end-of-file marker", "[token]")
{
    const Token token{};
    REQUIRE(token.kind == TokenKind::END_OF_FILE);
    REQUIRE(token.lexeme.empty());
    REQUIRE(token.offset == 0);
}

TEST_CASE("LexResult carries value and error distinctly", "[token]")
{
    const LexResult ok = Token{TokenKind::KW_LET, SimpleStringView("let"), 0};
    REQUIRE(ok.has_value());
    REQUIRE(!ok.has_error());
    REQUIRE(ok.value().kind == TokenKind::KW_LET);
    REQUIRE(ok.value().lexeme == "let");

    const LexResult bad = LexError{LexError::Kind::INVALID_NUMBER, 7};
    REQUIRE(!bad.has_value());
    REQUIRE(bad.has_error());
    REQUIRE(bad.error().kind == LexError::Kind::INVALID_NUMBER);
    REQUIRE(bad.error().offset == 7);
}

TEST_CASE("line_col_of maps offsets to 1-based line and column", "[token]")
{
    const SimpleStringView source = "ab\ncd\nef";

    const LineCol at_start = line_col_of(source, 0);
    REQUIRE(at_start.line == 1);
    REQUIRE(at_start.col == 1);

    const LineCol at_second_line = line_col_of(source, 3);
    REQUIRE(at_second_line.line == 2);
    REQUIRE(at_second_line.col == 1);

    const LineCol at_last_char = line_col_of(source, 7);
    REQUIRE(at_last_char.line == 3);
    REQUIRE(at_last_char.col == 2);

    const LineCol one_past_end = line_col_of(source, 8);
    REQUIRE(one_past_end.line == 3);
    REQUIRE(one_past_end.col == 3);

    const LineCol empty = line_col_of(SimpleStringView{}, 0);
    REQUIRE(empty.line == 1);
    REQUIRE(empty.col == 1);
}

TEST_CASE("line_col_of clamps malformed offsets instead of reading out of bounds", "[token]")
{
    const SimpleStringView source = "ab\ncd\nef";

    const LineCol clamped = line_col_of(source, 1000);
    REQUIRE(clamped.line == 3);
    REQUIRE(clamped.col == 3);
}

TEST_CASE("keyword_kind maps every reserved word, defer included", "[token]")
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
            REQUIRE(keyword_kind(entry.text) == entry.kind);
        }
    // contextual words are plain identifiers
    REQUIRE(keyword_kind("main") == TokenKind::IDENT);
    REQUIRE(keyword_kind("u32") == TokenKind::IDENT);
    REQUIRE(keyword_kind("letx") == TokenKind::IDENT);
    REQUIRE(keyword_kind("") == TokenKind::IDENT);
    // keywords do not match by prefix or suffix
    REQUIRE(keyword_kind("lett") == TokenKind::IDENT);
    REQUIRE(keyword_kind("Lets") == TokenKind::IDENT);
}

TEST_CASE("kind classification helpers follow the TokenKind grouping", "[token]")
{
    REQUIRE(is_keyword(TokenKind::KW_DEFER));
    REQUIRE(is_keyword(TokenKind::KW_AS));
    REQUIRE(is_keyword(TokenKind::KW_WHILE));
    REQUIRE(!is_keyword(TokenKind::IDENT));
    REQUIRE(!is_keyword(TokenKind::ELLIPSIS));

    REQUIRE(is_punctuator(TokenKind::ELLIPSIS));
    REQUIRE(is_punctuator(TokenKind::HASH));
    REQUIRE(is_punctuator(TokenKind::DOLLAR));
    REQUIRE(!is_punctuator(TokenKind::IDENT));
    REQUIRE(!is_punctuator(TokenKind::KW_FN));

    REQUIRE(is_literal(TokenKind::INT_LITERAL));
    REQUIRE(is_literal(TokenKind::FLOAT_LITERAL));
    REQUIRE(is_literal(TokenKind::CHAR_LITERAL));
    REQUIRE(is_literal(TokenKind::PATTERN_LITERAL));
    REQUIRE(!is_literal(TokenKind::IDENT));
    REQUIRE(!is_literal(TokenKind::LINE_COMMENT));

    REQUIRE(is_trivia(TokenKind::LINE_COMMENT));
    REQUIRE(is_trivia(TokenKind::DOC_COMMENT));
    REQUIRE(is_trivia(TokenKind::BLOCK_COMMENT));
    REQUIRE(!is_trivia(TokenKind::IDENT));

    REQUIRE(is_identifier(TokenKind::IDENT));
    REQUIRE(!is_identifier(TokenKind::KW_LET));
}

TEST_CASE("Token::length reports the lexeme byte length", "[token]")
{
    const Token eof{};
    REQUIRE(eof.length() == 0);

    const Token let{TokenKind::KW_LET, SimpleStringView("let"), 0};
    REQUIRE(let.length() == 3);

    const Token arrow{TokenKind::ARROW, SimpleStringView("->"), 4};
    REQUIRE(arrow.length() == 2);
}
