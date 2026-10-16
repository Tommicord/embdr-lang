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

export module embdr.lex.parserTemplate;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserType;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::Maybe;

  // A generic parameter list `< T [ , U ... ] >` (identifiers only; bounds
  // are a planned extension). Returns a GENERIC_PARAMS node whose children
  // are PATH nodes carrying each parameter name.
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_generic_params (Parser<ArenaT>& p) noexcept
  {
    const Maybe<Token, ParseError> lt = p.expect (TokenKind::LT);
    if (lt.has_error ())
      {
        return lt.error ();
      }
    const Maybe<uint32_t, ParseError> params = p.make (NodeKind::GENERIC_PARAMS, lt.value ());
    if (params.has_error ())
      {
        return params;
      }
    // tolerate an empty list (`<>`)
    if (p.kind () != TokenKind::GT && p.kind () != TokenKind::GT_GT)
      {
        const Maybe<Token, ParseError> name = p.expect_ident ();
        if (name.has_error ())
          {
            return name.error ();
          }
        const Maybe<uint32_t, ParseError> idnode = p.make (NodeKind::PATH, name.value ());
        p.node (idnode.value ()).text = name.value ().lexeme;
        p.append (params.value (), idnode.value ());
      }
    while (p.consume_if (TokenKind::COMMA))
      {
        const Maybe<Token, ParseError> nm = p.expect_ident ();
        if (nm.has_error ())
          {
            return nm.error ();
          }
        const Maybe<uint32_t, ParseError> np = p.make (NodeKind::PATH, nm.value ());
        p.node (np.value ()).text = nm.value ().lexeme;
        p.append (params.value (), np.value ());
      }
    const Maybe<Token, ParseError> gt = p.expect_gt ();
    if (gt.has_error ())
      {
        return gt.error ();
      }
    return params;
  }
} // namespace embdr::lex
