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

export module embdr.lex.pattern;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.utfEncode;

namespace embdr::lex
{
  using cxxstd::decode_utf8;
  using cxxstd::is_unicode_scalar;
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;
  using cxxstd::Utf8Decode;
  using cxxstd::UtfError;

  // Compile-time constructible matching (syntax.txt §19).
  //
  // A p"..." literal is turned by compile_pattern() into a Thompson NFA and run
  // by a Pike-VM simulation — the architecture of Google RE2 and of Russ Cox's
  // linear-time matching articles. The reasons we hand-roll this rather than
  // embed a backtracking engine:
  //
  //   - NO backtracking. Each input position advances every live state in lock
  //     step, and a per-position "seen" stamp guarantees a program counter is
  //     entered at most once per character. Worst case is O(len(text) *
  //     len(program)): linear in the input, so (a+)+$ cannot blow up (no ReDoS).
  //   - NO recursion, NO heap. Compilation uses a bounded group stack and
  //     patch list; matching uses fixed thread stacks and a fixed seen[] array —
  //     all template-parameterized capacities and value types. Satisfies
  //     CODE_QUALITY §1.3 and keeps stack usage predictable on embedded targets.
  //   - CONSTEXPR end to end: §19 makes p"..." a compile-time constant, so
  //     compile_pattern() runs in constant evaluation and the program lands in
  //     .rodata.
  //
  // Deliberately unsupported — this is exactly what keeps matching linear and
  // the automaton small: backreferences and lookaround. syntax.txt §19 defines
  // p"..." as RE2-style, so their absence is by design, not a gap.
  //
  // Matching runs over Unicode scalar values decoded from the UTF-8 input. '.'
  // (ANY) excludes '\n' (RE2 default). A trailing invalid UTF-8 byte falls back
  // to its own byte value so matching stays total.

  export enum class PatternOp : uint8_t {
    CHAR, // consume one code point == x
    ANY, // consume one code point != '\n'
    CLASS, // consume one code point inside class (class_id)
    SPLIT, // fork: match x, then y (priority order encodes greedy/lazy)
    JMP, // unconditional jump to x
    BOL, // zero-width: assert start of input
    EOL, // zero-width: assert end of input
    MATCH, // accept
  };

  // Uniform instruction. CHAR stores its code point in x; SPLIT uses
  // x = preferred branch, y = alternative; JMP uses x = target. class_id is
  // meaningful only for CLASS. 12 bytes, no padding.
  export struct PatternInst
  {
    PatternOp op = PatternOp::MATCH;
    uint8_t reserved = 0;
    uint16_t class_id = 0;
    uint32_t x = 0;
    uint32_t y = 0;
  };

  // Classes are stored as interval lists, not a 0x110000-entry table: embedded
  // targets cannot spare 128 KiB per class and real patterns use few ranges.
  export struct PatternRange
  {
    char32_t lo = 0;
    char32_t hi = 0; // inclusive
  };

  export struct PatternClass
  {
    uint32_t range_begin = 0;
    uint32_t range_count = 0;
    bool negated = false; // [^...] — match outside the ranges
  };

  export enum class PatternErrorKind : uint8_t {
    UNEXPECTED_END,
    INVALID_ESCAPE,
    UNTERMINATED_CLASS,
    UNTERMINATED_GROUP,
    INVALID_REPEAT,
    INVALID_RANGE,
    INVALID_CODE_POINT,
    REPEAT_WITHOUT_ATOM,
    TOO_MANY_STATES,
    TOO_MANY_CLASSES,
    TOO_MANY_RANGES,
    TOO_MANY_PATCHES,
  };

  export struct PatternError
  {
    PatternErrorKind kind = PatternErrorKind::UNEXPECTED_END;
    uint32_t offset = 0; // byte offset into the pattern text
  };

  export [[nodiscard]] constexpr SimpleStringView as_str(const PatternErrorKind kind) noexcept
  {
    switch (kind)
      {
        case PatternErrorKind::UNEXPECTED_END:
          return "unexpected end of pattern";
        case PatternErrorKind::INVALID_ESCAPE:
          return "invalid escape sequence";
        case PatternErrorKind::UNTERMINATED_CLASS:
          return "unterminated character class";
        case PatternErrorKind::UNTERMINATED_GROUP:
          return "unterminated group";
        case PatternErrorKind::INVALID_REPEAT:
          return "invalid repeat bounds";
        case PatternErrorKind::INVALID_RANGE:
          return "invalid class range";
        case PatternErrorKind::INVALID_CODE_POINT:
          return "invalid code point";
        case PatternErrorKind::REPEAT_WITHOUT_ATOM:
          return "repeat operator with no preceding atom";
        case PatternErrorKind::TOO_MANY_STATES:
          return "pattern has too many states";
        case PatternErrorKind::TOO_MANY_CLASSES:
          return "pattern has too many character classes";
        case PatternErrorKind::TOO_MANY_RANGES:
          return "pattern has too many class ranges";
        case PatternErrorKind::TOO_MANY_PATCHES:
          return "pattern has too many open branches";
      }
    return "unknown pattern error";
  }

  // A compiled pattern: Thompson NFA program + class/range tables, all
  // fixed-capacity and value-typed.
  export template <size_t MaxProgram = 64, size_t MaxClass = 8, size_t MaxRange = 64>
  class CompiledPattern
  {
   public:
    static constexpr size_t program_capacity = MaxProgram;
    static constexpr size_t class_capacity = MaxClass;
    static constexpr size_t range_capacity = MaxRange;

    // Default-constructed pattern accepts only the empty input (one MATCH
    // instruction) — a safe placeholder before a real pattern is assigned.
    constexpr CompiledPattern() noexcept
    {
      _M_program[0] = PatternInst{PatternOp::MATCH, 0, 0, 0, 0};
      _M_program_len = 1;
    }

    // True when the entire input matches (anchored at both ends) — the
    // semantics a §19 match arm needs. Pike VM, linear time, no recursion.
    [[nodiscard]] constexpr bool matches(const SimpleStringView input) const noexcept
    {
      uint32_t clist[MaxProgram];
      uint32_t nlist[MaxProgram];
      uint32_t seen[MaxProgram];
      for (size_t i = 0; i < MaxProgram; ++i)
        {
          seen[i] = 0;
        }

      uint32_t generation = 0;
      uint32_t clen = 0;
      uint32_t nlen = 0;

      // Epsilon closure of `pc`, appended to `list`. Iterative via an
      // explicit stack so deep nesting cannot overflow the call stack. The
      // `seen` generation stamp is what bounds the whole simulation: each pc
      // is added at most once per input position.
      const auto add_thread =
          [&](uint32_t* const list, uint32_t& len, const uint32_t pc, const size_t pos, const uint32_t gen) noexcept
        {
          uint32_t stack[MaxProgram];
          size_t depth = 0;
          stack[depth++] = pc;
          while (depth != 0)
            {
              const uint32_t cur = stack[--depth];
              if (cur >= _M_program_len || seen[cur] == gen)
                {
                  continue;
                }
              seen[cur] = gen;
              const PatternInst& inst = _M_program[cur];
              switch (inst.op)
                {
                  case PatternOp::JMP:
                    stack[depth++] = inst.x;
                    break;
                  case PatternOp::SPLIT:
                    stack[depth++] = inst.y;
                    stack[depth++] = inst.x; // x pops first = greedy priority
                    break;
                  case PatternOp::BOL:
                    if (pos == 0)
                      {
                        stack[depth++] = cur + 1;
                      }
                    break;
                  case PatternOp::EOL:
                    if (pos == input.size())
                      {
                        stack[depth++] = cur + 1;
                      }
                    break;
                  default:
                    if (len < MaxProgram)
                      {
                        list[len++] = cur;
                      }
                    break;
                }
            }
        };

      ++generation;
      add_thread(clist, clen, 0, 0, generation);

      size_t pos = 0;
      while (true)
        {
          // Accept only when a MATCH state is live and input is spent.
          for (uint32_t i = 0; i < clen; ++i)
            {
              if (_M_program[clist[i]].op == PatternOp::MATCH && pos == input.size())
                {
                  return true;
                }
            }
          if (pos == input.size())
            {
              return false;
            }

          char32_t cp;
          uint32_t consumed;
          const Utf8Decode decoded = decode_utf8(input.data() + pos, input.size() - pos);
          if (decoded._M_error == UtfError::NONE)
            {
              cp = decoded._M_code_point;
              consumed = decoded._M_consumed;
            }
          else
            {
              cp = static_cast<unsigned char>(input[pos]);
              consumed = 1;
            }

          ++generation; // fresh stamp for the next position
          nlen = 0;
          for (uint32_t i = 0; i < clen; ++i)
            {
              const uint32_t pc = clist[i];
              if (consume_ok(_M_program[pc], cp))
                {
                  add_thread(nlist, nlen, pc + 1, pos + consumed, generation);
                }
            }

          for (uint32_t i = 0; i < nlen; ++i)
            {
              clist[i] = nlist[i];
            }
          clen = nlen;
          pos += consumed;
        }
    }

    [[nodiscard]] constexpr size_t program_len() const noexcept { return _M_program_len; }

    [[nodiscard]] constexpr const PatternInst& instruction(const size_t index) const noexcept
    {
      return _M_program[index];
    }

    [[nodiscard]] constexpr size_t class_count() const noexcept { return _M_class_len; }

    [[nodiscard]] constexpr const PatternClass& character_class(const size_t index) const noexcept
    {
      return _M_classes[index];
    }

    [[nodiscard]] constexpr size_t range_count() const noexcept { return _M_range_len; }

    [[nodiscard]] constexpr const PatternRange& range(const size_t index) const noexcept { return _M_ranges[index]; }

   private:
    template <size_t MP, size_t MC, size_t MR>
    friend class PatternBuilder;

    [[nodiscard]] constexpr bool consume_ok(const PatternInst& inst, const char32_t cp) const noexcept
    {
      switch (inst.op)
        {
          case PatternOp::CHAR:
            return cp == static_cast<char32_t>(inst.x);
          case PatternOp::ANY:
            return cp != '\n';
          case PatternOp::CLASS:
            return class_matches(_M_classes[inst.class_id], cp);
          default:
            return false;
        }
    }

    // Resolves a class against the compiled pattern's own range table.
    [[nodiscard]] constexpr bool class_matches(const PatternClass& cls, const char32_t cp) const noexcept
    {
      bool inside = false;
      const uint32_t end = cls.range_begin + cls.range_count;
      for (uint32_t i = cls.range_begin; i < end && i < _M_range_len; ++i)
        {
          if (_M_ranges[i].lo <= cp && cp <= _M_ranges[i].hi)
            {
              inside = true;
              break;
            }
        }
      return cls.negated ? !inside : inside;
    }

    PatternInst _M_program[MaxProgram]{};
    size_t _M_program_len = 0;
    PatternClass _M_classes[MaxClass]{};
    size_t _M_class_len = 0;
    PatternRange _M_ranges[MaxRange]{};
    size_t _M_range_len = 0;
  };

  // Recursive-descent compiler from a p"..." pattern string to a Thompson NFA.
  //
  // Grammar (RE2 subset, no backreferences / lookaround):
  //   alt     := concat ('|' concat)*
  //   concat  := repeat*
  //   repeat  := atom quantifier?
  //   atom    := '(' alt ')' | '(?:' alt ')' | '[' class ']' | '.' | '^' | '$'
  //            | '\' escape | literal
  //   quantifier := '*' | '+' | '?' | '{' n (',' m?)? '}'  each optionally
  //                 followed by '?' for the non-greedy form
  //
  // Groups are compiled inline (no capture slots): '(' emits nothing and
  // ')' joins the inner alternation. Alternation and groups use a fixed patch
  // list instead of recursion for their open branches, so compilation is also
  // stack-bounded.
  export template <size_t MaxProgram, size_t MaxClass, size_t MaxRange>
  class PatternBuilder
  {
   public:
    using PatternType = CompiledPattern<MaxProgram, MaxClass, MaxRange>;

    [[nodiscard]] static constexpr Maybe<PatternType, PatternError> build(const SimpleStringView pattern) noexcept
    {
      PatternBuilder builder{pattern};
      return builder._M_run();
    }

   private:
    explicit constexpr PatternBuilder(const SimpleStringView pattern) noexcept : _M_pattern(pattern) {}

    [[nodiscard]] constexpr Maybe<PatternType, PatternError> _M_run() noexcept
    {
      // The default-constructed PatternType seeds program[0] with MATCH as
      // a usable empty-program sentinel; the real program starts fresh.
      _M_out._M_program_len = 0;
      uint32_t frag_start = 0;
      uint32_t frag_end = 0;
      if (!parse_alt(frag_start, frag_end))
        {
          return _M_error;
        }
      if (_M_pos < _M_pattern.size())
        {
          // A stray ')' that parse_concat refused to consume.
          return PatternError{PatternErrorKind::UNTERMINATED_GROUP, static_cast<uint32_t>(_M_pos)};
        }
      const uint32_t match_index = _M_out._M_program_len;
      if (!emit(PatternInst{PatternOp::MATCH, 0, 0, 0, 0}))
        {
          return _M_error;
        }
      // Close any top-level alternation JMPs to the MATCH state.
      patch_alt_jumps(0, match_index);
      return _M_out;
    }

    [[nodiscard]] constexpr bool has_failed() const noexcept { return _M_failed; }

    constexpr void fail(const PatternErrorKind kind, const uint32_t offset) noexcept
    {
      if (!_M_failed)
        {
          _M_failed = true;
          _M_error = PatternError{kind, offset};
        }
    }

    [[nodiscard]] constexpr bool emit(const PatternInst inst) noexcept
    {
      if (_M_out._M_program_len >= MaxProgram)
        {
          fail(PatternErrorKind::TOO_MANY_STATES, _M_pos);
          return false;
        }
      _M_out._M_program[_M_out._M_program_len++] = inst;
      return true;
    }

    [[nodiscard]] constexpr char peek() const noexcept
    {
      return _M_pos < _M_pattern.size() ? _M_pattern[_M_pos] : '\0';
    }

    [[nodiscard]] constexpr char peek_at(const size_t ahead) const noexcept
    {
      return _M_pos + ahead < _M_pattern.size() ? _M_pattern[_M_pos + ahead] : '\0';
    }

    // Parses one code point at the cursor and advances past its UTF-8 bytes.
    [[nodiscard]] constexpr bool consume_code_point(char32_t& cp) noexcept
    {
      if (_M_pos >= _M_pattern.size())
        {
          fail(PatternErrorKind::UNEXPECTED_END, _M_pos);
          return false;
        }
      const Utf8Decode decoded = decode_utf8(_M_pattern.data() + _M_pos, _M_pattern.size() - _M_pos);
      if (decoded._M_error == UtfError::NONE)
        {
          cp = decoded._M_code_point;
          _M_pos += decoded._M_consumed;
          return true;
        }
      cp = static_cast<unsigned char>(_M_pattern[_M_pos]);
      ++_M_pos;
      return true;
    }

    // alt := concat ('|' concat)*.
    //
    // Returns the fragment's start/end program indices via out-params. Each
    // alternative is emitted contiguously. For two alternatives we emit SPLIT,
    // then fold a JMP into the end of the first fragment (shifting it one slot
    // right to make room). That JMP's `x` holds a *relative* target (the length
    // of the following alternative); every open JMP is pushed onto a stack and
    // patched to an absolute program counter when the enclosing group (or the
    // top level) closes. Nesting is safe because each group patches its own
    // JMPs at its own ')'.
    [[nodiscard]] constexpr bool parse_alt(uint32_t& frag_start, uint32_t& frag_end) noexcept
    {
      const uint32_t alt_start = _M_out._M_program_len;
      if (!parse_concat())
        {
          return false;
        }
      frag_start = alt_start;

      while (peek() == '|')
        {
          ++_M_pos; // '|'
          const uint32_t left_end = _M_out._M_program_len;

          if (!parse_concat())
            {
              return false;
            }

          // Fold: insert SPLIT at alt_start (shifts everything right by one),
          // then insert JMP right after the left fragment. Result is
          // contiguous: [SPLIT][alt1][JMP][alt2].
          if (!insert_inst(alt_start, PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t split_index = alt_start;
          const uint32_t alt1_start = alt_start + 1;
          const uint32_t jmp_index = left_end + 1;
          if (!insert_inst(jmp_index, PatternInst{PatternOp::JMP, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t alt2_start = jmp_index + 1;

          _M_out._M_program[split_index].x = alt1_start;
          _M_out._M_program[split_index].y = alt2_start;

          // The JMP skips alt2; store its length as a relative offset.
          const uint32_t alt2_len = _M_out._M_program_len - alt2_start;
          if (!push_patch(jmp_index, alt2_len))
            {
              return false;
            }
        }

      frag_end = _M_out._M_program_len;
      return !has_failed();
    }

    // Inserts `inst` at program index `index`, shifting everything from there
    // one slot right. Absolute program pointers inside the shifted
    // instructions (SPLIT targets, JMP targets of already-closed groups) are
    // bumped so they keep pointing at the same instructions. Unpatched JMPs
    // hold dead placeholder values that will be overwritten at group close,
    // so a spurious bump is harmless.
    [[nodiscard]] constexpr bool insert_inst(const uint32_t index, const PatternInst inst) noexcept
    {
      if (_M_out._M_program_len >= MaxProgram)
        {
          fail(PatternErrorKind::TOO_MANY_STATES, _M_pos);
          return false;
        }
      for (uint32_t i = _M_out._M_program_len; i > index; --i)
        {
          PatternInst& shifted = _M_out._M_program[i - 1];
          if (shifted.op == PatternOp::SPLIT)
            {
              if (shifted.x >= index)
                {
                  ++shifted.x;
                }
              if (shifted.y >= index)
                {
                  ++shifted.y;
                }
            }
          else if (shifted.op == PatternOp::JMP && shifted.x >= index)
            {
              ++shifted.x;
            }
          _M_out._M_program[i] = shifted;
        }
      _M_out._M_program[index] = inst;
      ++_M_out._M_program_len;
      return true;
    }

    // Records an open JMP whose target is a relative offset, to be made
    // absolute when the current group closes.
    [[nodiscard]] constexpr bool push_patch(const uint32_t jmp_index, const uint32_t rel_offset) noexcept
    {
      if (_M_patch_len >= MaxProgram)
        {
          fail(PatternErrorKind::TOO_MANY_PATCHES, _M_pos);
          return false;
        }
      _M_patches[_M_patch_len] = jmp_index;
      _M_patch_offsets[_M_patch_len] = rel_offset;
      ++_M_patch_len;
      return true;
    }

    // Patches every JMP opened since `mark` to jump to `target`.
    constexpr void patch_alt_jumps(const uint32_t mark, const uint32_t target) noexcept
    {
      for (uint32_t i = mark; i < _M_patch_len; ++i)
        {
          _M_out._M_program[_M_patches[i]].x = target;
        }
      _M_patch_len = mark;
    }

    // concat := repeat*  — a sequence of quantified atoms.
    [[nodiscard]] constexpr bool parse_concat() noexcept
    {
      while (_M_pos < _M_pattern.size())
        {
          const char c = peek();
          if (c == '|' || c == ')')
            {
              break;
            }
          if (!parse_repeat())
            {
              return false;
            }
        }
      return !has_failed();
    }

    // repeat := atom quantifier?
    [[nodiscard]] constexpr bool parse_repeat() noexcept
    {
      const uint32_t atom_start = _M_out._M_program_len;
      if (!parse_atom())
        {
          return false;
        }
      const uint32_t atom_end = _M_out._M_program_len;
      if (atom_end == atom_start)
        {
          // Zero-width atom (e.g. a bare anchor) can't be quantified.
          const char c = peek();
          if (c == '*' || c == '+' || c == '?' || c == '{')
            {
              fail(PatternErrorKind::REPEAT_WITHOUT_ATOM, _M_pos);
              return false;
            }
          return true;
        }
      return parse_quantifier(atom_start, atom_end);
    }

    [[nodiscard]] constexpr bool parse_quantifier(const uint32_t atom_start, const uint32_t atom_end) noexcept
    {
      const char c = peek();
      uint32_t lo = 0;
      uint32_t hi = 0; // hi == kUnbounded for *, +
      bool bounded = false;

      if (c == '*')
        {
          lo = 0;
          hi = kUnbounded;
          ++_M_pos;
        }
      else if (c == '+')
        {
          lo = 1;
          hi = kUnbounded;
          ++_M_pos;
        }
      else if (c == '?')
        {
          lo = 0;
          hi = 1;
          bounded = true; // optional: the bounded path wraps one copy in SPLIT(body, exit)
          ++_M_pos;
        }
      else if (c == '{')
        {
          if (!parse_closed_range(lo, hi, bounded))
            {
              return false; // not a quantifier, or a malformed one
            }
        }
      else
        {
          return true; // no quantifier
        }

      bool greedy = true;
      if (peek() == '?')
        {
          greedy = false;
          ++_M_pos;
        }

      if (bounded)
        {
          return emit_bounded_repeat(atom_start, atom_end, lo, hi, greedy);
        }
      return emit_unbounded_repeat(atom_start, atom_end, lo, hi, greedy);
    }

    // Parses '{n}', '{n,}' or '{n,m}'. Returns false (without failing) when the
    // braces are not a quantifier at all — the '{' is then a literal, handled by
    // the caller. A syntactically quantifier-shaped but invalid form (m > n,
    // missing digit) fails hard.
    [[nodiscard]] constexpr bool parse_closed_range(uint32_t& lo, uint32_t& hi, bool& bounded) noexcept
    {
      const size_t save = _M_pos;
      ++_M_pos; // '{'
      if (!parse_uint(lo))
        {
          _M_pos = save;
          return false; // '{' is a literal
        }
      hi = lo;
      bounded = true;
      if (peek() == ',')
        {
          ++_M_pos;
          if (peek() == '}')
            {
              hi = kUnbounded; // '{n,}'
            }
          else if (!parse_uint(hi))
            {
              fail(PatternErrorKind::INVALID_REPEAT, _M_pos);
              return false;
            }
        }
      if (peek() != '}')
        {
          fail(PatternErrorKind::INVALID_REPEAT, _M_pos);
          return false;
        }
      ++_M_pos; // '}'
      if (hi != kUnbounded && lo > hi)
        {
          fail(PatternErrorKind::INVALID_REPEAT, save);
          return false;
        }
      return true;
    }

    [[nodiscard]] constexpr bool parse_uint(uint32_t& value) noexcept
    {
      if (peek() < '0' || peek() > '9')
        {
          return false;
        }
      value = 0;
      while (peek() >= '0' && peek() <= '9')
        {
          value = value * 10U + static_cast<uint32_t>(peek() - '0');
          ++_M_pos;
        }
      return true;
    }

    // '*'/'+': rewrite the atom as a loop via a SPLIT (greedy: preferred branch
    // is the body; lazy: preferred branch is the exit).
    [[nodiscard]] constexpr bool emit_unbounded_repeat(const uint32_t atom_start, const uint32_t atom_end,
                                                       const uint32_t lo, const uint32_t hi, const bool greedy) noexcept
    {
      // For '+'/{1,} the atom must run at least once: emit it, then loop.
      if (lo >= 1)
        {
          // Append a SPLIT after the body that jumps back to the body start.
          const uint32_t split_index = _M_out._M_program_len;
          if (!emit(PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t exit_index = _M_out._M_program_len;
          if (!emit(PatternInst{PatternOp::JMP, 0, 0, atom_start, 0}))
            {
              return false;
            }
          // SPLIT: loop body vs exit.
          if (greedy)
            {
              _M_out._M_program[split_index].x = atom_start;
              _M_out._M_program[split_index].y = exit_index + 1;
            }
          else
            {
              _M_out._M_program[split_index].x = exit_index + 1;
              _M_out._M_program[split_index].y = atom_start;
            }
          return true;
        }
      // '*': insert SPLIT(body, exit) before the atom, then JMP back to it
      // after the atom. The atom is the last thing emitted by parse_concat,
      // but insert_inst shifts everything from atom_start rightward so the
      // construction is correct even if it were not.
      if (!insert_inst(atom_start, PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
        {
          return false;
        }
      const uint32_t split_index = atom_start;
      const uint32_t body_start = atom_start + 1;
      const uint32_t exit_index = _M_out._M_program_len; // where the JMP will land
      if (!emit(PatternInst{PatternOp::JMP, 0, 0, split_index, 0}))
        {
          return false;
        }
      if (greedy)
        {
          _M_out._M_program[split_index].x = body_start;
          _M_out._M_program[split_index].y = exit_index + 1; // past the JMP
        }
      else
        {
          _M_out._M_program[split_index].x = exit_index + 1;
          _M_out._M_program[split_index].y = body_start;
        }
      return true;
    }

    // '{n,m}': emit the atom `lo` times, then (hi - lo) optional copies, or a
    // loop when unbounded. Bounded by capacities; overflow is an error value.
    [[nodiscard]] constexpr bool emit_bounded_repeat(const uint32_t atom_start, const uint32_t atom_end,
                                                     const uint32_t lo, const uint32_t hi, const bool greedy) noexcept
    {
      const uint32_t body_len = atom_end - atom_start;
      if (body_len == 0)
        {
          fail(PatternErrorKind::REPEAT_WITHOUT_ATOM, _M_pos);
          return false;
        }

      // Mandatory copies: the atom already sits at [atom_start, atom_end).
      // Emit (lo - 1) further copies after it.
      for (uint32_t i = 1; i < lo; ++i)
        {
          if (!copy_body(atom_start, atom_end))
            {
              return false;
            }
        }

      if (hi == kUnbounded)
        {
          // '{n,}' / '+': loop only the LAST mandatory copy so every
          // count >= lo is reachable. Looping back to the first copy
          // would re-run all lo copies per iteration (e.g. a{2,} would
          // only match even counts). The mandatory-copies loop above
          // already ran, so the last copy ends at the current program
          // length.
          const uint32_t last_copy = _M_out._M_program_len - body_len;
          const uint32_t split_index = _M_out._M_program_len;
          if (!emit(PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t exit_index = _M_out._M_program_len;
          if (!emit(PatternInst{PatternOp::JMP, 0, 0, last_copy, 0}))
            {
              return false;
            }
          if (greedy)
            {
              _M_out._M_program[split_index].x = last_copy;
              _M_out._M_program[split_index].y = exit_index + 1;
            }
          else
            {
              _M_out._M_program[split_index].x = exit_index + 1;
              _M_out._M_program[split_index].y = last_copy;
            }
          return true;
        }

      // '{n,m}': (hi - lo) optional copies, each wrapped in SPLIT(body, skip).
      //
      // When lo == 0 the already-emitted atom is the FIRST optional copy:
      // insert a SPLIT in front of it so it can be skipped. When lo >= 1
      // the atom is the first mandatory copy and every optional copy is a
      // fresh append.
      if (lo == 0)
        {
          if (!insert_inst(atom_start, PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t split_index = atom_start;
          const uint32_t body_start = atom_start + 1;
          const uint32_t exit_index = atom_end + 1; // atom shifted right by the SPLIT
          if (greedy)
            {
              _M_out._M_program[split_index].x = body_start;
              _M_out._M_program[split_index].y = exit_index;
            }
          else
            {
              _M_out._M_program[split_index].x = exit_index;
              _M_out._M_program[split_index].y = body_start;
            }
          for (uint32_t i = 1; i < hi; ++i)
            {
              const uint32_t next_split = _M_out._M_program_len;
              if (!emit(PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
                {
                  return false;
                }
              const uint32_t next_body = _M_out._M_program_len;
              if (!copy_body(body_start, exit_index))
                {
                  return false;
                }
              const uint32_t next_exit = _M_out._M_program_len;
              if (greedy)
                {
                  _M_out._M_program[next_split].x = next_body;
                  _M_out._M_program[next_split].y = next_exit;
                }
              else
                {
                  _M_out._M_program[next_split].x = next_exit;
                  _M_out._M_program[next_split].y = next_body;
                }
            }
          return true;
        }
      for (uint32_t i = lo; i < hi; ++i)
        {
          const uint32_t split_index = _M_out._M_program_len;
          if (!emit(PatternInst{PatternOp::SPLIT, 0, 0, 0, 0}))
            {
              return false;
            }
          const uint32_t body_start = _M_out._M_program_len;
          if (!copy_body(atom_start, atom_end))
            {
              return false;
            }
          const uint32_t exit_index = _M_out._M_program_len;
          if (greedy)
            {
              _M_out._M_program[split_index].x = body_start;
              _M_out._M_program[split_index].y = exit_index;
            }
          else
            {
              _M_out._M_program[split_index].x = exit_index;
              _M_out._M_program[split_index].y = body_start;
            }
        }
      return true;
    }

    [[nodiscard]] constexpr bool copy_body(const uint32_t atom_start, const uint32_t atom_end) noexcept
    {
      const uint32_t body_len = atom_end - atom_start;
      if (_M_out._M_program_len + body_len > MaxProgram)
        {
          fail(PatternErrorKind::TOO_MANY_STATES, _M_pos);
          return false;
        }
      for (uint32_t i = 0; i < body_len; ++i)
        {
          _M_out._M_program[_M_out._M_program_len + i] = _M_out._M_program[atom_start + i];
        }
      _M_out._M_program_len += body_len;
      return true;
    }

    [[nodiscard]] constexpr bool parse_atom() noexcept
    {
      const char c = peek();
      switch (c)
        {
          case '(':
            return parse_group();
          case '[':
            return parse_class();
          case '.':
            ++_M_pos;
            return emit(PatternInst{PatternOp::ANY, 0, 0, 0, 0});
          case '^':
            ++_M_pos;
            return emit(PatternInst{PatternOp::BOL, 0, 0, 0, 0});
          case '$':
            ++_M_pos;
            return emit(PatternInst{PatternOp::EOL, 0, 0, 0, 0});
          case '\\':
            return parse_escape();
          case '*':
          case '+':
          case '?':
            fail(PatternErrorKind::REPEAT_WITHOUT_ATOM, _M_pos);
            return false;
          case '\0':
            fail(PatternErrorKind::UNEXPECTED_END, _M_pos);
            return false;
          default:
            break;
        }
      char32_t cp = 0;
      if (!consume_code_point(cp))
        {
          return false;
        }
      if (!is_unicode_scalar(cp))
        {
          fail(PatternErrorKind::INVALID_CODE_POINT, _M_pos);
          return false;
        }
      return emit(PatternInst{PatternOp::CHAR, 0, 0, static_cast<uint32_t>(cp), 0});
    }

    // '(' alt ')' / '(?:' alt ')' — groups emit nothing themselves; the inner
    // alternation's JMPs are patched to the program end when ')' closes them.
    [[nodiscard]] constexpr bool parse_group() noexcept
    {
      const uint32_t open = _M_pos;
      ++_M_pos; // '('
      if (peek() == '?' && peek_at(1) == ':')
        {
          _M_pos += 2; // non-capturing prefix (we never capture anyway)
        }
      // Remember the patch-stack height so we only close this group's JMPs.
      const uint32_t mark = _M_patch_len;
      uint32_t frag_start = 0;
      uint32_t frag_end = 0;
      if (!parse_alt(frag_start, frag_end))
        {
          return false;
        }
      if (peek() != ')')
        {
          fail(PatternErrorKind::UNTERMINATED_GROUP, open);
          return false;
        }
      ++_M_pos; // ')'
      // Any JMP this group opened now targets the code right after ')'.
      patch_alt_jumps(mark, _M_out._M_program_len);
      return true;
    }

    [[nodiscard]] constexpr bool parse_class() noexcept
    {
      const uint32_t open = _M_pos;
      ++_M_pos; // '['
      if (_M_out._M_class_len >= MaxClass)
        {
          fail(PatternErrorKind::TOO_MANY_CLASSES, open);
          return false;
        }
      const uint32_t class_id = static_cast<uint32_t>(_M_out._M_class_len);
      PatternClass cls{};
      cls.range_begin = static_cast<uint32_t>(_M_out._M_range_len);
      bool negated = false;
      if (peek() == '^')
        {
          negated = true;
          ++_M_pos;
        }

      bool first = true;
      while (true)
        {
          if (_M_pos >= _M_pattern.size())
            {
              fail(PatternErrorKind::UNTERMINATED_CLASS, open);
              return false;
            }
          if (peek() == ']' && !first)
            {
              ++_M_pos;
              break;
            }
          first = false;

          char32_t lo = 0;
          if (peek() == '\\')
            {
              if (!parse_class_escape(cls))
                {
                  return false;
                }
              continue; // escapes may add several ranges; no range pairing
            }
          if (!consume_code_point(lo))
            {
              return false;
            }
          if (!is_unicode_scalar(lo))
            {
              fail(PatternErrorKind::INVALID_CODE_POINT, _M_pos);
              return false;
            }

          // Range?
          if (peek() == '-' && peek_at(1) != ']' && _M_pos + 1 < _M_pattern.size())
            {
              ++_M_pos; // '-'
              char32_t hi = 0;
              if (peek() == '\\')
                {
                  if (!parse_escape_scalar(hi))
                    {
                      return false;
                    }
                }
              else if (!consume_code_point(hi))
                {
                  return false;
                }
              if (lo > hi)
                {
                  fail(PatternErrorKind::INVALID_RANGE, _M_pos);
                  return false;
                }
              if (!add_range(cls, lo, hi))
                {
                  return false;
                }
            }
          else
            {
              if (!add_range(cls, lo, lo))
                {
                  return false;
                }
            }
        }

      cls.negated = negated;
      cls.range_count = static_cast<uint32_t>(_M_out._M_range_len) - cls.range_begin;
      _M_out._M_classes[_M_out._M_class_len++] = cls;
      return emit(PatternInst{PatternOp::CLASS, 0, static_cast<uint16_t>(class_id), 0, 0});
    }

    [[nodiscard]] constexpr bool add_range(PatternClass& cls, const char32_t lo, const char32_t hi) noexcept
    {
      (void)cls;
      if (_M_out._M_range_len >= MaxRange)
        {
          fail(PatternErrorKind::TOO_MANY_RANGES, _M_pos);
          return false;
        }
      _M_out._M_ranges[_M_out._M_range_len++] = PatternRange{lo, hi};
      return true;
    }

    // Adds a predefined class (\d \w \s and negations) as concrete ranges.
    [[nodiscard]] constexpr bool add_predefined(PatternClass& cls, const char kind) noexcept
    {
      switch (kind)
        {
          case 'd':
            return add_range(cls, '0', '9');
          case 'D':
            return add_range(cls, 0, '0' - 1) && add_range(cls, '9' + 1, 0x10FFFF);
          case 'w':
            return add_range(cls, '0', '9') && add_range(cls, 'A', 'Z') && add_range(cls, 'a', 'z') &&
                   add_range(cls, '_', '_');
          case 'W':
            return add_range(cls, 0, '/') && add_range(cls, ':', '@') && add_range(cls, '[', '^') &&
                   add_range(cls, '`', '`') && add_range(cls, '{', 0x10FFFF);
          case 's':
            return add_range(cls, '\t', '\r') && add_range(cls, ' ', ' ');
          case 'S':
            return add_range(cls, 0, '\t' - 1) && add_range(cls, '\r' + 1, ' ' - 1) &&
                   add_range(cls, '!' + 1, 0x10FFFF);
          default:
            return false;
        }
    }

    [[nodiscard]] constexpr bool parse_class_escape(PatternClass& cls) noexcept
    {
      const uint32_t esc = _M_pos;
      ++_M_pos; // '\'
      if (_M_pos >= _M_pattern.size())
        {
          fail(PatternErrorKind::INVALID_ESCAPE, esc);
          return false;
        }
      const char e = peek();
      if (e == 'd' || e == 'D' || e == 'w' || e == 'W' || e == 's' || e == 'S')
        {
          ++_M_pos;
          if (!add_predefined(cls, e))
            {
              return false;
            }
          return true;
        }
      char32_t cp = 0;
      if (!parse_escape_scalar(cp))
        {
          return false;
        }
      return add_range(cls, cp, cp);
    }

    // Parses the body of an escape (after the backslash is accounted for) into a
    // single code point, or emits a whole predefined-class instruction.
    [[nodiscard]] constexpr bool parse_escape() noexcept
    {
      const uint32_t esc = _M_pos;
      ++_M_pos; // '\'
      if (_M_pos >= _M_pattern.size())
        {
          fail(PatternErrorKind::INVALID_ESCAPE, esc);
          return false;
        }
      const char e = peek();
      if (e == 'd' || e == 'D' || e == 'w' || e == 'W' || e == 's' || e == 'S')
        {
          ++_M_pos;
          if (_M_out._M_class_len >= MaxClass)
            {
              fail(PatternErrorKind::TOO_MANY_CLASSES, esc);
              return false;
            }
          const uint32_t class_id = static_cast<uint32_t>(_M_out._M_class_len);
          PatternClass cls{};
          cls.range_begin = static_cast<uint32_t>(_M_out._M_range_len);
          // \D \W \S are the complement; mark the class negated over the
          // positive ranges only, which matches RE2 semantics.
          const bool positive = e == 'd' || e == 'w' || e == 's';
          if (!add_predefined(cls, static_cast<char>(e | 0x20)))
            {
              return false;
            }
          cls.range_count = static_cast<uint32_t>(_M_out._M_range_len) - cls.range_begin;
          cls.negated = !positive;
          _M_out._M_classes[_M_out._M_class_len++] = cls;
          return emit(PatternInst{PatternOp::CLASS, 0, static_cast<uint16_t>(class_id), 0, 0});
        }
      char32_t cp = 0;
      if (!parse_escape_scalar(cp))
        {
          return false;
        }
      return emit(PatternInst{PatternOp::CHAR, 0, 0, static_cast<uint32_t>(cp), 0});
    }

    // Parses a single escaped character into `cp` (handles \n \xHH \u{...} etc).
    [[nodiscard]] constexpr bool parse_escape_scalar(char32_t& cp) noexcept
    {
      const uint32_t esc = _M_pos;
      if (peek() == '\\')
        {
          ++_M_pos;
        }
      if (_M_pos >= _M_pattern.size())
        {
          fail(PatternErrorKind::INVALID_ESCAPE, esc);
          return false;
        }
      const char e = peek();
      switch (e)
        {
          case 'n':
            cp = '\n';
            ++_M_pos;
            return true;
          case 'r':
            cp = '\r';
            ++_M_pos;
            return true;
          case 't':
            cp = '\t';
            ++_M_pos;
            return true;
          case 'f':
            cp = '\f';
            ++_M_pos;
            return true;
          case 'v':
            cp = '\v';
            ++_M_pos;
            return true;
          case '0':
            cp = '\0';
            ++_M_pos;
            return true;
          case 'x':
            ++_M_pos;
            return parse_hex_escape(cp);
          case 'u':
            ++_M_pos;
            return parse_unicode_escape(cp);
          default:
            break;
        }
      // Any other escaped character is itself (\\ \. \* \( ...).
      if (!consume_code_point(cp))
        {
          return false;
        }
      return true;
    }

    [[nodiscard]] constexpr bool parse_hex_escape(char32_t& cp) noexcept
    {
      const uint32_t start = _M_pos;
      uint32_t value = 0;
      uint32_t digits = 0;
      while (peek() >= '0' && peek() <= '9' || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F'))
        {
          value = value * 16U + static_cast<uint32_t>(hex_digit(peek()));
          ++_M_pos;
          ++digits;
          if (digits > 2)
            {
              fail(PatternErrorKind::INVALID_ESCAPE, start);
              return false;
            }
        }
      if (digits == 0)
        {
          fail(PatternErrorKind::INVALID_ESCAPE, start);
          return false;
        }
      cp = static_cast<char32_t>(value);
      return true;
    }

    [[nodiscard]] constexpr bool parse_unicode_escape(char32_t& cp) noexcept
    {
      const uint32_t start = _M_pos;
      if (peek() != '{')
        {
          fail(PatternErrorKind::INVALID_ESCAPE, start);
          return false;
        }
      ++_M_pos; // '{'
      uint32_t value = 0;
      uint32_t digits = 0;
      while (peek() != '}' && _M_pos < _M_pattern.size())
        {
          if (!is_hex(peek()))
            {
              fail(PatternErrorKind::INVALID_ESCAPE, start);
              return false;
            }
          value = value * 16U + static_cast<uint32_t>(hex_digit(peek()));
          ++_M_pos;
          ++digits;
          if (digits > 6)
            {
              fail(PatternErrorKind::INVALID_CODE_POINT, start);
              return false;
            }
        }
      if (peek() != '}' || digits == 0)
        {
          fail(PatternErrorKind::INVALID_ESCAPE, start);
          return false;
        }
      ++_M_pos; // '}'
      if (!is_unicode_scalar(static_cast<char32_t>(value)))
        {
          fail(PatternErrorKind::INVALID_CODE_POINT, start);
          return false;
        }
      cp = static_cast<char32_t>(value);
      return true;
    }

    [[nodiscard]] static constexpr bool is_hex(const char c) noexcept
    {
      return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    [[nodiscard]] static constexpr int hex_digit(const char c) noexcept
    {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      return c - 'A' + 10;
    }

    static constexpr uint32_t kUnbounded = 0xFFFFFFFFU;

    SimpleStringView _M_pattern;
    size_t _M_pos = 0;
    PatternType _M_out{};
    PatternError _M_error{};
    bool _M_failed = false;
    uint32_t _M_patches[MaxProgram]{};
    uint32_t _M_patch_offsets[MaxProgram]{};
    uint32_t _M_patch_len = 0;
  };

  export template <size_t MaxProgram = 64, size_t MaxClass = 8, size_t MaxRange = 64>
  [[nodiscard]] constexpr Maybe<CompiledPattern<MaxProgram, MaxClass, MaxRange>, PatternError>
  compile_pattern(const SimpleStringView pattern) noexcept
  {
    return PatternBuilder<MaxProgram, MaxClass, MaxRange>::build(pattern);
  }
} // namespace embdr::lex
