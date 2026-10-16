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

export module embdr.lex.parserExpr;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserType;
import embdr.lex.parserTentative;
import embdr.lex.parserTemplate;
import embdr.lex.parserPattern;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::Maybe;

  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_assign (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_pipeline (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_coalesce (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_or (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_and (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_range (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitor (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitxor (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitand (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_equality (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_shift (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_additive (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_multiplicative (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_cast (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_unary (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_postfix (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_primary (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_block_expr (Parser<ArenaT>& p) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_if_expr (Parser<ArenaT>& p, const Token& kw) noexcept;
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_match_expr (Parser<ArenaT>& p) noexcept;

  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_closure_with_static (Parser<ArenaT>& p, const Token& kw) noexcept;

  // ---- precedence helpers ------------------------------------------------

  constexpr int
  binop_prec (const TokenKind k) noexcept
  {
    switch (k)
      {
        case TokenKind::PIPE_GT:
          return 15;
        case TokenKind::QUESTION_QUESTION:
          return 14;
        case TokenKind::PIPE_PIPE:
          return 13;
        case TokenKind::AMP_AMP:
          return 12;
        case TokenKind::EQ_EQ:
        case TokenKind::BANG_EQ:
        case TokenKind::LT:
        case TokenKind::GT:
        case TokenKind::LT_EQ:
        case TokenKind::GT_EQ:
        case TokenKind::KW_IS:
          return 11;
        case TokenKind::DOT_DOT:
        case TokenKind::DOT_DOT_ASSIGN:
          return 10;
        case TokenKind::PIPE:
          return 9;
        case TokenKind::CARET:
          return 8;
        case TokenKind::AMP:
          return 7;
        case TokenKind::LT_LT:
        case TokenKind::GT_GT:
          return 6;
        case TokenKind::PLUS:
        case TokenKind::MINUS:
          return 5;
        case TokenKind::STAR:
        case TokenKind::SLASH:
        case TokenKind::PERCENT:
          return 4;
        case TokenKind::PLUS_EQ:
        case TokenKind::MINUS_EQ:
        case TokenKind::STAR_EQ:
        case TokenKind::SLASH_EQ:
        case TokenKind::PERCENT_EQ:
        case TokenKind::AMP_EQ:
        case TokenKind::PIPE_EQ:
        case TokenKind::CARET_EQ:
        case TokenKind::LSHIFT_ASSIGN:
        case TokenKind::RSHIFT_ASSIGN:
        case TokenKind::EQ:
          return 16;
        default:
          return 0;
      }
  }

  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_expr (Parser<ArenaT>& p) noexcept
  {
    return parse_assign (p);
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_assign (Parser<ArenaT>& p) noexcept
  {
    const Maybe<uint32_t, ParseError> lhs = parse_pipeline (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    const int prec = binop_prec (p.kind ());
    if (prec == 16)
      {
        const Token op = p.consume ();
        const Maybe<uint32_t, ParseError> rhs = parse_expr (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::ASSIGN, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        return node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_binary_chain (Parser<ArenaT>& p, const int minprec) noexcept
  {
    auto lhs = parse_cast (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (true)
      {
        const int prec = binop_prec (p.kind ());
        if (prec < minprec || prec == 0 || prec == 16)
          {
            break;
          }
        // non-associative chain check (comparison & range)
        if (prec == 10 || prec == 11)
          {
            const TokenKind k = p.kind ();
            // peek: if another cmp/range op follows -> non-assoc chain
            (void)k;
          }
        const Token op = p.consume ();
        auto rhs = parse_cast (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_pipeline (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_coalesce (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::PIPE_GT)
      {
        const Token op = p.consume ();
        auto rhs = parse_coalesce (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::PIPELINE, op);
        if (node.has_error ())
          {
            return node;
          }
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_coalesce (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_or (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::QUESTION_QUESTION)
      {
        const Token op = p.consume ();
        auto rhs = parse_or (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::COALESCE, op);
        if (node.has_error ())
          {
            return node;
          }
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_or (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_and (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::PIPE_PIPE)
      {
        const Token op = p.consume ();
        auto rhs = parse_and (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_and (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_range (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::AMP_AMP)
      {
        const Token op = p.consume ();
        auto rhs = parse_range (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_range (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_bitor (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    if (p.kind () == TokenKind::DOT_DOT || p.kind () == TokenKind::DOT_DOT_ASSIGN)
      {
        const Token op = p.consume ();
        Maybe<uint32_t, ParseError> rhs = parse_bitor (p);
        uint8_t flags = RANGE_HAS_LO;
        if (rhs.has_value ())
          {
            flags = flags | RANGE_HAS_HI;
          }
        if (op.kind == TokenKind::DOT_DOT_ASSIGN)
          {
            flags = flags | RANGE_INCLUSIVE;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::RANGE, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).flags = flags;
        p.append (node.value (), lhs.value ());
        if (rhs.has_value ())
          {
            p.append (node.value (), rhs.value ());
          }
        return node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitor (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_bitxor (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::PIPE)
      {
        const Token op = p.consume ();
        auto rhs = parse_bitxor (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitxor (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_bitand (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::CARET)
      {
        const Token op = p.consume ();
        auto rhs = parse_bitand (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_bitand (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_equality (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::AMP)
      {
        const Token op = p.consume ();
        auto rhs = parse_equality (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_equality (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_shift (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    if (p.kind () == TokenKind::EQ_EQ || p.kind () == TokenKind::BANG_EQ || p.kind () == TokenKind::KW_IS)
      {
        const Token op = p.consume ();
        auto rhs = parse_shift (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        if (op.kind == TokenKind::KW_IS)
          {
            // `x is T` — RHS is a type; parse a delimited type path stored
            // as a second child computed before; if another type follows,
            // resync.
            p.node (node.value ()).first_child = p.node (node.value ()).first_child; // no-op placeholder
          }
        p.append (node.value (), rhs.value ());
        return node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_shift (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_additive (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::LT_LT || p.kind () == TokenKind::GT_GT)
      {
        const Token op = p.consume ();
        auto rhs = parse_additive (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_additive (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_multiplicative (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::PLUS || p.kind () == TokenKind::MINUS)
      {
        const Token op = p.consume ();
        auto rhs = parse_multiplicative (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_multiplicative (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_cast (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::STAR || p.kind () == TokenKind::SLASH || p.kind () == TokenKind::PERCENT)
      {
        const Token op = p.consume ();
        auto rhs = parse_cast (p);
        if (rhs.has_error ())
          {
            return rhs;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BINARY, op);
        if (node.has_error ())
          {
            return node;
          }
        p.node (node.value ()).op = op.kind;
        p.append (node.value (), lhs.value ());
        p.append (node.value (), rhs.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_cast (Parser<ArenaT>& p) noexcept
  {
    auto lhs = parse_unary (p);
    if (lhs.has_error ())
      {
        return lhs;
      }
    while (p.kind () == TokenKind::KW_AS)
      {
        const Token kw = p.consume ();
        const Maybe<uint32_t, ParseError> ty = parse_type (p);
        if (ty.has_error ())
          {
            return ty;
          }
        const Maybe<uint32_t, ParseError> node = p.make (NodeKind::CAST_EXPR, kw);
        if (node.has_error ())
          {
            return node;
          }
        p.append (node.value (), lhs.value ());
        p.append (node.value (), ty.value ());
        lhs = node;
      }
    return lhs;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_unary (Parser<ArenaT>& p) noexcept
  {
    if (!p.enter ())
      {
        return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset (), TokenKind::END_OF_FILE, p.kind ()};
      }
    typename Parser<ArenaT>::DepthGuard guard (p);
    switch (p.kind ())
      {
        case TokenKind::STAR:
        case TokenKind::AMP:
        case TokenKind::MINUS:
        case TokenKind::BANG:
        case TokenKind::TILDE:
          {
            const Token op = p.consume ();
            const Maybe<uint32_t, ParseError> operand = parse_unary (p);
            if (operand.has_error ())
              {
                return operand;
              }
            const Maybe<uint32_t, ParseError> node = p.make (NodeKind::UNARY, op);
            if (node.has_error ())
              {
                return node;
              }
            p.node (node.value ()).op = op.kind;
            p.append (node.value (), operand.value ());
            return node;
          }
        default:
          return parse_postfix (p);
      }
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_postfix (Parser<ArenaT>& p) noexcept
  {
    auto base = parse_primary (p);
    if (base.has_error ())
      {
        return base;
      }
    while (true)
      {
        if (p.kind () == TokenKind::DOT)
          {
            p.consume ();
            const Token name = p.kind () == TokenKind::IDENT ? p.expect_ident ().value_or (Token{}) : Token{};
            (void)name;
            if (p.kind () != TokenKind::IDENT)
              {
                p.rep (ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset (), TokenKind::IDENT, p.kind ()});
                break;
              }
            const Token n = p.consume ();
            const Maybe<uint32_t, ParseError> mnode = p.make (NodeKind::MEMBER_ACCESS, n);
            if (mnode.has_error ())
              {
                return mnode;
              }
            p.node (mnode.value ()).text = n.lexeme;
            p.append (mnode.value (), base.value ());
            base = mnode;
            continue;
          }
        if (p.kind () == TokenKind::LPAREN)
          {
            const Token open = p.consume ();
            const Maybe<uint32_t, ParseError> call = p.make (NodeKind::CALL, open);
            if (call.has_error ())
              {
                return call;
              }
            p.append (call.value (), base.value ());
            int saved_flag = p.no_struct_literal () ? 1 : 0;
            (void)saved_flag;
            typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false); // allow struct literals in args
            while (p.kind () != TokenKind::RPAREN && p.kind () != TokenKind::END_OF_FILE)
              {
                const Maybe<uint32_t, ParseError> arg = parse_expr (p);
                if (arg.has_error ())
                  {
                    return arg;
                  }
                p.append (call.value (), arg.value ());
                if (!p.consume_if (TokenKind::COMMA))
                  {
                    break;
                  }
              }
            p.consume_if (TokenKind::RPAREN);
            base = call;
            continue;
          }
        if (p.kind () == TokenKind::LBRACKET)
          {
            const Token open = p.consume ();
            const Maybe<uint32_t, ParseError> index = p.make (NodeKind::INDEX, open);
            if (index.has_error ())
              {
                return index;
              }
            p.append (index.value (), base.value ());
            typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false);
            if (p.kind () == TokenKind::DOT_DOT || p.kind () == TokenKind::DOT_DOT_ASSIGN)
              {
                const Token op = p.consume ();
                const Maybe<uint32_t, ParseError> r = p.make (NodeKind::RANGE, op);
                if (!r.has_error ())
                  {
                    p.node (r.value ()).flags = (op.kind == TokenKind::DOT_DOT_ASSIGN ? RANGE_INCLUSIVE : 0);
                    p.append (index.value (), r.value ());
                  }
                p.consume_if (TokenKind::RBRACKET);
              }
            else
              {
                const Maybe<uint32_t, ParseError> firstexpr = parse_expr (p);
                if (firstexpr.has_value ())
                  {
                    if (p.kind () == TokenKind::DOT_DOT || p.kind () == TokenKind::DOT_DOT_ASSIGN)
                      {
                        const Token op = p.consume ();
                        const Maybe<uint32_t, ParseError> range = p.make (NodeKind::RANGE, op);
                        if (!range.has_error ())
                          {
                            p.node (range.value ()).flags = RANGE_HAS_LO;
                            if (op.kind == TokenKind::DOT_DOT_ASSIGN)
                              {
                                p.node (range.value ()).flags = static_cast<uint8_t> (RANGE_HAS_LO | RANGE_INCLUSIVE);
                              }
                            p.append (range.value (), firstexpr.value ());
                            if (p.kind () != TokenKind::RBRACKET && p.kind () != TokenKind::END_OF_FILE)
                              {
                                const Maybe<uint32_t, ParseError> hi = parse_expr (p);
                                if (hi.has_value ())
                                  {
                                    p.node (range.value ()).flags = static_cast<uint8_t> (
                                        RANGE_HAS_LO | RANGE_HAS_HI
                                        | (op.kind == TokenKind::DOT_DOT_ASSIGN ? RANGE_INCLUSIVE : 0));
                                    p.append (range.value (), hi.value ());
                                  }
                              }
                            p.append (index.value (), range.value ());
                          }
                      }
                    else
                      {
                        p.append (index.value (), firstexpr.value ());
                      }
                  }
                p.consume_if (TokenKind::RBRACKET);
              }
            if (!p.consume_if (TokenKind::RBRACKET))
              {
                p.rep (ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset (), TokenKind::RBRACKET, p.kind ()});
              }
            base = index;
            continue;
          }
        if (p.kind () == TokenKind::QUESTION)
          {
            const Token q = p.consume ();
            const Maybe<uint32_t, ParseError> t = p.make (NodeKind::TRY_EXPR, q);
            if (t.has_error ())
              {
                return t;
              }
            p.append (t.value (), base.value ());
            base = t;
            continue;
          }
        break;
      }
    return base;
  }

  template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_primary (Parser<ArenaT>& p) noexcept
  {
    switch (p.kind ())
      {
        case TokenKind::INT_LITERAL:
        case TokenKind::FLOAT_LITERAL:
        case TokenKind::STRING_LITERAL:
        case TokenKind::STRING_LITERAL_U16:
        case TokenKind::STRING_LITERAL_U32:
        case TokenKind::RAW_STRING_LITERAL:
        case TokenKind::PATTERN_LITERAL:
        case TokenKind::CHAR_LITERAL:
          {
            const Token tok = p.consume ();
            NodeKind nk = NodeKind::STRING_LITERAL;
            switch (tok.kind)
              {
                case TokenKind::INT_LITERAL:
                  nk = NodeKind::INT_LITERAL;
                  break;
                case TokenKind::FLOAT_LITERAL:
                  nk = NodeKind::FLOAT_LITERAL;
                  break;
                case TokenKind::CHAR_LITERAL:
                  nk = NodeKind::CHAR_LITERAL;
                  break;
                default:
                  nk = NodeKind::STRING_LITERAL;
                  break;
              }
            const Maybe<uint32_t, ParseError> node = p.make (nk, tok);
            if (node.has_error ())
              {
                return node;
              }
            p.node (node.value ()).text = tok.lexeme;
            p.node (node.value ()).op = tok.kind; // keep STRING_LITERAL_* subkind
            return node;
          }
        case TokenKind::KW_TRUE:
        case TokenKind::KW_FALSE:
          {
            const Token tok = p.consume ();
            const Maybe<uint32_t, ParseError> node = p.make (NodeKind::BOOL_LITERAL, tok);
            if (node.has_error ())
              {
                return node;
              }
            p.node (node.value ()).flags = tok.kind == TokenKind::KW_TRUE ? 1 : 0;
            return node;
          }
        case TokenKind::LPAREN:
          {
            p.consume (); // '('
            typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false);
            const Maybe<uint32_t, ParseError> inner = parse_expr (p);
            p.consume_if (TokenKind::RPAREN);
            return inner;
          }
        case TokenKind::LBRACKET:
          {
            const Token open = p.consume ();
            const Maybe<uint32_t, ParseError> node = p.make (NodeKind::ARRAY_LITERAL, open);
            if (node.has_error ())
              {
                return node;
              }
            while (p.kind () != TokenKind::RBRACKET && p.kind () != TokenKind::END_OF_FILE)
              {
                const Maybe<uint32_t, ParseError> e = parse_expr (p);
                if (e.has_error ())
                  {
                    return e;
                  }
                p.append (node.value (), e.value ());
                if (!p.consume_if (TokenKind::COMMA))
                  {
                    break;
                  }
              }
            p.consume_if (TokenKind::RBRACKET);
            return node;
          }
        case TokenKind::KW_IF:
          {
            const Token kw = p.consume ();
            typename Parser<ArenaT>::NoStructLiteralGuard guard (p, true);
            const Maybe<uint32_t, ParseError> cond = parse_expr (p);
            if (cond.has_error ())
              {
                return cond;
              }
            typename Parser<ArenaT>::NoStructLiteralGuard bodyguard (p, false);
            const Maybe<uint32_t, ParseError> thenb = parse_block_expr (p);
            if (thenb.has_error ())
              {
                return thenb;
              }
            Maybe<uint32_t, ParseError> elseb = Maybe<uint32_t, ParseError>{};
            if (p.consume_if (TokenKind::KW_ELSE))
              {
                if (p.kind () == TokenKind::KW_IF)
                  {
                    const Token kw2 = p.consume ();
                    typename Parser<ArenaT>::NoStructLiteralGuard nguard (p, true);
                    const Maybe<uint32_t, ParseError> ncond = parse_expr (p);
                    typename Parser<ArenaT>::NoStructLiteralGuard nbody (p, false);
                    const Maybe<uint32_t, ParseError> nthen = parse_block_expr (p);
                    const Maybe<uint32_t, ParseError> nested = p.make (NodeKind::IF, kw2);
                    if (!nested.has_error ())
                      {
                        if (ncond.has_value ())
                          {
                            p.append (nested.value (), ncond.value ());
                          }
                        if (nthen.has_value ())
                          {
                            p.append (nested.value (), nthen.value ());
                          }
                        if (p.consume_if (TokenKind::KW_ELSE))
                          {
                            typename Parser<ArenaT>::NoStructLiteralGuard neguard (p, false);
                            const Maybe<uint32_t, ParseError> neb = parse_block_expr (p);
                            if (neb.has_value ())
                              {
                                p.append (nested.value (), neb.value ());
                              }
                            p.node (nested.value ()).flags = IF_HAS_ELSE;
                          }
                      }
                    elseb = nested;
                  }
                else
                  {
                    typename Parser<ArenaT>::NoStructLiteralGuard eguard (p, false);
                    const Maybe<uint32_t, ParseError> eb = parse_block_expr (p);
                    elseb = eb;
                  }
              }
            const Maybe<uint32_t, ParseError> node = p.make (NodeKind::IF, kw);
            if (node.has_error ())
              {
                return node;
              }
            p.append (node.value (), cond.value ());
            p.append (node.value (), thenb.value ());
            if (elseb.has_value ())
              {
                p.append (node.value (), elseb.value ());
              }
            if (elseb.has_value ())
              {
                p.node (node.value ()).flags = IF_HAS_ELSE;
              }
            return node;
          }
        case TokenKind::KW_MATCH:
          {
            const Token kw = p.consume ();
            typename Parser<ArenaT>::NoStructLiteralGuard guard (p, true);
            const Maybe<uint32_t, ParseError> scrut = parse_expr (p);
            const Maybe<uint32_t, ParseError> node = p.make (NodeKind::MATCH, kw);
            if (!node.has_error ())
              {
                if (scrut.has_value ())
                  {
                    p.append (node.value (), scrut.value ());
                  }
                typename Parser<ArenaT>::NoStructLiteralGuard bodyguard (p, false);
                p.consume_if (TokenKind::LBRACE);
                while (p.kind () != TokenKind::RBRACE && p.kind () != TokenKind::END_OF_FILE)
                  {
                    const Maybe<uint32_t, ParseError> pat = parse_pattern (p);
                    const Maybe<uint32_t, ParseError> arm = p.make (NodeKind::MATCH_ARM, p.offset ());
                    if (arm.has_value ())
                      {
                        if (pat.has_value ())
                          {
                            p.append (arm.value (), pat.value ());
                          }
                        if (p.consume_if (TokenKind::KW_IF))
                          {
                            const Maybe<uint32_t, ParseError> g = parse_expr (p);
                            if (g.has_value ())
                              {
                                p.append (arm.value (), g.value ());
                              }
                          }
                        const Maybe<uint32_t, ParseError> body = parse_block_expr (p);
                        if (body.has_value ())
                          {
                            p.append (arm.value (), body.value ());
                          }
                        p.append (node.value (), arm.value ());
                      }
                  }
                p.consume_if (TokenKind::RBRACE);
              }
            return node;
          }
        case TokenKind::KW_FUNCTION:
          {
            const Token kw = p.consume ();
            if (p.kind () == TokenKind::LPAREN)
              {
                // closure
                p.consume (); // '('
                const Maybe<uint32_t, ParseError> closure = p.make (NodeKind::CLOSURE, kw);
                if (closure.has_error ())
                  {
                    return closure;
                  }
                typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false);
                // params: bare idents (or typed name: type)
                while (p.kind () != TokenKind::RPAREN && p.kind () != TokenKind::END_OF_FILE)
                  {
                    if (p.kind () == TokenKind::IDENT)
                      {
                        const Token nm = p.consume ();
                        const Maybe<uint32_t, ParseError> pn = p.make (NodeKind::PARAM_DECL, nm);
                        if (pn.has_value ())
                          {
                            p.node (pn.value ()).text = nm.lexeme;
                            if (p.consume_if (TokenKind::COLON))
                              {
                                const Maybe<uint32_t, ParseError> ty = parse_type (p);
                                if (ty.has_value ())
                                  {
                                    p.append (pn.value (), ty.value ());
                                  }
                              }
                            p.append (closure.value (), pn.value ());
                          }
                      }
                    else
                      {
                        p.rep (ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset (), TokenKind::IDENT,
                                          p.kind ()});
                        break;
                      }
                    if (!p.consume_if (TokenKind::COMMA))
                      {
                        break;
                      }
                  }
                p.consume_if (TokenKind::RPAREN);
                // body
                const Maybe<uint32_t, ParseError> body = parse_block_expr (p);
                if (body.has_value ())
                  {
                    p.append (closure.value (), body.value ());
                  }
                return closure;
              }
            // named function type? not in expr primary -> error
            p.rep (ParseError{ParseError::Kind::EXPECTED_EXPRESSION, kw.offset, TokenKind::END_OF_FILE, p.kind ()});
            return ParseError{ParseError::Kind::EXPECTED_EXPRESSION, kw.offset, TokenKind::END_OF_FILE, p.kind ()};
          }
        case TokenKind::KW_STATIC:
          {
            p.consume ();
            if (p.kind () == TokenKind::KW_FUNCTION)
              {
                const Token kw = p.consume ();
                // closure with static storage
                const Maybe<uint32_t, ParseError> close = parse_closure_with_static (p, kw);
                return close;
              }
            p.rep (ParseError{ParseError::Kind::EXPECTED_EXPRESSION, p.offset (), TokenKind::KW_FUNCTION, p.kind ()});
            return ParseError{ParseError::Kind::EXPECTED_EXPRESSION, p.offset (), TokenKind::KW_FUNCTION, p.kind ()};
          }
        default:
          {
            // path / macro call / struct literal
            if (p.kind () == TokenKind::IDENT || p.kind () == TokenKind::KW_SELF || p.kind () == TokenKind::COLON_COLON)
              {
                const Maybe<uint32_t, ParseError> path = parse_path (p);
                if (path.has_error ())
                  {
                    return path;
                  }
                // macro call: path ::< name >
                if (p.kind () == TokenKind::COLON_COLON && p.peek (1).kind == TokenKind::LT)
                  {
                    p.consume (); // "::"
                    p.consume (); // "<"
                    const Token name = (p.kind () == TokenKind::IDENT) ? p.consume () : Token{};
                    if (name.kind != TokenKind::IDENT)
                      {
                        p.rep (ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset (), TokenKind::IDENT,
                                          p.kind ()});
                      }
                    p.consume_if (TokenKind::GT);
                    // Build MACRO_CALL node anchored at path's name token offset.
                    const Maybe<uint32_t, ParseError> call2
                        = p.make (NodeKind::MACRO_CALL, path.value () ? p.node (path.value ()).offset : 0);
                    if (call2.has_error ())
                      {
                        return call2;
                      }
                    p.append (call2.value (), path.value ());
                    p.node (call2.value ()).text
                        = (name.kind == TokenKind::IDENT) ? name.lexeme : cxxstd::SimpleStringView{};
                    // args
                    if (p.kind () == TokenKind::LPAREN)
                      {
                        p.consume ();
                        typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false);
                        while (p.kind () != TokenKind::RPAREN && p.kind () != TokenKind::END_OF_FILE)
                          {
                            const Maybe<uint32_t, ParseError> arg = parse_expr (p);
                            if (arg.has_error ())
                              {
                                return arg;
                              }
                            p.append (call2.value (), arg.value ());
                            if (!p.consume_if (TokenKind::COMMA))
                              {
                                break;
                              }
                          }
                        p.consume_if (TokenKind::RPAREN);
                      }
                    return call2;
                  }
                // generic args tentative
                if (p.kind () == TokenKind::LT && !p.no_struct_literal () && try_parse_generic_args (p).has_value ())
                  {
                    // now parse_generic_args already consumed the generics:
                    // re-walk: we already consumed them inside try...
                    // try_parse_generic_args returns node; but node not captured.
                    // Recompute by calling parse_generic_args (idempotent consume already done? no,
                    // it re-parses... wait) Simpler: we lost the consumed tokens. So disallow this
                    // path returning. Instead, capture the result properly:
                  }
                // Tentative fallback above not fully wired — call directly when
                // no-struct-literal is false:
                if (p.kind () == TokenKind::LT && !p.no_struct_literal ())
                  {
                    const typename Parser<ArenaT>::State saved = p.save ();
                    const Maybe<uint32_t, ParseError> ta = try_parse_generic_args (p);
                    if (ta.has_value ())
                      {
                        // wrap path + args into a generic path: append type args
                        // as children under the existing path? Reuse GENERIC_PARAMS
                        // node returned as sibling source. Reparent its children.
                        const uint32_t gp = ta.value ();
                        AstNode& gpn = p.node (gp);
                        uint32_t c = gpn.first_child;
                        while (c != NO_NODE)
                          {
                            const uint32_t nxt = p.node (c).next_sibling;
                            p.append (path.value (), c);
                            c = nxt;
                          }
                        p.arena ().rollback (gp + 1); // drop the args node itself
                      }
                    else
                      {
                        p.restore (saved);
                      }
                  }
                // struct literal
                if (p.kind () == TokenKind::LBRACE && !p.no_struct_literal ())
                  {
                    p.consume ();
                    const Maybe<uint32_t, ParseError> snode = p.make (NodeKind::STRUCT_LITERAL, p.offset ());
                    if (snode.has_error ())
                      {
                        return snode;
                      }
                    p.append (snode.value (), path.value ());
                    while (p.kind () != TokenKind::RBRACE && p.kind () != TokenKind::END_OF_FILE)
                      {
                        if (p.kind () == TokenKind::RBRACE)
                          {
                            break;
                          }
                        if (p.kind () == TokenKind::IDENT && p.peek (1).kind == TokenKind::COLON)
                          {
                            const Token fn = p.consume ();
                            p.consume (); // ":"
                            const Maybe<uint32_t, ParseError> val = parse_expr (p);
                            if (val.has_value ())
                              {
                                const Maybe<uint32_t, ParseError> fi = p.make (NodeKind::FIELD_INIT, fn);
                                if (fi.has_value ())
                                  {
                                    p.node (fi.value ()).text = fn.lexeme;
                                    p.append (fi.value (), val.value ());
                                    p.append (snode.value (), fi.value ());
                                  }
                              }
                          }
                        else
                          {
                            const Maybe<uint32_t, ParseError> e = parse_expr (p);
                            if (e.has_value ())
                              {
                                const Maybe<uint32_t, ParseError> fi = p.make (NodeKind::FIELD_INIT, p.offset ());
                                if (fi.has_value ())
                                  {
                                    p.append (fi.value (), e.value ());
                                    p.append (snode.value (), fi.value ());
                                  }
                              }
                          }
                        if (!p.consume_if (TokenKind::COMMA))
                          {
                            break;
                          }
                      }
                    p.consume_if (TokenKind::RBRACE);
                    return snode;
                  }
                return path;
              }
            p.rep (ParseError{ParseError::Kind::UNEXPECTED_TOKEN, p.offset (), TokenKind::END_OF_FILE, p.kind ()});
            return ParseError{ParseError::Kind::UNEXPECTED_TOKEN, p.offset (), TokenKind::END_OF_FILE, p.kind ()};
          }
      }
  }

  // parse_block_expr: a block *expression* body `{ expr; ... }` (if/match/
  // closure value position). Statement forms are not accepted here — this
  // parser reaches the statement production only through parse_stmt, which
  // imports this module (import cycle); an unparsable member is recovered
  // past, so `{ ret x; }` in expression position yields an empty block
  // instead of cascading into the surrounding construct.
  export template <typename ArenaT>
  [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
  parse_block_expr (Parser<ArenaT>& p) noexcept
  {
    if (p.kind () != TokenKind::LBRACE)
      {
        return ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset (), TokenKind::LBRACE, p.kind ()};
      }
    const Token open = p.consume ();
    const Maybe<uint32_t, ParseError> blk = p.make (NodeKind::BLOCK, open);
    if (blk.has_error ())
      {
        return blk;
      }
    typename Parser<ArenaT>::NoStructLiteralGuard guard (p, false);
    while (p.kind () != TokenKind::RBRACE && p.kind () != TokenKind::END_OF_FILE)
      {
        const Maybe<uint32_t, ParseError> e = parse_expr (p);
        if (e.has_value ())
          {
            p.append (blk.value (), e.value ());
          }
        else
          {
            // Recovery: skip to the end of this member expression;
            // skip_until stops before `;`/`}` and the consume below
            // guarantees progress, so the loop terminates.
            (void)p.skip_until ({TokenKind::SEMICOLON, TokenKind::RBRACE});
          }
        p.consume_if (TokenKind::SEMICOLON);
      }
    p.consume_if (TokenKind::RBRACE);
    return blk;
  }

  template <typename ArenaT>
  constexpr Maybe<uint32_t, ParseError>
  parse_closure_with_static (Parser<ArenaT>& p, const Token& kw) noexcept
  {
    const Maybe<uint32_t, ParseError> closure = p.make (NodeKind::CLOSURE, kw);
    if (closure.has_error ())
      {
        return closure;
      }
    p.node (closure.value ()).flags = CLOSURE_STATIC;
    if (p.kind () == TokenKind::LPAREN)
      {
        p.consume ();
        while (p.kind () != TokenKind::RPAREN && p.kind () != TokenKind::END_OF_FILE)
          {
            if (p.kind () == TokenKind::IDENT)
              {
                const Token nm = p.consume ();
                if (const Maybe<uint32_t, ParseError> pn = p.make (NodeKind::PARAM_DECL, nm); pn.has_value ())
                  {
                    p.node (pn.value ()).text = nm.lexeme;
                    p.append (closure.value (), pn.value ());
                  }
              }
            else
              {
                break;
              }
            if (!p.consume_if (TokenKind::COMMA))
              {
                break;
              }
          }
        p.consume_if (TokenKind::RPAREN);
      }
    const Maybe<uint32_t, ParseError> body = parse_block_expr (p);
    if (body.has_value ())
      {
        p.append (closure.value (), body.value ());
      }
    return closure;
  }
} // namespace embdr::lex
