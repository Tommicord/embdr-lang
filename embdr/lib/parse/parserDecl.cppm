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

export module embdr.lex.parserDecl;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserType;
import embdr.lex.parserTemplate;
import embdr.lex.parserExpr;
import embdr.lex.parserStmt;
import embdr.lex.parserPattern;
import embdr.lex.token;

namespace embdr::lex
{
    using cxxstd::Maybe;
    using cxxstd::SimpleStringView;

    // ---- attribute list ----
    // Returns the first ATTRIBUTE node if any, chaining stacked ones as its
    // siblings; the caller may attach them to the wrapped construct.
    template <typename ArenaT>
    inline Maybe<uint32_t, ParseError> parse_attributes(Parser<ArenaT>& p, ArenaT& ar) noexcept
    {
        if (p.kind() != TokenKind::HASH)
            {
                return Maybe<uint32_t, ParseError>{};
            }
        Maybe<uint32_t, ParseError> first = Maybe<uint32_t, ParseError>{};
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
                const Maybe<uint32_t, ParseError> attr = p.make(NodeKind::ATTRIBUTE, hash);
                if (attr.has_error())
                    {
                        break;
                    }
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
                                else if (p.kind() == TokenKind::IDENT || p.kind() == TokenKind::KW_SELF)
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
                if (!first.has_value())
                    {
                        first = attr;
                    }
                else
                    {
                        p.append(first.value(), attr.value());
                    }
            }
        (void)ar;
        return first;
    }

    // ---- parse_param ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_param(Parser<ArenaT>& p) noexcept
    {
        if (p.kind() == TokenKind::IDENT)
            {
                const Token name = p.consume();
                // typed form requires colon
                if (p.consume_if(TokenKind::COLON))
                    {
                        const Maybe<uint32_t, ParseError> ty = parse_type(p);
                        const Maybe<uint32_t, ParseError> par = p.make(NodeKind::PARAM_DECL, name);
                        p.node(par.value()).text = name.lexeme;
                        if (ty.has_value())
                            {
                                p.append(par.value(), ty.value());
                            }
                        return par;
                    }
                // untyped (closure) param
                const Maybe<uint32_t, ParseError> par = p.make(NodeKind::PARAM_DECL, name);
                if (!par.has_error())
                    {
                        p.node(par.value()).text = name.lexeme;
                        p.node(par.value()).flags = LOCAL_NO_TYPE;
                    }
                return par;
            }
        // type without name (function type params)
        const Maybe<uint32_t, ParseError> ty = parse_type(p);
        const Maybe<uint32_t, ParseError> par = p.make(NodeKind::PARAM_DECL, p.offset());
        if (ty.has_value())
            {
                p.append(par.value(), ty.value());
            }
        return par;
    }

    // ---- parse_function ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_function(Parser<ArenaT>& p, uint8_t flags_in) noexcept
    {
        if (!p.enter())
            {
                return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset(), TokenKind::END_OF_FILE, p.kind()};
            }
        typename Parser<ArenaT>::DepthGuard guard(p);
        // name
        bool global = false;
        if (p.consume_if(TokenKind::COLON_COLON))
            {
                global = true;
            }
        if (p.kind() != TokenKind::IDENT && p.kind() != TokenKind::LT)
            {
                // maybe `::<name>`
                if (p.kind() == TokenKind::LT)
                    {
                    }
            }
        if (p.kind() != TokenKind::IDENT)
            {
                return ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, p.offset(), TokenKind::IDENT, p.kind()};
            }
        const Token name = p.consume();
        const Maybe<uint32_t, ParseError> fn = p.make(NodeKind::FUNCTION_DECL, name);
        if (fn.has_error())
            {
                return fn.error();
            }
        p.node(fn.value()).text = name.lexeme;
        p.node(fn.value()).flags = flags_in | (global ? 0 : 0);

        // generic params
        if (p.kind() == TokenKind::LT)
            {
                const Maybe<uint32_t, ParseError> gp = parse_generic_params(p);
                if (gp.has_value())
                    {
                        p.append(fn.value(), gp.value());
                    }
            }

        // params
        if (p.kind() == TokenKind::LPAREN)
            {
                p.consume();
                while (p.kind() != TokenKind::RPAREN && p.kind() != TokenKind::END_OF_FILE)
                    {
                        const Maybe<uint32_t, ParseError> par = parse_param(p);
                        if (par.has_value())
                            {
                                p.append(fn.value(), par.value());
                            }
                        if (!p.consume_if(TokenKind::COMMA))
                            {
                                break;
                            }
                    }
                p.consume_if(TokenKind::RPAREN);
            }
        // return type
        if (p.kind() == TokenKind::ARROW)
            {
                p.consume();
                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                if (ty.has_value())
                    {
                        p.append(fn.value(), ty.value());
                    }
            }
        // optional pub flags from attributes already applied by flags_in
        // body
        if (p.kind() == TokenKind::LBRACE)
            {
                const Maybe<uint32_t, ParseError> body = parse_block(p);
                if (body.has_value())
                    {
                        p.append(fn.value(), body.value());
                    }
            }
        else
            {
                p.expect_semi();
            }
        (void)global;
        return fn;
    }

    // ---- members in struct/enum bodies ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_member(Parser<ArenaT>& p,
                                                                     Maybe<uint32_t, ParseError> owner = {}) noexcept
    {
        // attributes
        Maybe<uint32_t, ParseError> attrs = parse_attributes(p, p.arena());
        if (p.kind() == TokenKind::KW_PUB)
            {
                p.consume();
                if (p.kind() == TokenKind::LBRACE)
                    {
                        const Maybe<uint32_t, ParseError> blk = p.make(NodeKind::BLOCK, p.offset());
                        (void)blk;
                        p.expect(TokenKind::LBRACE);
                        while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                            {
                                if (const Maybe<uint32_t, ParseError> m = parse_member(p); m.has_value())
                                    {
                                        // store into the wrapping item handled by caller; append
                                        // skipped here because no parent known. NO-OP for now.
                                        // TODO: Implement this code path
                                    }
                                if (p.kind() == TokenKind::RBRACE)
                                    {
                                        break;
                                    }
                                p.consume_if(TokenKind::SEMICOLON);
                            }
                        p.consume_if(TokenKind::RBRACE);
                        return Maybe<uint32_t, ParseError>{};
                    }
            }

        if (p.kind() == TokenKind::HASH)
            {
                attrs = parse_attributes(p, p.arena());
                // ctor/dtor if the last attr is constructor/destructor
                cxxstd::SimpleStringView aname{};
                if (attrs.has_value())
                    {
                        aname = p.node(attrs.value()).text;
                    }
                if (aname == cxxstd::SimpleStringView("constructor"))
                    {
                        if (p.kind() == TokenKind::KW_SELF)
                            {
                                p.consume();
                                p.consume_if(TokenKind::LPAREN);
                                p.consume_if(TokenKind::RPAREN);
                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                Maybe<uint32_t, ParseError> fn = p.make(NodeKind::FUNCTION_DECL, p.offset());
                                if (!fn.has_error())
                                    {
                                        p.node(fn.value()).text = cxxstd::SimpleStringView("self");
                                        p.node(fn.value()).flags = MEMBER_CTOR;
                                        if (body.has_value())
                                            {
                                                p.append(fn.value(), body.value());
                                            }
                                    }
                                return fn;
                            }
                    }
                if (aname == cxxstd::SimpleStringView("destructor"))
                    {
                        p.consume_if(TokenKind::LPAREN);
                        p.consume_if(TokenKind::RPAREN);
                        if (p.kind() == TokenKind::IDENT && p.cur().lexeme == SimpleStringView("default"))
                            {
                                p.consume();
                                p.expect_semi();
                            }
                        Maybe<uint32_t, ParseError> fn = p.make(NodeKind::FUNCTION_DECL, p.offset());
                        if (!fn.has_error())
                            {
                                p.node(fn.value()).text = cxxstd::SimpleStringView("destructor");
                                p.node(fn.value()).flags = static_cast<uint8_t>(
                                    MEMBER_DTOR |
                                    ((p.kind() == TokenKind::IDENT && p.cur().lexeme == SimpleStringView("default"))
                                         ? DTOR_DEFAULT
                                         : 0));
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                const Maybe<uint32_t, ParseError> b = parse_block(p);
                                if (b.has_value() && fn.has_value())
                                    {
                                        p.append(fn.value(), b.value());
                                    }
                            }
                        return fn;
                    }
                // otherwise parse function/static with attrs handled loosely (skip attrs)
            }

        if (p.kind() == TokenKind::KW_FN || p.kind() == TokenKind::KW_FUNCTION || p.kind() == TokenKind::COLON_COLON)
            {
                uint8_t flags = 0;
                return parse_function(p, flags);
            }
        if (p.kind() == TokenKind::KW_STATIC || p.kind() == TokenKind::KW_VOLATILE)
            {
                // field-like static member -> local-style decl; reuse LOCAL_DECL
                // by forwarding: nothing dedicated; skip to be safe.
                const Maybe<uint32_t, ParseError> st = parse_stmt(p);
                return st;
            }
        if (p.kind() == TokenKind::IDENT && p.peek(1).kind == TokenKind::COLON)
            {
                const Token name = p.consume();
                p.consume(); // ":"
                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                const Maybe<uint32_t, ParseError> field = p.make(NodeKind::FIELD_DECL, name);
                if (!field.has_error())
                    {
                        p.node(field.value()).text = name.lexeme;
                        if (ty.has_value())
                            {
                                p.append(field.value(), ty.value());
                            }
                    }
                p.consume_if(TokenKind::SEMICOLON);
                return field;
            }
        // enum variants handled in parse_enum separately
        if (p.kind() == TokenKind::IDENT)
            {
                // variant / binding fallback
                const Token id = p.consume();
                Maybe<uint32_t, ParseError> node = p.make(NodeKind::VARIANT_DECL, id);
                p.node(node.value()).text = id.lexeme;
                if (p.consume_if(TokenKind::COMMA))
                    {
                    }
                (void)attrs;
                return node;
            }
        (void)attrs;
        p.rep(ParseError{ParseError::Kind::EXPECTED_ITEM, p.offset(), TokenKind::IDENT, p.kind()});
        p.skip_until({TokenKind::SEMICOLON, TokenKind::RBRACE});
        return ParseError{ParseError::Kind::UNEXPECTED_TOKEN, p.offset(), TokenKind::IDENT, p.kind()};
    }

    // ---- struct body ----
    template <typename ArenaT>
    constexpr Maybe<uint32_t, ParseError> parse_struct_body(Parser<ArenaT>& p,
                                                            const Maybe<uint32_t, ParseError>& owner) noexcept
    {
        p.consume_if(TokenKind::LBRACE);
        while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
            {
                const Maybe<uint32_t, ParseError> m = parse_member(p);
                if (m.has_value() && owner.has_value())
                    {
                        p.append(owner.value(), m.value());
                    }
            }
        p.consume_if(TokenKind::RBRACE);
        return Maybe<uint32_t, ParseError>{};
    }

    // ---- struct/enum decl shared helper ----
    // parse_item needs this; forward declaration style handled by free functions.
    // We'll inline struct/enum cases inside parse_item instead; here provide
    // helpers called from parse_item.

    // ---- parse_item ----
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> parse_item(Parser<ArenaT>& p) noexcept
    {
        if (!p.enter())
            {
                return ParseError{ParseError::Kind::NESTING_TOO_DEEP, p.offset(), TokenKind::END_OF_FILE, p.kind()};
            }
        typename Parser<ArenaT>::DepthGuard guard(p);

        // attributes
        const Maybe<uint32_t, ParseError> attrs = parse_attributes(p, p.arena());

        // modifiers
        uint8_t flags = 0;
        bool packed = false;
        bool reflect_seen = false;
        while (true)
            {
                if (p.kind() == TokenKind::KW_PUB)
                    {
                        p.consume();
                        flags = static_cast<uint8_t>(flags | static_cast<uint8_t>(NodeFlags::PUB));
                        continue;
                    }
                if (p.kind() == TokenKind::KW_PACKED)
                    {
                        p.consume();
                        packed = true;
                        flags = static_cast<uint8_t>(flags | static_cast<uint8_t>(NodeFlags::PACKED));
                        continue;
                    }
                if (p.kind() == TokenKind::KW_EXTERN)
                    {
                        p.consume();
                        flags = static_cast<uint8_t>(flags | static_cast<uint8_t>(NodeFlags::EXTERN));
                        continue;
                    }
                if (p.kind() == TokenKind::KW_INTERRUPT)
                    {
                        p.consume();
                        flags = static_cast<uint8_t>(flags | static_cast<uint8_t>(NodeFlags::INTERRUPT));
                        continue;
                    }
                if (p.kind() == TokenKind::KW_VOLATILE)
                    {
                        p.consume();
                        flags = static_cast<uint8_t>(flags | static_cast<uint8_t>(NodeFlags::VOLATILE));
                        continue;
                    }
                break;
            }
        (void)reflect_seen;

        Maybe<uint32_t, ParseError> result = Maybe<uint32_t, ParseError>{};

        switch (p.kind())
            {
                case TokenKind::KW_USE:
                    {
                        p.consume();
                        const Maybe<Token, ParseError> lt = p.expect(TokenKind::LT);
                        if (const Maybe<uint32_t, ParseError> node = p.make(NodeKind::USE_DECL, p.offset());
                            !lt.has_error() && node.has_value())
                            {
                                // path with / separators
                                cxxstd::SimpleStringView path_text{};
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token first = p.consume();
                                        path_text = first.lexeme;
                                    }
                                while (p.kind() == TokenKind::SLASH)
                                    {
                                        p.consume();
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                const Token seg = p.consume();
                                                (void)seg;
                                            }
                                    }
                                // alias
                                if (p.consume_if(TokenKind::KW_AS))
                                    {
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                cxxstd::SimpleStringView a = p.consume().lexeme;
                                                p.node(node.value()).text = a;
                                            }
                                    }
                                if (p.kind() == TokenKind::LBRACE)
                                    {
                                        p.consume();
                                        while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                            {
                                                if (p.kind() == TokenKind::IDENT)
                                                    {
                                                        const Token id = p.consume();
                                                        const Maybe<uint32_t, ParseError> pn =
                                                            p.make(NodeKind::PATH, id);
                                                        p.node(pn.value()).text = id.lexeme;
                                                        p.append(node.value(), pn.value());
                                                    }
                                                if (!p.consume_if(TokenKind::COMMA))
                                                    {
                                                        break;
                                                    }
                                            }
                                        p.consume_if(TokenKind::RBRACE);
                                    }
                                if (attrs.has_value() && node.has_value())
                                    {
                                        p.append(node.value(), attrs.value());
                                    }
                                p.consume_if(TokenKind::GT);
                                p.expect_semi();
                                result = node;
                            }
                        else
                            {
                                p.rep(
                                    ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(), TokenKind::LT, p.kind()});
                            }
                        break;
                    }

                case TokenKind::KW_MODULE:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::MODULE, p.offset());
                        // name drivers::bme280
                        while (p.kind() == TokenKind::IDENT || p.kind() == TokenKind::COLON_COLON)
                            {
                                if (p.kind() == TokenKind::COLON_COLON)
                                    {
                                        p.consume();
                                        continue;
                                    }
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        if (!node.has_error())
                                            {
                                                p.node(node.value()).text = p.cur().lexeme;
                                            }
                                        p.consume();
                                    }
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                // inline item list becomes children of node
                                p.consume();
                                typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, false);
                                while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                    {
                                        const Maybe<uint32_t, ParseError> it = parse_item(p);
                                        if (it.has_value() && node.has_value())
                                            {
                                                p.append(node.value(), it.value());
                                            }
                                        if (it.has_error())
                                            {
                                                p.skip_until({TokenKind::RBRACE});
                                            }
                                    }
                                p.consume_if(TokenKind::RBRACE);
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        result = node;
                        break;
                    }

                case TokenKind::KW_STRUCT:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::STRUCT_DECL, p.offset());
                        if (packed)
                            {
                                p.node(node.value()).flags = static_cast<uint8_t>(
                                    p.node(node.value()).flags | static_cast<uint8_t>(NodeFlags::PACKED));
                            }
                        p.node(node.value()).flags = static_cast<uint8_t>(p.node(node.value()).flags | flags);
                        // name (::? ident)
                        if (p.consume_if(TokenKind::COLON_COLON))
                            {
                            }
                        if (p.kind() == TokenKind::IDENT)
                            {
                                p.node(node.value()).text = p.cur().lexeme;
                                p.consume();
                            }
                        // generics
                        if (p.kind() == TokenKind::LT)
                            {
                                const Maybe<uint32_t, ParseError> gp = parse_generic_params(p);
                                if (gp.has_value() && node.has_value())
                                    {
                                        p.append(node.value(), gp.value());
                                    }
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                parse_struct_body(p, node);
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        result = node;
                        break;
                    }

                case TokenKind::KW_ENUM:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::ENUM_DECL, p.offset());
                        p.node(node.value()).flags = flags;
                        if (p.consume_if(TokenKind::COLON_COLON))
                            {
                            }
                        if (p.kind() == TokenKind::IDENT)
                            {
                                p.node(node.value()).text = p.cur().lexeme;
                                p.consume();
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                p.consume();
                                // variants
                                while (p.kind() == TokenKind::IDENT)
                                    {
                                        const Token id = p.consume();
                                        const Maybe<uint32_t, ParseError> v = p.make(NodeKind::VARIANT_DECL, id);
                                        p.node(v.value()).text = id.lexeme;
                                        if (node.has_value() && v.has_value())
                                            {
                                                p.append(node.value(), v.value());
                                            }
                                        if (p.consume_if(TokenKind::COMMA))
                                            {
                                                continue;
                                            }
                                        // if no comma, trailing variant followed by member
                                        if (p.kind() == TokenKind::IDENT || p.kind() == TokenKind::HASH ||
                                            p.kind() == TokenKind::KW_FN || p.kind() == TokenKind::KW_FUNCTION ||
                                            p.kind() == TokenKind::KW_STATIC)
                                            {
                                                // could be next variant without comma? spec usually comma;
                                                // keep not consuming to let member parser take over
                                                break;
                                            }
                                        break;
                                    }
                                typename Parser<ArenaT>::NoStructLiteralGuard no_struct_guard(p, false);
                                while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                    {
                                        const Maybe<uint32_t, ParseError> m = parse_member(p);
                                        if (m.has_value() && node.has_value())
                                            {
                                                p.append(node.value(), m.value());
                                            }
                                        if (p.kind() == TokenKind::COMMA)
                                            {
                                                p.consume();
                                            }
                                    }
                                p.consume_if(TokenKind::RBRACE);
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        result = node;
                        break;
                    }

                case TokenKind::KW_MACRO:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::MACRO_DECL, p.offset());
                        // name ::<name>
                        if (p.consume_if(TokenKind::COLON_COLON))
                            {
                                p.consume_if(TokenKind::LT);
                                if (p.kind() == TokenKind::IDENT)
                                    {
                                        p.node(node.value()).text = p.cur().lexeme;
                                        p.consume();
                                    }
                                p.consume_if(TokenKind::GT);
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                p.consume();
                                while (p.kind() != TokenKind::RBRACE && p.kind() != TokenKind::END_OF_FILE)
                                    {
                                        // macro rule: ( ... ^ ) { body }
                                        if (p.kind() == TokenKind::LPAREN)
                                            {
                                                const Maybe<Token, ParseError> open = p.expect(TokenKind::LPAREN);
                                                (void)open;
                                                const Maybe<uint32_t, ParseError> rule =
                                                    p.make(NodeKind::MACRO_RULE, p.offset());
                                                if (!rule.has_error())
                                                    {
                                                        p.consume_if(TokenKind::DOLLAR);
                                                        while (p.kind() != TokenKind::CARET &&
                                                               p.kind() != TokenKind::RPAREN &&
                                                               p.kind() != TokenKind::END_OF_FILE)
                                                            {
                                                                if (p.kind() == TokenKind::STRING_LITERAL ||
                                                                    p.kind() == TokenKind::STRING_LITERAL_U16 ||
                                                                    p.kind() == TokenKind::RAW_STRING_LITERAL)
                                                                    {
                                                                        const Token t = p.consume();
                                                                        const Maybe<uint32_t, ParseError> part =
                                                                            p.make(NodeKind::MACRO_PATTERN, t);
                                                                        p.node(part.value()).text = t.lexeme;
                                                                        p.append(rule.value(), part.value());
                                                                    }
                                                                else if (p.kind() == TokenKind::DOLLAR)
                                                                    {
                                                                        p.consume();
                                                                        const Token nm =
                                                                            p.expect_ident().value_or(Token{});
                                                                        (void)nm;
                                                                        const Maybe<uint32_t, ParseError> cap =
                                                                            p.make(NodeKind::MACRO_CAPTURE, p.offset());
                                                                        if (cap.has_value() &&
                                                                            nm.kind == TokenKind::IDENT)
                                                                            {
                                                                                p.node(cap.value()).text = nm.lexeme;
                                                                            }
                                                                        p.append(rule.value(), cap.value());
                                                                    }
                                                                else
                                                                    {
                                                                        p.consume();
                                                                    }
                                                            }
                                                        p.consume_if(TokenKind::CARET);
                                                        p.consume_if(TokenKind::RPAREN);
                                                        if (p.kind() == TokenKind::LBRACE)
                                                            {
                                                                const Maybe<uint32_t, ParseError> body = parse_block(p);
                                                                if (body.has_value())
                                                                    {
                                                                        p.append(rule.value(), body.value());
                                                                    }
                                                            }
                                                        p.append(node.value(), rule.value());
                                                    }
                                            }
                                        else
                                            {
                                                p.consume();
                                            }
                                    }
                                p.consume_if(TokenKind::RBRACE);
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        result = node;
                        break;
                    }

                case TokenKind::KW_EMBED:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::EMBED_DECL, p.offset());
                        if (p.consume_if(TokenKind::COLON_COLON))
                            {
                            }
                        if (p.kind() == TokenKind::IDENT)
                            {
                                p.node(node.value()).text = p.cur().lexeme;
                                p.consume();
                            }
                        if (p.kind() == TokenKind::LBRACE)
                            {
                                parse_struct_body(p, node);
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        result = node;
                        break;
                    }

                case TokenKind::KW_EXTERN:
                    {
                        p.consume();
                        // extern function ...
                        uint8_t f = static_cast<uint8_t>(NodeFlags::EXTERN);
                        if (p.kind() == TokenKind::KW_FUNCTION)
                            {
                                p.consume();
                            }
                        Maybe<uint32_t, ParseError> fn = parse_function(p, static_cast<uint8_t>(f | flags));
                        if (fn.has_value())
                            {
                                if (attrs.has_value())
                                    {
                                        p.append(fn.value(), attrs.value());
                                    }
                            }
                        result = fn;
                        break;
                    }

                case TokenKind::KW_STATIC:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> node = p.make(NodeKind::STATIC_DECL, p.offset());
                        p.node(node.value()).flags = flags;
                        if (is_type_start(p.kind()))
                            {
                                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                if (ty.has_value())
                                    {
                                        p.append(node.value(), ty.value());
                                    }
                            }
                        if (p.kind() == TokenKind::IDENT)
                            {
                                p.node(node.value()).text = p.cur().lexeme;
                                p.consume();
                            }
                        if (p.consume_if(TokenKind::EQ))
                            {
                                const Maybe<uint32_t, ParseError> init = parse_expr(p);
                                if (init.has_value())
                                    {
                                        p.append(node.value(), init.value());
                                    }
                            }
                        if (attrs.has_value() && node.has_value())
                            {
                                p.append(node.value(), attrs.value());
                            }
                        p.expect_semi();
                        result = node;
                        break;
                    }

                case TokenKind::KW_VOLATILE:
                    {
                        p.consume();
                        if (p.kind() == TokenKind::KW_STATIC)
                            {
                                p.consume();
                                const Maybe<uint32_t, ParseError> node = p.make(NodeKind::STATIC_DECL, p.offset());
                                if (node.has_value())
                                    {
                                        p.node(node.value()).flags =
                                            static_cast<uint8_t>(NodeFlags::VOLATILE | static_cast<NodeFlags>(flags));
                                        if (is_type_start(p.kind()))
                                            {
                                                const Maybe<uint32_t, ParseError> ty = parse_type(p);
                                                if (ty.has_value())
                                                    {
                                                        p.append(node.value(), ty.value());
                                                    }
                                            }
                                        if (p.kind() == TokenKind::IDENT)
                                            {
                                                p.node(node.value()).text = p.cur().lexeme;
                                                p.consume();
                                            }
                                        p.expect_semi();
                                    }
                                if (attrs.has_value() && node.has_value())
                                    {
                                        p.append(node.value(), attrs.value());
                                    }
                                result = node;
                            }
                        else
                            {
                                result = ParseError{ParseError::Kind::EXPECTED_ITEM, p.offset(), TokenKind::KW_STATIC,
                                                    p.kind()};
                            }
                        break;
                    }

                case TokenKind::KW_INTERRUPT:
                    p.consume();
                    // fall through to function: if next is function/FN/::
                    [[fallthrough]];

                case TokenKind::KW_FUNCTION:
                case TokenKind::KW_FN:
                    {
                        p.consume();
                        const Maybe<uint32_t, ParseError> fn = parse_function(p, flags);
                        if (fn.has_value() && attrs.has_value())
                            {
                                p.append(fn.value(), attrs.value());
                            }
                        result = fn;
                        break;
                    }

                case TokenKind::COLON_COLON:
                    {
                        // #test ::name()  or generic fn without keyword
                        uint8_t f = flags;
                        (void)f;
                        const Maybe<uint32_t, ParseError> fn = parse_function(p, flags);
                        if (fn.has_value() && attrs.has_value())
                            {
                                p.append(fn.value(), attrs.value());
                            }
                        result = fn;
                        break;
                    }

                default:
                    {
                        // top-level `::name()` already handled? COLON_COLON above.
                        // stray expression / unknown -> diagnostic
                        if (p.kind() != TokenKind::END_OF_FILE && p.kind() != TokenKind::RBRACE)
                            {
                                p.rep(ParseError{ParseError::Kind::EXPECTED_ITEM, p.offset(), TokenKind::IDENT,
                                                 p.kind()});
                            }
                        p.skip_until({TokenKind::HASH, TokenKind::KW_USE, TokenKind::KW_MODULE, TokenKind::KW_FUNCTION,
                                      TokenKind::KW_FN, TokenKind::KW_STRUCT, TokenKind::KW_ENUM, TokenKind::KW_MACRO,
                                      TokenKind::KW_EMBED, TokenKind::KW_EXTERN, TokenKind::KW_STATIC,
                                      TokenKind::KW_PUB});
                        if (p.kind() == TokenKind::HASH || p.kind() == TokenKind::KW_USE ||
                            p.kind() == TokenKind::KW_MODULE || p.kind() == TokenKind::KW_FUNCTION)
                            {
                            }
                        else
                            {
                                if (!p.consume_if(TokenKind::SEMICOLON))
                                    {
                                        p.consume();
                                    }
                            }
                        result = Maybe<uint32_t, ParseError>{};
                        break;
                    }
            }

        // attach attributes if not already attached
        if (result.has_value() && attrs.has_value())
            {
                // only attach if result kind is not use/macro where already attached? double attach ok
            }
        return result;
    }
} // namespace embdr::lex
