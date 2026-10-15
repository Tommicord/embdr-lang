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
import embdr.lex.pattern;

using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

namespace
{
    // Default capacities used by most tests.
    using Pat = CompiledPattern<64, 8, 64>;

    [[nodiscard]] Pat compile(const SimpleStringView text)
    {
        auto result = compile_pattern<64, 8, 64>(text);
        REQUIRE(result.has_value());
        return result.value();
    }

    [[nodiscard]] bool matches(const SimpleStringView pattern, const SimpleStringView input)
    {
        return compile(pattern).matches(input);
    }

    [[nodiscard]] PatternErrorKind compile_error(const SimpleStringView text)
    {
        auto result = compile_pattern<64, 8, 64>(text);
        REQUIRE(result.has_error());
        return result.error().kind;
    }
} // namespace

TEST_CASE("as_str covers every PatternErrorKind", "[pattern]")
{
    REQUIRE(as_str(PatternErrorKind::UNEXPECTED_END) == "unexpected end of pattern");
    REQUIRE(as_str(PatternErrorKind::INVALID_ESCAPE) == "invalid escape sequence");
    REQUIRE(as_str(PatternErrorKind::UNTERMINATED_CLASS) == "unterminated character class");
    REQUIRE(as_str(PatternErrorKind::UNTERMINATED_GROUP) == "unterminated group");
    REQUIRE(as_str(PatternErrorKind::INVALID_REPEAT) == "invalid repeat bounds");
    REQUIRE(as_str(PatternErrorKind::INVALID_RANGE) == "invalid class range");
    REQUIRE(as_str(PatternErrorKind::INVALID_CODE_POINT) == "invalid code point");
    REQUIRE(as_str(PatternErrorKind::REPEAT_WITHOUT_ATOM) == "repeat operator with no preceding atom");
    REQUIRE(as_str(PatternErrorKind::TOO_MANY_STATES) == "pattern has too many states");
    REQUIRE(as_str(PatternErrorKind::TOO_MANY_CLASSES) == "pattern has too many character classes");
    REQUIRE(as_str(PatternErrorKind::TOO_MANY_RANGES) == "pattern has too many class ranges");
    REQUIRE(as_str(PatternErrorKind::TOO_MANY_PATCHES) == "pattern has too many open branches");
}

TEST_CASE("a default-constructed pattern matches the empty input", "[pattern]")
{
    const Pat empty;
    REQUIRE(empty.matches(""));
    REQUIRE(!empty.matches("x"));
}

TEST_CASE("literal patterns match exactly", "[pattern]")
{
    REQUIRE(matches("abc", "abc"));
    REQUIRE(!matches("abc", "ab"));
    REQUIRE(!matches("abc", "abcd"));
    REQUIRE(!matches("abc", "ABC"));
    REQUIRE(matches("", ""));
}

TEST_CASE("dot matches any character except newline", "[pattern]")
{
    REQUIRE(matches("a.c", "abc"));
    REQUIRE(matches("a.c", "a.c"));
    REQUIRE(matches("...", "xyz"));
    REQUIRE(!matches("a.c", "a\nc"));
    REQUIRE(!matches("a.c", "ac"));
}

TEST_CASE("anchors bind to start and end of input", "[pattern]")
{
    REQUIRE(matches("^abc", "abc"));
    REQUIRE(!matches("^abc", "xabc"));
    REQUIRE(matches("abc$", "abc"));
    REQUIRE(!matches("abc$", "abcx"));
    REQUIRE(matches("^abc$", "abc"));
    REQUIRE(!matches("^abc$", "xabcx"));
}

TEST_CASE("alternation matches any branch", "[pattern]")
{
    REQUIRE(matches("a|b", "a"));
    REQUIRE(matches("a|b", "b"));
    REQUIRE(!matches("a|b", "c"));
    REQUIRE(matches("cat|dog|emu", "dog"));
    REQUIRE(!matches("cat|dog|emu", "cow"));
}

TEST_CASE("character classes match their members", "[pattern]")
{
    REQUIRE(matches("[abc]", "a"));
    REQUIRE(!matches("[abc]", "d"));
    REQUIRE(matches("[a-z]+", "hello"));
    REQUIRE(!matches("[a-z]+", "heLLo"));
    REQUIRE(matches("[0-9]{3}", "123"));
    REQUIRE(!matches("[0-9]{3}", "12"));
}

TEST_CASE("negated classes match the complement", "[pattern]")
{
    REQUIRE(matches("[^abc]", "d"));
    REQUIRE(!matches("[^abc]", "a"));
    REQUIRE(matches("^[^0-9]+$", "hello"));
    REQUIRE(!matches("^[^0-9]+$", "h3llo"));
}

TEST_CASE("perl character classes work", "[pattern]")
{
    REQUIRE(matches("^\\d+$", "12345"));
    REQUIRE(!matches("^\\d+$", "12a45"));
    REQUIRE(matches("^\\w+$", "abc_09"));
    REQUIRE(!matches("^\\w+$", "abc-09"));
    REQUIRE(matches("^\\s+$", " \t\n"));
    REQUIRE(!matches("^\\s+$", " a "));
    // Negated forms.
    REQUIRE(matches("^\\D+$", "abc"));
    REQUIRE(!matches("^\\D+$", "ab3c"));
}

TEST_CASE("quantifiers star plus and question", "[pattern]")
{
    REQUIRE(matches("ab*c", "ac"));
    REQUIRE(matches("ab*c", "abc"));
    REQUIRE(matches("ab*c", "abbbbbc"));
    REQUIRE(!matches("ab*c", "abd"));

    REQUIRE(matches("ab+c", "abc"));
    REQUIRE(!matches("ab+c", "ac"));
    REQUIRE(matches("ab+c", "abbbbc"));

    REQUIRE(matches("ab?c", "ac"));
    REQUIRE(matches("ab?c", "abc"));
    REQUIRE(!matches("ab?c", "abbc"));
}

TEST_CASE("counted quantifiers", "[pattern]")
{
    REQUIRE(matches("^a{3}$", "aaa"));
    REQUIRE(!matches("^a{3}$", "aa"));
    REQUIRE(!matches("^a{3}$", "aaaa"));

    REQUIRE(matches("^a{2,}$", "aa"));
    REQUIRE(matches("^a{2,}$", "aaaaa"));
    REQUIRE(!matches("^a{2,}$", "a"));

    REQUIRE(matches("^a{2,4}$", "aa"));
    REQUIRE(matches("^a{2,4}$", "aaaa"));
    REQUIRE(!matches("^a{2,4}$", "a"));
    REQUIRE(!matches("^a{2,4}$", "aaaaa"));
}

TEST_CASE("non-greedy quantifiers still match the full input", "[pattern]")
{
    // With full-match semantics, greedy and non-greedy agree on accept/reject.
    REQUIRE(matches("^a*?$", ""));
    REQUIRE(matches("^a*?$", "aaa"));
    REQUIRE(matches("^a+?$", "a"));
    REQUIRE(matches("^a{2,4}?$", "aaa"));
}

TEST_CASE("groups and non-capturing groups", "[pattern]")
{
    REQUIRE(matches("^(ab)+$", "ababab"));
    REQUIRE(!matches("^(ab)+$", "aba"));
    REQUIRE(matches("^(?:xy)*$", "xyxy"));
    REQUIRE(!matches("^(?:xy)*$", "xz"));
    REQUIRE(matches("^(a|b)+$", "abba"));
}

TEST_CASE("the IPv4 example from syntax.txt matches", "[pattern]")
{
    const Pat ip = compile("^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$");
    REQUIRE(ip.matches("192.168.0.1"));
    REQUIRE(ip.matches("0.0.0.0"));
    REQUIRE(ip.matches("255.255.255.255"));
    REQUIRE(ip.matches("1.2.3.4"));
    REQUIRE(!ip.matches("1.2.3"));
    REQUIRE(!ip.matches("1.2.3.4.5"));
    REQUIRE(!ip.matches("192.168.0.256"));
    REQUIRE(!ip.matches("abc"));
    REQUIRE(!ip.matches(""));
}

TEST_CASE("the gray example from syntax.txt matches", "[pattern]")
{
    REQUIRE(matches("gr[ae]y", "gray"));
    REQUIRE(matches("gr[ae]y", "grey"));
    REQUIRE(!matches("gr[ae]y", "gruy"));
}

TEST_CASE("escaped literals match literally", "[pattern]")
{
    REQUIRE(matches("^a\\.b$", "a.b"));
    REQUIRE(!matches("^a\\.b$", "axb"));
    REQUIRE(matches("^\\(hi\\)$", "(hi)"));
    REQUIRE(matches("^a\\*b$", "a*b"));
    REQUIRE(matches("^a\\|b$", "a|b"));
    REQUIRE(matches("^100\\%$", "100%"));
}

TEST_CASE("escape sequences decode to the right scalars", "[pattern]")
{
    REQUIRE(matches("^\\n$", "\n"));
    REQUIRE(matches("^\\t$", "\t"));
    REQUIRE(matches("^\\r$", "\r"));
    REQUIRE(matches("^\\x41$", "A"));
    REQUIRE(matches("^\\u{1F600}$", "\xF0\x9F\x98\x80")); // U+1F600 emoji
}

TEST_CASE("patterns match over multi-code-point UTF-8 input", "[pattern]")
{
    // "café" is 5 bytes, 4 code points; 'é' is 2 bytes.
    REQUIRE(matches("^café$", "caf\xC3\xA9"));
    REQUIRE(matches("^c.té$", "caf\xC3\xA9"));
    REQUIRE(matches("^[à-ÿ]+$", "\xC3\xA9")); // é in a class range
    REQUIRE(!matches("^café$", "cafe"));
}

TEST_CASE("a lone invalid UTF-8 byte matches as its own value", "[pattern]")
{
    // 0xFF is not valid UTF-8; the engine falls back to the byte value so the
    // match stays defined rather than erroring.
    const char invalid[] = {'a', '\xFF', 'b'};
    REQUIRE(matches("^a.b$", SimpleStringView(invalid, 3)));
}

TEST_CASE("unterminated character class is an error", "[pattern]")
{
    REQUIRE(compile_error("[abc") == PatternErrorKind::UNTERMINATED_CLASS);
    REQUIRE(compile_error("[a-z") == PatternErrorKind::UNTERMINATED_CLASS);
    REQUIRE(compile_error("[^") == PatternErrorKind::UNTERMINATED_CLASS);
}

TEST_CASE("unterminated group is an error", "[pattern]")
{
    REQUIRE(compile_error("(abc") == PatternErrorKind::UNTERMINATED_GROUP);
    REQUIRE(compile_error("(?:abc") == PatternErrorKind::UNTERMINATED_GROUP);
    REQUIRE(compile_error("a(b|c") == PatternErrorKind::UNTERMINATED_GROUP);
}

TEST_CASE("invalid escapes are rejected", "[pattern]")
{
    REQUIRE(compile_error("a\\qb") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\x") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\xZZ") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\u") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\u{}") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\u{XYZ}") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("\\u{41") == PatternErrorKind::INVALID_ESCAPE);
    REQUIRE(compile_error("abc\\") == PatternErrorKind::INVALID_ESCAPE);
}

TEST_CASE("class ranges with lo > hi are rejected", "[pattern]")
{
    REQUIRE(compile_error("[z-a]") == PatternErrorKind::INVALID_RANGE);
}

TEST_CASE("code points outside the Unicode scalar range are rejected", "[pattern]")
{
    // U+D800 is a surrogate; U+110000 is beyond 0x10FFFF.
    REQUIRE(compile_error("\\u{D800}") == PatternErrorKind::INVALID_CODE_POINT);
    REQUIRE(compile_error("\\u{110000}") == PatternErrorKind::INVALID_CODE_POINT);
    REQUIRE(compile_error("[\\u{D800}]") == PatternErrorKind::INVALID_CODE_POINT);
}

TEST_CASE("a repeat with no preceding atom is rejected", "[pattern]")
{
    REQUIRE(compile_error("*abc") == PatternErrorKind::REPEAT_WITHOUT_ATOM);
    REQUIRE(compile_error("+abc") == PatternErrorKind::REPEAT_WITHOUT_ATOM);
    REQUIRE(compile_error("?abc") == PatternErrorKind::REPEAT_WITHOUT_ATOM);
    REQUIRE(compile_error("a|*b") == PatternErrorKind::REPEAT_WITHOUT_ATOM);
}

TEST_CASE("invalid repeat bounds are rejected", "[pattern]")
{
    REQUIRE(compile_error("a{3,1}") == PatternErrorKind::INVALID_REPEAT);
    REQUIRE(compile_error("a{") == PatternErrorKind::INVALID_REPEAT);
    REQUIRE(compile_error("a{2") == PatternErrorKind::INVALID_REPEAT);
    REQUIRE(compile_error("a{x}") == PatternErrorKind::INVALID_REPEAT);
}

TEST_CASE("a program exceeding MaxProgram is rejected, not truncated", "[pattern]")
{
    // A long chain of 'a' that needs more states than a tiny program can hold.
    auto result = compile_pattern<4, 8, 64>("aaaaaaaa");
    REQUIRE(result.has_error());
    REQUIRE(result.error().kind == PatternErrorKind::TOO_MANY_STATES);
}

TEST_CASE("a pattern with too many classes is rejected", "[pattern]")
{
    auto result = compile_pattern<256, 2, 64>("[a][b][c][d]");
    REQUIRE(result.has_error());
    REQUIRE(result.error().kind == PatternErrorKind::TOO_MANY_CLASSES);
}

TEST_CASE("a pattern with too many ranges is rejected", "[pattern]")
{
    auto result = compile_pattern<256, 8, 4>("[a-c][d-f][g-i][j-l][m-o]");
    REQUIRE(result.has_error());
    REQUIRE(result.error().kind == PatternErrorKind::TOO_MANY_RANGES);
}

TEST_CASE("catastrophic backtracking pattern completes in linear time", "[pattern]")
{
    // This is the classic ReDoS trigger: a backtracking engine takes exponential
    // time here. The Pike VM must return false almost immediately.
    const Pat evil = compile("^(a+)+$");
    const char* input = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX"; // 34 a's then a mismatch
    REQUIRE(!evil.matches(SimpleStringView(input)));
}

TEST_CASE("patterns compile and match at compile time", "[pattern]")
{
    constexpr auto build = []() noexcept
        {
            auto result = compile_pattern<64, 8, 64>(SimpleStringView("^a[0-9]b$"));
            if (result.has_error())
                {
                    return false;
                }
            return result.value().matches(SimpleStringView("a5b")) && !result.value().matches(SimpleStringView("axb"));
        };
    static_assert(build(), "compile_pattern and matches must be constexpr-evaluable");
    REQUIRE(build());
}
