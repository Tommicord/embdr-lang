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

export module embdr.lex.semaContext;
import embdr.cxxstd.stringNumber;
import embdr.cxxstd.stringView;
import embdr.lex.ast;
import embdr.lex.semaDiagnostic;
import embdr.lex.semaScope;
import embdr.lex.semaType;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::SimpleStringView;

  // Per-function checking state pushed while walking a body. `ret_type` is
  // the declared return type (NO_TYPE for none); `loop_depth` gates
  // break/continue; `volatile_depth` gates raw-pointer dereference
  // (CODE_QUALITY §1.4: `volatile` is the only opt-in to hardware access);
  // `next_local` is the running local-slot counter the liveness analysis
  // consumes; `label_count` sizes the active-label stack for `break :outer`.
  export struct FunctionContext
  {
    TypeId ret_type = NO_TYPE;
    bool has_explicit_ret = false;
    bool is_extern = false;
    uint32_t loop_depth = 0;
    uint32_t defer_depth = 0;
    uint32_t volatile_depth = 0;
    uint32_t next_local = 0; // next local slot for the liveness analysis
    // Active labels (scope-preserving innermost stack for `break :outer`).
    static constexpr uint32_t LABEL_CAPACITY = 8;
    SimpleStringView labels[LABEL_CAPACITY]{};
    uint32_t label_count = 0;
    // Symbol idx of the function being checked (call-graph edges).
    uint32_t function_symbol = NO_SYMBOL;
  };

  // One recorded call edge (caller function symbol -> callee function
  // symbol), captured during expression checking so the call-graph analysis
  // never has to re-resolve paths.
  export struct CallEdge
  {
    uint32_t caller = NO_SYMBOL;
    uint32_t callee = NO_SYMBOL;
  };

  // The Sema context: one bundling of the arena reference, diagnostic engine,
  // type table, scope stack and current-function state shared by the decl,
  // expression and statement passes. In Clang these are members of the single
  // `Sema` object whose methods are spread across SemaDecl/SemaExpr/SemaStmt;
  // here the same state is a plain struct so the checking passes can be split
  // into separate modules.
  //
  // The expression and statement checkers are mutually recursive (an
  // if-expression contains statements). To avoid a module cycle, the statement
  // checker is installed here (`set_stmt_checker`) by semaStmt and invoked by
  // semaExpr through `check_stmt`. The pointer is never null: it is wired to
  // a no-op in the constructor, which CODE_QUALITY §1.4 prefers over a
  // nullable hook that every call would have to null-check.
  export template <typename ArenaT>
  class SemaContext
  {
  public:
    using StmtChecker = void (*) (SemaContext&, uint32_t);
    static constexpr size_t MAX_NODES = ArenaT::capacity_value;
    static constexpr size_t MAX_CALL_EDGES = 512;

    // Pre-wire statement checking to a no-op so `check_stmt` can always
    // dispatch (§1.4: never a nullable function hook).
    static constexpr void
    noop_stmt_checker (SemaContext&, uint32_t) noexcept
    {
    }

    constexpr SemaContext (const ArenaT& arena, const SimpleStringView source, DiagnosticEngine& diags) noexcept :
        _M_arena (arena), _M_source (source), _M_diags (diags)
    {
      for (size_t i = 0; i < MAX_NODES; ++i)
        {
          _M_node_symbol[i] = NO_SYMBOL;
          _M_node_type[i] = NO_TYPE;
        }
    }

    [[nodiscard]] constexpr const ArenaT&
    arena () const noexcept
    {
      return _M_arena;
    }

    [[nodiscard]] constexpr const AstNode&
    node (const uint32_t id) const noexcept
    {
      return _M_arena[id];
    }

    // Number of direct children (walks the first_child -> next_sibling list).
    [[nodiscard]] constexpr uint32_t
    child_count (const uint32_t id) const noexcept
    {
      uint32_t n = 0;
      uint32_t c = _M_arena[id].first_child;
      while (c != NO_NODE)
        {
          ++n;
          c = _M_arena[c].next_sibling;
        }
      return n;
    }

    // The index-th child of `id`, or NO_NODE when out of range.
    [[nodiscard]] constexpr uint32_t
    child_at (const uint32_t id, const uint32_t index) const noexcept
    {
      uint32_t n = 0;
      uint32_t c = _M_arena[id].first_child;
      while (c != NO_NODE)
        {
          if (n == index)
            {
              return c;
            }
          ++n;
          c = _M_arena[c].next_sibling;
        }
      return NO_NODE;
    }

    [[nodiscard]] constexpr SimpleStringView
    source () const noexcept
    {
      return _M_source;
    }

    [[nodiscard]] constexpr DiagnosticEngine&
    diags () noexcept
    {
      return _M_diags;
    }

    [[nodiscard]] constexpr TypeTable<>&
    types () noexcept
    {
      return _M_types;
    }

    [[nodiscard]] constexpr ScopeStack<>&
    scopes () noexcept
    {
      return _M_scopes;
    }

    [[nodiscard]] constexpr FunctionContext&
    fn () noexcept
    {
      return _M_fn;
    }

    [[nodiscard]] constexpr const FunctionContext&
    fn () const noexcept
    {
      return _M_fn;
    }

    // --- resolution side tables (consumed by the flow analyses) ---

    // Records that expression node `node_id` resolved to symbol `sym`.
    constexpr void
    record_symbol (const uint32_t node_id, const uint32_t sym) noexcept
    {
      if (node_id < MAX_NODES)
        {
          _M_node_symbol[node_id] = sym;
        }
    }

    [[nodiscard]] constexpr uint32_t
    node_symbol (const uint32_t node_id) const noexcept
    {
      return node_id < MAX_NODES ? _M_node_symbol[node_id] : NO_SYMBOL;
    }

    // Records a call from the current function to `callee_sym`.
    constexpr void
    record_call (const uint32_t callee_sym) noexcept
    {
      if (_M_call_edge_count >= MAX_CALL_EDGES)
        {
          return;
        }
      _M_call_edges[_M_call_edge_count] = CallEdge{_M_fn.function_symbol, callee_sym};
      ++_M_call_edge_count;
    }

    [[nodiscard]] constexpr size_t
    call_edge_count () const noexcept
    {
      return _M_call_edge_count;
    }

    [[nodiscard]] constexpr const CallEdge&
    call_edge (const size_t i) const noexcept
    {
      return _M_call_edges[i];
    }

    constexpr void
    set_current_function (const uint32_t sym) noexcept
    {
      _M_fn.function_symbol = sym;
    }

    [[nodiscard]] constexpr uint32_t
    current_function () const noexcept
    {
      return _M_fn.function_symbol;
    }

    // Records the computed type of an expression node so the statement
    // checker, the driving pass and the analyses can read it back without
    // re-checking.
    constexpr void
    record_type (const uint32_t node_id, const TypeId type) noexcept
    {
      if (node_id < MAX_NODES)
        {
          _M_node_type[node_id] = type;
        }
    }

    [[nodiscard]] constexpr TypeId
    node_type_of (const uint32_t node_id) const noexcept
    {
      return node_id < MAX_NODES ? _M_node_type[node_id] : NO_TYPE;
    }

    // --- labels (active-label stack for `break :name` / `continue :name`) ---

    // Pushes a label; silently dropped when the stack is full (bound, not
    // an allocation). Returns the previous count so callers can pop.
    [[nodiscard]] constexpr uint32_t
    push_label (const SimpleStringView name) noexcept
    {
      if (_M_fn.label_count >= FunctionContext::LABEL_CAPACITY)
        {
          return _M_fn.label_count;
        }
      _M_fn.labels[_M_fn.label_count] = name;
      return _M_fn.label_count++;
    }

    [[nodiscard]] constexpr bool
    label_active (const SimpleStringView name) const noexcept
    {
      for (uint32_t i = 0; i < _M_fn.label_count; ++i)
        {
          if (_M_fn.labels[i] == name)
            {
              return true;
            }
        }
      return false;
    }

    // Restores the label stack to a previously returned count (scope exit).
    constexpr void
    pop_labels (const uint32_t count) noexcept
    {
      if (count < FunctionContext::LABEL_CAPACITY)
        {
          _M_fn.label_count = count;
        }
    }

    constexpr void
    set_stmt_checker (const StmtChecker stmt_checker) noexcept
    {
      _M_stmt_checker = stmt_checker;
    }

    // Always dispatches through a valid function: writes are never dropped.
    constexpr void
    check_stmt (const uint32_t node_id) noexcept
    {
      _M_stmt_checker (*this, node_id);
    }

    // --- diagnostics helpers (compose a message, intern it, report) ---

    constexpr void
    report (const DiagSeverity sev, const DiagId id, const uint32_t offset, const SimpleStringView message,
            const uint16_t span = 1) noexcept
    {
      _M_diags.report (sev, id, offset, _M_diags.intern (message), span);
    }

    // Formats `dst` (a caller-owned char buffer) with the human name of the
    // type `id` and returns a view over it. Composite names are spelled
    // `*elem`, `elem[]`, `name`, `fn(...)->ret`. Never allocates.
    template <unsigned int N>
    [[nodiscard]] constexpr SimpleStringView
    type_name (const TypeId id, char (&dst)[N]) const noexcept
    {
      MessageBuilder mb{dst, N};
      write_type_name (id, mb);
      return mb.view ();
    }

  private:
    constexpr void
    write_type_name (const TypeId id, MessageBuilder& mb) const noexcept
    {
      if (id == NO_TYPE)
        {
          mb.str ("<unknown>");
          return;
        }
      const Type& t = _M_types[id];
      switch (t.kind)
        {
          case TypeKind::POINTER:
            mb.ch ('*');
            write_type_name (t.inner, mb);
            return;
          case TypeKind::REFERENCE:
            write_type_name (t.inner, mb);
            mb.str (" ref");
            return;
          case TypeKind::ARRAY:
            mb.ch ('[');
            mb.u64 (t.array_len);
            mb.ch (']');
            write_type_name (t.inner, mb);
            return;
          case TypeKind::SLICE:
            mb.str ("[]");
            write_type_name (t.inner, mb);
            return;
          case TypeKind::MAYBE:
            mb.str ("maybe<");
            write_type_name (t.inner, mb);
            mb.ch ('>');
            return;
          case TypeKind::RESULT:
            mb.str ("result<");
            write_type_name (t.inner, mb);
            mb.ch (',');
            mb.ch (' ');
            write_type_name (t.inner2, mb);
            mb.ch ('>');
            return;
          case TypeKind::FUNCTION:
            mb.str ("fn(");
            for (uint8_t i = 0; i < t.param_count; ++i)
              {
                if (i != 0)
                  {
                    mb.str (", ");
                  }
                write_type_name (t.params[i], mb);
              }
            mb.str (")->");
            write_type_name (t.inner, mb);
            return;
          case TypeKind::STRUCT:
          case TypeKind::ENUM:
            mb.str (t.name);
            return;
          default:
            mb.str (as_str (t.kind));
            return;
        }
    }

    const ArenaT& _M_arena;
    SimpleStringView _M_source;
    DiagnosticEngine& _M_diags;
    TypeTable<> _M_types{};
    ScopeStack<> _M_scopes{};
    FunctionContext _M_fn{};
    // Wired in the constructor to `noop_stmt_checker` (§1.4: no nullable
    // hooks); semaStmt replaces it with the real statement checker.
    StmtChecker _M_stmt_checker = &noop_stmt_checker;
    uint32_t _M_node_symbol[MAX_NODES];
    TypeId _M_node_type[MAX_NODES]{};
    CallEdge _M_call_edges[MAX_CALL_EDGES]{};
    size_t _M_call_edge_count = 0;
  };
} // namespace embdr::lex
