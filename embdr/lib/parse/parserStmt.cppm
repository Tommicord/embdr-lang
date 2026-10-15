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

export module embdr.lex.parserStmt;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserType;
import embdr.lex.parserExpr;
import embdr.lex.parserPattern;
import embdr.lex.token;

namespace embdr::lex
{
    using cxxstd::Maybe;

    // ---- Block ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_block(Parser<ArenaT>& p) noexcept
    {
        const Maybe<Token, ParseError> open = p.expect(TokenKind::LBRACE);
        if (open.has_error())
            {
                return open.error();
            }
        const Maybe<uint32_t, ParseError> blk = p.make(NodeKind::BLOCK, open.value());
        if (blk.has_error())
            {
                return blk;
            }
        typename Parser<ArenaT>::NoStructLiteralGuard guard(p, false);
        while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
            {
                const Maybe<uint32_t, ParseError> st = parse_stmt(p);
                if (st.has_value())
                    {
                        p.append(blk.value(), st.value());
                    }
                if (p.kind() == TokenKind::RBRACE)
                    {
                        break;
                    }
                if (p.kind() == TokenKind::END_OF_FILE)
                    {
                        break;
                    }
                if (st.has_error())
                    {
                        p.skip_until({TokenKind::SEMICOLON, TokenKind::RBRACE});
                        p.consume_if(TokenKind::SEMICOLON);
                    }
            }
        p.consume_if(TokenKind::RBRACE);
        return blk;
    }

    // ---- Statement ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_stmt(Parser<ArenaT>& p) noexcept
    {
        if (!p.enter())
            {
                return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset(), TokenKind::END_OF_FILE, p.kind()};
            }
        typename Parser<ArenaT>::DepthGuard guard(p);
        // attributes
        // simple attribute list: if HASH, parse attr nodes (reuse parserDecl style
        // by exporting helper? inline minimal).
        Maybe<uint32_t, ParseError> attr_node = Maybe<uint32_t, ParseError>{};
        uint32_t attr_count = 0;
        while (p.kind() == TokenKind::HASH)
            {
                const Token hash = p.consume();
                if (p.kind() != TokenKind::IDENT)
                    {
                        p.rep(
                            ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset(), TokenKind::IDENT, p.kind()});
                        break;
                    }
                const Token nm = p.consume();
                Maybe<uint32_t, ParseError> attr = p.make(NodeKind::ATTRIBUTE, hash);
                p.node(attr.value()).text = nm.lexeme;
                if (p.consume_if(TokenKind::LPAREN))
                    {
                        while (p.kind() != TokenKind::RPAREN && p.kind() != TokenKind::END_OF_FILE)
                            {
                                if (p.kind() == TokenKind::INT_LITERAL || p.kind() == TokenKind::FLOAT_LITERAL ||
                                    p.kind() == TokenKind::STRING_LITERAL || p.kind() == TokenKind::CHAR_LITERAL)
                                    {
                                        const Token arg = p.consume();
                                        const Maybe<uint32_t, ParseError> an = p.make(NodeKind::INT_LITERAL, arg);
                                        p.node(an.value()).text = arg.lexeme;
                                        p.node(an.value()).op = arg.kind;
                                        p.append(attr.value(), an.value());
                                    }
                                else if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token arg = p.consume();
                                        const Maybe<uint32_t, ParseError> an = p.make(NodeKind::PATH, arg);
                                        p.node(an.value()).text = arg.lexeme;
                                        p.append(attr.value(), an.value());
                                    }
                                else
                                    {
                                        p.rep(ParseError{ParseError::Kind::EXPECTED_EXPRESSION, p.offset(),
                                                         TokenKind::IDENT, p.kind()});
                                        break;
                                    }
                                if (!p.consume_if(TokenKind::COMMA))
                                    {
                                        break;
                                    }
                            }
                        p.consume_if(TokenKind::RPAREN);
                    }
                if (attr_count == 0)
                    {
                        attr_node = attr;
                    }
                else if (attr_node.has_value())
                    {
                        // attach via wrapper? just append as sibling via parent later not here.
                    }
                if (attr_node.has_value() && attr_count > 0)
                    {
                        // chain attributes by reparenting not trivial; skip beyond 1
                    }
                ++attr_count;
                (void)attr;
            }
        // KW_PUB loop not needed in statements

        // actual dispatch
        Maybe<uint32_t, ParseError> out = Maybe<uint32_t, ParseError>{};

        switch (p.kind())
            {
                case TokenKind::SEMICOLON:
                    p.consume();
                    return Maybe<uint32_t, ParseError>{};

                case TokenKind::LBRACE:
                    return parse_block(p);

                case TokenKind::KW_LET:
                    {
                        const bool cexpr_flag = p.consume_if(TokenKind::KW_CEXPR);
                        (void)cexpr_flag;
                        if (!p.consume_if(TokenKind::KW_LET))
                            {
                                // typed or let-style: fall through
                            }
                        bool seen_let = false;
                        (void)seen_let;
                        // If we consumed CEXPR then KW_LET already done. If cexpr was the
                        // only keyword and not followed by let (e.g. `cexpr int32 a`),
                        // we still parse it as local decl.
                        const Maybe<uint32_t, ParseError> decl = p.make(NodeKind::LOCAL_DECL, p.offset());
                        if (decl.has_error())
                            {
                                return decl.error();
                            }
                        uint8_t flags = cexpr_flag ? LOCAL_NO_TYPE : 0; // cexpr flag stored via CEXPR? NodeFlags CEXPR
                        p.node(decl.value()).flags = cexpr_flag ? static_cast<uint8_t>(NodeFlags::CEXPR) : 0;
                        // optional name / type
                        // typed form: type IDENT ...
                        // tentative: is next a type?
                        if (is_type_start(p.kind()))
                            {
                                const typename Parser<ArenaT>::State saved = p.save();
                                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                if (ty.has_value() && p.kind() == TokenKind::IDENT)
                                    {
                                        const Token name = p.consume();
                                        p.node(decl.value()).text = name.lexeme;
                                        p.append(decl.value(), ty.value());
                                        if (p.consume_if(TokenKind::EQ))
                                            {
                                                const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                if (init.has_value())
                                                    {
                                                        p.append(decl.value(), init.value());
                                                    }
                                            }
                                        p.expect_semi();
                                        out = decl;
                                    }
                                else
                                    {
                                        p.restore(saved);
                                        // untyped: name = expr
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                const Token name = p.consume();
                                                p.node(decl.value()).text = name.lexeme;
                                                p.node(decl.value()).flags =
                                                    static_cast<uint8_t>(p.node(decl.value()).flags | LOCAL_NO_TYPE);
                                                if (p.consume_if(TokenKind::EQ))
                                                    {
                                                        const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                        if (init.has_value())
                                                            {
                                                                p.append(decl.value(), init.value());
                                                            }
                                                    }
                                                p.expect_semi();
                                                out = decl;
                                            }
                                    }
                            }
                        else if (p.kind() == TokenKind::IDENT)
                            {
                                const Token name = p.consume();
                                p.node(decl.value()).text = name.lexeme;
                                p.node(decl.value()).flags =
                                    static_cast<uint8_t>(p.node(decl.value()).flags | LOCAL_NO_TYPE);
                                if (p.consume_if(TokenKind::EQ))
                                    {
                                        const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                        if (init.has_value())
                                            {
                                                p.append(decl.value(), init.value());
                                            }
                                    }
                                p.expect_semi();
                                out = decl;
                            }
                        (void)flags;
                        break;
                    }

                case TokenKind::KW_IF:
                    {
                        const Token kw = p.consume();
                        typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, true);
                        const Maybe<uint32_t, ParseError> cond = parse_expr(p);
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::IF, kw);
                        if (!node.has_error())
                            {
                                if (cond.has_value())
                                    {
                                        p.append(node.value(), cond.value());
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard bodyguard(p, false);
                                const Maybe<uint32_t, ParseError> thenb = parse_block(p);
                                if (thenb.has_value())
                                    {
                                        p.append(node.value(), thenb.value());
                                    }
                                if (p.consume_if(TokenKind::KW_ELSE))
                                    {
                                        if (p.kind() == TokenKind::KW_IF)
                                            {
                                                const Maybe<uint32_t, ParseError> nested = parse_stmt(p);
                                                if (nested.has_value())
                                                    {
                                                        p.append(node.value(), nested.value());
                                                    }
                                            }
                                        else
                                            {
                                                const Maybe<uint32_t, ParseError> eb = parse_block(p);
                                                if (eb.has_value())
                                                    {
                                                        p.append(node.value(), eb.value());
                                                    }
                                            }
                                        p.node(node.value()).flags = IF_HAS_ELSE;
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_WHILE:
                    {
                        const Token kw = p.consume();
                        typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, true);
                        const Maybe<uint32_t, ParseError> cond = parse_expr(p);
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::WHILE, kw);
                        if (!node.has_error())
                            {
                                if (cond.has_value())
                                    {
                                        p.append(node.value(), cond.value());
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard guard2(p, false);
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                if (body.has_value())
                                    {
                                        p.append(node.value(), body.value());
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_LOOP:
                    {
                        const Token kw = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::LOOP, kw);
                        if (!node.has_error())
                            {
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                if (body.has_value())
                                    {
                                        p.append(node.value(), body.value());
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_FOR:
                    {
                        const Token kw = p.consume();
                        bool cexpr_flag = false;
                        (void)cexpr_flag;
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::FOR, kw);
                        // parse bindings: `id` or `idx, id`
                        if (!node.has_error())
                            {
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token first = p.consume();
                                        const Maybe<uint32_t, ParseError> bnode = p.make(NodeKind::PATH, first);
                                        p.node(bnode.value()).text = first.lexeme;
                                        p.append(node.value(), bnode.value());
                                        if (p.consume_if(TokenKind::COMMA))
                                            {
                                                p.node(node.value()).flags = FOR_HAS_INDEX;
                                                if (p.kind() == TokenKind::IDENT)
                                                    {
                                                        const Token second = p.consume();
                                                        const Maybe<uint32_t, ParseError> bnode2 =
                                                            p.make(NodeKind::PATH, second);
                                                        p.node(bnode2.value()).text = second.lexeme;
                                                        p.append(node.value(), bnode2.value());
                                                    }
                                            }
                                    }
                                // `in`
                                if (p.kind() == TokenKind::KW_IN)
                                    {
                                        p.consume();
                                    }
                                else
                                    {
                                        p.rep(ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(), TokenKind::KW_IN,
                                                         p.kind()});
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, true);
                                const Maybe<uint32_t, ParseError> iter = parse_expr(p);
                                if (iter.has_value())
                                    {
                                        p.append(node.value(), iter.value());
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard bodyguard(p, false);
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                if (body.has_value())
                                    {
                                        p.append(node.value(), body.value());
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_CEXPR:
                    {
                        // `cexpr for ...` handle; else fall through to local decl parsing
                        p.consume();
                        if (p.kind() == TokenKind::KW_FOR)
                            {
                                const Maybe<uint32_t, ParseError> node = p.make(NodeKind::FOR, p.offset());
                                if (!node.has_error())
                                    {
                                        p.node(node.value()).flags = FOR_CEXPR;
                                        // reuse FOR parsing from above by manual re-consume:
                                        // For simplicity, rewind? Can't. Instead replicate minimal:
                                        // consume already? We consumed KW_CEXPR, now KW_FOR is current.
                                        p.consume(); // FOR
                                        // bindings
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                const Token first = p.consume();
                                                const Maybe<uint32_t, ParseError> bnode = p.make(NodeKind::PATH, first);
                                                p.node(bnode.value()).text = first.lexeme;
                                                p.append(node.value(), bnode.value());
                                                if (p.consume_if(TokenKind::COMMA))
                                                    {
                                                        p.node(node.value()).flags = static_cast<uint8_t>(
                                                            p.node(node.value()).flags | FOR_HAS_INDEX);
                                                        if (p.kind() == TokenKind::IDENT)
                                                            {
                                                                const Token second = p.consume();
                                                                const Maybe<uint32_t, ParseError> bnode2 =
                                                                    p.make(NodeKind::PATH, second);
                                                                p.node(bnode2.value()).text = second.lexeme;
                                                                p.append(node.value(), bnode2.value());
                                                            }
                                                    }
                                            }
                                        if (p.kind() == TokenKind::KW_IN)
                                            {
                                                p.consume();
                                            }
                                        typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, true);
                                        const Maybe<uint32_t, ParseError> iter = parse_expr(p);
                                        if (iter.has_value())
                                            {
                                                p.append(node.value(), iter.value());
                                            }
                                        typename Parser<ArenaT>::NoStructLiteralGuard guard2(p, false);
                                        const Maybe<uint32_t, ParseError> body = parse_block(p);
                                        if (body.has_value())
                                            {
                                                p.append(node.value(), body.value());
                                            }
                                    }
                                out = node;
                                break;
                            }
                        // fall back to local decl with cexpr: restore? We already consumed.
                        // Parse untyped/t kh typ ed binding.
                        const Maybe<uint32_t, ParseError> decl = p.make(NodeKind::LOCAL_DECL, p.offset());
                        if (!decl.has_error())
                            {
                                p.node(decl.value()).flags = static_cast<uint8_t>(NodeFlags::CEXPR);
                                if (p.kind() == TokenKind::IDENT && is_type_start(p.kind()))
                                    {
                                    }
                                if (is_type_start(p.kind()))
                                    {
                                        const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                        if (ty.has_value() && p.kind() == TokenKind::IDENT)
                                            {
                                                const Token name = p.consume();
                                                p.node(decl.value()).text = name.lexeme;
                                                p.append(decl.value(), ty.value());
                                                if (p.consume_if(TokenKind::EQ))
                                                    {
                                                        const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                        if (init.has_value())
                                                            {
                                                                p.append(decl.value(), init.value());
                                                            }
                                                    }
                                                p.expect_semi();
                                                out = decl;
                                                break;
                                            }
                                    }
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token name = p.consume();
                                        p.node(decl.value()).text = name.lexeme;
                                        p.node(decl.value()).flags =
                                            static_cast<uint8_t>(p.node(decl.value()).flags | LOCAL_NO_TYPE);
                                        if (p.consume_if(TokenKind::EQ))
                                            {
                                                if (const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                    init.has_value())
                                                    {
                                                        p.append(decl.value(), init.value());
                                                    }
                                            }
                                        p.expect_semi();
                                        out = decl;
                                        break;
                                    }
                            }
                        out = decl;
                        break;
                    }

                case TokenKind::KW_MATCH:
                    {
                        const Token kw = p.consume();
                        typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, true);
                        const Maybe<uint32_t, ParseError> scrut = parse_expr(p);
                        const Maybe<Token, ParseError> open = p.expect(TokenKind::LBRACE);
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::MATCH, kw);
                        if (!node.has_error())
                            {
                                if (scrut.has_value())
                                    {
                                        p.append(node.value(), scrut.value());
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard bodyguard(p, false);
                                while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                    {
                                        const Maybe<uint32_t, ParseError> arm = p.make(NodeKind::MATCH_ARM, p.offset());
                                        if (!arm.has_error())
                                            {
                                                const Maybe<uint32_t, ParseError> pat = parse_pattern(p);
                                                if (pat.has_value())
                                                    {
                                                        p.append(arm.value(), pat.value());
                                                    }
                                                // guard
                                                if (p.consume_if(TokenKind::KW_IF))
                                                    {
                                                        const Maybe<uint32_t, ParseError> g = parse_expr(p);
                                                        if (g.has_value())
                                                            {
                                                                p.append(arm.value(), g.value());
                                                            }
                                                    }
                                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                                if (body.has_value())
                                                    {
                                                        p.append(arm.value(), body.value());
                                                    }
                                                p.append(node.value(), arm.value());
                                            }
                                        if (p.kind() == TokenKind::COMMA)
                                            {
                                                p.consume();
                                            }
                                    }
                                p.consume_if(TokenKind::RBRACE);
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_RET:
                    {
                        const Token kw = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::RET, kw);
                        if (!node.has_error())
                            {
                                if (p.kind() != TokenKind::SEMICOLON && p.kind() != TokenKind::RBRACE)
                                    {
                                        const Maybe<uint32_t, ParseError> e = parse_expr(p);
                                        if (e.has_value())
                                            {
                                                p.append(node.value(), e.value());
                                            }
                                    }
                            }
                        p.expect_semi();
                        out = node;
                        break;
                    }

                case TokenKind::KW_BREAK:
                    {
                        const Token kw = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::BREAK, kw);
                        if (!node.has_error())
                            {
                                if (p.consume_if(TokenKind::COLON))
                                    {
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                const Token lab = p.consume();
                                                const Maybe<uint32_t, ParseError> pn = p.make(NodeKind::PATH, lab);
                                                if (pn.has_value())
                                                    {
                                                        p.node(pn.value()).text = lab.lexeme;
                                                        p.append(node.value(), pn.value());
                                                    }
                                            }
                                    }
                                else if (p.kind() != TokenKind::SEMICOLON && p.kind() != TokenKind::RBRACE)
                                    {
                                        const Maybe<uint32_t, ParseError> e = parse_expr(p);
                                        if (e.has_value())
                                            {
                                                p.append(node.value(), e.value());
                                            }
                                    }
                            }
                        p.expect_semi();
                        out = node;
                        break;
                    }

                case TokenKind::KW_CONTINUE:
                    {
                        const Token kw = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::CONTINUE, kw);
                        p.expect_semi();
                        out = node;
                        break;
                    }

                case TokenKind::KW_DEFER:
                    {
                        const Token kw = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::DEFER, kw);
                        if (!node.has_error())
                            {
                                if (p.kind() == TokenKind::LBRACE)
                                    {
                                        const Maybe<uint32_t, ParseError> b = parse_block(p);
                                        if (b.has_value())
                                            {
                                                p.append(node.value(), b.value());
                                            }
                                    }
                                else
                                    {
                                        const Maybe<uint32_t, ParseError> e = parse_expr(p);
                                        if (e.has_value())
                                            {
                                                p.append(node.value(), e.value());
                                            }
                                        p.expect_semi();
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::COLON:
                    {
                        p.consume(); // ":"
                        const Maybe<Token, ParseError> lab = p.expect_ident();
                        const Maybe<uint32_t, ParseError> node =
                            p.make(NodeKind::LABEL,
                                   lab.has_value() ? lab.value() : Token{TokenKind::END_OF_FILE, {}, p.offset()});
                        if (!node.has_error() && lab.has_value())
                            {
                                p.node(node.value()).text = lab.value().lexeme;
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                if (body.has_value())
                                    {
                                        p.append(node.value(), body.value());
                                    }
                            }
                        out = node;
                        break;
                    }

                case TokenKind::KW_VOLATILE:
                    {
                        const Token kw = p.consume();
                        if (p.consume_if(TokenKind::LPAREN))
                            {
                                // volatile(asm) or volatile(ident)
                                const Token kindtok = (p.kind() == TokenKind::IDENT) ? p.consume() : Token{};
                                const Maybe<Token, ParseError> close = p.expect(TokenKind::RPAREN);
                                (void)close;
                                // ASM block
                                if (kindtok.kind == TokenKind::IDENT &&
                                    kindtok.lexeme == cxxstd::SimpleStringView("asm"))
                                    {
                                        const Maybe<Token, ParseError> open = p.expect(TokenKind::LBRACE);
                                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::ASM_BLOCK, kw);
                                        typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, false);
                                        while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                            {
                                                if (p.kind() == TokenKind::IDENT)
                                                    {
                                                        const Token mnemonic = p.consume();
                                                        const Maybe<uint32_t, ParseError> instr =
                                                            p.make(NodeKind::ASM_INSTR, mnemonic);
                                                        p.node(instr.value()).text = mnemonic.lexeme;
                                                        while (p.kind() != TokenKind::RBRACE &&
                                                               p.kind() != TokenKind::END_OF_FILE && false)
                                                            {
                                                                const Token op = p.consume();
                                                                const Maybe<uint32_t, ParseError> on =
                                                                    p.make(NodeKind::PATH, op);
                                                                p.node(on.value()).text = op.lexeme;
                                                                p.append(instr.value(), on.value());
                                                            }
                                                        p.append(node.value(), instr.value());
                                                    }
                                                else
                                                    {
                                                        p.consume();
                                                    }
                                            }
                                        p.consume_if(TokenKind::RBRACE);
                                        out = node;
                                        break;
                                    }
                                // other volatile(...) — treat as block with following brace
                                (void)kindtok;
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                const Maybe<uint32_t, ParseError> node = p.make(NodeKind::VOLATILE_BLOCK, kw);
                                typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, false);
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                if (body.has_value() && !node.has_error())
                                    {
                                        p.append(node.value(), body.value());
                                    }
                                out = node;
                                break;
                            }
                        // volatile static ...
                        if (p.kind() == TokenKind::KW_STATIC)
                            {
                                // parse local static decl by falling through to static handling
                            }
                        if (out.has_error())
                            {
                                out = p.make(NodeKind::VOLATILE_BLOCK, kw);
                            }
                        break;
                    }

                case TokenKind::KW_STATIC:
                    {
                        const Token kw = p.consume();
                        // static decl (local): optional type then IDENT = init?;
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::LOCAL_DECL, kw);
                        if (!node.has_error())
                            {
                                p.node(node.value()).flags = LOCAL_STATIC;
                                if (is_type_start(p.kind()))
                                    {
                                        const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                        if (ty.has_value() && p.kind() == TokenKind::IDENT)
                                            {
                                                const Token name = p.consume();
                                                p.node(node.value()).text = name.lexeme;
                                                p.append(node.value(), ty.value());
                                                if (p.consume_if(TokenKind::EQ))
                                                    {
                                                        const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                        if (init.has_value())
                                                            {
                                                                p.append(node.value(), init.value());
                                                            }
                                                    }
                                                p.expect_semi();
                                                out = node;
                                                break;
                                            }
                                    }
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token name = p.consume();
                                        p.node(node.value()).text = name.lexeme;
                                        p.node(node.value()).flags =
                                            static_cast<uint8_t>(p.node(node.value()).flags | LOCAL_NO_TYPE);
                                        if (p.consume_if(TokenKind::EQ))
                                            {
                                                const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                if (init.has_value())
                                                    {
                                                        p.append(node.value(), init.value());
                                                    }
                                            }
                                        p.expect_semi();
                                        out = node;
                                        break;
                                    }
                            }
                        p.rep(
                            ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset(), TokenKind::IDENT, p.kind()});
                        out = node;
                        break;
                    }

                default:
                    {
                        // attributes already handled? If HASH passed (handled above loop
                        // but not reattached) — nothing here attaches attr_node.
                        // (Local decl vs expr)
                        const typename Parser<ArenaT>::State saved = p.save();
                        if (is_type_start(p.kind()))
                            {
                                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                if (ty.has_value() && p.kind() == TokenKind::IDENT)
                                    {
                                        const Token name = p.consume();
                                        const Maybe<uint32_t, ParseError> decl = p.make(NodeKind::LOCAL_DECL, name);
                                        if (!decl.has_error())
                                            {
                                                p.node(decl.value()).text = name.lexeme;
                                                p.append(decl.value(), ty.value());
                                                if (p.consume_if(TokenKind::EQ))
                                                    {
                                                        const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                                        if (init.has_value())
                                                            {
                                                                p.append(decl.value(), init.value());
                                                            }
                                                    }
                                                p.expect_semi();
                                                out = decl;
                                                break;
                                            }
                                    }
                            }
                        p.restore(saved);
                        // expression statement
                        {
                            const Maybe<uint32_t, ParseError> e = parse_expr(p);
                            if (e.has_value())
                                {
                                    const Maybe<uint32_t, ParseError> es = p.make(NodeKind::EXPR_STMT, p.offset());
                                    if (es.has_value())
                                        {
                                            p.append(es.value(), e.value());
                                            if (attr_node.has_value())
                                                {
                                                    p.append(es.value(), attr_node.value());
                                                }
                                            // allow omitting semicolon before }
                                            if (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                                {
                                                    p.expect_semi();
                                                }
                                            out = es;
                                        }
                                    else
                                        {
                                            out = e;
                                        }
                                }
                            else
                                {
                                    if (e.has_error())
                                        {
                                            p.rep(e.error());
                                        }
                                    (void)out;
                                    p.skip_until({TokenKind::SEMICOLON, TokenKind::RBRACE});
                                    p.consume_if(TokenKind::SEMICOLON);
                                }
                        }
                        break;
                    }
            }

        if (attr_node.has_value() && out.has_value())
            {
                // prepend attribute as first child (not strictly typed; store append)
                // We'll append; consumers check children loosely.
                p.append(out.value(), attr_node.value());
            }
        return out;
    }

    // helper: whether the current token starts a physical line (TokenFlags).
    constexpr bool token_starts_line(const Token& t) noexcept { return any(t.flags & TokenFlags::START_OF_LINE); }
} // namespace embdr::lex
