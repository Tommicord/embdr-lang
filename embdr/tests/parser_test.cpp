/* Copyright(c) 2026 Tommicord */
#include <catch2/catch_all.hpp>
import embdr.cxxstd.stringView;
import embdr.lex.parseAst;
import embdr.lex.parser;
import embdr.lex.ast;
using embdr::cxxstd::SimpleStringView;
using embdr::lex::Parser;

TEST_CASE ("parser input 0", "[parser]")
{
  const char* src = "";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 1", "[parser]")
{
  const char* src = " ";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 2", "[parser]")
{
  const char* src = "function ::f() { }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 3", "[parser]")
{
  const char* src = "struct ::s { }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 4", "[parser]")
{
  const char* src = "enum ::e { A }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 5", "[parser]")
{
  const char* src = "function ::g(x: int32) { ret x; }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 6", "[parser]")
{
  const char* src = "#addr(main) function ::main() { std::<println>(\"Hello, World!\"); ret 0; }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 7", "[parser]")
{
  const char* src = "function ::a() { if 1 { ret 2 } else { ret 3 } }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 8", "[parser]")
{
  const char* src = "JIHGFEDCB";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 9", "[parser]")
{
  const char* src = "#\"! ~}|{zyxwvutsrqpo";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 10", "[parser]")
{
  const char* src = "LKJIHGFED";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 11", "[parser]")
{
  const char* src = "%$#\"! ~}|{zyxwvutsrq";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 12", "[parser]")
{
  const char* src = "NMLKJIHGF";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 13", "[parser]")
{
  const char* src = "'&%$#\"! ~}|{zyxwvuts";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 14", "[parser]")
{
  const char* src = "PONMLKJIH";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 15", "[parser]")
{
  const char* src = ")('&%$#\"! ~}|{zyxwvu";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 16", "[parser]")
{
  const char* src = "RQPONMLKJ";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 17", "[parser]")
{
  const char* src = "+*)('&%$#\"! ~}|{zyxw";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 18", "[parser]")
{
  const char* src = "TSRQPONML";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 19", "[parser]")
{
  const char* src = "-,+*)('&%$#\"! ~}|{zy";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 20", "[parser]")
{
  const char* src = "VUTSRQPON";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 21", "[parser]")
{
  const char* src = "/.-,+*)('&%$#\"! ~}|{";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 22", "[parser]")
{
  const char* src = "XWVUTSRQP";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 23", "[parser]")
{
  const char* src = "10/.-,+*)('&%$#\"! ~}";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 24", "[parser]")
{
  const char* src = "ZYXWVUTSR";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 25", "[parser]")
{
  const char* src = "3210/.-,+*)('&%$#\"! ";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 26", "[parser]")
{
  const char* src = "\\[ZYXWVUT";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 27", "[parser]")
{
  const char* src = "543210/.-,+*)('&%$#\"";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 28", "[parser]")
{
  const char* src = "^]\\[ZYXWV";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 29", "[parser]")
{
  const char* src = "76543210/.-,+*)('&%$";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 30", "[parser]")
{
  const char* src = "`_^]\\[ZYX";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 31", "[parser]")
{
  const char* src = "9876543210/.-,+*)('&";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 32", "[parser]")
{
  const char* src = "ba`_^]\\[Z";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 33", "[parser]")
{
  const char* src = ";:9876543210/.-,+*)(";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 34", "[parser]")
{
  const char* src = "dcba`_^]\\";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 35", "[parser]")
{
  const char* src = "=<;:9876543210/.-,+*";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 36", "[parser]")
{
  const char* src = "fedcba`_^";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 37", "[parser]")
{
  const char* src = "?>=<;:9876543210/.-,";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 38", "[parser]")
{
  const char* src = "hgfedcba`";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 39", "[parser]")
{
  const char* src = "A@?>=<;:9876543210/.";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 40", "[parser]")
{
  const char* src = "jihgfedcb";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 41", "[parser]")
{
  const char* src = "CBA@?>=<;:9876543210";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 42", "[parser]")
{
  const char* src = "lkjihgfed";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 43", "[parser]")
{
  const char* src = "EDCBA@?>=<;:98765432";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 44", "[parser]")
{
  const char* src = "nmlkjihgf";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 45", "[parser]")
{
  const char* src = "GFEDCBA@?>=<;:987654";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 46", "[parser]")
{
  const char* src = "ponmlkjih";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 47", "[parser]")
{
  const char* src = "IHGFEDCBA@?>=<;:9876";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 48", "[parser]")
{
  const char* src = "rqponmlkj";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 49", "[parser]")
{
  const char* src = "KJIHGFEDCBA@?>=<;:98";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 50", "[parser]")
{
  const char* src = "tsrqponml";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 51", "[parser]")
{
  const char* src = "MLKJIHGFEDCBA@?>=<;:";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 52", "[parser]")
{
  const char* src = "vutsrqpon";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 53", "[parser]")
{
  const char* src = "ONMLKJIHGFEDCBA@?>=<";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 54", "[parser]")
{
  const char* src = "xwvutsrqp";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 55", "[parser]")
{
  const char* src = "QPONMLKJIHGFEDCBA@?>";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 56", "[parser]")
{
  const char* src = "zyxwvutsr";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 57", "[parser]")
{
  const char* src = "SRQPONMLKJIHGFEDCBA@";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 58", "[parser]")
{
  const char* src = "A";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 59", "[parser]")
{
  const char* src = "AA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 60", "[parser]")
{
  const char* src = "AAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 61", "[parser]")
{
  const char* src = "AAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 62", "[parser]")
{
  const char* src = "AAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 63", "[parser]")
{
  const char* src = "AAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 64", "[parser]")
{
  const char* src = "AAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 65", "[parser]")
{
  const char* src = "AAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 66", "[parser]")
{
  const char* src = "AAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 67", "[parser]")
{
  const char* src = "AAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 68", "[parser]")
{
  const char* src = "AAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 69", "[parser]")
{
  const char* src = "AAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 70", "[parser]")
{
  const char* src = "AAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 71", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 72", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 73", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 74", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 75", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 76", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 77", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 78", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 79", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 80", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 81", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 82", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 83", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 84", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 85", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 86", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 87", "[parser]")
{
  const char* src = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA{ }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 88", "[parser]")
{
  const char* src = "function ::f() {  }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 89", "[parser]")
{
  const char* src = "function ::f() { 0 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 90", "[parser]")
{
  const char* src = "function ::f() { 0, 1 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 91", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 92", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 93", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 94", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 95", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 96", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 97", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 98", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 99", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 100", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 101", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 102", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 103", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 104", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 105", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 106", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 107", "[parser]")
{
  const char* src = "function ::f() { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 }";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}

TEST_CASE ("parser input 108", "[parser]")
{
  const char* src = "";
  SimpleStringView sv (src ? src : "");
  Parser<> p{sv};
  auto r = embdr::lex::parse_translation_unit (p);
  CHECK (r.root != embdr::lex::NO_NODE);
}
