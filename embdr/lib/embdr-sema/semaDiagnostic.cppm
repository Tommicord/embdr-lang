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
#include <cstdio>

export module embdr.lex.semaDiagnostic;
import embdr.cxxstd.io;
import embdr.cxxstd.stringNumber;
import embdr.cxxstd.stringView;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::SimpleStringView;

  // Severity of a semantic diagnostic, ordered by increasing politeness.
  export enum class DiagSeverity : uint8_t {
    ERROR,
    WARNING,
    NOTE,
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const DiagSeverity severity) noexcept
  {
    switch (severity)
      {
        case DiagSeverity::ERROR:
          return "error";
        case DiagSeverity::WARNING:
          return "warning";
        case DiagSeverity::NOTE:
          return "note";
      }
    return "error";
  }

  // Diagnostic color policy. AUTO probes stderr exactly like the logger
  // does (isatty), so redirected output stays free of escape sequences.
  export enum class ColorMode : uint8_t {
    AUTO,
    ALWAYS,
    NEVER,
  };

  // Stable diagnostic identifiers. The value is a tag for tests and tooling
  // (rendered as `as_str(DiagId)` in `<...>` after the severity); the human
  // message is composed at the report site where every needed name and type
  // is known, then interned in the engine's pool.
  export enum class DiagId : uint16_t {
    UNDECLARED_IDENTIFIER,
    UNDECLARED_TYPE,
    REDECLARED,
    DUPLICATE_PARAM,
    DUPLICATE_FIELD,
    DUPLICATE_VARIANT,
    DUPLICATE_MAIN,
    DUPLICATE_BINDING,
    DUPLICATE_LABEL,
    INIT_TYPE_MISMATCH,
    ASSIGN_TYPE_MISMATCH,
    ARG_TYPE_MISMATCH,
    PATTERN_TYPE_MISMATCH,
    RETURN_TYPE_MISMATCH,
    COALESCE_TYPE_MISMATCH,
    MISSING_INITIALIZER,
    CONDITION_NOT_BOOL,
    INVALID_OPERAND,
    INVALID_UNARY_OPERAND,
    NOT_LVALUE,
    NOT_CALLABLE,
    WRONG_ARG_COUNT,
    UNKNOWN_MEMBER,
    NOT_A_STRUCT,
    NOT_INDEXABLE,
    INDEX_TYPE,
    INVALID_CAST,
    NOT_ITERABLE,
    BREAK_OUTSIDE_LOOP,
    CONTINUE_OUTSIDE_LOOP,
    UNKNOWN_LABEL,
    RET_UNEXPECTED,
    RET_MISSING,
    TRY_ON_NON_RESULT,
    COALESCE_ON_NON_MAYBE,
    PIPELINE_OPERAND,
    RAW_DEREF_OUTSIDE_VOLATILE,
    CEXPR_NOT_CONSTANT,
    INVALID_ARRAY_SIZE,
    SYMBOL_TABLE_FULL,
    TYPE_TABLE_FULL,
    UNKNOWN_VARIANT,
    UNDECLARED_FUNCTION,
    MISSING_RETURN,
    UNREACHABLE_CODE,
    USE_BEFORE_INIT,
    MATCH_NOT_EXHAUSTIVE,
    UNREACHABLE_PATTERN,
    DIVISION_BY_ZERO,
    UNKNOWN_ATTRIBUTE,
    EXTERN_WITH_BODY,
    UNINITIALIZED_FIELD,
    UNKNOWN_ASM_MNEMONIC,
    RECURSIVE_CYCLE,
    INTERNAL_OVERFLOW,
  };

  // Short stable tag for a diagnostic id, rendered after the severity word
  // (`error: ... <redeclared>`), so tests and scripts can match classes of
  // diagnostics without freezing the exact English wording.
  export [[nodiscard]] constexpr SimpleStringView
  as_tag (const DiagId id) noexcept
  {
    switch (id)
      {
        case DiagId::UNDECLARED_IDENTIFIER:
          return "undeclared-identifier";
        case DiagId::UNDECLARED_TYPE:
          return "undeclared-type";
        case DiagId::REDECLARED:
          return "redeclared";
        case DiagId::DUPLICATE_PARAM:
          return "duplicate-param";
        case DiagId::DUPLICATE_FIELD:
          return "duplicate-field";
        case DiagId::DUPLICATE_VARIANT:
          return "duplicate-variant";
        case DiagId::DUPLICATE_MAIN:
          return "duplicate-main";
        case DiagId::DUPLICATE_BINDING:
          return "duplicate-binding";
        case DiagId::DUPLICATE_LABEL:
          return "duplicate-label";
        case DiagId::INIT_TYPE_MISMATCH:
          return "init-type-mismatch";
        case DiagId::ASSIGN_TYPE_MISMATCH:
          return "assign-type-mismatch";
        case DiagId::ARG_TYPE_MISMATCH:
          return "arg-type-mismatch";
        case DiagId::PATTERN_TYPE_MISMATCH:
          return "pattern-type-mismatch";
        case DiagId::RETURN_TYPE_MISMATCH:
          return "return-type-mismatch";
        case DiagId::COALESCE_TYPE_MISMATCH:
          return "coalesce-type-mismatch";
        case DiagId::MISSING_INITIALIZER:
          return "missing-initializer";
        case DiagId::CONDITION_NOT_BOOL:
          return "condition-not-bool";
        case DiagId::INVALID_OPERAND:
          return "invalid-operand";
        case DiagId::INVALID_UNARY_OPERAND:
          return "invalid-unary-operand";
        case DiagId::NOT_LVALUE:
          return "not-lvalue";
        case DiagId::NOT_CALLABLE:
          return "not-callable";
        case DiagId::WRONG_ARG_COUNT:
          return "wrong-arg-count";
        case DiagId::UNKNOWN_MEMBER:
          return "unknown-member";
        case DiagId::NOT_A_STRUCT:
          return "not-a-struct";
        case DiagId::NOT_INDEXABLE:
          return "not-indexable";
        case DiagId::INDEX_TYPE:
          return "index-type";
        case DiagId::INVALID_CAST:
          return "invalid-cast";
        case DiagId::NOT_ITERABLE:
          return "not-iterable";
        case DiagId::BREAK_OUTSIDE_LOOP:
          return "break-outside-loop";
        case DiagId::CONTINUE_OUTSIDE_LOOP:
          return "continue-outside-loop";
        case DiagId::UNKNOWN_LABEL:
          return "unknown-label";
        case DiagId::RET_UNEXPECTED:
          return "ret-unexpected";
        case DiagId::RET_MISSING:
          return "ret-missing";
        case DiagId::TRY_ON_NON_RESULT:
          return "try-on-non-result";
        case DiagId::COALESCE_ON_NON_MAYBE:
          return "coalesce-on-non-maybe";
        case DiagId::PIPELINE_OPERAND:
          return "pipeline-operand";
        case DiagId::RAW_DEREF_OUTSIDE_VOLATILE:
          return "raw-deref-outside-volatile";
        case DiagId::CEXPR_NOT_CONSTANT:
          return "cexpr-not-constant";
        case DiagId::INVALID_ARRAY_SIZE:
          return "invalid-array-size";
        case DiagId::SYMBOL_TABLE_FULL:
          return "symbol-table-full";
        case DiagId::TYPE_TABLE_FULL:
          return "type-table-full";
        case DiagId::UNKNOWN_VARIANT:
          return "unknown-variant";
        case DiagId::UNDECLARED_FUNCTION:
          return "undeclared-function";
        case DiagId::MISSING_RETURN:
          return "missing-return";
        case DiagId::UNREACHABLE_CODE:
          return "unreachable-code";
        case DiagId::USE_BEFORE_INIT:
          return "use-before-init";
        case DiagId::MATCH_NOT_EXHAUSTIVE:
          return "match-not-exhaustive";
        case DiagId::UNREACHABLE_PATTERN:
          return "unreachable-pattern";
        case DiagId::DIVISION_BY_ZERO:
          return "division-by-zero";
        case DiagId::UNKNOWN_ATTRIBUTE:
          return "unknown-attribute";
        case DiagId::EXTERN_WITH_BODY:
          return "extern-with-body";
        case DiagId::UNINITIALIZED_FIELD:
          return "uninitialized-field";
        case DiagId::UNKNOWN_ASM_MNEMONIC:
          return "unknown-asm-mnemonic";
        case DiagId::RECURSIVE_CYCLE:
          return "recursive-cycle";
        case DiagId::INTERNAL_OVERFLOW:
          return "internal-overflow";
      }
    return "unknown";
  }

  // One recorded diagnostic. `message` points into the engine's intern pool
  // (or into a static literal) and stays valid for the engine's lifetime.
  export struct Diagnostic
  {
    DiagSeverity _M_severity = DiagSeverity::ERROR;
    DiagId _M_id = DiagId::INTERNAL_OVERFLOW;
    uint32_t offset = 0; // byte offset into the source
    uint16_t span = 1; // bytes to underline below the caret
    SimpleStringView message{};
    Diagnostic () = default;
  };

  export inline constexpr size_t SEMA_MAX_DIAGNOSTICS = 64;
  export inline constexpr size_t SEMA_DIAG_POOL_SIZE = 4096;
  export inline constexpr size_t SEMA_DIAG_RENDER_SIZE = 0x2000;

  // Bounded string builder for diagnostic messages. Never allocates; the
  // output is capped at the caller-provided capacity (a truncated message
  // is still a valid view, and intern() below copies it into the engine).
  export class MessageBuilder
  {
  public:
    constexpr explicit MessageBuilder (char* const buf, const size_t cap) noexcept : _M_buf (buf), _M_cap (cap) {}

    constexpr MessageBuilder&
    str (const SimpleStringView text) noexcept
    {
      for (size_t i = 0; i < text.size (); ++i)
        {
          this->ch (text[i]);
        }
      return *this;
    }

    constexpr MessageBuilder&
    ch (const char c) noexcept
    {
      if (_M_pos < _M_cap)
        {
          _M_buf[_M_pos] = c;
        }
      ++_M_pos;
      return *this;
    }

    constexpr MessageBuilder&
    u64 (const uint64_t value) noexcept
    {
      char digits[24]{};
      const unsigned long long len = cxxstd::uitoa (digits, value);
      for (unsigned long long i = 0; i < len; ++i)
        {
          this->ch (digits[i]);
        }
      return *this;
    }

    // 'quoted' — the single-quote form used for identifiers and types.
    [[nodiscard]] constexpr MessageBuilder&
    quoted (const SimpleStringView text) noexcept
    {
      this->ch ('\'').str (text).ch ('\'');
      return *this;
    }

    [[nodiscard]] constexpr SimpleStringView
    view () const noexcept
    {
      const size_t n = _M_pos < _M_cap ? _M_pos : _M_cap;
      return SimpleStringView{_M_buf, n};
    }

    [[nodiscard]] constexpr size_t
    size () const noexcept
    {
      return _M_pos;
    }

  private:
    char* _M_buf;
    size_t _M_cap;
    size_t _M_pos = 0;
  };

  // The diagnostic engine: a fixed-capacity list of diagnostics over one
  // source buffer plus the clang-style pretty-printer. Structure mirrors
  // clang's DiagnosticEngine (ids + severity + location + text) shrunk to
  // the no-allocation subset: messages are interned into a bounded pool
  // owned by the engine instead of being built lazily at render time.
  export class DiagnosticEngine
  {
  public:
    explicit constexpr DiagnosticEngine (const SimpleStringView source,
                                         const SimpleStringView filename = SimpleStringView{"input"}) noexcept :
        _M_source (source), _M_filename (filename)
    {
    }

    // Copies `text` (plus a terminator) into the engine pool; the
    // returned view is valid until the engine is destroyed. When the
    // pool is exhausted the text is truncated — reporting never fails,
    // it only loses tail bytes of the message.
    [[nodiscard]] constexpr SimpleStringView
    intern (const SimpleStringView text) noexcept
    {
      // Invariant: _M_pool_used <= _M_pool_cap - 1, so `room` never
      // underflows and always leaves space for the terminator.
      const size_t room = _M_pool_cap - _M_pool_used - 1;
      const size_t n = text.size () < room ? text.size () : room;
      for (size_t i = 0; i < n; ++i)
        {
          _M_pool[_M_pool_used + i] = text[i];
        }
      _M_pool[_M_pool_used + n] = '\0';
      const SimpleStringView out{_M_pool + _M_pool_used, n};
      _M_pool_used += n + 1;
      return out;
    }

    // Records a diagnostic. The list keeps the first SEMA_MAX_DIAGNOSTICS;
    // later ones still count (callers never mistake "storage full" for
    // "program fine").
    constexpr void
    report (const DiagSeverity severity, const DiagId id, const uint32_t offset, const SimpleStringView message,
            const uint16_t span = 1) noexcept
    {
      if (severity == DiagSeverity::ERROR)
        {
          ++_M_error_count;
        }
      else if (severity == DiagSeverity::WARNING)
        {
          ++_M_warning_count;
        }
      if (_M_count >= SEMA_MAX_DIAGNOSTICS)
        {
          return;
        }
      Diagnostic& slot = _M_diags[_M_count];
      slot._M_severity = severity;
      slot._M_id = id;
      slot.offset = offset;
      slot.span = span == 0 ? static_cast<uint16_t> (1) : span;
      slot.message = message;
      ++_M_count;
    }

    [[nodiscard]] constexpr size_t
    count () const noexcept
    {
      return _M_count;
    }

    [[nodiscard]] constexpr size_t
    error_count () const noexcept
    {
      return _M_error_count;
    }

    [[nodiscard]] constexpr size_t
    warning_count () const noexcept
    {
      return _M_warning_count;
    }

    [[nodiscard]] constexpr bool
    has_errors () const noexcept
    {
      return _M_error_count != 0;
    }

    [[nodiscard]] constexpr const Diagnostic&
    at (const size_t i) const noexcept
    {
      return _M_diags[i];
    }

    // True when rendered output may contain ANSI escapes under `mode`.
    [[nodiscard]] static constexpr bool
    use_colors (const ColorMode mode) noexcept
    {
      switch (mode)
        {
          case ColorMode::ALWAYS:
            return true;
          case ColorMode::NEVER:
            return false;
          case ColorMode::AUTO:
            // Terminal probe, exactly like the logger's auto mode.
            return cxxstd::is_terminal (cxxstd::Stderr{});
        }
      return false;
    }

    // Renders one diagnostic:
    //
    //   input:3:9: error: use of undeclared identifier 'z' <tag>
    //     3 | let z = q + 1;
    //       |         ^
    //
    // Returns the number of bytes written (excluding the terminator); a
    // truncated render still NUL-terminates. Byte-oriented columns: the
    // caret aligns on source bytes, which is exact for ASCII and stable
    // (never wrong by re-encoding) for UTF-8 sources.
    [[nodiscard]] constexpr size_t
    render_one (const Diagnostic& diag, const ColorMode mode, char* const buf, const size_t cap) const noexcept
    {
      const bool color = use_colors (mode);
      RenderState st{buf, cap};
      const LineCol lc = line_col_of (_M_source, diag.offset);

      // "<file>:<line>:<col>: <severity>: <message> <<tag>>"
      if (color)
        {
          st.str ("\x1b[1m");
        }
      st.str (_M_filename).ch (':').u32 (lc.line).ch (':').u32 (lc.col).str (": ");
      if (color)
        {
          st.str ("\x1b[0m");
        }
      if (color)
        {
          st.str (severity_color (diag._M_severity));
        }
      st.str (as_str (diag._M_severity));
      if (color)
        {
          st.str ("\x1b[0m");
        }
      st.str (": ").str (diag.message).str (" <").str (as_tag (diag._M_id)).str (">\n");

      // The quoted source line plus caret ruler.
      uint32_t line_start = diag.offset;
      while (line_start > 0 && _M_source[line_start - 1] != '\n' && _M_source[line_start - 1] != '\r')
        {
          --line_start;
        }
      uint32_t line_end = diag.offset <= _M_source.size () ? diag.offset : static_cast<uint32_t> (_M_source.size ());
      while (line_end < _M_source.size () && _M_source[line_end] != '\n' && _M_source[line_end] != '\r')
        {
          ++line_end;
        }
      char numbuf[16]{};
      const unsigned long long digits = cxxstd::uitoa (numbuf, lc.line);
      const size_t gutter = digits;

      st.ch (' ')
          .str (color ? SimpleStringView{"\x1b[1;34m"} : SimpleStringView{})
          .str (" |")
          .str (color ? SimpleStringView{"\x1b[0m"} : SimpleStringView{})
          .ch ('\n');
      st.right_align (lc.line, gutter);
      st.ch (' ')
          .str (color ? SimpleStringView{"\x1b[1;34m"} : SimpleStringView{})
          .str (" | ")
          .str (color ? SimpleStringView{"\x1b[0m"} : SimpleStringView{})
          .str (SimpleStringView{_M_source.data () + line_start, line_end - line_start})
          .ch ('\n');
      for (size_t i = 0; i < gutter + 1; ++i)
        {
          st.ch (' ');
        }
      st.str (color ? SimpleStringView{"\x1b[1;34m"} : SimpleStringView{})
          .str (" | ")
          .str (color ? SimpleStringView{"\x1b[0m"} : SimpleStringView{});
      const uint32_t caret_col = lc.col > 0 ? lc.col - 1 : 0;
      for (uint32_t i = 0; i < caret_col; ++i)
        {
          st.ch (' ');
        }
      if (color)
        {
          st.str (severity_color (diag._M_severity));
        }
      st.ch ('^');
      for (uint16_t i = 1; i < diag.span; ++i)
        {
          st.ch ('~');
        }
      if (color)
        {
          st.str ("\x1b[0m");
        }
      st.ch ('\n');
      return st.finish ();
    }

    // Renders every recorded diagnostic, separated by newlines.
    [[nodiscard]] constexpr size_t
    render_all (const ColorMode mode, char* const buf, const size_t cap) const noexcept
    {
      RenderState st{buf, cap};
      char one[SEMA_DIAG_RENDER_SIZE]{};
      for (size_t i = 0; i < _M_count; ++i)
        {
          const size_t n = render_one (_M_diags[i], mode, one, sizeof (one));
          st.str (SimpleStringView{one, n});
        }
      return st.finish ();
    }

    // Pretty-prints everything to stderr. Colors follow `mode`; AUTO
    // colors only when stderr is a terminal.
    constexpr void
    print_all (const ColorMode mode) const noexcept
    {
      char buf[SEMA_DIAG_RENDER_SIZE]{};
      const size_t n = render_all (mode, buf, sizeof (buf) - 1);
      if (n > 0)
        {
          (void)std::fwrite (buf, 1, n, stderr);
        }
    }

    [[nodiscard]] constexpr SimpleStringView
    source () const noexcept
    {
      return _M_source;
    }

  private:
    struct RenderState
    {
      char* _M_buf;
      size_t _M_cap;
      size_t _M_pos = 0;

      constexpr RenderState&
      put (const char c) noexcept
      {
        if (_M_pos + 1 < _M_cap)
          {
            _M_buf[_M_pos] = c;
          }
        ++_M_pos;
        return *this;
      }
      constexpr RenderState&
      ch (const char c) noexcept
      {
        return put (c);
      }
      constexpr RenderState&
      str (const SimpleStringView text) noexcept
      {
        for (size_t i = 0; i < text.size (); ++i)
          {
            put (text[i]);
          }
        return *this;
      }

      constexpr RenderState&
      u32 (const uint32_t value) noexcept
      {
        char digits[12]{};
        const unsigned long long len = cxxstd::uitoa (digits, value);
        for (unsigned long long i = 0; i < len; ++i)
          {
            put (digits[i]);
          }
        return *this;
      }

      // Right-aligns a number in `width` columns (line-number gutter).
      constexpr RenderState&
      right_align (const uint32_t value, const size_t width) noexcept
      {
        char digits[12]{};
        const unsigned long long len = cxxstd::uitoa (digits, value);
        for (size_t i = len; i < width; ++i)
          {
            put (' ');
          }
        for (unsigned long long i = 0; i < len; ++i)
          {
            put (digits[i]);
          }
        return *this;
      }
      constexpr size_t
      finish () noexcept
      {
        const size_t written = _M_pos < _M_cap ? _M_pos : (_M_cap > 0 ? _M_cap - 1 : 0);
        if (_M_cap > 0)
          {
            _M_buf[written] = '\0';
          }
        return written;
      }
    };

    [[nodiscard]] static constexpr SimpleStringView
    severity_color (const DiagSeverity severity) noexcept
    {
      switch (severity)
        {
          case DiagSeverity::ERROR:
            return "\x1b[1;31m"; // bold red
          case DiagSeverity::WARNING:
            return "\x1b[1;33m"; // bold yellow
          case DiagSeverity::NOTE:
            return "\x1b[1;36m"; // bold cyan
        }
      return "\x1b[1;31m";
    }

    SimpleStringView _M_source;
    SimpleStringView _M_filename;
    Diagnostic _M_diags[SEMA_MAX_DIAGNOSTICS]{};
    size_t _M_count = 0;
    size_t _M_error_count = 0;
    size_t _M_warning_count = 0;
    char _M_pool[SEMA_DIAG_POOL_SIZE]{};
    size_t _M_pool_used = 0;
    static constexpr size_t _M_pool_cap = SEMA_DIAG_POOL_SIZE;
  };
} // namespace embdr::lex
