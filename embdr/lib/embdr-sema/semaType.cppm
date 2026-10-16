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

export module embdr.lex.semaType;
import embdr.cxxstd.stringView;

namespace embdr::lex
{
  using cxxstd::SimpleStringView;

  // A TypeId is a slot index into the TypeTable. NO_TYPE is a structural
  // marker for "absent type" (an unannotated PARAM, a failed resolution) —
  // never an error signal (failures are Maybe values).
  export using TypeId = uint32_t;
  export inline constexpr TypeId NO_TYPE = 0xFFFFFFFFu;

  // Fixed inline capacity for a function type's parameter list. Embedded
  // signatures are short; exceeding it clamps the count rather than
  // allocating (the type stays a valid opaque FUNCTION).
  export inline constexpr size_t MAX_TYPE_PARAMS = 8;

  // Semantic type categories. ERROR is the poison type: it is assignable to
  // and from everything so one bad sub-expression does not cascade.
  export enum class TypeKind : uint8_t {
    ERROR,
    // Untyped literal pseudo-kinds: assignable to any numeric (or string/
    // char, for their literals) contextual type, so `int64 x = 5;` works.
    // They never appear as a stored variable's final type — an unannotated
    // binding defaults to int32/float64.
    INT_LITERAL,
    FLOAT_LITERAL,
    VOID,
    BOOL,
    CHAR,
    INT8,
    INT16,
    INT32,
    INT64,
    INT128,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    UINT128,
    FLOAT16,
    FLOAT32,
    FLOAT64,
    U8STRING,
    U16STRING,
    STRUCT,
    ENUM,
    POINTER,
    REFERENCE,
    ARRAY,
    SLICE,
    FUNCTION,
    MAYBE,
    RESULT,
    RANGE,
  };

  export [[nodiscard]] constexpr bool
  is_integral_kind (const TypeKind kind) noexcept
  {
    return kind >= TypeKind::INT8 && kind <= TypeKind::UINT128;
  }

  export [[nodiscard]] constexpr bool
  is_signed_kind (const TypeKind kind) noexcept
  {
    return kind >= TypeKind::INT8 && kind <= TypeKind::INT128;
  }

  export [[nodiscard]] constexpr bool
  is_float_kind (const TypeKind kind) noexcept
  {
    return kind >= TypeKind::FLOAT16 && kind <= TypeKind::FLOAT64;
  }

  export [[nodiscard]] constexpr bool
  is_numeric_kind (const TypeKind kind) noexcept
  {
    return is_integral_kind (kind) || is_float_kind (kind);
  }

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const TypeKind kind) noexcept
  {
    switch (kind)
      {
        case TypeKind::ERROR:
          return "<error>";
        case TypeKind::INT_LITERAL:
          return "int literal";
        case TypeKind::FLOAT_LITERAL:
          return "float literal";
        case TypeKind::VOID:
          return "void";
        case TypeKind::BOOL:
          return "bool";
        case TypeKind::CHAR:
          return "char";
        case TypeKind::INT8:
          return "int8";
        case TypeKind::INT16:
          return "int16";
        case TypeKind::INT32:
          return "int32";
        case TypeKind::INT64:
          return "int64";
        case TypeKind::INT128:
          return "int128";
        case TypeKind::UINT8:
          return "uint8";
        case TypeKind::UINT16:
          return "uint16";
        case TypeKind::UINT32:
          return "uint32";
        case TypeKind::UINT64:
          return "uint64";
        case TypeKind::UINT128:
          return "uint128";
        case TypeKind::FLOAT16:
          return "float16";
        case TypeKind::FLOAT32:
          return "float32";
        case TypeKind::FLOAT64:
          return "float64";
        case TypeKind::U8STRING:
          return "u8string";
        case TypeKind::U16STRING:
          return "u16string";
        case TypeKind::STRUCT:
          return "struct";
        case TypeKind::ENUM:
          return "enum";
        case TypeKind::POINTER:
          return "pointer";
        case TypeKind::REFERENCE:
          return "reference";
        case TypeKind::ARRAY:
          return "array";
        case TypeKind::SLICE:
          return "slice";
        case TypeKind::FUNCTION:
          return "function";
        case TypeKind::MAYBE:
          return "maybe";
        case TypeKind::RESULT:
          return "result";
        case TypeKind::RANGE:
          return "range";
      }
    return "unknown";
  }

  // Builtin slots are interned at fixed indices in the first constructor of a
  // TypeTable, so their TypeIds are stable and shared across a compilation.
  // builtin_by_name returns these exact slots.
  export enum class BuiltinSlot : uint32_t {
    VOID = 1,
    BOOL = 2,
    CHAR = 3,
    INT8 = 4,
    INT16 = 5,
    INT32 = 6,
    INT64 = 7,
    INT128 = 8,
    UINT8 = 9,
    UINT16 = 10,
    UINT32 = 11,
    UINT64 = 12,
    UINT128 = 13,
    FLOAT16 = 14,
    FLOAT32 = 15,
    FLOAT64 = 16,
    U8STRING = 17,
    U16STRING = 18,
    RANGE = 19,
    INT_LITERAL = 20,
    FLOAT_LITERAL = 21,
    COUNT = 22, // first free slot
  };

  export [[nodiscard]] constexpr TypeId
  builtin_by_name (const SimpleStringView name) noexcept
  {
    if (name == "void")
      return static_cast<TypeId> (BuiltinSlot::VOID);
    if (name == "bool")
      return static_cast<TypeId> (BuiltinSlot::BOOL);
    if (name == "char")
      return static_cast<TypeId> (BuiltinSlot::CHAR);
    if (name == "int8")
      return static_cast<TypeId> (BuiltinSlot::INT8);
    if (name == "int16")
      return static_cast<TypeId> (BuiltinSlot::INT16);
    if (name == "int32")
      return static_cast<TypeId> (BuiltinSlot::INT32);
    if (name == "int64")
      return static_cast<TypeId> (BuiltinSlot::INT64);
    if (name == "int128")
      return static_cast<TypeId> (BuiltinSlot::INT128);
    if (name == "uint8")
      return static_cast<TypeId> (BuiltinSlot::UINT8);
    if (name == "uint16")
      return static_cast<TypeId> (BuiltinSlot::UINT16);
    if (name == "uint32")
      return static_cast<TypeId> (BuiltinSlot::UINT32);
    if (name == "uint64")
      return static_cast<TypeId> (BuiltinSlot::UINT64);
    if (name == "uint128")
      return static_cast<TypeId> (BuiltinSlot::UINT128);
    if (name == "float16")
      return static_cast<TypeId> (BuiltinSlot::FLOAT16);
    if (name == "float32")
      return static_cast<TypeId> (BuiltinSlot::FLOAT32);
    if (name == "float64")
      return static_cast<TypeId> (BuiltinSlot::FLOAT64);
    if (name == "u8string")
      return static_cast<TypeId> (BuiltinSlot::U8STRING);
    if (name == "u16string")
      return static_cast<TypeId> (BuiltinSlot::U16STRING);
    return NO_TYPE;
  }

  // One interned type. Composite kinds reuse `inner`/`inner2`/`params` by
  // kind: POINTER/REFERENCE/ARRAY/SLICE/MAYBE store the element in `inner`;
  // RESULT stores value in `inner` and error in `inner2`; FUNCTION stores the
  // return type in `inner` and parameters in `params`. `name` is a
  // non-owning view into the source (or a static literal) and must outlive
  // the table.
  export struct Type
  {
    TypeKind kind = TypeKind::ERROR;
    uint8_t param_count = 0;
    uint32_t array_len = 0;
    TypeId inner = NO_TYPE;
    TypeId inner2 = NO_TYPE;
    TypeId params[MAX_TYPE_PARAMS]{};
    SimpleStringView name{};
  };

  // Fixed-capacity intern table. Slot 0 is the ERROR poison; builtin scalars
  // occupy BuiltinSlot-fixed indices seeded by the constructor; composites are
  // interned on demand so structurally identical types share one slot (type
  // equality is slot equality). A full table returns NO_TYPE from the
  // interning methods — the caller surfaces it as a diagnostic, never an
  // allocation.
  export template <size_t Capacity = 256>
  class TypeTable
  {
    static_assert (Capacity > static_cast<size_t> (BuiltinSlot::COUNT),
                   "TypeTable capacity must fit every builtin slot");

  public:
    static constexpr size_t capacity_value = Capacity;

    constexpr TypeTable () noexcept
    {
      _M_types[0] = Type{TypeKind::ERROR, 0, 0, NO_TYPE, NO_TYPE, {}, "error"};
      seed (static_cast<TypeId> (BuiltinSlot::VOID), TypeKind::VOID, "void");
      seed (static_cast<TypeId> (BuiltinSlot::BOOL), TypeKind::BOOL, "bool");
      seed (static_cast<TypeId> (BuiltinSlot::CHAR), TypeKind::CHAR, "char");
      seed (static_cast<TypeId> (BuiltinSlot::INT8), TypeKind::INT8, "int8");
      seed (static_cast<TypeId> (BuiltinSlot::INT16), TypeKind::INT16, "int16");
      seed (static_cast<TypeId> (BuiltinSlot::INT32), TypeKind::INT32, "int32");
      seed (static_cast<TypeId> (BuiltinSlot::INT64), TypeKind::INT64, "int64");
      seed (static_cast<TypeId> (BuiltinSlot::INT128), TypeKind::INT128, "int128");
      seed (static_cast<TypeId> (BuiltinSlot::UINT8), TypeKind::UINT8, "uint8");
      seed (static_cast<TypeId> (BuiltinSlot::UINT16), TypeKind::UINT16, "uint16");
      seed (static_cast<TypeId> (BuiltinSlot::UINT32), TypeKind::UINT32, "uint32");
      seed (static_cast<TypeId> (BuiltinSlot::UINT64), TypeKind::UINT64, "uint64");
      seed (static_cast<TypeId> (BuiltinSlot::UINT128), TypeKind::UINT128, "uint128");
      seed (static_cast<TypeId> (BuiltinSlot::FLOAT16), TypeKind::FLOAT16, "float16");
      seed (static_cast<TypeId> (BuiltinSlot::FLOAT32), TypeKind::FLOAT32, "float32");
      seed (static_cast<TypeId> (BuiltinSlot::FLOAT64), TypeKind::FLOAT64, "float64");
      seed (static_cast<TypeId> (BuiltinSlot::U8STRING), TypeKind::U8STRING, "u8string");
      seed (static_cast<TypeId> (BuiltinSlot::U16STRING), TypeKind::U16STRING, "u16string");
      seed (static_cast<TypeId> (BuiltinSlot::RANGE), TypeKind::RANGE, "range");
      seed (static_cast<TypeId> (BuiltinSlot::INT_LITERAL), TypeKind::INT_LITERAL, "int literal");
      seed (static_cast<TypeId> (BuiltinSlot::FLOAT_LITERAL), TypeKind::FLOAT_LITERAL, "float literal");
      _M_count = static_cast<uint32_t> (BuiltinSlot::COUNT);
    }

    [[nodiscard]] constexpr TypeId
    error () const noexcept
    {
      return 0;
    }

    [[nodiscard]] constexpr TypeId
    int_literal () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::INT_LITERAL);
    }

    [[nodiscard]] constexpr TypeId
    float_literal () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::FLOAT_LITERAL);
    }

    [[nodiscard]] constexpr TypeId
    void_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::VOID);
    }

    [[nodiscard]] constexpr TypeId
    bool_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::BOOL);
    }

    [[nodiscard]] constexpr TypeId
    char_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::CHAR);
    }

    [[nodiscard]] constexpr TypeId
    int32_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::INT32);
    }

    [[nodiscard]] constexpr TypeId
    uint32_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::UINT32);
    }

    [[nodiscard]] constexpr TypeId
    float64_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::FLOAT64);
    }

    [[nodiscard]] constexpr TypeId
    u8string_type () const noexcept
    {
      return static_cast<TypeId> (BuiltinSlot::U8STRING);
    }

    [[nodiscard]] constexpr const Type&
    operator[] (const TypeId id) const noexcept
    {
      return _M_types[id];
    }

    [[nodiscard]] constexpr TypeKind
    kind_of (const TypeId id) const noexcept
    {
      return id == NO_TYPE ? TypeKind::ERROR : _M_types[id].kind;
    }

    [[nodiscard]] constexpr bool
    is_error (const TypeId id) const noexcept
    {
      return kind_of (id) == TypeKind::ERROR;
    }

    [[nodiscard]] constexpr bool
    valid (const TypeId id) const noexcept
    {
      return id != NO_TYPE && id < _M_count;
    }

    // Interns a composite or named type; identical shapes share a slot.
    [[nodiscard]] constexpr TypeId
    intern (const Type type) noexcept
    {
      for (uint32_t i = 1; i < _M_count; ++i)
        {
          if (same (_M_types[i], type))
            {
              return i;
            }
        }
      if (_M_count >= Capacity)
        {
          return NO_TYPE;
        }
      _M_types[_M_count] = type;
      return _M_count++;
    }

    [[nodiscard]] constexpr TypeId
    pointer_to (const TypeId pointee) noexcept
    {
      Type t{};
      t.kind = TypeKind::POINTER;
      t.inner = pointee;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    reference_to (const TypeId referent) noexcept
    {
      Type t{};
      t.kind = TypeKind::REFERENCE;
      t.inner = referent;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    array_of (const TypeId elem, const uint32_t len) noexcept
    {
      Type t{};
      t.kind = TypeKind::ARRAY;
      t.inner = elem;
      t.array_len = len;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    slice_of (const TypeId elem) noexcept
    {
      Type t{};
      t.kind = TypeKind::SLICE;
      t.inner = elem;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    maybe_of (const TypeId inner) noexcept
    {
      Type t{};
      t.kind = TypeKind::MAYBE;
      t.inner = inner;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    result_of (const TypeId value, const TypeId error) noexcept
    {
      Type t{};
      t.kind = TypeKind::RESULT;
      t.inner = value;
      t.inner2 = error;
      return intern (t);
    }

    // Interns a named STRUCT/ENUM reference. Identifiers share a slot by
    // (kind, name), so a struct and an enum of the same name stay distinct.
    [[nodiscard]] constexpr TypeId
    named (const TypeKind kind, const SimpleStringView name) noexcept
    {
      Type t{};
      t.kind = kind;
      t.name = name;
      return intern (t);
    }

    [[nodiscard]] constexpr TypeId
    function_of (const TypeId ret, const TypeId* const params, const size_t param_count) noexcept
    {
      Type t{};
      t.kind = TypeKind::FUNCTION;
      t.inner = ret;
      t.param_count
          = param_count > MAX_TYPE_PARAMS ? static_cast<uint8_t> (MAX_TYPE_PARAMS) : static_cast<uint8_t> (param_count);
      for (uint8_t i = 0; i < t.param_count; ++i)
        {
          t.params[i] = params[i];
        }
      return intern (t);
    }

    // Implicit-convertibility: can a value of `src` be used where `dst` is
    // expected? ERROR (either side) is permissive to suppress cascades.
    [[nodiscard]] constexpr bool
    assignable (const TypeId dst, const TypeId src) const noexcept
    {
      if (dst == NO_TYPE || src == NO_TYPE)
        {
          return true;
        }
      if (dst == src)
        {
          return true;
        }
      const TypeKind dk = kind_of (dst);
      const TypeKind sk = kind_of (src);
      if (dk == TypeKind::ERROR || sk == TypeKind::ERROR)
        {
          return true;
        }
      // Untyped literals adopt their contextual type: an integer literal
      // fits any numeric (or char) target, a float literal any float.
      if (sk == TypeKind::INT_LITERAL && (is_numeric_kind (dk) || dk == TypeKind::CHAR))
        {
          return true;
        }
      if (sk == TypeKind::FLOAT_LITERAL && is_float_kind (dk))
        {
          return true;
        }
      // Named types compare by kind+name.
      if (dk == sk && (dk == TypeKind::STRUCT || dk == TypeKind::ENUM))
        {
          return _M_types[dst].name == _M_types[src].name;
        }
      // Pointers / references / arrays / slices / maybe / result recurse.
      if (dk == TypeKind::POINTER && sk == TypeKind::POINTER)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner);
        }
      if (dk == TypeKind::REFERENCE && sk == TypeKind::REFERENCE)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner);
        }
      if (dk == TypeKind::ARRAY && sk == TypeKind::ARRAY)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner);
        }
      if (dk == TypeKind::SLICE && sk == TypeKind::SLICE)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner);
        }
      if (dk == TypeKind::MAYBE && sk == TypeKind::MAYBE)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner);
        }
      if (dk == TypeKind::RESULT && sk == TypeKind::RESULT)
        {
          return assignable (_M_types[dst].inner, _M_types[src].inner)
                 && assignable (_M_types[dst].inner2, _M_types[src].inner2);
        }
      if (dk == TypeKind::RANGE && sk == TypeKind::RANGE)
        {
          return true;
        }
      return false;
    }

    // Result type of an arithmetic/logical operation on `a` and `b`.
    // Literals adopt the concrete operand; mixing a concrete integer with a
    // float yields float64; two concrete integers unify to the wider one
    // (signed preferred on a tie). Returns the ERROR slot when either side
    // is already an error.
    [[nodiscard]] constexpr TypeId
    unify_numeric (const TypeId a, const TypeId b) const noexcept
    {
      if (a == NO_TYPE || b == NO_TYPE)
        {
          return error ();
        }
      if (a == b)
        {
          return a;
        }
      const TypeKind ak = kind_of (a);
      const TypeKind bk = kind_of (b);
      if (ak == TypeKind::ERROR || bk == TypeKind::ERROR)
        {
          return error ();
        }
      if (ak == TypeKind::INT_LITERAL && is_numeric_kind (bk))
        {
          return b;
        }
      if (bk == TypeKind::INT_LITERAL && is_numeric_kind (ak))
        {
          return a;
        }
      if (ak == TypeKind::FLOAT_LITERAL && is_float_kind (bk))
        {
          return b;
        }
      if (bk == TypeKind::FLOAT_LITERAL && is_float_kind (ak))
        {
          return a;
        }
      if ((ak == TypeKind::INT_LITERAL && bk == TypeKind::FLOAT_LITERAL)
          || (ak == TypeKind::FLOAT_LITERAL && bk == TypeKind::INT_LITERAL))
        {
          return float64_type ();
        }
      if (is_float_kind (ak) || is_float_kind (bk))
        {
          return float64_type ();
        }
      if (is_integral_kind (ak) && is_integral_kind (bk))
        {
          return rank (ak) > rank (bk) || (rank (ak) == rank (bk) && is_signed_kind (ak)) ? a : b;
        }
      return error ();
    }

    [[nodiscard]] constexpr size_t
    count () const noexcept
    {
      return _M_count;
    }

  private:
    // Approximate bit-width of an integer kind, used to pick the wider of
    // two concrete integers. Non-integral kinds map to 0.
    [[nodiscard]] static constexpr uint32_t
    rank (const TypeKind k) noexcept
    {
      switch (k)
        {
          case TypeKind::INT8:
          case TypeKind::UINT8:
            return 8;
          case TypeKind::INT16:
          case TypeKind::UINT16:
            return 16;
          case TypeKind::INT32:
          case TypeKind::UINT32:
            return 32;
          case TypeKind::INT64:
          case TypeKind::UINT64:
            return 64;
          case TypeKind::INT128:
          case TypeKind::UINT128:
            return 128;
          default:
            return 0;
        }
    }

    constexpr void
    seed (const TypeId slot, const TypeKind kind, const SimpleStringView name) noexcept
    {
      _M_types[slot] = Type{kind, 0, 0, NO_TYPE, NO_TYPE, {}, name};
    }

    [[nodiscard]] static constexpr bool
    same (const Type& a, const Type& b) noexcept
    {
      if (a.kind != b.kind || a.array_len != b.array_len || a.param_count != b.param_count)
        {
          return false;
        }
      // Named/struct/enum composite equality includes the name.
      if ((a.kind == TypeKind::STRUCT || a.kind == TypeKind::ENUM) && a.name != b.name)
        {
          return false;
        }
      if (a.inner != b.inner || a.inner2 != b.inner2)
        {
          return false;
        }
      for (uint8_t i = 0; i < a.param_count; ++i)
        {
          if (a.params[i] != b.params[i])
            {
              return false;
            }
        }
      return true;
    }

    Type _M_types[Capacity]{};
    uint32_t _M_count = 0;
  };
} // namespace embdr::lex
