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

export module embdr.lex.parserTentative;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserType;
import embdr.lex.token;

namespace embdr::lex
{
    using cxxstd::Maybe;

    // Tentative parse: after a path, try `< type (, type)* >` and commit only
    // when the trailing token is a call/struct-literal opener (so `foo < bar`
    // comparison is rejected and rolled back without leaving diagnostics).
    export template <typename ArenaT>
    [[nodiscard]] constexpr Maybe<uint32_t, ParseError> try_parse_generic_args(Parser<ArenaT>& p) noexcept
    {
        const typename Parser<ArenaT>::State saved = p.save();
        (void)p.set_no_struct_literal(p.no_struct_literal()); // keep current ctx
        // Save quiet flag via State: save outer quiet by restoring at end on both
        // paths; ensure speculative diagnostics never leak on rollback.
        if (const Maybe<Token, ParseError> lt = p.expect(TokenKind::LT); lt.has_error())
            {
                p.restore(saved);
                return lt.error();
            }
        // Parse at least one type.
        const Maybe<uint32_t, ParseError> first = parse_type(p);
        if (first.has_error())
            {
                p.restore(saved);
                return first.error();
            }
        // Warn: need at least one arg already consumed; collect extras.
        // Build args node from the first consumed arg by re-lexing... Instead,
        // since parse_type consumed, we wrap later.
        // For tentative success we only need a committed node on commit.
        // Consume `, type` pairs (best-effort; failure -> not-generics).
        Maybe<uint32_t, ParseError> second_saved = first;
        while (p.kind() == TokenKind::COMMA)
            {
                p.consume(); // ","
                const Maybe<uint32_t, ParseError> arg = parse_type(p);
                if (arg.has_error())
                    {
                        second_saved = arg;
                        second_saved = first; // ignore; mark failure
                        p.restore(saved);
                        return ParseError{ParseError::Kind::UNEXPECTED_TOKEN, p.offset(), TokenKind::GT, p.kind()};
                    }
            }
        if (p.kind() != TokenKind::GT && p.kind() != TokenKind::GT_GT)
            {
                p.restore(saved);
                return ParseError{ParseError::Kind::EXPECTED_TOKEN, p.offset(), TokenKind::GT, p.kind()};
            }
        p.consume(); // GT (or the consumed half)
        // follow check: commit only if a call/struct-literal follows
        const TokenKind follow = p.kind();
        if (follow != TokenKind::LPAREN && follow != TokenKind::LBRACE)
            {
                p.restore(saved);
                return ParseError{ParseError::Kind::UNEXPECTED_TOKEN, p.offset(), TokenKind::LPAREN, p.kind()};
            }
        // Build generic params node from rollback... already rolled? No, we kept.
        // Reconstruct: we need an args node. Since parse_generic_args already
        // exists (type-side), replicate simply by collecting from first arg's
        // path back is complex. Instead, build an empty args node + re-parse.
        // But we already consumed. Solution: rollback saved and call
        // parse_generic_args legitimately with the follow already seen? No follow
        // consumed... follow stays.
        // Simplest commit: call parse_generic_args which re-saves its own state
        // and re-parses the same `<...>` (idempotent). To avoid double
        // consumption, restore first then parse_generic_args.
        p.restore(saved);
        return parse_generic_args(p);
    }
} // namespace embdr::lex
