/* Temporary AST probe 2. */
#include <catch2/catch_all.hpp>
#include <cstdio>
import embdr.cxxstd.stringView;
import embdr.lex.parseAst;
import embdr.lex.parser;
import embdr.lex.ast;
using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

TEST_CASE ("ast probe2", "[probe2]")
{
  const char* src = "struct ::s { a: int32; b: bool; }";
  std::printf ("start\n");
  SimpleStringView sv (src);
  Parser<> p{sv};
  std::printf ("parser built\n");
  auto r = parse_translation_unit (p);
  std::printf ("=== errors=%zu\n", r.errors);
  CHECK (r.root != NO_NODE);
}
