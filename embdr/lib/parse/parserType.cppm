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

export module embdr.lex.parserType;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.token;

namespace embdr::lex
{
    using cxxstd::Maybe;
    using cxxstd::SimpleStringView;

    // ---- Path (shared by expressions and types) ----
    // A PATH node's children are a chain of PATH nodes, each carrying one
    // segment's lexeme. Bit 0 of flags (GLOBAL) records a leading `::`.
    // parserDecl/parserExpr rely on this representation.
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_path(Parser<ArenaT>& p) noexcept
    {
        if (!p.enter())
            {
                return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset(), TokenKind::END_OF_FILE, p.kind()};
            }
        typename Parser<ArenaT>::DepthGuard guard(p);
        bool global = false;
        uint32_t offset = p.offset();
        if (p.kind() == TokenKind::COLON_COLON)
            {
                p.consume(); // "::"
                global = true;
            }
        if (p.kind() != TokenKind::IDENT && p.kind() != TokenKind::KW_SELF)
            {
                return ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset(), TokenKind::IDENT, p.kind()};
            }
        const Token first = p.kind() == TokenKind::KW_SELF ? p.consume() : p.expect_ident().value();
        const Maybe<uint32_t, ParseError> root = p.make(NodeKind::PATH, first);
        if (root.has_error())
            {
                return root;
            }
        p.append(root.value(), root.value());
        p.node(root.value()).text = first.lexeme;
        p.node(root.value()).flags =
            global ? static_cast<uint8_t>(NodeFlags::PUB) : 0; // GLOBAL stored below via flag bit
        // GLOBAL bit reuse: store separately in value? Use bits: bit0 of flags = GLOBAL (path). Override flags field
        // meaning.
        p.node(root.value()).flags = global ? 1 : 0;
        uint32_t prev = root.value();
        // additional segments
        while (p.kind() == TokenKind::COLON_COLON && p.peek(1).kind == TokenKind::IDENT)
            {
                p.consume(); // "::"
                const Token seg = p.expect_ident().value();
                const Maybe<uint32_t, ParseError> segnode = p.make(NodeKind::PATH, seg);
                if (segnode.has_error())
                    {
                        return segnode;
                    }
                p.node(segnode.value()).text = seg.lexeme;
                p.node(segnode.value()).next_sibling = NO_NODE;
                p.append(root.value(), segnode.value());
                prev = segnode.value();
            }
        (void)offset;
        (void)prev;
        return root;
    }

    // ---- Generic type-argument list ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_generic_args(Parser<ArenaT>& p) noexcept
    {
        const Maybe<Token, ParseError> lt = p.expect(TokenKind::LT);
        if (lt.has_error())
            {
                return lt.error();
            }
        const Maybe<uint32_t, ParseError> args = p.make(NodeKind::GENERIC_PARAMS, lt.value());
        if (args.has_error())
            {
                return args;
            }
        const Maybe<uint32_t, ParseError> first = parse_type(p);
        if (first.has_error())
            {
                return first;
            }
        p.append(args.value(), first.value());
        while (p.consume_if(TokenKind::COMMA))
            {
                const Maybe<uint32_t, ParseError> next = parse_type(p);
                if (next.has_error())
                    {
                        return next;
                    }
                p.append(args.value(), next.value());
            }
        const Maybe<Token, ParseError> gt = p.expect_gt();
        if (gt.has_error())
            {
                return gt.error();
            }
        return args;
    }

    // ---- Type ----
    // `ref` is an optional suffix (uint32 ref).
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_type(Parser<ArenaT>& p) noexcept
    {
        if (!p.enter())
            {
                return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset(), TokenKind::END_OF_FILE, p.kind()};
            }
        typename Parser<ArenaT>::DepthGuard guard(p);
        uint32_t node = NO_NODE;
        switch (p.kind())
            {
                case TokenKind::STAR:
                    {
                        const Token star = p.consume();
                        const Maybe<uint32_t, ParseError> inner = parse_type(p);
                        if (inner.has_error())
                            {
                                return inner;
                            }
                        node = p.make(NodeKind::TYPE_POINTER, star).value();
                        p.append(node, inner.value());
                        break;
                    }
                case TokenKind::LBRACKET:
                    {
                        const Token open = p.consume();
                        auto len = Maybe<uint32_t, ParseError>{};
                        bool unsized = false;
                        bool slice = false;
                        if (p.kind() == TokenKind::RBRACKET)
                            {
                                slice = true;
                                p.consume();
                            }
                        else if (p.kind() == TokenKind::QUESTION)
                            {
                                unsized = true;
                                p.consume();
                                if (!p.consume_if(TokenKind::RBRACKET))
                                    {
                                        p.rep(ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(),
                                                         TokenKind::RBRACKET, p.kind()});
                                    }
                            }
                        else
                            {
                                // Array lengths are either an integer literal or a const path.
                                // (Full cexpr lengths lower later; parser keeps the extent that
                                // exists today as a single literal/path node.)
                                if (p.kind() == TokenKind::INT_LITERAL)
                                    {
                                        const Token tok = p.consume();
                                        uint32_t id = p.make(NodeKind::INT_LITERAL, tok).value();
                                        p.node(id).text = tok.lexeme;
                                        len = Maybe<uint32_t, ParseError>{id};
                                    }
                                else if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Maybe<uint32_t, ParseError> identpath = parse_path(p);
                                        len = identpath;
                                    }
                                else
                                    {
                                        p.rep(ParseError{ParseError::Kind::EXPECTED_EXPRESSION, p.offset(),
                                                         TokenKind::INT_LITERAL, p.kind()});
                                    }
                                if (!p.consume_if(TokenKind::RBRACKET))
                                    {
                                        p.rep(ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(),
                                                         TokenKind::RBRACKET, p.kind()});
                                    }
                            }
                        const Maybe<uint32_t, ParseError> elem = parse_type(p);
                        if (elem.has_error())
                            {
                                return elem;
                            }
                        node = p.make(NodeKind::TYPE_ARRAY, open).value();
                        if (len.has_value())
                            {
                                p.append(node, len.value());
                            }
                        p.append(node, elem.value());
                        p.node(node).flags = (slice ? 1 : 0) | (unsized ? 2 : 0);
                        break;
                    }
                case TokenKind::KW_FN:
                    {
                        const Token fn = p.consume();
                        // `(` param-type list `)`
                        const Maybe<Token, ParseError> open = p.expect(TokenKind::LPAREN);
                        if (open.has_error())
                            {
                                return open.error();
                            }
                        node = p.make(NodeKind::TYPE_FUNCTION, fn).value();
                        while (p.kind() != TokenKind::RPAREN && p.kind() != TokenKind::END_OF_FILE)
                            {
                                const Maybe<uint32_t, ParseError> elemtype = parse_type(p);
                                if (elemtype.has_error())
                                    {
                                        return elemtype;
                                    }
                                Maybe<uint32_t, ParseError> pname =
                                    p.make(NodeKind::PARAM_DECL, elemtype.value() ? elemtype.value() : p.offset());
                                if (!pname.has_error())
                                    {
                                        p.append(node, pname.value());
                                        p.append(pname.value(), elemtype.value());
                                    }
                                if (p.consume_if(TokenKind::COMMA))
                                    {
                                        continue;
                                    }
                                break;
                            }
                        p.consume_if(TokenKind::RPAREN); // swallow closing (tolerant)
                        // optional return type: `-> T` or bare `T` (§20 spells fn(...) int32)
                        if (p.consume_if(TokenKind::ARROW))
                            {
                                const Maybe<uint32_t, ParseError> ret = parse_type(p);
                                if (ret.has_value())
                                    {
                                        p.append(node, ret.value());
                                    }
                            }
                        else if (is_type_start(p.kind()))
                            {
                                const Maybe<uint32_t, ParseError> ret = parse_type(p);
                                if (ret.has_value())
                                    {
                                        p.append(node, ret.value());
                                    }
                            }
                        break;
                    }
                case TokenKind::LPAREN:
                    {
                        p.consume();
                        node = parse_type(p).value_or(NO_NODE);
                        if (!p.consume_if(TokenKind::RPAREN))
                            {
                                p.rep(ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(), TokenKind::RPAREN,
                                                 p.kind()});
                            }
                        if (node == NO_NODE)
                            {
                                // parse_type failed — produce TRANSLATION_UNIT? bail as error
                                return ParseError{ParseError::Kind::EXPECTED_TYPE, p.offset(), TokenKind::END_OF_FILE,
                                                  p.kind()};
                            }
                        break;
                    }
                default:
                    {
                        const Maybe<uint32_t, ParseError> path = parse_path(p);
                        if (path.has_error())
                            {
                                return path;
                            }
                        node = path.value();
                        // optional generic args
                        if (p.kind() == TokenKind::LT)
                            {
                                const Maybe<uint32_t, ParseError> args = parse_generic_args(p);
                                if (args.has_value())
                                    {
                                        const uint32_t base = node;
                                        const Maybe<uint32_t, ParseError> tg =
                                            p.make(NodeKind::TYPE_NAME, p.node(base).offset);
                                        if (tg.has_value())
                                            {
                                                p.append(tg.value(), base);
                                                node = tg.value();
                                                // reparent generic args as siblings under a TYPE_NAME-like
                                                // container; reuse GENERIC_PARAMS children directly:
                                                AstNode& argsnode = p.node(args.value());
                                                uint32_t c = argsnode.first_child;
                                                while (c != NO_NODE)
                                                    {
                                                        const uint32_t nxt = p.node(c).next_sibling;
                                                        p.append(node, c);
                                                        c = nxt;
                                                    }
                                            }
                                    }
                            }
                        // `->` already handled by caller for TYPE_FUNCTION; ref suffix:
                        break;
                    }
            }
        // ref suffix (`uint32 ref`)
        if (node != NO_NODE && p.kind() == TokenKind::KW_REF)
            {
                const Token refTok = p.consume();
                const uint32_t refnode = p.make(NodeKind::TYPE_REFERENCE, refTok).value();
                p.append(refnode, node);
                node = refnode;
            }
        if (node == NO_NODE)
            {
                return ParseError{ParseError::Kind::EXPECTED_TYPE, p.offset(), TokenKind::IDENT, p.kind()};
            }
        return node;
    }

    // Helper used elsewhere: is a token kind a valid type start?
    export [[nodiscard]] constexpr bool is_type_start(const TokenKind kind) noexcept
    {
        switch (kind)
            {
                case TokenKind::IDENT:
                case TokenKind::STAR:
                case TokenKind::LBRACKET:
                case TokenKind::KW_FN:
                case TokenKind::LPAREN:
                case TokenKind::COLON_COLON:
                    return true;
                default:
                    return false;
            }
    }
} // namespace embdr::lex
