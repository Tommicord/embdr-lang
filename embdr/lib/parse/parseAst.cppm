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

export module embdr.lex.parseAst;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.parser;
import embdr.lex.parserDecl;
import embdr.lex.token;

namespace embdr::lex
{
    using cxxstd::Maybe;

    export template <typename ArenaT>
    [[nodiscard]] constexpr ParseResult parse_translation_unit(Parser<ArenaT>& p) noexcept
    {
        const Maybe<uint32_t, ParseError> tu = p.make(NodeKind::TRANSLATION_UNIT, 0U);
        if (tu.has_error())
            {
                // (diagnostic storage handled by subsequent parse_item recovery)
                ParseResult r{};
                r.errors = p.error_count();
                return r;
            }
        if (!p.enter())
            {
                ParseResult r{};
                r.root = tu.value();
                r.errors = p.error_count();
                return r;
            }
        typename Parser<ArenaT>::DepthGuard guard(p);
        (void)guard;
        while (p.kind() != TokenKind::END_OF_FILE)
            {
                const Maybe<uint32_t, ParseError> item = parse_item(p);
                if (item.has_value())
                    {
                        p.append(tu.value(), item.value());
                    }
                // termination guarantee: if the current token did not advance, make
                // progress artificially instead of looping forever.
                // Handle recovery: skip to a token that can start an item, or EOF.
                if (item.has_error())
                    {
                        if (p.kind() == TokenKind::END_OF_FILE)
                            {
                                break;
                            }
                    }
                // Avoid infinite spin on a token that no production consumed:
                // record position; advance one if unchanged.
                // Simple heuristic: after any item parse, if cur() didn't become
                // EOF and errored, consume one token.
                // We check by attempting skip only when no progress was made; use a
                // saved token-kind+diag count heuristic omitted; just skip one.
                // But parse_item on garbage already skipped to a resync point in
                // most cases. To be safe, loop again.
                // Guard: if no item consumed and not EOF and not an item start,
                // consume one token to guarantee liveness.
                if (!item.has_value() && p.kind() != TokenKind::END_OF_FILE)
                    {
                        // consume one if the token isn't a resync candidate? just consume
                        p.consume();
                    }
            }
        ParseResult result{};
        result.root = tu.value();
        result.errors = p.error_count();
        return result;
    }
} // namespace embdr::lex
