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
#include <concepts>
#include <cstddef>
#include <type_traits>

export module embdr.cxxstd.typedValue;

// TypedValue<T, Tag> is the project's strong-typedef primitive: two instantiations that share
// the same underlying representation but carry a different Tag are distinct, incompatible types.
// All operations are constrained so that a raw T value can never slip into a TypedValue slot (or
// back out) without an explicit construction, and no arithmetic is available by default — a tag
// domain must state exactly which operations make sense for it.

namespace embdr::cxxstd
{
  export template <typename T, typename Tag>
  class TypedValue
  {
    // The wrapped representation must be an actually storable, zero-overhead value type.
    static_assert (!std::is_array_v<T>, "embdr::cxxstd::TypedValue cannot wrap an array type (it decays)");
    static_assert (!std::is_reference_v<T>, "embdr::cxxstd::TypedValue cannot wrap a reference type");
    static_assert (!std::is_volatile_v<T>, "embdr::cxxstd::TypedValue cannot wrap a volatile type");
    static_assert (!std::is_void_v<T>, "embdr::cxxstd::TypedValue cannot wrap void");
    static_assert (!std::is_function_v<T>, "embdr::cxxstd::TypedValue cannot wrap a function type");
    static_assert (!std::is_abstract_v<T>, "embdr::cxxstd::TypedValue cannot wrap an abstract class type");
    static_assert (!std::is_pointer_v<T>,
                   "embdr::cxxstd::TypedValue cannot wrap a pointer: use Maybe<T*, E> to express absence");
    // Size-first rule (binary size outranks everything else): the wrapper must compile to the
    // exact same code as the raw T. A non-trivially-copyable payload needs a hidden function
    // call on every copy — use the raw type or a dedicated domain type instead.
    static_assert (std::is_trivially_copyable_v<T>,
                   "embdr::cxxstd::TypedValue payload must be trivially copyable to guarantee zero-overhead wrapping");
    static_assert (!std::is_same_v<std::remove_cv_t<T>, std::nullptr_t>,
                   "embdr::cxxstd::TypedValue cannot wrap nullptr_t: nullptr is not a value type in this codebase");

  public:
    using value_type = T;
    using tag_type = Tag;

    // Value-initialized: a default-constructed TypedValue is the zero of T, never an
    // indeterminate bit pattern (keeps the type friendly to static storage).
    constexpr TypedValue () noexcept {}

    // Explicit by design: `ClockId` ids never convert to/from the raw count implicitly.
    // This is the one door between raw T and the typed domain.
    constexpr explicit TypedValue (const T value) noexcept (std::is_nothrow_copy_constructible_v<T>) : _M_value (value)
    {
    }

    // nullptr can never name a TypedValue, not even through overload resolution.
    constexpr TypedValue (std::nullptr_t) = delete;

    constexpr TypedValue (const TypedValue&) noexcept = default;
    constexpr TypedValue (TypedValue&&) noexcept = default;
    constexpr TypedValue&
    operator= (const TypedValue&) noexcept = default;
    constexpr TypedValue&
    operator= (TypedValue&&) noexcept = default;
    constexpr ~TypedValue () noexcept = default;

    [[nodiscard]] constexpr T
    get () const noexcept (std::is_nothrow_copy_constructible_v<T>)
    {
      return this->_M_value;
    }

    // Same-tag swap only. Swapping two instantiations with different tags would silently
    // pun one domain onto another, so the parameters are exact.
    constexpr void
    swap (TypedValue& other) noexcept
    {
      TypedValue tmp = other;
      other = *this;
      *this = tmp;
    }

  private:
    T _M_value{};
  };

  // Trait + concept mirrors of the std::* traits used elsewhere in embdr (see
  // IsStringConvertible in stringView.cppm): class-template primary + partial specializations.
  export template <typename>
  struct IsTypedValue : std::false_type
  {
  };
  template <typename T, typename Tag>
  struct IsTypedValue<TypedValue<T, Tag>> : std::true_type
  {
  };
  export template <typename T>
  inline constexpr bool IsTypedValueV = IsTypedValue<T>::value;

  // Same-tag equality only. Deliberately a function template (not a friend defined inside the
  // class): deduction from two different Tag instantiations fails, so `a == b` across domains is
  // a compile-time error, not a false match. Comparing against a raw `5` cannot deduce either.
  export template <typename T, typename Tag>
    requires std::equality_comparable<T>
  [[nodiscard]] constexpr bool
  operator== (const TypedValue<T, Tag>& lhs,
              const TypedValue<T, Tag>& rhs) noexcept (noexcept (lhs.get () == rhs.get ()))
  {
    return lhs.get () == rhs.get ();
  }

  // Same-tag ordering only. `<`, `<=`, `>`, `>=` and `!=` are rewritten from these two by the
  // language rules, so nothing extra has to be defined (and nothing can be overridden per domain).
  export template <typename T, typename Tag>
    requires std::totally_ordered<T>
  [[nodiscard]] constexpr auto
  operator<=> (const TypedValue<T, Tag>& lhs,
               const TypedValue<T, Tag>& rhs) noexcept (noexcept (lhs.get () <=> rhs.get ()))
  {
    return lhs.get () <=> rhs.get ();
  }

  // Factory mirroring the `rust`-style `unsafe-free from()` constructors used in timeCore: a
  // named, therefore greppable, conversion point from a raw value into a typed domain.
  export template <typename T, typename Tag>
  [[nodiscard]] constexpr TypedValue<T, Tag>
  typed_value (const T value) noexcept (std::is_nothrow_copy_constructible_v<T>)
  {
    return TypedValue<T, Tag> (value);
  }

  // Compile-time probe used by tests and callers to prove that two typed domains are mutually
  // non-constructible (the strongest form of unit-style separation available in C++26).
  export template <typename FromTy, typename ToTy>
    requires IsTypedValue<FromTy>::value && IsTypedValue<ToTy>::value
  inline constexpr bool TypedValueDistinct
      = !std::is_constructible_v<ToTy, FromTy> && !std::is_convertible_v<FromTy, ToTy>;
} // namespace embdr::cxxstd
