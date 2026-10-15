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
#include <limits>

export module embdr.lex.lexer;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.utfEncode;
import embdr.lex.charinfo;
import embdr.lex.literal;
import embdr.lex.token;

namespace embdr::lex
{
    using charinfo::is_digit;
    using charinfo::is_horizontal_space;
    using charinfo::is_identifier_body;
    using charinfo::is_identifier_start;
    using charinfo::is_newline;
    using cxxstd::decode_utf8;
    using cxxstd::Maybe;
    using cxxstd::SimpleStringView;
    using cxxstd::Utf8Decode;
    using cxxstd::UtfError;

    // Forward-only, zero-allocation lexer over a borrowed source view.
    //
    // The architecture follows clang's clang/lib/Lex/Lexer.h wherever the
    // embedded subset allows it:
    //   - every token flows through ONE funnel (form_token, clang's
    //     FormTokenWithChars) that computes the flags and slices the lexeme, so
    //     no path can forget part of the token bookkeeping;
    //   - token boundaries and token *validation* are separate stages, exactly
    //     as clang splits LexNumberConstant (raw extent) from
    //     NumericLiteralParser (validation in literal.cppm): the lexer
    //     consumes the full glued extent of a malformed literal and lets
    //     literal.cppm pinpoint the error inside it, so the next call
    //     resynchronizes immediately;
    //   - State/save_pos/restore_pos (clang SavePos/RestorePos) make the lexer
    //     replayable: a parser can look ahead and rewind without any token
    //     buffer, because the lexer is a pure cursor over the source;
    //   - keep_comments (clang SetKeepComments) surfaces comments as tokens for
    //     doc harvesting (#reflect);
    //   - tokens carry the GCC libcpp BOL / PREV_WHITE information
    //     (TokenFlags::START_OF_LINE / LEADING_SPACE), which the parser needs
    //     to disambiguate '<' / '>' (generics vs. comparison) and '#' attributes
    //     from directives;
    //   - a UTF-8 BOM at offset 0 is skipped; line/column are tracked
    //     incrementally (O(1) per token) with \r, \n and \r\n all recognized
    //     as a single line break;
    //   - block comments nest; '///' starts a doc comment;
    //   - every failure returns a LexError value AND consumes at least one
    //     byte, so a driver can keep calling next() to resynchronize — the
    //     lexer never stalls on one offset;
    //   - everything is constexpr, so a whole translation unit can be lexed at
    //     compile time.
    //
    // Invariant: after any call the position advanced, or the source size was
    // rejected (SOURCE_TOO_LARGE) before a single byte was read.
    export class Lexer
    {
       public:
        // Complete cursor snapshot (clang's Lexer::State). Because the lexer
        // holds no state beyond the cursor and the trivia flags, save/restore
        // gives exact lookahead and backtracking with no allocation.
        struct State
        {
            size_t pos = 0;
            uint32_t line = 1;
            size_t line_start = 0;
            bool at_line_start = true;
            bool has_leading_space = false;
        };

        explicit constexpr Lexer(const SimpleStringView source) noexcept : _M_source(source)
        {
            // Strip a UTF-8 BOM once, before any position or line bookkeeping.
            // The BOM occupies no column of its own (clang counts it as one
            // character; we discount it entirely so the first real token sits
            // at column 1).
            if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
                static_cast<unsigned char>(source[1]) == 0xBB && static_cast<unsigned char>(source[2]) == 0xBF)
                {
                    _M_pos = 3;
                    _M_line_start = 3;
                }
        }

        // Returns the next token or a LexError (clang Lex). On an error (other
        // than SOURCE_TOO_LARGE) the offending span has been consumed, so the
        // next call makes progress. END_OF_FILE is returned forever once
        // exhausted.
        [[nodiscard]] constexpr LexResult next() noexcept
        {
            if (_M_source.size() > static_cast<size_t>((std::numeric_limits<uint32_t>::max)()))
                {
                    return LexError{LexError::Kind::SOURCE_TOO_LARGE, 0};
                }

            // Skip whitespace and comments; surface a comment as a token when
            // keep_comments is on.
            const LexResult trivia = skip_trivia();
            if (trivia.has_error())
                {
                    return trivia; // a genuine trivia error (unterminated comment)
                }
            if (trivia.value().kind != TokenKind::END_OF_FILE)
                {
                    return trivia; // a kept comment token
                }

            if (_M_pos >= _M_source.size())
                {
                    return form_token(TokenKind::END_OF_FILE, _M_pos);
                }

            const char c = _M_source[_M_pos];
            if (is_digit(c))
                {
                    return lex_number();
                }
            if (c == '"')
                {
                    return lex_string_literal(_M_pos, TokenKind::STRING_LITERAL, false);
                }
            if (c == '\'')
                {
                    return lex_char_constant();
                }
            if (is_identifier_start(c))
                {
                    return lex_identifier();
                }
            return lex_punctuator();
        }

        // Lexes the next token WITHOUT consuming it (save + next + restore).
        // Errors behave exactly like next(), but leave the cursor untouched.
        [[nodiscard]] constexpr LexResult peek() noexcept
        {
            const State state = save_pos();
            const LexResult result = next();
            restore_pos(state);
            return result;
        }

        // Captures the cursor for later restore_pos() — the backtracking hook
        // a parser needs for speculative lookahead.
        [[nodiscard]] constexpr State save_pos() const noexcept
        {
            return State{_M_pos, _M_line, _M_line_start, _M_at_line_start, _M_has_leading_space};
        }

        constexpr void restore_pos(const State& state) noexcept
        {
            _M_pos = state.pos;
            _M_line = state.line;
            _M_line_start = state.line_start;
            _M_at_line_start = state.at_line_start;
            _M_has_leading_space = state.has_leading_space;
        }

        // When enabled, comments are returned as LINE_COMMENT / DOC_COMMENT /
        // BLOCK_COMMENT tokens instead of being skipped (used to harvest /// doc
        // for #reflect). Off by default.
        constexpr void set_keep_comments(const bool keep) noexcept { _M_keep_comments = keep; }

        [[nodiscard]] constexpr bool keep_comments() const noexcept { return _M_keep_comments; }

        // True when the next token will begin a physical source line — the
        // predicate clang's IsAtStartOfLine gives the preprocessor to spot
        // directives.
        [[nodiscard]] constexpr bool is_at_start_of_line() const noexcept { return _M_at_line_start; }

        [[nodiscard]] constexpr SimpleStringView source() const noexcept { return _M_source; }

        [[nodiscard]] constexpr uint32_t offset() const noexcept { return static_cast<uint32_t>(_M_pos); }

        // Line/column of the current read position, maintained incrementally.
        [[nodiscard]] constexpr LineCol line_col() const noexcept
        {
            return LineCol{_M_line, static_cast<uint32_t>(_M_pos - _M_line_start) + 1};
        }

        // clang GetTokenRawLength: the byte length of the raw token starting at
        // `offset` (comments count as one token). Returns 0 at or past the end
        // of the source, and 1 for a byte the lexer rejects — every error span
        // consumes at least one byte.
        [[nodiscard]] static constexpr size_t raw_token_length(const SimpleStringView source,
                                                               const size_t offset) noexcept
        {
            if (offset >= source.size())
                {
                    return 0;
                }
            Lexer lexer{source};
            lexer.set_keep_comments(true);
            lexer._M_pos = offset;
            lexer._M_line_start = offset;
            const LexResult result = lexer.next();
            if (result.has_error())
                {
                    return 1;
                }
            return result.value().length() == 0 ? 1 : result.value().length();
        }

       private:
        // The single funnel every successful token passes through (clang
        // FormTokenWithChars): grabs the pending trivia flags and slices the
        // lexeme [start, _M_pos). Nothing else builds a Token.
        [[nodiscard]] constexpr Token form_token(const TokenKind kind, const size_t start) noexcept
        {
            return Token{kind, _M_source.substr(start, _M_pos - start), static_cast<uint32_t>(start),
                         take_pending_flags()};
        }

        [[nodiscard]] constexpr TokenFlags take_pending_flags() noexcept
        {
            TokenFlags flags = TokenFlags::NONE;
            if (_M_at_line_start)
                {
                    flags = flags | TokenFlags::START_OF_LINE;
                }
            if (_M_has_leading_space)
                {
                    flags = flags | TokenFlags::LEADING_SPACE;
                }
            _M_at_line_start = false;
            _M_has_leading_space = false;
            return flags;
        }

        // Consumes whitespace and comments (clang ReadWhitespaceAndComments).
        // Returns:
        //   - an error for an unterminated block comment;
        //   - a comment token when keep_comments is on;
        //   - an END_OF_FILE-kind token as the documented "nothing was
        //     emitted" sentinel (this function never produces a real EOF
        //     token otherwise).
        [[nodiscard]] constexpr LexResult skip_trivia() noexcept
        {
            while (true)
                {
                    if (_M_pos >= _M_source.size())
                        {
                            return Token{TokenKind::END_OF_FILE, SimpleStringView{}, static_cast<uint32_t>(_M_pos),
                                         TokenFlags::NONE};
                        }
                    const char c = _M_source[_M_pos];
                    if (is_horizontal_space(c))
                        {
                            _M_has_leading_space = true;
                            ++_M_pos;
                            continue;
                        }
                    if (is_newline(c))
                        {
                            // "\r\n" is ONE line break; a lone '\r' breaks the
                            // line too (pre-CRLF fixtures and embedded targets
                            // emit both spellings).
                            if (c == '\r' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '\n')
                                {
                                    _M_pos += 2;
                                }
                            else
                                {
                                    ++_M_pos;
                                }
                            ++_M_line;
                            _M_line_start = _M_pos;
                            _M_at_line_start = true;
                            _M_has_leading_space = false;
                            continue;
                        }
                    if (c == '/' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '/')
                        {
                            if (_M_keep_comments)
                                {
                                    return lex_line_comment();
                                }
                            // A line comment leaves its line break for the loop above.
                            while (_M_pos < _M_source.size() && !is_newline(_M_source[_M_pos]))
                                {
                                    ++_M_pos;
                                }
                            continue;
                        }
                    if (c == '/' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '*')
                        {
                            if (_M_keep_comments)
                                {
                                    return lex_block_comment();
                                }
                            skip_block_comment();
                            if (_M_unterminated_comment)
                                {
                                    return LexError{LexError::Kind::UNTERMINATED_COMMENT,
                                                    static_cast<uint32_t>(_M_unterminated_comment_offset)};
                                }
                            _M_has_leading_space = true; // a comment acts as leading space
                            continue;
                        }
                    return Token{TokenKind::END_OF_FILE, SimpleStringView{}, static_cast<uint32_t>(_M_pos),
                                 TokenFlags::NONE};
                }
        }

        [[nodiscard]] constexpr LexResult lex_line_comment() noexcept
        {
            const size_t start = _M_pos;
            const bool doc = _M_pos + 2 < _M_source.size() && _M_source[_M_pos + 2] == '/';
            while (_M_pos < _M_source.size() && !is_newline(_M_source[_M_pos]))
                {
                    ++_M_pos;
                }
            const TokenKind kind = doc ? TokenKind::DOC_COMMENT : TokenKind::LINE_COMMENT;
            return form_token(kind, start);
        }

        [[nodiscard]] constexpr LexResult lex_block_comment() noexcept
        {
            const size_t start = _M_pos;
            // skip_block_comment arms LEADING_SPACE for the token AFTER the
            // comment; the comment itself must carry the flags that were
            // pending when it started.
            const bool at_line_start = _M_at_line_start;
            const bool has_leading_space = _M_has_leading_space;
            skip_block_comment();
            if (_M_unterminated_comment)
                {
                    // Position is already at end, so the next call reaches EOF
                    // instead of looping.
                    return LexError{LexError::Kind::UNTERMINATED_COMMENT,
                                    static_cast<uint32_t>(_M_unterminated_comment_offset)};
                }
            _M_at_line_start = at_line_start;
            _M_has_leading_space = has_leading_space;
            const Token token = form_token(TokenKind::BLOCK_COMMENT, start);
            _M_has_leading_space = true; // a comment acts as leading space
            return token;
        }

        // Shared block-comment scanner for both the kept and the skipped path:
        // nested /* /* */ */ pairs, line breaks counted. Records an
        // unterminated open in _M_unterminated_comment instead of returning a
        // value, so the two callers can attach the right token flags.
        constexpr void skip_block_comment() noexcept
        {
            const size_t open = _M_pos;
            _M_unterminated_comment = false;
            _M_pos += 2;
            uint32_t depth = 1;
            while (_M_pos < _M_source.size() && depth != 0)
                {
                    const char b = _M_source[_M_pos];
                    if (b == '/' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '*')
                        {
                            ++depth;
                            _M_pos += 2;
                        }
                    else if (b == '*' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '/')
                        {
                            --depth;
                            _M_pos += 2;
                        }
                    else
                        {
                            if (is_newline(b))
                                {
                                    // Fold "\r\n" like the trivia loop does.
                                    if (b == '\r' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '\n')
                                        {
                                            _M_pos += 2;
                                        }
                                    else
                                        {
                                            ++_M_pos;
                                        }
                                    ++_M_line;
                                    _M_line_start = _M_pos;
                                    continue;
                                }
                            ++_M_pos;
                        }
                }
            if (depth != 0)
                {
                    _M_unterminated_comment = true;
                    _M_unterminated_comment_offset = open;
                }
            else
                {
                    // Any comment acts as leading space for the next token; a
                    // multi-line one does NOT put it back at a line start.
                    _M_has_leading_space = true;
                }
        }

        [[nodiscard]] constexpr LexResult lex_identifier() noexcept
        {
            const size_t start = _M_pos;
            while (_M_pos < _M_source.size() && is_identifier_body(_M_source[_M_pos]))
                {
                    ++_M_pos;
                }
            const SimpleStringView text = _M_source.substr(start, _M_pos - start);

            // A string prefix must be glued to the quote: u8" u16" u32" r" p".
            if (_M_pos < _M_source.size() && _M_source[_M_pos] == '"')
                {
                    if (text == "u8")
                        return lex_string_literal(start, TokenKind::STRING_LITERAL, false);
                    if (text == "u16")
                        return lex_string_literal(start, TokenKind::STRING_LITERAL_U16, false);
                    if (text == "u32")
                        return lex_string_literal(start, TokenKind::STRING_LITERAL_U32, false);
                    if (text == "r")
                        return lex_string_literal(start, TokenKind::RAW_STRING_LITERAL, true);
                    if (text == "p")
                        return lex_string_literal(start, TokenKind::PATTERN_LITERAL, true);
                    // "u<digits>" glued to a quote is a wrong spelling of an
                    // integer type (u32 is a *string* prefix; integer types
                    // are uint32).
                    if (text[0] == 'u')
                        {
                            bool all_digits = true;
                            for (size_t i = 1; i < text.size(); ++i)
                                {
                                    if (!is_digit(text[i]))
                                        {
                                            all_digits = false;
                                            break;
                                        }
                                }
                            if (all_digits)
                                {
                                    // Consume the prefix so the next call starts
                                    // at the quote instead of re-lexing the same
                                    // prefix forever.
                                    return LexError{LexError::Kind::INVALID_PREFIX, static_cast<uint32_t>(start)};
                                }
                        }
                    // Any other ident + " is a separate identifier followed by a string.
                }

            return form_token(keyword_kind(text), start);
        }

        // start = offset of the first prefix char (or the quote itself).
        [[nodiscard]] constexpr LexResult lex_string_literal(const size_t start, const TokenKind kind,
                                                             const bool raw) noexcept
        {
            _M_pos = start;
            while (_M_pos < _M_source.size() && _M_source[_M_pos] != '"')
                {
                    ++_M_pos; // skip u8 / u16 / u32 / r / p (or nothing)
                }
            ++_M_pos; // consume the opening quote
            while (true)
                {
                    if (_M_pos >= _M_source.size())
                        {
                            return LexError{LexError::Kind::UNTERMINATED_STRING, static_cast<uint32_t>(start)};
                        }
                    const char c = _M_source[_M_pos];
                    if (c == '"')
                        {
                            ++_M_pos;
                            return form_token(kind, start);
                        }
                    if (is_newline(c))
                        {
                            if (!raw)
                                {
                                    return LexError{LexError::Kind::UNTERMINATED_STRING, static_cast<uint32_t>(start)};
                                }
                            // Raw strings may span lines: keep the bookkeeping right.
                            if (c == '\r' && _M_pos + 1 < _M_source.size() && _M_source[_M_pos + 1] == '\n')
                                {
                                    _M_pos += 2;
                                }
                            else
                                {
                                    ++_M_pos;
                                }
                            ++_M_line;
                            _M_line_start = _M_pos;
                            continue;
                        }
                    if (c == '\\' && !raw)
                        {
                            const EscapeResult esc = scan_escape(_M_source, _M_pos);
                            if (!esc.ok)
                                {
                                    return LexError{LexError::Kind::INVALID_ESCAPE, static_cast<uint32_t>(start)};
                                }
                            _M_pos = esc.next;
                            continue;
                        }
                    if (static_cast<unsigned char>(c) >= 0x80)
                        {
                            const Utf8Decode decoded =
                                decode_utf8(_M_source.data() + _M_pos, _M_source.size() - _M_pos);
                            if (decoded._M_error != UtfError::NONE)
                                {
                                    return LexError{LexError::Kind::INVALID_UTF8, static_cast<uint32_t>(_M_pos)};
                                }
                            _M_pos += decoded._M_consumed;
                            continue;
                        }
                    ++_M_pos;
                }
        }

        [[nodiscard]] constexpr LexResult lex_char_constant() noexcept
        {
            const size_t open = _M_pos;
            ++_M_pos; // consume '\''
            if (_M_pos >= _M_source.size() || is_newline(_M_source[_M_pos]))
                {
                    return LexError{LexError::Kind::UNTERMINATED_CHAR, static_cast<uint32_t>(open)};
                }
            if (_M_source[_M_pos] == '\'')
                {
                    return LexError{LexError::Kind::INVALID_CHAR, static_cast<uint32_t>(open)};
                }
            if (_M_source[_M_pos] == '\\')
                {
                    const EscapeResult esc = scan_escape(_M_source, _M_pos);
                    if (!esc.ok)
                        {
                            return LexError{LexError::Kind::INVALID_ESCAPE, static_cast<uint32_t>(open)};
                        }
                    _M_pos = esc.next;
                }
            else if (static_cast<unsigned char>(_M_source[_M_pos]) >= 0x80)
                {
                    const Utf8Decode decoded = decode_utf8(_M_source.data() + _M_pos, _M_source.size() - _M_pos);
                    if (decoded._M_error != UtfError::NONE)
                        {
                            return LexError{LexError::Kind::INVALID_UTF8, static_cast<uint32_t>(_M_pos)};
                        }
                    _M_pos += decoded._M_consumed;
                }
            else
                {
                    ++_M_pos;
                }
            // Running into end of file or a line break means the literal was
            // never closed — that is UNTERMINATED_CHAR, not a bad character.
            if (_M_pos >= _M_source.size() || is_newline(_M_source[_M_pos]))
                {
                    return LexError{LexError::Kind::UNTERMINATED_CHAR, static_cast<uint32_t>(open)};
                }
            if (_M_source[_M_pos] != '\'')
                {
                    return LexError{LexError::Kind::INVALID_CHAR, static_cast<uint32_t>(open)};
                }
            ++_M_pos; // consume the closing '\''
            return form_token(TokenKind::CHAR_LITERAL, open);
        }

        // Lexes the full glued extent of a numeric literal, then validates it
        // with literal.cppm (clang LexNumberConstant + NumericLiteralParser):
        // the extent scan is radix-agnostic on purpose — "0b12" must stay ONE
        // span so the diagnostic can blame the '2' and the next call still
        // resynchronizes.
        [[nodiscard]] constexpr LexResult lex_number() noexcept
        {
            const size_t start = _M_pos;
            size_t i = start;
            bool decimal = true;
            if (_M_source[i] == '0' && i + 1 < _M_source.size())
                {
                    const char marker = _M_source[i + 1];
                    if (marker == 'x' || marker == 'X' || marker == 'o' || marker == 'O' || marker == 'b' ||
                        marker == 'B')
                        {
                            decimal = false;
                            i += 2;
                        }
                }
            while (i < _M_source.size())
                {
                    const char c = _M_source[i];
                    if (is_identifier_body(c))
                        {
                            ++i; // digits, separator '_' and the suffix letters
                            continue;
                        }
                    if (c == '.')
                        {
                            if (i + 1 < _M_source.size() && _M_source[i + 1] == '.')
                                {
                                    break; // ".." is the range operator, never part of the number
                                }
                            if (decimal && (i + 1 >= _M_source.size() || !is_digit(_M_source[i + 1])))
                                {
                                    break; // member access on a number: "1.foo"
                                }
                            ++i;
                            continue;
                        }
                    if (!decimal)
                        {
                            break;
                        }
                    // An exponent sign belongs to the literal only between
                    // 'e'/'E' and a digit, so "1-5" and "1e-5" split correctly.
                    if ((c == '+' || c == '-') && i > 0 && (_M_source[i - 1] == 'e' || _M_source[i - 1] == 'E') &&
                        i + 1 < _M_source.size() && is_digit(_M_source[i + 1]))
                        {
                            ++i;
                            continue;
                        }
                    break;
                }
            _M_pos = i;

            const SimpleStringView lexeme = _M_source.substr(start, i - start);
            const Maybe<NumericLiteralInfo, NumericError> parsed = parse_numeric_literal(lexeme);
            if (parsed.has_error())
                {
                    // The whole glued extent stays consumed, so the next call
                    // cannot stall inside a half-lexed literal.
                    return LexError{numeric_error_kind(parsed.error().kind),
                                    static_cast<uint32_t>(start + parsed.error().offset)};
                }
            const TokenKind kind = parsed.value().is_float ? TokenKind::FLOAT_LITERAL : TokenKind::INT_LITERAL;
            return form_token(kind, start);
        }

        [[nodiscard]] constexpr LexResult lex_punctuator() noexcept
        {
            const size_t start = _M_pos;
            const SimpleStringView rest = _M_source.substr(start);
            for (const PunctEntry& entry : _S_punct_table)
                {
                    if (rest.starts_with(entry.str))
                        {
                            _M_pos = start + entry.str.size();
                            return form_token(entry.kind, start);
                        }
                }
            // Unknown byte: consume it so the next call cannot stall here.
            ++_M_pos;
            return LexError{LexError::Kind::UNEXPECTED_CHAR, static_cast<uint32_t>(start)};
        }

        // Maps literal.cppm diagnostics onto the lexer's error vocabulary.
        [[nodiscard]] static constexpr LexError::Kind numeric_error_kind(const NumericErrorKind kind) noexcept
        {
            switch (kind)
                {
                    case NumericErrorKind::INVALID_SUFFIX:
                    case NumericErrorKind::SUFFIX_MISMATCH:
                        return LexError::Kind::INVALID_SUFFIX;
                    default:
                        return LexError::Kind::INVALID_NUMBER;
                }
        }

        struct PunctEntry
        {
            SimpleStringView str;
            TokenKind kind;
        };

        // Longest-first: the first starts_with() hit wins (maximal munch).
        static constexpr PunctEntry _S_punct_table[] = {
            {"<<=", TokenKind::LSHIFT_ASSIGN},
            {">>=", TokenKind::RSHIFT_ASSIGN},
            {"..=", TokenKind::DOT_DOT_ASSIGN},
            {"...", TokenKind::ELLIPSIS},
            {"|>", TokenKind::PIPE_GT},
            {"::", TokenKind::COLON_COLON},
            {"->", TokenKind::ARROW},
            {"..", TokenKind::DOT_DOT},
            {"&&", TokenKind::AMP_AMP},
            {"||", TokenKind::PIPE_PIPE},
            {"==", TokenKind::EQ_EQ},
            {"!=", TokenKind::BANG_EQ},
            {"<=", TokenKind::LT_EQ},
            {">=", TokenKind::GT_EQ},
            {"<<", TokenKind::LT_LT},
            {">>", TokenKind::GT_GT},
            {"+=", TokenKind::PLUS_EQ},
            {"-=", TokenKind::MINUS_EQ},
            {"*=", TokenKind::STAR_EQ},
            {"/=", TokenKind::SLASH_EQ},
            {"%=", TokenKind::PERCENT_EQ},
            {"&=", TokenKind::AMP_EQ},
            {"|=", TokenKind::PIPE_EQ},
            {"^=", TokenKind::CARET_EQ},
            {"??", TokenKind::QUESTION_QUESTION},
            {"?", TokenKind::QUESTION},
            {"!", TokenKind::BANG},
            {"~", TokenKind::TILDE},
            {"+", TokenKind::PLUS},
            {"-", TokenKind::MINUS},
            {"*", TokenKind::STAR},
            {"/", TokenKind::SLASH},
            {"%", TokenKind::PERCENT},
            {"^", TokenKind::CARET},
            {"&", TokenKind::AMP},
            {"|", TokenKind::PIPE},
            {"<", TokenKind::LT},
            {">", TokenKind::GT},
            {"=", TokenKind::EQ},
            {"(", TokenKind::LPAREN},
            {")", TokenKind::RPAREN},
            {"{", TokenKind::LBRACE},
            {"}", TokenKind::RBRACE},
            {"[", TokenKind::LBRACKET},
            {"]", TokenKind::RBRACKET},
            {";", TokenKind::SEMICOLON},
            {",", TokenKind::COMMA},
            {".", TokenKind::DOT},
            {":", TokenKind::COLON},
            {"#", TokenKind::HASH},
            {"$", TokenKind::DOLLAR},
        };

        SimpleStringView _M_source;
        size_t _M_pos = 0;
        uint32_t _M_line = 1;
        size_t _M_line_start = 0;
        size_t _M_unterminated_comment_offset = 0;
        bool _M_at_line_start = true;
        bool _M_has_leading_space = false;
        bool _M_keep_comments = false;
        bool _M_unterminated_comment = false;
    };
} // namespace embdr::lex
