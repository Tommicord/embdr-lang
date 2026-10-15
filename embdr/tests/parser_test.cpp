/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <catch2/catch_all.hpp>
#include <string>

import embdr.cxxstd.stringView;
import embdr.lex.parseAst;
import embdr.lex.parser;
import embdr.lex.ast;

using embdr::cxxstd::SimpleStringView;
using embdr::lex::Parser;

TEST_CASE("parser parses hello world", "[parser]")
{
    const char* src = "#addr(main) function ::main() { std::<println>(\"Hello, World!\"); ret 0; }";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.root != embdr::lex::NO_NODE);
    CHECK(r.errors == 0);
}

TEST_CASE("parser records missing semicolon diagnostic", "[parser]")
{
    const char* src = "function ::f() { ret 0 }";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.errors > 0);
}

TEST_CASE("parser parses struct with pub fields", "[parser]")
{
    const char* src = "struct ::point { pub { x: int32; y: int32; } }";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.root != embdr::lex::NO_NODE);
    CHECK(r.errors == 0);
}

TEST_CASE("parser parses enum with variants", "[parser]")
{
    const char* src = "enum ::color { RED, GREEN, BLUE }";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.root != embdr::lex::NO_NODE);
    CHECK(r.errors == 0);
}

TEST_CASE("parser parses match expr", "[parser]")
{
    const char* src = "function ::f(x: int32) { ret match x { 0 => { 1 } _ => { 2 } }; }";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.root != embdr::lex::NO_NODE);
}

TEST_CASE("parser handles empty translation unit", "[parser]")
{
    const char* src = "";
    Parser<> p{SimpleStringView(src)};
    auto r = embdr::lex::parse_translation_unit(p);
    CHECK(r.root != embdr::lex::NO_NODE);
    CHECK(r.errors == 0);
}
