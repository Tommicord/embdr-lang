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

import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.delimiter;
import embdr.lex.lexer;
import embdr.lex.token;

using embdr::cxxstd::Maybe;
using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

namespace
{
    /* Feeds every token of `source` through a fresh checker; returns the first error (or success). */
    Maybe<bool, DelimiterError> check(const SimpleStringView source)
    {
        Lexer lexer{source};
        DelimiterChecker<16> checker;
        while (true)
            {
                const LexResult result = lexer.next();
                REQUIRE(result.has_value());
                const Maybe<bool, DelimiterError> fed = checker.feed(result.value());
                if (fed.has_error())
                    return fed.error();
                if (result.value().kind == TokenKind::END_OF_FILE)
                    break;
            }
        return checker.finish();
    }
} // namespace

TEST_CASE("Delimiter error messages cover every error kind", "[delimiter]")
{
    REQUIRE(as_str(DelimiterError::Kind::MISMATCHED_CLOSE) == "mismatched closing delimiter");
    REQUIRE(as_str(DelimiterError::Kind::EXTRA_CLOSE) == "closing delimiter without an opener");
    REQUIRE(as_str(DelimiterError::Kind::UNTERMINATED_OPEN) == "unclosed opening delimiter");
    REQUIRE(as_str(DelimiterError::Kind::TOO_DEEPLY_NESTED) == "delimiters nested too deeply");
}

TEST_CASE("balanced delimiters pass", "[delimiter]")
{
    REQUIRE(check("").has_value());
    REQUIRE(check("let x = (1 + 2) * [a];").has_value());
    REQUIRE(check("fn f() { if x { y(); } }").has_value());
    REQUIRE(check("((()))").has_value());
    REQUIRE(check("{[{}]}").has_value());
    // comments and strings are just tokens to the checker
    REQUIRE(check("f(/* ) */ \"str}\")").has_value());
}

TEST_CASE("a mismatched close names the opener it should have matched", "[delimiter]")
{
    const Maybe<bool, DelimiterError> result = check("let a = (1];");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == DelimiterError::Kind::MISMATCHED_CLOSE);
    REQUIRE(result.error().offset == 10);
    REQUIRE(result.error().open_offset == 8);
    REQUIRE(result.error().expected == TokenKind::LPAREN);
}

TEST_CASE("a close without an opener is reported", "[delimiter]")
{
    const Maybe<bool, DelimiterError> result = check("let a = 1);");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == DelimiterError::Kind::EXTRA_CLOSE);
    REQUIRE(result.error().offset == 9);
}

TEST_CASE("unclosed openers are reported at the end of input", "[delimiter]")
{
    const Maybe<bool, DelimiterError> result = check("fn f( {");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == DelimiterError::Kind::UNTERMINATED_OPEN);
    // the outermost unclosed delimiter is the '(' of the parameter list
    REQUIRE(result.error().expected == TokenKind::LPAREN);
    REQUIRE(result.error().open_offset == 4);
}

TEST_CASE("nesting deeper than the capacity is an error value", "[delimiter]")
{
    DelimiterChecker<2> checker;
    const Token open{TokenKind::LPAREN, SimpleStringView("("), 0, TokenFlags::NONE};
    REQUIRE(checker.feed(open).has_value());
    REQUIRE(checker.feed(open).has_value());
    REQUIRE(checker.depth() == 2);
    REQUIRE(checker.capacity() == 2);

    const Maybe<bool, DelimiterError> overflow = checker.feed(open);
    REQUIRE(!overflow.has_value());
    REQUIRE(overflow.error().kind == DelimiterError::Kind::TOO_DEEPLY_NESTED);
    REQUIRE(overflow.error().offset == 0);
}

TEST_CASE("non-delimiter tokens pass through untouched", "[delimiter]")
{
    DelimiterChecker<4> checker;
    const Token ident{TokenKind::IDENT, SimpleStringView("x"), 0, TokenFlags::NONE};
    const Maybe<bool, DelimiterError> fed = checker.feed(ident);
    REQUIRE(fed.has_value());
    REQUIRE(fed.value());
    REQUIRE(checker.depth() == 0);
    // finish() succeeds on an empty stack
    REQUIRE(checker.finish().has_value());
}

TEST_CASE("the first structural problem is remembered deterministically", "[delimiter]")
{
    // the extra ')' is found first; the checker does not cascade into the
    // still-open '('
    const Maybe<bool, DelimiterError> result = check(") (");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().kind == DelimiterError::Kind::EXTRA_CLOSE);
}

static_assert(DelimiterChecker<4>{}.depth() == 0, "delimiter checking must work in constant evaluation");
