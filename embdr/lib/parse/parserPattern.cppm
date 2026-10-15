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

export module embdr.lex.parserPattern;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.token;
import embdr.lex.parserType;

namespace embdr::lex
{
    using cxxstd::Maybe;
    using cxxstd::SimpleStringView;

    // A match arm pattern; returns PATTERN_* nodes and ranges. Guards and arm
    // body are handled by the caller.
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_pattern(Parser<ArenaT>& p) noexcept
    {
        Maybe<uint32_t, ParseError> result = Maybe<uint32_t, ParseError>{};
        if (p.kind() == TokenKind::IDENT && SimpleStringView("_").size() == p.cur().lexeme.size() &&
            p.cur().lexeme == SimpleStringView("_"))
            {
                const Token t = p.consume();
                return p.make(NodeKind::PATTERN_WILDCARD, t);
            }
        if (p.kind() == TokenKind::ELLIPSIS)
            {
                const Token t = p.consume();
                return p.make(NodeKind::PATTERN_REST, t);
            }
        if (p.kind() == TokenKind::PATTERN_LITERAL)
            {
                const Token t = p.consume();
                const Maybe<uint32_t, ParseError> node = p.make(NodeKind::PATTERN_REGEX, t);
                p.node(node.value()).text = t.lexeme;
                return node;
            }
        if (p.kind() == TokenKind::INT_LITERAL || p.kind() == TokenKind::FLOAT_LITERAL ||
            p.kind() == TokenKind::STRING_LITERAL || p.kind() == TokenKind::CHAR_LITERAL ||
            p.kind() == TokenKind::KW_TRUE || p.kind() == TokenKind::KW_FALSE)
            {
                // literal pattern
                if (p.kind() == TokenKind::KW_TRUE || p.kind() == TokenKind::KW_FALSE)
                    {
                        const Token t = p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::BOOL_LITERAL, t);
                        p.node(node.value()).flags = t.kind == TokenKind::KW_TRUE ? 1 : 0;
                        result = node;
                    }
                else
                    {
                        const Token t = p.consume();
                        NodeKind nk = NodeKind::INT_LITERAL;
                        if (t.kind == TokenKind::FLOAT_LITERAL)
                            nk = NodeKind::FLOAT_LITERAL;
                        else if (t.kind == TokenKind::CHAR_LITERAL)
                            nk = NodeKind::CHAR_LITERAL;
                        else if (t.kind == TokenKind::STRING_LITERAL || t.kind == TokenKind::RAW_STRING_LITERAL ||
                                 t.kind == TokenKind::PATTERN_LITERAL)
                            nk = NodeKind::STRING_LITERAL;
                        const Maybe<uint32_t, ParseError> node = p.make(nk, t);
                        p.node(node.value()).text = t.lexeme;
                        p.node(node.value()).op = t.kind;
                        result = node;
                    }
            }
        else if (p.kind() == TokenKind::IDENT || p.kind() == TokenKind::COLON_COLON)
            {
                const Maybe<uint32_t, ParseError> path = parse_path(p);
                if (!path.has_error())
                    {
                        result = p.make(NodeKind::PATTERN_VARIANT, p.offset());
                        if (!result.has_error())
                            {
                                p.append(result.value(), path.value());
                            }
                    }
                else
                    {
                        result = path;
                    }
            }
        else
            {
                return ParseError{ParseError::Kind::EXPECTED_PATTERN, p.offset(), TokenKind::IDENT, p.kind()};
            }
        // optional range
        if (p.kind() == TokenKind::DOT_DOT || p.kind() == TokenKind::DOT_DOT_ASSIGN)
            {
                const Token op = p.consume();
                const Maybe<uint32_t, ParseError> hi = parse_pattern(p); // recurse on RHS
                const Maybe<uint32_t, ParseError> rnode = p.make(NodeKind::PATTERN_RANGE, op);
                if (!rnode.has_error() && result.has_value())
                    {
                        p.node(rnode.value()).flags = op.kind == TokenKind::DOT_DOT_ASSIGN ? RANGE_INCLUSIVE : 0;
                        p.append(rnode.value(), result.value());
                        if (hi.has_value())
                            {
                                p.node(rnode.value()).flags =
                                    static_cast<uint8_t>(p.node(rnode.value()).flags | RANGE_HAS_HI);
                                p.append(rnode.value(), hi.value());
                            }
                        result = rnode;
                    }
            }
        // alternation
        if (p.kind() == TokenKind::PIPE)
            {
                const Token op = p.consume();
                const Maybe<uint32_t, ParseError> rhs = parse_pattern(p);
                const Maybe<uint32_t, ParseError> alt = p.make(NodeKind::PATTERN_ALT, op);
                if (alt.has_value() && result.has_value())
                    {
                        p.append(alt.value(), result.value());
                        if (rhs.has_value())
                            {
                                p.append(alt.value(), rhs.value());
                            }
                        result = alt;
                    }
            }
        if (result.has_error())
            {
                return ParseError{ParseError::Kind::EXPECTED_PATTERN, p.offset(), TokenKind::IDENT, p.kind()};
            }
        return result;
    }
} // namespace embdr::lex
