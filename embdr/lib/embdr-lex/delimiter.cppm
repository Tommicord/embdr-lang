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

export module embdr.lex.delimiter;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;

  // Delimiter balance tracking — the job of clang's
  // clang/lib/Lex/MismatchedDelimiterChecker, which walks the raw token list
  // and reports mismatches before the parser ever sees them.
  //
  // Differences forced by the subset (CODE_QUALITY §1.3):
  //   - the open-delimiter stack is a fixed array (MaxDepth template
  //     parameter), not a heap container; overflow is an error value;
  //   - errors are values (Maybe), not diagnostics callbacks;
  //   - everything is constexpr so a whole TU can be checked at compile time.
  //
  // Usage: feed() every token the lexer produces; finish() after END_OF_FILE.
  // The checker remembers only the first structural problem, so a mangled
  // file yields one actionable diagnostic instead of a cascade.
  export struct DelimiterError
  {
    enum class Kind : uint8_t
    {
      MISMATCHED_CLOSE, // ']' tried to close an open '('
      EXTRA_CLOSE, // ')' with nothing open
      UNTERMINATED_OPEN, // end of input with unclosed '('
      TOO_DEEPLY_NESTED, // more than MaxDepth open delimiters
    };

    Kind kind = Kind::EXTRA_CLOSE;
    uint32_t offset = 0; // offset of the offending token
    uint32_t open_offset = 0; // where the relevant opener was (0 when none)
    TokenKind expected = TokenKind::END_OF_FILE; // opener kind the close would need
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const DelimiterError::Kind kind) noexcept
  {
    switch (kind)
      {
        case DelimiterError::Kind::MISMATCHED_CLOSE:
          return "mismatched closing delimiter";
        case DelimiterError::Kind::EXTRA_CLOSE:
          return "closing delimiter without an opener";
        case DelimiterError::Kind::UNTERMINATED_OPEN:
          return "unclosed opening delimiter";
        case DelimiterError::Kind::TOO_DEEPLY_NESTED:
          return "delimiters nested too deeply";
      }
    return "unknown delimiter error";
  }

  export template <size_t MaxDepth = 64>
  class DelimiterChecker
  {
    static_assert (MaxDepth > 0, "a delimiter checker needs room for at least one opener");

  public:
    // Feeds one token. Returns an engaged Maybe carrying `false` when the
    // token is not a delimiter (always fine) or when the balance still
    // holds; an error only for the first structural problem.
    [[nodiscard]] constexpr Maybe<bool, DelimiterError>
    feed (const Token& token) noexcept
    {
      switch (token.kind)
        {
          case TokenKind::LPAREN:
          case TokenKind::LBRACKET:
          case TokenKind::LBRACE:
            if (_M_depth >= MaxDepth)
              {
                return DelimiterError{DelimiterError::Kind::TOO_DEEPLY_NESTED, token.offset, 0, TokenKind::END_OF_FILE};
              }
            _M_open[_M_depth] = token.kind;
            _M_offsets[_M_depth] = token.offset;
            ++_M_depth;
            return true;
          case TokenKind::RPAREN:
            return close (TokenKind::LPAREN, token.offset);
          case TokenKind::RBRACKET:
            return close (TokenKind::LBRACKET, token.offset);
          case TokenKind::RBRACE:
            return close (TokenKind::LBRACE, token.offset);
          default:
            return true; // every non-delimiter token passes through
        }
    }

    // Call after the END_OF_FILE token: succeeds when no opener is left
    // open, otherwise reports the outermost unclosed delimiter.
    [[nodiscard]] constexpr Maybe<bool, DelimiterError>
    finish () noexcept
    {
      if (_M_depth == 0)
        return true;
      return DelimiterError{DelimiterError::Kind::UNTERMINATED_OPEN, _M_offsets[0], _M_offsets[0], _M_open[0]};
    }

    [[nodiscard]] constexpr size_t
    depth () const noexcept
    {
      return _M_depth;
    }

    [[nodiscard]] static constexpr size_t
    capacity () noexcept
    {
      return MaxDepth;
    }

  private:
    [[nodiscard]] constexpr Maybe<bool, DelimiterError>
    close (const TokenKind opener, const uint32_t offset) noexcept
    {
      if (_M_depth == 0)
        return DelimiterError{DelimiterError::Kind::EXTRA_CLOSE, offset, 0, TokenKind::END_OF_FILE};
      const size_t top = _M_depth - 1;
      if (_M_open[top] != opener)
        {
          return DelimiterError{DelimiterError::Kind::MISMATCHED_CLOSE, offset, _M_offsets[top], _M_open[top]};
        }
      --_M_depth;
      return true;
    }

    TokenKind _M_open[MaxDepth]{};
    uint32_t _M_offsets[MaxDepth]{};
    size_t _M_depth = 0;
  };
} // namespace embdr::lex
