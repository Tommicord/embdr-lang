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

export module embdr.lex.token;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;

namespace embdr::lex
{
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;

  // Mirrors syntax.txt §24: identifiers, the reserved keyword set, every literal
  // form and the maximal-munch punctuation vocabulary. Numeric type *names* are
  // not keywords — they are literal suffixes (see INT_SUFFIX / FLOAT_SUFFIX).
  //
  // The grouping is the .def-file contract, exactly as clang's TokenKinds.def
  // relies on it: identifiers and literals first, then trivia, then keywords
  // (KW_AS..KW_WHILE), then punctuators (ELLIPSIS..DOLLAR), then END_OF_FILE.
  // is_keyword()/is_punctuator()/is_literal()/is_trivia() below classify by
  // range, so a new enumerator must be added inside its group.
  export enum class TokenKind : uint16_t {
    IDENT,
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    STRING_LITERAL_U16,
    STRING_LITERAL_U32,
    RAW_STRING_LITERAL,
    PATTERN_LITERAL,
    CHAR_LITERAL,

    // Trivia. Emitted only when the lexer is asked to keep comments (doc
    // extraction for #reflect); otherwise skipped like whitespace.
    LINE_COMMENT,
    DOC_COMMENT,
    BLOCK_COMMENT,

    KW_AS,
    KW_BREAK,
    KW_CEXPR,
    KW_CONTINUE,
    KW_DEFER,
    KW_ELSE,
    KW_EMBED,
    KW_ENUM,
    KW_EXTERN,
    KW_FALSE,
    KW_FN,
    KW_FOR,
    KW_FUNCTION,
    KW_IF,
    KW_IN,
    KW_INTERRUPT,
    KW_IS,
    KW_LET,
    KW_LOOP,
    KW_MACRO,
    KW_MATCH,
    KW_MODULE,
    KW_PACKED,
    KW_PUB,
    KW_REF,
    KW_RET,
    KW_SELF,
    KW_STATIC,
    KW_STRUCT,
    KW_TRUE,
    KW_USE,
    KW_VOLATILE,
    KW_WHILE,

    ELLIPSIS,
    LSHIFT_ASSIGN,
    RSHIFT_ASSIGN,
    DOT_DOT_ASSIGN,
    PIPE_GT,
    COLON_COLON,
    ARROW,
    DOT_DOT,
    AMP_AMP,
    PIPE_PIPE,
    EQ_EQ,
    BANG_EQ,
    LT_EQ,
    GT_EQ,
    LT_LT,
    GT_GT,
    PLUS_EQ,
    MINUS_EQ,
    STAR_EQ,
    SLASH_EQ,
    PERCENT_EQ,
    AMP_EQ,
    PIPE_EQ,
    CARET_EQ,
    QUESTION_QUESTION,
    QUESTION,
    BANG,
    TILDE,
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    CARET,
    AMP,
    PIPE,
    LT,
    GT,
    EQ,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    SEMICOLON,
    COMMA,
    DOT,
    COLON,
    HASH,
    DOLLAR,

    END_OF_FILE,
  };

  // Kind classification by enumerator range — the generated helpers of clang's
  // TokenKinds.def (tok::isKeyword, tok::isPunctuation, ...), relying on the
  // grouping documented on TokenKind.
  export [[nodiscard]] constexpr bool is_identifier(const TokenKind kind) noexcept { return kind == TokenKind::IDENT; }

  export [[nodiscard]] constexpr bool is_literal(const TokenKind kind) noexcept
  {
    return kind >= TokenKind::INT_LITERAL && kind <= TokenKind::CHAR_LITERAL;
  }

  export [[nodiscard]] constexpr bool is_trivia(const TokenKind kind) noexcept
  {
    return kind >= TokenKind::LINE_COMMENT && kind <= TokenKind::BLOCK_COMMENT;
  }

  export [[nodiscard]] constexpr bool is_keyword(const TokenKind kind) noexcept
  {
    return kind >= TokenKind::KW_AS && kind <= TokenKind::KW_WHILE;
  }

  export [[nodiscard]] constexpr bool is_punctuator(const TokenKind kind) noexcept
  {
    return kind >= TokenKind::ELLIPSIS && kind <= TokenKind::DOLLAR;
  }

  // Reserved keywords per syntax.txt §24. Bucketed by length so the common
  // path is a size switch plus a couple of compares — no .rodata keyword
  // table, where clang would use its sorted KeywordTable lookup (binary size
  // outranks lookup speed here, CODE_QUALITY §1).
  export [[nodiscard]] constexpr TokenKind keyword_kind(const SimpleStringView text) noexcept
  {
    switch (text.size())
      {
        case 2:
          if (text == "as")
            return TokenKind::KW_AS;
          if (text == "fn")
            return TokenKind::KW_FN;
          if (text == "if")
            return TokenKind::KW_IF;
          if (text == "in")
            return TokenKind::KW_IN;
          if (text == "is")
            return TokenKind::KW_IS;
          break;
        case 3:
          if (text == "for")
            return TokenKind::KW_FOR;
          if (text == "let")
            return TokenKind::KW_LET;
          if (text == "pub")
            return TokenKind::KW_PUB;
          if (text == "ref")
            return TokenKind::KW_REF;
          if (text == "ret")
            return TokenKind::KW_RET;
          if (text == "use")
            return TokenKind::KW_USE;
          break;
        case 4:
          if (text == "else")
            return TokenKind::KW_ELSE;
          if (text == "enum")
            return TokenKind::KW_ENUM;
          if (text == "loop")
            return TokenKind::KW_LOOP;
          if (text == "self")
            return TokenKind::KW_SELF;
          if (text == "true")
            return TokenKind::KW_TRUE;
          break;
        case 5:
          if (text == "break")
            return TokenKind::KW_BREAK;
          if (text == "cexpr")
            return TokenKind::KW_CEXPR;
          if (text == "defer")
            return TokenKind::KW_DEFER;
          if (text == "embed")
            return TokenKind::KW_EMBED;
          if (text == "false")
            return TokenKind::KW_FALSE;
          if (text == "macro")
            return TokenKind::KW_MACRO;
          if (text == "match")
            return TokenKind::KW_MATCH;
          if (text == "while")
            return TokenKind::KW_WHILE;
          break;
        case 6:
          if (text == "extern")
            return TokenKind::KW_EXTERN;
          if (text == "module")
            return TokenKind::KW_MODULE;
          if (text == "packed")
            return TokenKind::KW_PACKED;
          if (text == "static")
            return TokenKind::KW_STATIC;
          if (text == "struct")
            return TokenKind::KW_STRUCT;
          break;
        case 8:
          if (text == "continue")
            return TokenKind::KW_CONTINUE;
          if (text == "function")
            return TokenKind::KW_FUNCTION;
          if (text == "volatile")
            return TokenKind::KW_VOLATILE;
          break;
        case 9:
          if (text == "interrupt")
            return TokenKind::KW_INTERRUPT;
          break;
        default:
          break;
      }
    return TokenKind::IDENT;
  }

  // Carries the GCC "PREV_WHITE"/"BOL" information forward to the parser without
  // forcing it to re-lex: START_OF_LINE marks the first token of a physical line,
  // LEADING_SPACE marks horizontal whitespace (or a comment) immediately before
  // the token. The parser needs both to disambiguate '<'/'>' (generics versus
  // comparison) and '#' attributes from directives.
  export enum class TokenFlags : uint8_t {
    NONE = 0,
    START_OF_LINE = 1U << 0U,
    LEADING_SPACE = 1U << 1U,
  };

  export [[nodiscard]] constexpr TokenFlags operator|(const TokenFlags lhs, const TokenFlags rhs) noexcept
  {
    return static_cast<TokenFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
  }

  export [[nodiscard]] constexpr TokenFlags operator&(const TokenFlags lhs, const TokenFlags rhs) noexcept
  {
    return static_cast<TokenFlags>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
  }

  export [[nodiscard]] constexpr bool any(const TokenFlags flags) noexcept { return static_cast<uint8_t>(flags) != 0; }

  export struct Token
  {
    // clang keeps (Location, TokenLength) and re-reads the text from the
    // source manager; we keep the lexeme view instead, so it is the same
    // information without the lookup.
    [[nodiscard]] constexpr uint32_t length() const noexcept { return static_cast<uint32_t>(lexeme.size()); }

    TokenKind kind = TokenKind::END_OF_FILE;
    SimpleStringView lexeme{};
    uint32_t offset = 0;
    TokenFlags flags = TokenFlags::NONE;
  };

  export struct LexError
  {
    enum class Kind : uint8_t
    {
      UNEXPECTED_CHAR,
      UNTERMINATED_COMMENT,
      UNTERMINATED_STRING,
      UNTERMINATED_CHAR,
      INVALID_ESCAPE,
      INVALID_NUMBER,
      INVALID_SUFFIX,
      INVALID_PREFIX,
      INVALID_CHAR,
      INVALID_UTF8,
      SOURCE_TOO_LARGE,
    };

    Kind kind = Kind::UNEXPECTED_CHAR;
    uint32_t offset = 0;
  };

  export struct LineCol
  {
    uint32_t line = 1;
    uint32_t col = 1;
  };

  // Maps a byte offset to a 1-based (line, col). '\r', '\n' and '\r\n' all
  // end a line, matching the lexer's incremental bookkeeping.
  export [[nodiscard]] constexpr LineCol line_col_of(const SimpleStringView source, const uint32_t offset) noexcept
  {
    const size_t pos = offset <= source.size() ? static_cast<size_t>(offset) : source.size();
    uint32_t line = 1;
    size_t line_start = 0;
    for (size_t i = 0; i < pos; ++i)
      {
        if (source[i] == '\r')
          {
            if (i + 1 < pos && source[i + 1] == '\n')
              {
                ++i; // "\r\n" is one break
              }
            ++line;
            line_start = i + 1;
          }
        else if (source[i] == '\n')
          {
            ++line;
            line_start = i + 1;
          }
      }
    return LineCol{line, static_cast<uint32_t>(pos - line_start) + 1};
  }

  export [[nodiscard]] constexpr SimpleStringView as_str(const LexError::Kind kind) noexcept
  {
    switch (kind)
      {
        case LexError::Kind::UNEXPECTED_CHAR:
          return "unexpected character";
        case LexError::Kind::UNTERMINATED_COMMENT:
          return "unterminated block comment";
        case LexError::Kind::UNTERMINATED_STRING:
          return "unterminated string literal";
        case LexError::Kind::UNTERMINATED_CHAR:
          return "unterminated character literal";
        case LexError::Kind::INVALID_ESCAPE:
          return "invalid escape sequence";
        case LexError::Kind::INVALID_NUMBER:
          return "invalid number literal";
        case LexError::Kind::INVALID_SUFFIX:
          return "invalid literal suffix";
        case LexError::Kind::INVALID_PREFIX:
          return "invalid literal prefix";
        case LexError::Kind::INVALID_CHAR:
          return "invalid character literal";
        case LexError::Kind::INVALID_UTF8:
          return "invalid UTF-8 sequence";
        case LexError::Kind::SOURCE_TOO_LARGE:
          return "source file is too large";
      }
    return "unknown lex error";
  }

  export [[nodiscard]] constexpr SimpleStringView as_str(const TokenKind kind) noexcept
  {
    switch (kind)
      {
        case TokenKind::IDENT:
          return "identifier";
        case TokenKind::INT_LITERAL:
          return "integer literal";
        case TokenKind::FLOAT_LITERAL:
          return "float literal";
        case TokenKind::STRING_LITERAL:
          return "string literal";
        case TokenKind::STRING_LITERAL_U16:
          return "u16 string literal";
        case TokenKind::STRING_LITERAL_U32:
          return "u32 string literal";
        case TokenKind::RAW_STRING_LITERAL:
          return "raw string literal";
        case TokenKind::PATTERN_LITERAL:
          return "pattern literal";
        case TokenKind::CHAR_LITERAL:
          return "character literal";
        case TokenKind::LINE_COMMENT:
          return "line comment";
        case TokenKind::DOC_COMMENT:
          return "doc comment";
        case TokenKind::BLOCK_COMMENT:
          return "block comment";
        case TokenKind::KW_AS:
          return "as";
        case TokenKind::KW_BREAK:
          return "break";
        case TokenKind::KW_CEXPR:
          return "cexpr";
        case TokenKind::KW_CONTINUE:
          return "continue";
        case TokenKind::KW_DEFER:
          return "defer";
        case TokenKind::KW_ELSE:
          return "else";
        case TokenKind::KW_EMBED:
          return "embed";
        case TokenKind::KW_ENUM:
          return "enum";
        case TokenKind::KW_EXTERN:
          return "extern";
        case TokenKind::KW_FALSE:
          return "false";
        case TokenKind::KW_FN:
          return "fn";
        case TokenKind::KW_FOR:
          return "for";
        case TokenKind::KW_FUNCTION:
          return "function";
        case TokenKind::KW_IF:
          return "if";
        case TokenKind::KW_IN:
          return "in";
        case TokenKind::KW_INTERRUPT:
          return "interrupt";
        case TokenKind::KW_IS:
          return "is";
        case TokenKind::KW_LET:
          return "let";
        case TokenKind::KW_LOOP:
          return "loop";
        case TokenKind::KW_MACRO:
          return "macro";
        case TokenKind::KW_MATCH:
          return "match";
        case TokenKind::KW_MODULE:
          return "module";
        case TokenKind::KW_PACKED:
          return "packed";
        case TokenKind::KW_PUB:
          return "pub";
        case TokenKind::KW_REF:
          return "ref";
        case TokenKind::KW_RET:
          return "ret";
        case TokenKind::KW_SELF:
          return "self";
        case TokenKind::KW_STATIC:
          return "static";
        case TokenKind::KW_STRUCT:
          return "struct";
        case TokenKind::KW_TRUE:
          return "true";
        case TokenKind::KW_USE:
          return "use";
        case TokenKind::KW_VOLATILE:
          return "volatile";
        case TokenKind::KW_WHILE:
          return "while";
        case TokenKind::ELLIPSIS:
          return "...";
        case TokenKind::LSHIFT_ASSIGN:
          return "<<=";
        case TokenKind::RSHIFT_ASSIGN:
          return ">>=";
        case TokenKind::DOT_DOT_ASSIGN:
          return "..=";
        case TokenKind::PIPE_GT:
          return "|>";
        case TokenKind::COLON_COLON:
          return "::";
        case TokenKind::ARROW:
          return "->";
        case TokenKind::DOT_DOT:
          return "..";
        case TokenKind::AMP_AMP:
          return "&&";
        case TokenKind::PIPE_PIPE:
          return "||";
        case TokenKind::EQ_EQ:
          return "==";
        case TokenKind::BANG_EQ:
          return "!=";
        case TokenKind::LT_EQ:
          return "<=";
        case TokenKind::GT_EQ:
          return ">=";
        case TokenKind::LT_LT:
          return "<<";
        case TokenKind::GT_GT:
          return ">>";
        case TokenKind::PLUS_EQ:
          return "+=";
        case TokenKind::MINUS_EQ:
          return "-=";
        case TokenKind::STAR_EQ:
          return "*=";
        case TokenKind::SLASH_EQ:
          return "/=";
        case TokenKind::PERCENT_EQ:
          return "%=";
        case TokenKind::AMP_EQ:
          return "&=";
        case TokenKind::PIPE_EQ:
          return "|=";
        case TokenKind::CARET_EQ:
          return "^=";
        case TokenKind::QUESTION_QUESTION:
          return "??";
        case TokenKind::QUESTION:
          return "?";
        case TokenKind::BANG:
          return "!";
        case TokenKind::TILDE:
          return "~";
        case TokenKind::PLUS:
          return "+";
        case TokenKind::MINUS:
          return "-";
        case TokenKind::STAR:
          return "*";
        case TokenKind::SLASH:
          return "/";
        case TokenKind::PERCENT:
          return "%";
        case TokenKind::CARET:
          return "^";
        case TokenKind::AMP:
          return "&";
        case TokenKind::PIPE:
          return "|";
        case TokenKind::LT:
          return "<";
        case TokenKind::GT:
          return ">";
        case TokenKind::EQ:
          return "=";
        case TokenKind::LPAREN:
          return "(";
        case TokenKind::RPAREN:
          return ")";
        case TokenKind::LBRACE:
          return "{";
        case TokenKind::RBRACE:
          return "}";
        case TokenKind::LBRACKET:
          return "[";
        case TokenKind::RBRACKET:
          return "]";
        case TokenKind::SEMICOLON:
          return ";";
        case TokenKind::COMMA:
          return ",";
        case TokenKind::DOT:
          return ".";
        case TokenKind::COLON:
          return ":";
        case TokenKind::HASH:
          return "#";
        case TokenKind::DOLLAR:
          return "$";
        case TokenKind::END_OF_FILE:
          return "end of file";
      }
    return "unknown token";
  }

  export using LexResult = Maybe<Token, LexError>;
} // namespace embdr::lex
