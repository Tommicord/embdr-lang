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

export module embdr.lex.semaScope;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.semaType;

namespace embdr::lex
{
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;

  // Sentinel index meaning "no local slot" for the liveness analysis (a
  // global, a method or an over-capacity local). Structural marker only —
  // failures that carry information are Maybe values.
  export inline constexpr uint32_t NO_SYMBOL = 0xFFFFFFFFu;

  export enum class SymbolKind : uint8_t {
    FUNCTION,
    STRUCT,
    ENUM,
    VARIANT,
    FIELD,
    PARAM,
    LOCAL,
    STATIC,
    ALIAS, // `use` binding
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const SymbolKind kind) noexcept
  {
    switch (kind)
      {
        case SymbolKind::FUNCTION:
          return "function";
        case SymbolKind::STRUCT:
          return "struct";
        case SymbolKind::ENUM:
          return "enum";
        case SymbolKind::VARIANT:
          return "variant";
        case SymbolKind::FIELD:
          return "field";
        case SymbolKind::PARAM:
          return "parameter";
        case SymbolKind::LOCAL:
          return "local";
        case SymbolKind::STATIC:
          return "static";
        case SymbolKind::ALIAS:
          return "alias";
      }
    return "symbol";
  }

  // One declared name. `name`/`type` are non-owning; the symbol table stores
  // values so a popped scope invalidates only the indices above its mark.
  export struct Symbol
  {
    SimpleStringView name{};
    SymbolKind kind = SymbolKind::LOCAL;
    TypeId type = NO_TYPE;
    uint32_t node = 0; // AST node id of the declaration
    uint32_t offset = 0; // source offset for "previous declaration" notes
    uint32_t owner = NO_SYMBOL; // struct/enum this FIELD/VARIANT/FUNCTION belongs to
    uint32_t local_index = NO_SYMBOL; // liveness slot (params 0..n-1, then locals)
    bool is_global = false;
    bool is_const = false;
    bool has_body = false; // functions: definition seen
    bool addr_main = false; // `#addr(main)` entry marker
    uint8_t local_flags = 0; // LOCAL_STATIC etc. (LOCAL_DECL bits)
  };

  // A lexical scope: a name, a parent link, and the symbol index where it
  // began (its "mark"). Scopes nest; the symbol table is a stack and popping
  // a scope truncates back to its mark.
  export struct Scope
  {
    SimpleStringView name{};
    uint32_t parent = 0; // enclosing scope index (self for file scope)
    uint32_t mark = 0; // first symbol index belonging to this scope
    uint32_t node = 0; // AST node that opened the scope
  };

  // Fixed-capacity symbol table plus a scope stack. Lookup walks outward from
  // the innermost scope; redeclaration is detected only within one scope
  // (shadowing an outer name is legal). Everything is bounded — a full table
  // reports via Maybe, never an allocation.
  export template <size_t MaxSymbols = 512, size_t MaxScopes = 64>
  class ScopeStack
  {
    static_assert (MaxSymbols > 0 && MaxScopes > 0, "ScopeStack capacities must be positive");

  public:
    static constexpr size_t max_symbols = MaxSymbols;
    static constexpr size_t max_scopes = MaxScopes;

    // Establishes the file scope as scope 0.
    constexpr ScopeStack () noexcept
    {
      _M_scopes[0] = Scope{"", 0, 0, 0};
      _M_scope_count = 1;
    }

    [[nodiscard]] constexpr uint32_t
    scope_count () const noexcept
    {
      return _M_scope_count;
    }

    [[nodiscard]] constexpr uint32_t
    symbol_count () const noexcept
    {
      return _M_top;
    }

    [[nodiscard]] constexpr const Scope&
    scope (const uint32_t idx) const noexcept
    {
      return _M_scopes[idx];
    }

    [[nodiscard]] constexpr const Symbol&
    symbol (const uint32_t idx) const noexcept
    {
      return _M_symbols[idx];
    }

    [[nodiscard]] constexpr uint32_t
    current_scope () const noexcept
    {
      return _M_scope_count - 1;
    }

    [[nodiscard]] constexpr Maybe<uint32_t, bool>
    push_scope (const SimpleStringView name, const uint32_t node) noexcept
    {
      if (_M_scope_count >= MaxScopes)
        {
          return false;
        }
      _M_scopes[_M_scope_count] = Scope{name, current_scope (), _M_top, node};
      return _M_scope_count++;
    }

    // Pops the innermost scope, discarding its symbols. Popping the file
    // scope is a no-op (it stays live for the whole translation unit).
    constexpr void
    pop_scope () noexcept
    {
      if (_M_scope_count > 1)
        {
          _M_top = _M_scopes[_M_scope_count - 1].mark;
          --_M_scope_count;
        }
    }

    // Declares `sym` in the innermost scope. Returns the stored index, or
    // an error carrying the existing symbol's index when the name already
    // lives in *this* scope (a redefinition). Shadowing outer scopes is fine.
    [[nodiscard]] constexpr Maybe<uint32_t, uint32_t>
    declare (const Symbol& sym) noexcept
    {
      if (_M_top >= MaxSymbols)
        {
          return NO_SYMBOL;
        }
      const uint32_t mark = _M_scopes[current_scope ()].mark;
      for (uint32_t i = mark; i < _M_top; ++i)
        {
          if (_M_symbols[i].name == sym.name)
            {
              return i; // error value = conflicting symbol index
            }
        }
      _M_symbols[_M_top] = sym;
      return _M_top++;
    }

    // Outward name lookup from the innermost scope. NO_SYMBOL when absent.
    [[nodiscard]] constexpr uint32_t
    lookup (const SimpleStringView name) const noexcept
    {
      uint32_t s = current_scope ();
      while (true)
        {
          for (uint32_t i = _M_scopes[s].mark; i < _M_top; ++i)
            {
              if (_M_symbols[i].name == name)
                {
                  return i;
                }
            }
          if (s == 0)
            {
              break;
            }
          s = _M_scopes[s].parent;
        }
      return NO_SYMBOL;
    }

    // Lookup restricted to the innermost scope (redeclaration checks).
    [[nodiscard]] constexpr uint32_t
    lookup_local (const SimpleStringView name) const noexcept
    {
      const uint32_t mark = _M_scopes[current_scope ()].mark;
      for (uint32_t i = mark; i < _M_top; ++i)
        {
          if (_M_symbols[i].name == name)
            {
              return i;
            }
        }
      return NO_SYMBOL;
    }

    // Finds the member of `owner_sym` (a struct field, method, enum
    // variant or named function) by name. Scans the whole table so the
    // declaration order of members never matters; entries belonging to
    // other owners and never-declared slots carry owner == NO_SYMBOL and
    // are skipped. NO_SYMBOL when the owner has no such member.
    [[nodiscard]] constexpr uint32_t
    find_member (const uint32_t owner_sym, const SimpleStringView name) const noexcept
    {
      for (uint32_t i = 0; i < MaxSymbols; ++i)
        {
          if (_M_symbols[i].owner == owner_sym && _M_symbols[i].name == name)
            {
              return i;
            }
        }
      return NO_SYMBOL;
    }

  private:
    Symbol _M_symbols[MaxSymbols]{};
    Scope _M_scopes[MaxScopes]{};
    uint32_t _M_top = 0;
    uint32_t _M_scope_count = 0;
  };
} // namespace embdr::lex
