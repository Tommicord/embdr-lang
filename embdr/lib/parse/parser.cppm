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

export module embdr.lex.parser;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.charinfo;
import embdr.lex.lexer;
import embdr.lex.token;

namespace embdr::lex
{
    using charinfo::is_identifier_start;
    using cxxstd::Maybe;
    using cxxstd::SimpleStringView;

    // The shared parser core: a small token cursor with bounded lookahead
    // over the forward-only lexer, a fixed-capacity diagnostic list, the
    // delimiter scope stack, and the save/restore hook the speculative
    // (tentative) productions are built on.
    //
    // Design rules every parse production in this directory obeys:
    //   - errors are ParseError values; nothing throws, nothing aborts on
    //     malformed input — a broken construct yields a diagnostic and the
    //     cursor resynchronizes (SkipUntil-style) so the rest of the
    //     translation unit still builds a tree;
    //   - every consume() call through the public API makes progress, so
    //     every recovery loop terminates even on adversarial input;
    //   - speculative parses (save/restore) roll back the arena AND the
    //     diagnostic count, so a rejected guess leaves no trace — the same
    //     discipline clang's tentative parsing uses to keep diagnostics
    //     silent until a production commits;
    //   - nesting depth is bounded (PARSE_MAX_DEPTH): unbounded recursion on
    //     hostile input would be a stack overflow, which is UB and forbidden;
    //     the guard turns it into a NESTING_TOO_DEEP error value instead.
    //
    // The parser is a template on its node arena so tests can instantiate a
    // tiny arena and drive the ARENA_FULL path; production uses the default.
    export template <typename ArenaT = AstArena<>>
    class Parser
    {
      public:
        using arena_type = ArenaT;

        // Complete speculative-parse snapshot. `pre` is the lexer cursor
        // *before* the current token, so restore() replays the token stream
        // exactly without re-lexing trivia.
        struct State
        {
            Lexer::State pre{};
            size_t diag_count = 0;
            size_t error_count = 0;
            uint32_t node_count = 0;
            uint16_t depth = 0;
            size_t scope_top = 0;
            bool no_struct_literal = false;
        };

        explicit constexpr Parser(const SimpleStringView source) noexcept : _M_lexer(source) {}

        // -- cursor ------------------------------------------------------------

        // Current (unconsumed) token. Lex errors are reported into the
        // diagnostic list and skipped, so cur() always yields a real token.
        [[nodiscard]] constexpr const Token& cur() noexcept
        {
            if (_M_index >= _M_count)
                {
                    fill_slot(_M_count);
                }
            return _M_cache[_M_index];
        }

        [[nodiscard]] constexpr TokenKind kind() noexcept { return cur().kind; }

        // n-th lookahead token (0 == cur()). Bounded by LOOKAHEAD; requesting
        // more saturates at the last token the cache can hold.
        [[nodiscard]] constexpr const Token& peek(const size_t n) noexcept
        {
            size_t want = _M_index + n;
            if (want >= LOOKAHEAD)
                {
                    want = LOOKAHEAD - 1;
                }
            while (_M_count <= want && _M_count - _M_index < LOOKAHEAD)
                {
                    fill_slot(_M_count);
                }
            return _M_cache[want];
        }

        // Consumes and returns the current token (raw: no delimiter tracking).
        [[nodiscard]] constexpr Token consume() noexcept
        {
            const Token token = cur();
            ++_M_index;
            return token;
        }

        // Consumes `kind` if present. Tracks delimiter scopes so the unclosed
        // diagnostics at EOF can point at the opener that was never closed.
        constexpr bool consume_if(const TokenKind kind) noexcept
        {
            if (cur().kind != kind)
                {
                    return false;
                }
            note_delimiter(consume());
            return true;
        }

        // Consumes `kind` or reports "expected X, found Y" without consuming
        // (the caller decides how to recover). Never fails silently.
        [[nodiscard]] constexpr Maybe<Token, ParseError> expect(const TokenKind kind) noexcept
        {
            if (cur().kind != kind)
                {
                    if (cur().kind == TokenKind::END_OF_FILE)
                        {
                            return ParseError{ParseError::Kind::UNEXPECTED_EOF, cur().offset, kind, cur().kind};
                        }
                    return ParseError{ParseError::Kind::EXPECTED_TOKEN, cur().offset, kind, cur().kind};
                }
            return note_delimiter(consume());
        }

        [[nodiscard]] constexpr Maybe<Token, ParseError> expect_ident() noexcept
        {
            if (cur().kind != TokenKind::IDENT)
                {
                    return ParseError{ParseError::Kind::EXPECTED_IDENTIFIER, cur().offset, TokenKind::IDENT,
                                      cur().kind};
                }
            return note_delimiter(consume());
        }

        // `>>` closes two nested generic argument lists but lexes as one
        // token; when a production needs a single `>` and finds `>>`, this
        // splits it in place (the consumed half stays consumed, the remainder
        // becomes the current token).
        [[nodiscard]] constexpr Maybe<Token, ParseError> expect_gt() noexcept
        {
            if (cur().kind == TokenKind::GT)
                {
                    return note_delimiter(consume());
                }
            if (cur().kind == TokenKind::GT_GT)
                {
                    const Token wide = consume();
                    const Token first{TokenKind::GT, wide.lexeme.substr(0, 1), wide.offset, TokenFlags::NONE};
                    const Token second{TokenKind::GT, wide.lexeme.substr(1, 1), wide.offset + 1, TokenFlags::NONE};
                    insert_current(second);
                    return first;
                }
            if (cur().kind == TokenKind::END_OF_FILE)
                {
                    return ParseError{ParseError::Kind::UNEXPECTED_EOF, cur().offset, TokenKind::GT, cur().kind};
                }
            return ParseError{ParseError::Kind::EXPECTED_TOKEN, cur().offset, TokenKind::GT, cur().kind};
        }

        // `;` with recovery: a missing semicolon is one of the most common
        // errors, so it gets a dedicated path that skips to the next `;` or `}`
        // instead of cascading. Returns whether a `;` was actually present.
        constexpr bool expect_semi() noexcept
        {
            if (consume_if(TokenKind::SEMICOLON))
                {
                    return true;
                }
            rep(ParseError{ParseError::Kind::EXPECTED_TOKEN, cur().offset, TokenKind::SEMICOLON, cur().kind});
            skip_until({TokenKind::SEMICOLON, TokenKind::RBRACE});
            consume_if(TokenKind::SEMICOLON);
            return false;
        }

        // -- recovery ----------------------------------------------------------

        // Skips tokens until one of `stop` (or EOF), skipping *over* balanced
        // (), [], {} regions so a broken item's body does not resynchronize in
        // the middle of the next one. Stops BEFORE the match (leave it for the
        // caller to consume). Returns false only at EOF.
        template <size_t N>
        [[nodiscard]] constexpr bool skip_until(const TokenKind (&stop)[N]) noexcept
        {
            uint32_t depth = 0;
            while (true)
                {
                    const TokenKind k = cur().kind;
                    if (depth == 0)
                        {
                            for (size_t i = 0; i < N; ++i)
                                {
                                    if (k == stop[i])
                                        {
                                            return true;
                                        }
                                }
                        }
                    if (k == TokenKind::END_OF_FILE)
                        {
                            return false;
                        }
                    switch (k)
                        {
                            case TokenKind::LPAREN:
                            case TokenKind::LBRACKET:
                            case TokenKind::LBRACE:
                                ++depth;
                                break;
                            case TokenKind::RPAREN:
                            case TokenKind::RBRACKET:
                            case TokenKind::RBRACE:
                                if (depth == 0)
                                    {
                                        return true; // a closer the caller must see
                                    }
                                --depth;
                                break;
                            default:
                                break;
                        }
                    consume();
                }
        }

        // -- diagnostics -------------------------------------------------------

        // Records a diagnostic; returns false when the list is full (the error
        // still counts toward error_count(), so callers never mistake "storage
        // full" for "input fine"). The first PARSE_MAX_DIAGNOSTICS are kept.
        constexpr bool rep(const ParseError& error) noexcept
        {
            ++_M_error_count;
            if (_M_diag_count >= PARSE_MAX_DIAGNOSTICS)
                {
                    return false;
                }
            _M_diags[_M_diag_count] = error;
            ++_M_diag_count;
            return true;
        }

        [[nodiscard]] constexpr size_t error_count() const noexcept { return _M_error_count; }

        [[nodiscard]] constexpr bool has_errors() const noexcept { return _M_error_count != 0; }

        [[nodiscard]] constexpr size_t diag_count() const noexcept { return _M_diag_count; }

        [[nodiscard]] constexpr const ParseError& diagnostic(const size_t i) const noexcept { return _M_diags[i]; }

        // first_error omitted: read diagnostic(0) when diag_count() != 0.
        // -- speculative parse -------------------------------------------------

        [[nodiscard]] constexpr State save() noexcept
        {
            // Materialize the current token so its pre-state is on record.
            cur();
            return State{_M_pre[_M_index], _M_diag_count, _M_error_count,      _M_arena.count(),
                         _M_depth,         _M_scope_top,  _M_no_struct_literal};
        }

        constexpr void restore(const State& state) noexcept
        {
            _M_lexer.restore_pos(state.pre);
            _M_count = 0;
            _M_index = 0;
            _M_diag_count = state.diag_count;
            _M_error_count = state.error_count;
            _M_arena.rollback(state.node_count);
            _M_depth = state.depth;
            _M_scope_top = state.scope_top;
            _M_no_struct_literal = state.no_struct_literal;
        }

        // -- AST ---------------------------------------------------------------

        [[nodiscard]] constexpr Maybe<uint32_t, ParseError> make(const NodeKind kind, const Token& at) noexcept
        {
            return _M_arena.make(kind, at.offset);
        }

        [[nodiscard]] constexpr Maybe<uint32_t, ParseError> make(const NodeKind kind, const uint32_t offset) noexcept
        {
            return _M_arena.make(kind, offset);
        }

        constexpr void append(const uint32_t parent, const uint32_t child) noexcept { _M_arena.append(parent, child); }

        [[nodiscard]] constexpr AstNode& node(const uint32_t id) noexcept { return _M_arena[id]; }

        [[nodiscard]] constexpr const AstNode& node(const uint32_t id) const noexcept { return _M_arena[id]; }

        [[nodiscard]] constexpr ArenaT& arena() noexcept { return _M_arena; }

        [[nodiscard]] constexpr const ArenaT& arena() const noexcept { return _M_arena; }

        // -- context -----------------------------------------------------------

        // True while inside a construct whose `{` starts a BLOCK, not a struct
        // literal: if/while/match/for headers clear it for their sub-expressions
        // only through paren_context(). See parserExpr.cppm.
        [[nodiscard]] constexpr bool no_struct_literal() const noexcept { return _M_no_struct_literal; }

        constexpr void set_no_struct_literal(const bool v) noexcept { _M_no_struct_literal = v; }

        struct NoStructLiteralGuard
        {
            Parser& _M_parser;
            bool _M_saved = false;
            constexpr explicit NoStructLiteralGuard(Parser& p, const bool v) noexcept : _M_parser(p)
            {
                _M_saved = p._M_no_struct_literal;
                p._M_no_struct_literal = v;
            }
            constexpr ~NoStructLiteralGuard() noexcept { _M_parser._M_no_struct_literal = _M_saved; }
        };

        [[nodiscard]] constexpr uint16_t depth() const noexcept { return _M_depth; }

        // Enters one recursion level; false when the depth budget is spent
        // (the caller unwinds with a NESTING_TOO_DEEP error value).
        [[nodiscard]] constexpr bool enter() noexcept
        {
            if (_M_depth >= PARSE_MAX_DEPTH)
                {
                    rep(ParseError{ParseError::Kind::NESTING_TOO_DEEP, cur().offset, TokenKind::END_OF_FILE,
                                   cur().kind});
                    return false;
                }
            ++_M_depth;
            return true;
        }

        constexpr void exit() noexcept { --_M_depth; }

        struct DepthGuard
        {
            Parser& _M_parser;
            constexpr explicit DepthGuard(Parser& p) noexcept : _M_parser(p) {}
            constexpr ~DepthGuard() noexcept { _M_parser.exit(); }
        };

        [[nodiscard]] constexpr SimpleStringView source() const noexcept { return _M_lexer.source(); }

        [[nodiscard]] constexpr uint32_t offset() noexcept { return cur().offset; }

        [[nodiscard]] constexpr LineCol line_col(const uint32_t offset) const noexcept
        {
            return line_col_of(_M_lexer.source(), offset);
        }

        // -- delimiter scope stack ---------------------------------------------

        // Records openers/closers consumed through expect()/consume_if() so an
        // unclosed `(`/`{` at EOF can be reported against its opener.
        [[nodiscard]] constexpr const Token& note_delimiter(const Token& token) noexcept
        {
            switch (token.kind)
                {
                    case TokenKind::LPAREN:
                    case TokenKind::LBRACKET:
                    case TokenKind::LBRACE:
                        if (_M_scope_top < PARSE_MAX_DEPTH)
                            {
                                _M_scope[_M_scope_top] = token.kind;
                                _M_scope_offsets[_M_scope_top] = token.offset;
                                ++_M_scope_top;
                            }
                        break;
                    case TokenKind::RPAREN:
                    case TokenKind::RBRACKET:
                    case TokenKind::RBRACE:
                        if (_M_scope_top > 0)
                            {
                                --_M_scope_top;
                            }
                        break;
                    default:
                        break;
                }
            return token;
        }

        [[nodiscard]] constexpr size_t scope_top() const noexcept { return _M_scope_top; }

        [[nodiscard]] constexpr uint32_t scope_offset(const size_t i) const noexcept { return _M_scope_offsets[i]; }

        [[nodiscard]] constexpr TokenKind scope_kind(const size_t i) const noexcept { return _M_scope[i]; }

      private:
        static constexpr size_t LOOKAHEAD = 5;

        // Refills cache slot `slot` with the next non-error token; lex errors
        // are reported and skipped (the lexer always consumes the offending
        // span, so this loop makes progress and cannot stall).
        constexpr void fill_slot(const size_t slot) noexcept
        {
            while (true)
                {
                    const Lexer::State pre = _M_lexer.save_pos();
                    const LexResult result = _M_lexer.next();
                    if (result.has_error())
                        {
                            rep(ParseError{ParseError::Kind::LEX_ERROR, result.error().offset, TokenKind::END_OF_FILE,
                                           TokenKind::END_OF_FILE});
                            if (result.error().kind == LexError::Kind::SOURCE_TOO_LARGE)
                                {
                                    _M_cache[slot] = Token{TokenKind::END_OF_FILE, SimpleStringView{},
                                                           result.error().offset, TokenFlags::NONE};
                                    _M_pre[slot] = pre;
                                    _M_count = slot + 1;
                                    return;
                                }
                            continue;
                        }
                    _M_cache[slot] = result.value();
                    _M_pre[slot] = pre;
                    _M_count = slot + 1;
                    return;
                }
        }

        // Puts a synthesized token back as the current one (>> splitting).
        constexpr void insert_current(const Token& token) noexcept
        {
            // Compact the consumed prefix so the shift below always has room:
            // the cache keeps at most LOOKAHEAD tokens after _M_index.
            if (_M_index > 0)
                {
                    for (size_t i = _M_index; i < _M_count; ++i)
                        {
                            _M_cache[i - _M_index] = _M_cache[i];
                            _M_pre[i - _M_index] = _M_pre[i];
                        }
                    _M_count -= _M_index;
                    _M_index = 0;
                }
            for (size_t i = _M_count; i > 1; --i)
                {
                    _M_cache[i] = _M_cache[i - 1];
                    _M_pre[i] = _M_pre[i - 1];
                }
            _M_cache[1] = _M_cache[0];
            _M_pre[1] = _M_pre[0];
            _M_cache[0] = token;
            _M_count = (_M_count < LOOKAHEAD ? _M_count + 1 : _M_count);
        }

        Lexer _M_lexer;
        ArenaT _M_arena{};
        Token _M_cache[LOOKAHEAD + 1]{}; // +1 slot of shift slack for >> splitting
        Lexer::State _M_pre[LOOKAHEAD + 1]{};
        ParseError _M_diags[PARSE_MAX_DIAGNOSTICS]{};
        TokenKind _M_scope[PARSE_MAX_DEPTH]{};
        uint32_t _M_scope_offsets[PARSE_MAX_DEPTH]{};
        size_t _M_count = 0;
        size_t _M_index = 0;
        size_t _M_diag_count = 0;
        size_t _M_error_count = 0;
        size_t _M_scope_top = 0;
        uint16_t _M_depth = 0;
        bool _M_no_struct_literal = false;
    };

    // Result of a full translation-unit parse: the root node id (always
    // allocated unless the arena refused even that) plus the error count, so
    // callers can require both a tree AND zero diagnostics.
    export struct ParseResult
    {
        uint32_t root = NO_NODE;
        size_t errors = 0;

        [[nodiscard]] constexpr bool ok() const noexcept { return root != NO_NODE && errors == 0; }
    };
} // namespace embdr::lex
