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

#include <catch2/catch_all.hpp>
#include <concepts>
#include <cstdint>
#include <type_traits>

// glibc defines these as macros; they collide with the SystemTime::CLOCK_* members.
#undef CLOCK_MONOTONIC
#undef CLOCK_MONOTONIC_RAW
#undef CLOCK_REALTIME

import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.typedValue;
import embdr.cxxstd.timeCore;

using embdr::cxxstd::ClockId;
using embdr::cxxstd::IsTypedValue;
using embdr::cxxstd::IsTypedValueV;
using embdr::cxxstd::Maybe;
using embdr::cxxstd::SystemTime;
using embdr::cxxstd::TypedValue;

// Two mutually incompatible domains over the same underlying representation.
struct RowOffsetTag
{
};
struct ColumnOffsetTag
{
};
struct LineIndexTag
{
};

using RowOffset = TypedValue<int, RowOffsetTag>;
using ColumnOffset = TypedValue<int, ColumnOffsetTag>;
using LineIndex = TypedValue<int, LineIndexTag>;
using ByteCount = TypedValue<std::uint64_t, struct ByteCountTag>;

// ---------------------------------------------------------------------------
// Type-level guarantees. These are the heart of the "extreme type safety" contract:
// they fail the build, not the test run, the moment the wrapper regresses.
// ---------------------------------------------------------------------------

// Distinct tags over the same representation never mix, not even through construction
// or implicit conversion in one direction.
static_assert (embdr::cxxstd::TypedValueDistinct<RowOffset, ColumnOffset>);
static_assert (embdr::cxxstd::TypedValueDistinct<ColumnOffset, RowOffset>);
static_assert (embdr::cxxstd::TypedValueDistinct<RowOffset, LineIndex>);

// No implicit conversion from the raw value into the typed domain.
static_assert (!std::is_convertible_v<int, RowOffset>);
static_assert (!std::is_convertible_v<std::uint64_t, ByteCount>);

// ...but an explicit construction from the raw value is exactly one door, by design.
static_assert (std::is_constructible_v<RowOffset, int>);
static_assert (std::is_constructible_v<ByteCount, std::uint64_t>);

// Cross-tag construction is deleted by the type system (different specializations).
static_assert (!std::is_constructible_v<ColumnOffset, RowOffset>);
static_assert (!std::is_constructible_v<RowOffset, ColumnOffset>);

// Cross-tag comparisons do not exist: the operators require exact tag agreement,
// so deduction fails for any mixed use.
// Cross-tag and cross-value (raw) comparisons do not exist: the operators require exact
// tag agreement, so overload resolution finds no candidate. The probes are concepts with
// *dependent* parameter types (the only exact SFINAE-checkable probe form on GCC); the
// language then guarantees these expressions simply do not compile for any instantiation.
template <typename LhsTy, typename RhsTy>
concept EqualityObtainable = requires (const LhsTy lhs, const RhsTy rhs) {
  { lhs == rhs } -> std::convertible_to<bool>;
};
template <typename LhsTy, typename RhsTy>
concept LessComparable = requires (const LhsTy lhs, const RhsTy rhs) {
  { lhs < rhs } -> std::convertible_to<bool>;
};
template <typename LhsTy, typename RhsTy>
concept SpaceshipComparable = requires (const LhsTy lhs, const RhsTy rhs) {
  { lhs <=> rhs };
};

static_assert (EqualityObtainable<RowOffset, RowOffset>, "same tag compares");
static_assert (LessComparable<RowOffset, RowOffset>, "same tag orders");
static_assert (SpaceshipComparable<RowOffset, RowOffset>, "same tag spaceships");
static_assert (!EqualityObtainable<RowOffset, ColumnOffset>);
static_assert (!EqualityObtainable<ColumnOffset, RowOffset>);
static_assert (!EqualityObtainable<RowOffset, LineIndex>);
static_assert (!LessComparable<RowOffset, ColumnOffset>);
static_assert (!SpaceshipComparable<RowOffset, ColumnOffset>);

// Raw-value comparisons cannot deduce the operator templates either, in either direction.
static_assert (!EqualityObtainable<RowOffset, int>);
static_assert (!EqualityObtainable<int, RowOffset>);
static_assert (!LessComparable<RowOffset, int>);
static_assert (!LessComparable<int, RowOffset>);

// The wrapper is zero-overhead: same size, same triviality for a trivial payload.
static_assert (sizeof (RowOffset) == sizeof (int));
static_assert (std::is_trivially_copyable_v<RowOffset>);
static_assert (std::is_trivially_destructible_v<RowOffset>);

// Trait reflection agrees with itself.
static_assert (IsTypedValue<RowOffset>::value);
static_assert (!IsTypedValue<int>::value);
static_assert (IsTypedValueV<ByteCount>);
static_assert (!IsTypedValueV<const char*>);

// Domain types used in this codebase are first-class typed values.
static_assert (IsTypedValueV<ClockId>);
static_assert (sizeof (ClockId) == sizeof (int));

TEST_CASE ("default construction value-initializes to the zero of T", "[typedValue]")
{
  ByteCount zero{};
  REQUIRE (zero.get () == 0);
}

TEST_CASE ("explicit construction stores and returns the payload", "[typedValue]")
{
  const LineIndex line{42};
  REQUIRE (line.get () == 42);

  // Constructing with the same raw value produces an equal value...
  REQUIRE (line == LineIndex{42});
  // ...and any other value is distinguishable.
  REQUIRE (!(line == LineIndex{43}));
  REQUIRE (line < LineIndex{43});
  REQUIRE (LineIndex{0} < line);
}

TEST_CASE ("ordering derives from the wrapped type", "[typedValue]")
{
  const RowOffset low{2};
  const RowOffset middle{5};
  const RowOffset high{9};

  REQUIRE (low < middle);
  REQUIRE (middle < high);
  REQUIRE (low <= middle);
  REQUIRE (middle <= middle);
  REQUIRE (high > middle);
  REQUIRE (low >= low);
  REQUIRE (!(low == middle));
  REQUIRE (low != middle);
}

TEST_CASE ("equality and ordering work in constexpr contexts", "[typedValue]")
{
  static_assert (ByteCount{1} == ByteCount{1});
  static_assert (ByteCount{1} < ByteCount{2});
  static_assert (ByteCount{3}.get () + 1 == 4);
  REQUIRE (true);
}

TEST_CASE ("typed_value factory constructs from the raw value", "[typedValue]")
{
  constexpr auto span = embdr::cxxstd::typed_value<std::uint64_t, ByteCountTag> (512);
  REQUIRE (span.get () == 512);
  static_assert (embdr::cxxstd::typed_value<std::uint64_t, ByteCountTag> (1).get () == 1);
}

TEST_CASE ("swap exchanges same-tag values only", "[typedValue]")
{
  RowOffset first{1};
  RowOffset second{7};
  first.swap (second);
  REQUIRE (first.get () == 7);
  REQUIRE (second.get () == 1);
}

TEST_CASE ("SystemTime clock ids share the wrapper's zero-overhead behavior", "[typedValue]")
{
  // The strong clock id must stay copyable through every now_with_clock path.
  const ClockId monotonic = SystemTime::CLOCK_MONOTONIC;
  REQUIRE (monotonic.get () == SystemTime::CLOCK_MONOTONIC.get ());
  REQUIRE (monotonic == SystemTime::CLOCK_MONOTONIC);
  REQUIRE (!(monotonic == SystemTime::CLOCK_REALTIME));

  // Platform-specific ids can still be built explicitly (that is the one door);
  // unsupported ones are rejected at the syscall, as the timeCore test asserts.
  const ClockId radical{99};
  REQUIRE (!SystemTime::now_with_clock (radical).has_value ());
}

TEST_CASE ("TypedValue integrates with Maybe", "[typedValue]")
{
  const auto wrapped = embdr::cxxstd::Maybe<LineIndex, int> (LineIndex{7});
  REQUIRE (wrapped.has_value ());
  REQUIRE (wrapped.value ().get () == 7);

  const auto empty = embdr::cxxstd::Maybe<LineIndex, int>{};
  REQUIRE (!empty.has_value ());
}
