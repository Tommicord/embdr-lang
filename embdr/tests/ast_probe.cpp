/* Temporary AST probe (removed before delivery). */
#include <catch2/catch_all.hpp>
#include <cstdio>
import embdr.cxxstd.stringView;
import embdr.lex.parseAst;
import embdr.lex.parser;
import embdr.lex.ast;
using embdr::cxxstd::SimpleStringView;
using namespace embdr::lex;

static void
dump (const AstArena<>& a, uint32_t id, int depth)
{
  if (depth > 12)
    return;
  const AstNode& n = a[id];
  std::printf ("%*s#%u %s off=%u flags=%u op=%d text='%.*s' children=[", depth * 2, "", id, as_str (n.kind).data (),
               n.offset, n.flags, static_cast<int> (n.op), static_cast<int> (n.text.size ()), n.text.data ());
  uint32_t c = n.first_child;
  int guard = 0;
  while (c != NO_NODE && guard++ < 40)
    {
      std::printf ("%u ", c);
      c = a[c].next_sibling;
    }
  std::printf ("]\n");
  c = n.first_child;
  guard = 0;
  while (c != NO_NODE && guard++ < 40)
    {
      if (c != id)
        dump (a, c, depth + 1);
      c = a[c].next_sibling;
    }
}

TEST_CASE ("ast probe", "[probe]")
{
  const char* srcs[] = {
      "function ::f(x: int32) -> int32 { ret x; }",
      "function ::a() { if 1 { ret 2 } else { ret 3 } }",
      "function ::a() { let y = if true { 1 } else { 2 }; }",
      "function ::a() { int32 q = 0; let z = q + 1; }",
      "struct ::s { a: int32; b: bool; }",
      "struct ::s2 { pub { a: int32; } }",
      "enum ::c { red, green }",
      "#addr(main) function ::main() { ret 0; }",
      "function ::m() { for i in 0 .. 10 { } }",
      "function ::m() { let f = function(a) { ret a; }; }",
      "function ::m() { match x { 0 { } _ { } } }",
      "function ::g() { let c = std::<println>(\"x\"); }",
      "function ::h() { volatile { *p = 1; } }",
      "function ::k() { std::math::sqrt(2.0); }",
  };
  for (auto* src : srcs)
    {
      SimpleStringView sv (src);
      Parser<> p{sv};
      auto r = parse_translation_unit (p);
      std::printf ("=== '%s' errors=%zu diags=%zu\n", src, r.errors, p.diag_count ());
      for (size_t i = 0; i < p.diag_count (); ++i)
        {
          const auto& d = p.diagnostic (i);
          std::printf ("    diag kind=%d off=%u exp=%d found=%d\n", static_cast<int> (d.kind), d.offset,
                       static_cast<int> (d.expected), static_cast<int> (d.found));
        }
      if (r.root != NO_NODE)
        dump (p.arena (), r.root, 1);
    }
  CHECK (true);
}
