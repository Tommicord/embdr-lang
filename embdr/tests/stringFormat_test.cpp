/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

#include <limits>
#include <string>

import embdr.cxxstd.stringFormat;
import embdr.cxxstd.stringNumber;
import embdr.cxxstd.stringView;

using namespace embdr::cxxstd;

namespace {
        template <unsigned int N>
        constexpr bool fmt_is(const BasicTemplatedStringView<N, char>& view, const SimpleStringView expected) noexcept {
                return SimpleStringView(view) == expected;
        }

        std::string fmt_str(const SimpleStringView view) { return {view.data(), view.size()}; }

        enum class Color : int { red = 3, blue = 42 };
} // namespace

TEST_CASE("stringFormat parses literal text without arguments", "[stringFormat]") {
        char buf[64];
        REQUIRE(parse(buf, "hello") == 5);
        REQUIRE(std::string(buf) == "hello");
        REQUIRE(parse(buf, "") == 0);
        REQUIRE(std::string(buf) == "");
        REQUIRE(parse(buf, "{{}}") == 2);
        REQUIRE(std::string(buf) == "{}");
        REQUIRE(parse(buf, "a}}b{{c") == 5);
        REQUIRE(std::string(buf) == "a}b{c");
        REQUIRE(parse(buf, "no fields") == 9);
        REQUIRE(std::string(buf) == "no fields");
}

TEST_CASE("stringFormat reports malformed format text at runtime", "[stringFormat]") {
        char buf[64];
        REQUIRE(parse(buf, "a}b") == 1);
        REQUIRE(std::string(buf) == "a");
        REQUIRE(parse(buf, "abc{") == 3);
        REQUIRE(std::string(buf) == "abc");
        REQUIRE(parse(buf, "{}") == 0);
        REQUIRE(std::string(buf) == "");
}

TEST_CASE("stringFormat substitutes runtime arguments", "[stringFormat]") {
        char buf[64];
        REQUIRE(format(buf, "{}", 42) == 2);
        REQUIRE(std::string(buf) == "42");
        REQUIRE(format(buf, "{}", -42) == 3);
        REQUIRE(std::string(buf) == "-42");
        REQUIRE(format(buf, "{}", 0) == 1);
        REQUIRE(std::string(buf) == "0");
        REQUIRE(format(buf, "a {} b {}!", 1, "two") == 10);
        REQUIRE(std::string(buf) == "a 1 b two!");
        REQUIRE(format(buf, "{{{}}}", 7) == 3);
        REQUIRE(std::string(buf) == "{7}");
        REQUIRE(format(buf, "{}-{}", 1, 2) == 3);
        REQUIRE(std::string(buf) == "1-2");
        REQUIRE(format(buf, "{}", 1, 2) == 1);
        REQUIRE(std::string(buf) == "1");
        REQUIRE(format(buf, "{} {}", 1) == 2);
        REQUIRE(std::string(buf) == "1 ");
}

TEST_CASE("stringFormat formats integers in every supported base", "[stringFormat]") {
        char buf[72];
        REQUIRE(format(buf, "{}", 42) == 2);
        REQUIRE(std::string(buf) == "42");
        REQUIRE(format(buf, "{:d}", 42) == 2);
        REQUIRE(std::string(buf) == "42");
        REQUIRE(format(buf, "{:u}", 42) == 2);
        REQUIRE(std::string(buf) == "42");
        REQUIRE(format(buf, "{:x}", 255) == 2);
        REQUIRE(std::string(buf) == "ff");
        REQUIRE(format(buf, "{:X}", 255) == 2);
        REQUIRE(std::string(buf) == "FF");
        REQUIRE(format(buf, "{:#x}", 255) == 4);
        REQUIRE(std::string(buf) == "0xff");
        REQUIRE(format(buf, "{:#X}", 255) == 4);
        REQUIRE(std::string(buf) == "0XFF");
        REQUIRE(format(buf, "{:o}", 8) == 2);
        REQUIRE(std::string(buf) == "10");
        REQUIRE(format(buf, "{:#o}", 8) == 3);
        REQUIRE(std::string(buf) == "010");
        REQUIRE(format(buf, "{:#o}", 0) == 1);
        REQUIRE(std::string(buf) == "0");
        REQUIRE(format(buf, "{:b}", 10) == 4);
        REQUIRE(std::string(buf) == "1010");
        REQUIRE(format(buf, "{:#b}", 10) == 6);
        REQUIRE(std::string(buf) == "0b1010");
        REQUIRE(format(buf, "{:B}", 10) == 6);
        REQUIRE(std::string(buf) == "0B1010");
        REQUIRE(format(buf, "{:#x}", 0) == 3);
        REQUIRE(std::string(buf) == "0x0");
        REQUIRE(format(buf, "{:c}", 65) == 1);
        REQUIRE(std::string(buf) == "A");
        REQUIRE(format(buf, "{:x}", -1) == 16);
        REQUIRE(std::string(buf) == "ffffffffffffffff");
        REQUIRE(format(buf, "{:.4}", 42) == 4);
        REQUIRE(std::string(buf) == "0042");
        REQUIRE(format(buf, "{:8.4}", 42) == 8);
        REQUIRE(std::string(buf) == "    0042");
        REQUIRE(format(buf, "{:.1}", -42) == 3);
        REQUIRE(std::string(buf) == "-42");
}

TEST_CASE("stringFormat applies integer sign flags", "[stringFormat]") {
        char buf[32];
        REQUIRE(format(buf, "{:+d}", 42) == 3);
        REQUIRE(std::string(buf) == "+42");
        REQUIRE(format(buf, "{:+d}", -42) == 3);
        REQUIRE(std::string(buf) == "-42");
        REQUIRE(format(buf, "{: d}", 42) == 3);
        REQUIRE(std::string(buf) == " 42");
        REQUIRE(format(buf, "{: }", 42) == 3);
        REQUIRE(std::string(buf) == " 42");
        REQUIRE(format(buf, "{:+}", 0) == 2);
        REQUIRE(std::string(buf) == "+0");
        REQUIRE(format(buf, "{:+x}", 255) == 2);
        REQUIRE(std::string(buf) == "ff");
}

TEST_CASE("stringFormat pads and aligns runtime output", "[stringFormat]") {
        char buf[64];
        REQUIRE(format(buf, "{:5}", 42) == 5);
        REQUIRE(std::string(buf) == "   42");
        REQUIRE(format(buf, "{:<5}", 42) == 5);
        REQUIRE(std::string(buf) == "42   ");
        REQUIRE(format(buf, "{:>5}", 42) == 5);
        REQUIRE(std::string(buf) == "   42");
        REQUIRE(format(buf, "{:^5}", 42) == 5);
        REQUIRE(std::string(buf) == " 42  ");
        REQUIRE(format(buf, "{:*>5}", 42) == 5);
        REQUIRE(std::string(buf) == "***42");
        REQUIRE(format(buf, "{:*^7}", 42) == 7);
        REQUIRE(std::string(buf) == "**42***");
        REQUIRE(format(buf, "{:05}", 42) == 5);
        REQUIRE(std::string(buf) == "00042");
        REQUIRE(format(buf, "{:05}", -42) == 5);
        REQUIRE(std::string(buf) == "-0042");
        REQUIRE(format(buf, "{:6}", -42) == 6);
        REQUIRE(std::string(buf) == "   -42");
        REQUIRE(format(buf, "{:*<6}", "ab") == 6);
        REQUIRE(std::string(buf) == "ab****");
        REQUIRE(format(buf, "{:*>6}", "ab") == 6);
        REQUIRE(std::string(buf) == "****ab");
        REQUIRE(format(buf, "{:^5}", "ab") == 5);
        REQUIRE(std::string(buf) == " ab  ");
        REQUIRE(format(buf, "{:>3}", 'A') == 3);
        REQUIRE(std::string(buf) == "  A");
        REQUIRE(format(buf, "{:3}", 'A') == 3);
        REQUIRE(std::string(buf) == "A  ");
        REQUIRE(format(buf, "{:5}", true) == 5);
        REQUIRE(std::string(buf) == "true ");
        REQUIRE(format(buf, "{:>5}", true) == 5);
        REQUIRE(std::string(buf) == " true");
}

TEST_CASE("stringFormat formats strings, booleans and characters", "[stringFormat]") {
        char buf[64];
        REQUIRE(format(buf, "{}", "hi") == 2);
        REQUIRE(std::string(buf) == "hi");
        REQUIRE(format(buf, "{:s}", "hi") == 2);
        REQUIRE(std::string(buf) == "hi");
        REQUIRE(format(buf, "{:>4}", "hi") == 4);
        REQUIRE(std::string(buf) == "  hi");
        REQUIRE(format(buf, "{:.2}", "hello") == 2);
        REQUIRE(std::string(buf) == "he");
        REQUIRE(format(buf, "{:>6.2}", "hello") == 6);
        REQUIRE(std::string(buf) == "    he");
        REQUIRE(format(buf, "{}", SimpleStringView("view")) == 4);
        REQUIRE(std::string(buf) == "view");
        REQUIRE(format(buf, "{}", std::string("str")) == 3);
        REQUIRE(std::string(buf) == "str");
        REQUIRE(format(buf, "{}", true) == 4);
        REQUIRE(std::string(buf) == "true");
        REQUIRE(format(buf, "{}", false) == 5);
        REQUIRE(std::string(buf) == "false");
        REQUIRE(format(buf, "{:d}", true) == 1);
        REQUIRE(std::string(buf) == "1");
        REQUIRE(format(buf, "{:d}", false) == 1);
        REQUIRE(std::string(buf) == "0");
        REQUIRE(format(buf, "{:c}", false) == 1);
        REQUIRE(std::string(buf) == "0");
        REQUIRE(format(buf, "{}", 'A') == 1);
        REQUIRE(std::string(buf) == "A");
        REQUIRE(format(buf, "{}", Color::blue) == 2);
        REQUIRE(std::string(buf) == "42");
        REQUIRE(format(buf, "{:#x}", Color::red) == 3);
        REQUIRE(std::string(buf) == "0x3");
}

TEST_CASE("stringFormat formats floating point values", "[stringFormat]") {
        char buf[64];
        REQUIRE(format(buf, "{}", 1.5) == 3);
        REQUIRE(std::string(buf) == "1.5");
        REQUIRE(format(buf, "{}", 0.0) == 3);
        REQUIRE(std::string(buf) == "0.0");
        REQUIRE(format(buf, "{}", 0.1) == 3);
        REQUIRE(std::string(buf) == "0.1");
        REQUIRE(format(buf, "{:f}", 1.5) == 8);
        REQUIRE(std::string(buf) == "1.500000");
        REQUIRE(format(buf, "{:.2f}", 3.14159) == 4);
        REQUIRE(std::string(buf) == "3.14");
        REQUIRE(format(buf, "{:.0f}", 2.5) == 1);
        REQUIRE(std::string(buf) == "3");
        REQUIRE(format(buf, "{:.0f}", 0.4) == 1);
        REQUIRE(std::string(buf) == "0");
        REQUIRE(format(buf, "{:.0f}", 0.6) == 1);
        REQUIRE(std::string(buf) == "1");
        REQUIRE(format(buf, "{:.3f}", -1.5) == 6);
        REQUIRE(std::string(buf) == "-1.500");
        REQUIRE(format(buf, "{:.10f}", 0.1) == 12);
        REQUIRE(std::string(buf) == "0.1000000000");
        REQUIRE(format(buf, "{:#.0f}", 5.0) == 2);
        REQUIRE(std::string(buf) == "5.");
        REQUIRE(format(buf, "{:e}", 1.5) == 12);
        REQUIRE(std::string(buf) == "1.500000e+00");
        REQUIRE(format(buf, "{:.0e}", 12345.0) == 5);
        REQUIRE(std::string(buf) == "1e+04");
        REQUIRE(format(buf, "{:g}", 1.5) == 3);
        REQUIRE(std::string(buf) == "1.5");
        REQUIRE(format(buf, "{:g}", 0.1) == 3);
        REQUIRE(std::string(buf) == "0.1");
        REQUIRE(format(buf, "{:.3g}", 1.23456) == 4);
        REQUIRE(std::string(buf) == "1.23");
        REQUIRE(format(buf, "{:.3g}", 1.2356) == 4);
        REQUIRE(std::string(buf) == "1.24");
        REQUIRE(format(buf, "{:.2}", 1500.0) == 7);
        REQUIRE(std::string(buf) == "1.5e+03");
        REQUIRE(format(buf, "{:#g}", 1.5) == 7);
        REQUIRE(std::string(buf) == "1.50000");
        REQUIRE(format(buf, "{:>8.2f}", 3.14159) == 8);
        REQUIRE(std::string(buf) == "    3.14");
        REQUIRE(format(buf, "{:08.2f}", 3.14159) == 8);
        REQUIRE(std::string(buf) == "00003.14");
        REQUIRE(format(buf, "{:08.2f}", -3.14159) == 8);
        REQUIRE(std::string(buf) == "-0003.14");
}

TEST_CASE("stringFormat emits infinities and not-a-number", "[stringFormat]") {
        char buf[64];
        const double inf = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        REQUIRE(format(buf, "{}", inf) == 3);
        REQUIRE(std::string(buf) == "inf");
        REQUIRE(format(buf, "{}", -inf) == 4);
        REQUIRE(std::string(buf) == "-inf");
        REQUIRE(format(buf, "{:F}", -inf) == 4);
        REQUIRE(std::string(buf) == "-INF");
        REQUIRE(format(buf, "{}", nan) == 3);
        REQUIRE(std::string(buf) == "nan");
        REQUIRE(format(buf, "{:.1f}", inf) == 3);
        REQUIRE(std::string(buf) == "inf");
        REQUIRE(format(buf, "{:>+7}", inf) == 7);
        REQUIRE(std::string(buf) == "   +inf");
}

TEST_CASE("stringFormat formats pointers", "[stringFormat]") {
        char buf[64];
        int value = 7;
        int* ptr = &value;
        REQUIRE(format(buf, "{}", ptr) > 2);
        REQUIRE(std::string(buf).compare(0, 2, "0x") == 0);
        REQUIRE(format(buf, "{:p}", ptr) > 2);
        REQUIRE(std::string(buf).compare(0, 2, "0x") == 0);
        REQUIRE(format(buf, "{}", nullptr) == 7);
        REQUIRE(std::string(buf) == "nullptr");
        REQUIRE(format(buf, "{}", static_cast<const char*>(nullptr)) == 7);
        REQUIRE(std::string(buf) == "nullptr");
        const char* text = "abc";
        REQUIRE(format(buf, "{}", text) == 3);
        REQUIRE(std::string(buf) == "abc");
        REQUIRE(format(buf, "{}", static_cast<void*>(&value)) > 2);
        REQUIRE(std::string(buf).compare(0, 2, "0x") == 0);
}

TEST_CASE("stringFormat rejects invalid runtime format specifications", "[stringFormat]") {
        char buf[64];
        REQUIRE(format(buf, "{:z}", 1) == 0);
        REQUIRE(std::string(buf) == "");
        REQUIRE(format(buf, "{:.}", 1) == 0);
        REQUIRE(std::string(buf) == "");
        REQUIRE(format(buf, "{:s}", 1) == 0);
        REQUIRE(std::string(buf) == "");
        REQUIRE(format(buf, "{:x}", 1.5) == 0);
        REQUIRE(std::string(buf) == "");
        REQUIRE(format(buf, "{:d}", "text") == 0);
        REQUIRE(std::string(buf) == "");
}

TEST_CASE("stringFormat truncates into fixed destination buffers", "[stringFormat]") {
        char small[4];
        REQUIRE(format(small, "{}", "hello") == 5);
        REQUIRE(std::string(small) == "hel");
        REQUIRE(small[3] == '\0');
        char tiny[1];
        REQUIRE(format(tiny, "{}", "hello") == 5);
        REQUIRE(tiny[0] == '\0');
        char four[4];
        REQUIRE(format(four, "{}", "hell") == 4);
        REQUIRE(std::string(four) == "hel");
        char digits[4];
        REQUIRE(format(digits, "{}", 12345) == 5);
        REQUIRE(std::string(digits) == "123");
}

TEST_CASE("stringFormat formats at compile time", "[stringFormat]") {
        static_assert(fmt_is(format<"hello">(), "hello"));
        static_assert(fmt_is(format<"">(), ""));
        static_assert(fmt_is(format<"a {} b", 42>(), "a 42 b"));
        static_assert(fmt_is(format<"{}", -42>(), "-42"));
        static_assert(fmt_is(format<"{{}}">(), "{}"));
        static_assert(fmt_is(format<"{{{}}}", 7>(), "{7}"));
        static_assert(fmt_is(format<"{}", "hi">(), "hi"));
        static_assert(fmt_is(format<"{:.2}", "hello">(), "he"));
        static_assert(fmt_is(format<"{:*<6}", "ab">(), "ab****"));
        REQUIRE(fmt_str(format<"{:*>6}", "ab">()) == "****ab");
        static_assert(fmt_is(format<"{:x}", 255>(), "ff"));
        static_assert(fmt_is(format<"{:#x}", 255>(), "0xff"));
        static_assert(fmt_is(format<"{:#X}", 255>(), "0XFF"));
        static_assert(fmt_is(format<"{:o}", 8>(), "10"));
        static_assert(fmt_is(format<"{:#o}", 8>(), "010"));
        static_assert(fmt_is(format<"{:#b}", 10>(), "0b1010"));
        static_assert(fmt_is(format<"{:c}", 65>(), "A"));
        static_assert(fmt_is(format<"{:+d}", 42>(), "+42"));
        static_assert(fmt_is(format<"{: d}", 42>(), " 42"));
        static_assert(fmt_is(format<"{:07}", -42>(), "-000042"));
        static_assert(fmt_is(format<"{:.4}", 42>(), "0042"));
        REQUIRE(fmt_str(format<"{:^5}", 42>()) == " 42  ");
        static_assert(fmt_is(format<"{:*>5}", 42>(), "***42"));
        static_assert(fmt_is(format<"{}", true>(), "true"));
        static_assert(fmt_is(format<"{:d}", false>(), "0"));
        static_assert(fmt_is(format<"{}", 'z'>(), "z"));
        REQUIRE(fmt_str(format<"{:>3}", 'A'>()) == "  A");
        static_assert(fmt_is(format<"{}", 1.5>(), "1.5"));
        static_assert(fmt_is(format<"{}", 0.1>(), "0.1"));
        static_assert(fmt_is(format<"{:.2f}", 3.5>(), "3.50"));
        static_assert(fmt_is(format<"{:.0f}", 2.5>(), "3"));
        static_assert(fmt_is(format<"{:e}", 1.5>(), "1.500000e+00"));
        static_assert(fmt_is(format<"{:g}", 1.5>(), "1.5"));
        static_assert(fmt_is(format<"{:#g}", 1.5>(), "1.50000"));
        static_assert(fmt_is(format<"{}", std::numeric_limits<double>::infinity()>(), "inf"));
        static_assert(fmt_is(format<"{}", Color::red>(), "3"));
        static_assert(fmt_is(format<"{:x}", Color::blue>(), "2a"));
        static_assert(SimpleStringView(format<"hello">()) == "hello");
        static_assert(format<"{}: ", 42>().size() == 4);
}

TEST_CASE("stringifyNumber converts integers across bases", "[stringFormat]") {
        char buff[72];
        REQUIRE(uitoa(buff, 0ull) == 1);
        REQUIRE(std::string(buff) == "0");
        REQUIRE(uitoa(buff, 42ull) == 2);
        REQUIRE(std::string(buff) == "42");
        REQUIRE(uitoa(buff, 255ull, NumberBase::hexadecimal) == 2);
        REQUIRE(std::string(buff) == "ff");
        REQUIRE(uitoa(buff, 255ull, NumberBase::hexadecimal, true) == 2);
        REQUIRE(std::string(buff) == "FF");
        REQUIRE(uitoa(buff, 8ull, NumberBase::octal) == 2);
        REQUIRE(std::string(buff) == "10");
        REQUIRE(uitoa(buff, 10ull, NumberBase::binary) == 4);
        REQUIRE(std::string(buff) == "1010");
        REQUIRE(uitoa(buff, 10ull, NumberBase::binary, true) == 4);
        REQUIRE(std::string(buff) == "1010");
        REQUIRE(uitoa(buff, 42ull, 99) == 2);
        REQUIRE(std::string(buff) == "42");
        REQUIRE(uitoa(buff, 42ull, 1) == 2);
        REQUIRE(std::string(buff) == "42");
        REQUIRE(uitoa(buff, 35ull, 36) == 1);
        REQUIRE(std::string(buff) == "z");
        REQUIRE(uitoa(nullptr, 1ull) == 0);
        REQUIRE(itoa(buff, -42) == 3);
        REQUIRE(std::string(buff) == "-42");
        REQUIRE(itoa(buff, 7) == 1);
        REQUIRE(std::string(buff) == "7");
        REQUIRE(itoa(buff, -255, NumberBase::hexadecimal) == 3);
        REQUIRE(std::string(buff) == "-ff");
        REQUIRE(itoa(buff, -255, NumberBase::hexadecimal, true) == 3);
        REQUIRE(std::string(buff) == "-FF");
        REQUIRE(itoa(nullptr, -1) == 0);
}
