/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>
#include <cstdint>
#include <ctime>

// glibc defines these as macros; they collide with the SystemTime::CLOCK_* members.
#undef CLOCK_MONOTONIC
#undef CLOCK_MONOTONIC_RAW
#undef CLOCK_REALTIME

import embdr.cxxstd.timeCore;

using embdr::cxxstd::SystemTime;
using embdr::cxxstd::TimeDuration;
using embdr::cxxstd::TimeError;
using embdr::cxxstd::TimeVal;
using embdr::cxxstd::Timespec;

TEST_CASE("monotonic clock returns a valid non-zero time", "[timeCore]") {
        const auto now = SystemTime::monotonic();
        REQUIRE(now.has_value());
        const bool nonzero = now.value().secs() > 0 || now.value().nsecs() > 0;
        REQUIRE(nonzero);
}

TEST_CASE("monotonic time is non-decreasing", "[timeCore]") {
        const auto first = SystemTime::monotonic();
        REQUIRE(first.has_value());
        TimeVal previous = first.value();
        for (int i = 0; i < 256; ++i) {
                const auto current = SystemTime::monotonic();
                REQUIRE(current.has_value());
                REQUIRE(SystemTime::compare(previous, current.value()) <= 0);
                previous = current.value();
        }
}

TEST_CASE("realtime returns wall-clock time no earlier than 2020", "[timeCore]") {
        const auto wall = SystemTime::realtime();
        REQUIRE(wall.has_value());
        // 2020-01-01T00:00:00Z
        REQUIRE(wall.value().secs() >= 1'577'836'800ull);
        REQUIRE(wall.value().nsecs() < 1'000'000'000ull);
}

TEST_CASE("now() returns a sane monotonic reading", "[timeCore]") {
        const auto now = SystemTime::now();
        REQUIRE(now.has_value());
        REQUIRE(now.value().nsecs() < 1'000'000'000ull);
        const bool nonzero = now.value().secs() > 0 || now.value().nsecs() > 0;
        REQUIRE(nonzero);
        // now() and monotonic() read the same clock, taken in this order.
        const auto mono = SystemTime::monotonic();
        REQUIRE(mono.has_value());
        REQUIRE(SystemTime::compare(now.value(), mono.value()) <= 0);
}

TEST_CASE("monotonic_raw returns a valid reading", "[timeCore]") {
        const auto raw = SystemTime::monotonic_raw();
        REQUIRE(raw.has_value());
        REQUIRE(raw.value().nsecs() < 1'000'000'000ull);
}

TEST_CASE("conversions are correct", "[timeCore]") {
        REQUIRE(SystemTime::secs_to_nanos(1) == 1'000'000'000ull);
        REQUIRE(SystemTime::millis_to_nanos(1) == 1'000'000ull);
        REQUIRE(SystemTime::micros_to_nanos(1) == 1'000ull);
        REQUIRE(SystemTime::nanos_to_secs(1'000'000'000ull) == 1);
        REQUIRE(SystemTime::nanos_to_millis(1'000'000ull) == 1);
        REQUIRE(SystemTime::nanos_to_micros(1'000ull) == 1);
}

TEST_CASE("elapsed_since is positive", "[timeCore]") {
        const auto start_maybe = SystemTime::monotonic();
        REQUIRE(start_maybe.has_value());
        const TimeVal start = start_maybe.value();
        // Spin until the clock advances so the assertion below is deterministic.
        TimeVal probe = start;
        for (int i = 0; i < 10'000'000 && SystemTime::compare(probe, start) == 0; ++i)
                probe = SystemTime::monotonic().value();
        const auto elapsed = SystemTime::elapsed_since(start);
        REQUIRE(elapsed.has_value());
        REQUIRE(elapsed.value() > 0);
}

TEST_CASE("duration_since is non-negative", "[timeCore]") {
        const TimeVal earlier{5, 0};
        const TimeVal later{10, 0};
        REQUIRE(SystemTime::duration_since(earlier, later) == 5'000'000'000ull);
}

TEST_CASE("duration_since returns zero when reversed", "[timeCore]") {
        const TimeVal earlier{10, 0};
        const TimeVal later{5, 0};
        REQUIRE(SystemTime::duration_since(earlier, later) == 0);
}

TEST_CASE("nsecs_from_val is correct", "[timeCore]") {
        const TimeVal tv{2, 500'000'000};
        REQUIRE(SystemTime::nsecs_from_val(tv) == 2'500'000'000ull);
        REQUIRE(SystemTime::micros_from_val(tv) == 2'500'000ull);
        REQUIRE(SystemTime::millis_from_val(tv) == 2'500ull);
        REQUIRE(SystemTime::secs_from_val(tv) == 2);
}

TEST_CASE("add_timeval wraps nanos into seconds", "[timeCore]") {
        const TimeVal a{1, 900'000'000};
        const TimeVal b{2, 300'000'000};
        const TimeVal result = SystemTime::add_timeval(a, b);
        REQUIRE(result.secs() == 4);
        REQUIRE(result.nsecs() == 200'000'000);
}

TEST_CASE("sub_timeval borrows from seconds", "[timeCore]") {
        const TimeVal a{5, 500'000'000};
        const TimeVal b{3, 700'000'000};
        const TimeVal result = SystemTime::sub_timeval(a, b);
        REQUIRE(result.secs() == 1);
        REQUIRE(result.nsecs() == 800'000'000);
}

TEST_CASE("sub_timeval returns zero when reversed", "[timeCore]") {
        const TimeVal a{3, 0};
        const TimeVal b{5, 0};
        const TimeVal result = SystemTime::sub_timeval(a, b);
        const TimeVal zero{0, 0};
        REQUIRE(result == zero);
}

TEST_CASE("compare returns correct ordering", "[timeCore]") {
        const TimeVal a{1, 0};
        const TimeVal b{2, 0};
        REQUIRE(SystemTime::compare(a, b) == -1);
        REQUIRE(SystemTime::compare(b, a) == 1);
        REQUIRE(SystemTime::compare(a, a) == 0);
}

TEST_CASE("timespec roundtrips through libc layout", "[timeCore]") {
        const Timespec original{42, 1'234'567'890};
        const auto libc_ts = original.into_libc();
        const Timespec restored = Timespec::from_libc(libc_ts);
        REQUIRE(restored == original);
}

TEST_CASE("invalid clock id yields an error", "[timeCore]") {
        const auto bad = SystemTime::now_with_clock(-1);
        REQUIRE(!bad.has_value());

        TimeError error{};
        const auto bad_with_error = SystemTime::now_with_clock(-1, error);
        REQUIRE(!bad_with_error.has_value());
        REQUIRE(error.kind() == TimeError::Kind::SyscallError);
        REQUIRE(error.code() != 0);

        // An unknown positive clock id fails as well.
        const auto unknown = SystemTime::now_with_clock(42);
        REQUIRE(!unknown.has_value());
}

TEST_CASE("TimeError variants carry their identity", "[timeCore]") {
        const auto syscall = TimeError::syscall_error(-22);
        REQUIRE(syscall.kind() == TimeError::Kind::SyscallError);
        REQUIRE(syscall.code() == -22);
        REQUIRE(syscall == TimeError::syscall_error(-22));
        REQUIRE(!(syscall == TimeError::syscall_error(-1)));
        REQUIRE(TimeError::unsupported_clock().kind() == TimeError::Kind::UnsupportedClock);
        REQUIRE(TimeError::not_available().kind() == TimeError::Kind::NotAvailable);
}

TEST_CASE("nanos convenience accessors", "[timeCore]") {
        REQUIRE(SystemTime::monotonic_nanos() > 0);
        // Realtime nanoseconds must be past 2020-01-01.
        REQUIRE(SystemTime::realtime_nanos() >= 1'577'836'800ull * 1'000'000'000ull);
}

TEST_CASE("TimeDuration construction and unit conversion", "[timeCore]") {
        const auto duration = TimeDuration::from_secs(5);
        REQUIRE(duration.as_secs() == 5);
        REQUIRE(duration.subsec_nanos() == 0);

        const auto millis = TimeDuration::from_millis(2'569);
        REQUIRE(millis.as_secs() == 2);
        REQUIRE(millis.subsec_nanos() == 569'000'000);

        const auto micros = TimeDuration::from_micros(1'000'002);
        REQUIRE(micros.as_secs() == 1);
        REQUIRE(micros.subsec_nanos() == 2'000);

        const auto nanos = TimeDuration::from_nanos(1'000'000'123);
        REQUIRE(nanos.as_secs() == 1);
        REQUIRE(nanos.subsec_nanos() == 123);

        const TimeDuration five{5, 730'023'852};
        REQUIRE(five.as_secs() == 5);
        REQUIRE(five.subsec_millis() == 730);
        REQUIRE(five.subsec_micros() == 730'023);
        REQUIRE(five.subsec_nanos() == 730'023'852);
}

TEST_CASE("TimeDuration new carries nanoseconds into seconds", "[timeCore]") {
        const TimeDuration carried(5, 1'500'000'000u);
        REQUIRE(carried.as_secs() == 6);
        REQUIRE(carried.subsec_nanos() == 500'000'000u);

        const TimeDuration saturated(~std::uint64_t{0}, 1'500'000'000u);
        REQUIRE(saturated.as_secs() == ~std::uint64_t{0});
        REQUIRE(saturated.subsec_nanos() == 500'000'000u);
}

TEST_CASE("TimeDuration total-unit accessors", "[timeCore]") {
        const TimeDuration duration{5, 730'023'852};
        REQUIRE(static_cast<std::uint64_t>(duration.as_millis()) == 5'730ull);
        REQUIRE(static_cast<std::uint64_t>(duration.as_micros()) == 5'730'023ull);
        REQUIRE(static_cast<std::uint64_t>(duration.as_nanos()) == 5'730'023'852ull);
}

TEST_CASE("TimeDuration is_zero", "[timeCore]") {
        REQUIRE(TimeDuration::ZERO.is_zero());
        REQUIRE(TimeDuration(0, 0).is_zero());
        REQUIRE(TimeDuration::from_nanos(0).is_zero());
        REQUIRE(TimeDuration::from_secs(0).is_zero());
        REQUIRE(!TimeDuration(1, 1).is_zero());
        REQUIRE(!TimeDuration::from_nanos(1).is_zero());
        REQUIRE(!TimeDuration::from_secs(1).is_zero());
}

TEST_CASE("TimeDuration from_nanos_u128", "[timeCore]") {
        unsigned __int128 nanos = 1;
        for (int i = 0; i < 24; ++i)
                nanos *= 10;
        nanos += 321;
        const auto duration = TimeDuration::from_nanos_u128(nanos);
        REQUIRE(duration.has_value());
        REQUIRE(duration.value().as_secs() == 1'000'000'000'000'000ull);
        REQUIRE(duration.value().subsec_nanos() == 321);

        const unsigned __int128 too_big = ~static_cast<unsigned __int128>(0);
        REQUIRE(!TimeDuration::from_nanos_u128(too_big).has_value());
}

TEST_CASE("TimeDuration checked_add reports overflow", "[timeCore]") {
        const TimeDuration lhs{1, 0};
        const TimeDuration rhs{~std::uint64_t{0}, 0};

        const auto normal = TimeDuration(0, 0).checked_add(TimeDuration(0, 1));
        REQUIRE(normal.has_value());
        REQUIRE(normal.value() == TimeDuration(0, 1));

        REQUIRE(!lhs.checked_add(rhs).has_value());
        REQUIRE(lhs.saturating_add(rhs) == TimeDuration::MAX);
        REQUIRE(TimeDuration(0, 0).saturating_add(TimeDuration(0, 1)) == TimeDuration(0, 1));

        // Carry into the seconds component still fits.
        const auto carry = TimeDuration(0, 999'999'999u).checked_add(TimeDuration(0, 1));
        REQUIRE(carry.has_value());
        REQUIRE(carry.value() == TimeDuration(1, 0));
}

TEST_CASE("TimeDuration checked_sub reports underflow", "[timeCore]") {
        const auto normal = TimeDuration(0, 1).checked_sub(TimeDuration(0, 0));
        REQUIRE(normal.has_value());
        REQUIRE(normal.value() == TimeDuration(0, 1));

        REQUIRE(!TimeDuration(0, 0).checked_sub(TimeDuration(0, 1)).has_value());
        REQUIRE(TimeDuration(0, 0).saturating_sub(TimeDuration(0, 1)) == TimeDuration::ZERO);
        REQUIRE(TimeDuration(0, 1).saturating_sub(TimeDuration(0, 0)) == TimeDuration(0, 1));
        REQUIRE(!TimeDuration(0, 0).checked_sub(TimeDuration(1, 0)).has_value());

        // Borrow from the seconds component.
        const auto borrow = TimeDuration(1, 0).checked_sub(TimeDuration(0, 500'000'000u));
        REQUIRE(borrow.has_value());
        REQUIRE(borrow.value() == TimeDuration(0, 500'000'000u));
}

TEST_CASE("TimeDuration checked_mul reports overflow", "[timeCore]") {
        const auto normal = TimeDuration(0, 500'000'001u).checked_mul(2);
        REQUIRE(normal.has_value());
        REQUIRE(normal.value() == TimeDuration(1, 2));

        const TimeDuration big{~std::uint64_t{0} - 1, 0};
        REQUIRE(!big.checked_mul(2).has_value());
        REQUIRE(big.saturating_mul(2) == TimeDuration::MAX);
        REQUIRE(TimeDuration(0, 500'000'001u).saturating_mul(2) == TimeDuration(1, 2));
        REQUIRE(TimeDuration(1, 0).checked_mul(0).value_or(TimeDuration::ZERO).is_zero());
}

TEST_CASE("TimeDuration checked_div rejects zero divisor", "[timeCore]") {
        const auto halves = TimeDuration(2, 0).checked_div(2);
        REQUIRE(halves.has_value());
        REQUIRE(halves.value() == TimeDuration(1, 0));

        const auto subsec = TimeDuration(1, 0).checked_div(2);
        REQUIRE(subsec.has_value());
        REQUIRE(subsec.value() == TimeDuration(0, 500'000'000u));

        REQUIRE(!TimeDuration(2, 0).checked_div(0).has_value());
}

TEST_CASE("TimeDuration abs_diff", "[timeCore]") {
        REQUIRE(TimeDuration(100, 0).abs_diff(TimeDuration(80, 0)) == TimeDuration(20, 0));
        REQUIRE(TimeDuration(100, 400'000'000u).abs_diff(TimeDuration(110, 0)) == TimeDuration(9, 600'000'000u));
        REQUIRE(TimeDuration(80, 0).abs_diff(TimeDuration(100, 0)) == TimeDuration(20, 0));
        REQUIRE(TimeDuration::ZERO.abs_diff(TimeDuration::ZERO).is_zero());
}

TEST_CASE("TimeDuration arithmetic operators", "[timeCore]") {
        REQUIRE(TimeDuration(0, 500'000'001u) * 2u == TimeDuration(1, 2));
        REQUIRE(2u * TimeDuration(0, 500'000'001u) == TimeDuration(1, 2));
        REQUIRE(TimeDuration(1, 0) + TimeDuration(0, 500'000'000u) == TimeDuration(1, 500'000'000u));
        REQUIRE(TimeDuration(1, 500'000'000u) - TimeDuration(0, 500'000'000u) == TimeDuration(1, 0));
        REQUIRE(TimeDuration(2, 0) / 2u == TimeDuration(1, 0));

        // Rust's operator impls saturate to ZERO on overflow/underflow.
        REQUIRE(TimeDuration(1, 0) + TimeDuration(~std::uint64_t{0}, 0) == TimeDuration::ZERO);
        REQUIRE(TimeDuration(0, 0) - TimeDuration(0, 1) == TimeDuration::ZERO);
        REQUIRE(TimeDuration(2, 0) / 0u == TimeDuration::ZERO);

        TimeDuration acc(0, 0);
        acc += TimeDuration(0, 1);
        REQUIRE(acc == TimeDuration(0, 1));
        acc -= TimeDuration(0, 1);
        REQUIRE(acc.is_zero());
        acc += TimeDuration(0, 500'000'001u);
        acc *= 2u;
        REQUIRE(acc == TimeDuration(1, 2));
        acc /= 2u;
        REQUIRE(acc == TimeDuration(0, 500'000'001u));
}

TEST_CASE("TimeDuration ordering and constants", "[timeCore]") {
        REQUIRE(TimeDuration::ZERO == TimeDuration(0, 0));
        REQUIRE(TimeDuration::SECOND == TimeDuration(1, 0));
        REQUIRE(TimeDuration::MILLISECOND == TimeDuration(0, 1'000'000u));
        REQUIRE(TimeDuration::MICROSECOND == TimeDuration(0, 1'000u));
        REQUIRE(TimeDuration::NANOSECOND == TimeDuration(0, 1));
        REQUIRE(TimeDuration::MAX == TimeDuration(~std::uint64_t{0}, 999'999'999u));

        REQUIRE(TimeDuration::ZERO < TimeDuration::NANOSECOND);
        REQUIRE(TimeDuration::NANOSECOND < TimeDuration::MICROSECOND);
        REQUIRE(TimeDuration::MICROSECOND < TimeDuration::MILLISECOND);
        REQUIRE(TimeDuration::MILLISECOND < TimeDuration::SECOND);
        REQUIRE(TimeDuration::SECOND != TimeDuration::MILLISECOND);
}

TEST_CASE("TimeDuration floating point conversions", "[timeCore]") {
        const TimeDuration dur{2, 700'000'000u};
        REQUIRE(dur.as_secs_f64() == 2.7);
        REQUIRE(dur.as_secs_f32() == 2.7f);

        const TimeDuration millis{2, 345'678'000u};
        REQUIRE(millis.as_millis_f64() == 2'345.678);
        REQUIRE(millis.as_millis_f32() == 2'345.678f);

        const TimeDuration other{5, 400'000'000u};
        REQUIRE(dur.div_duration_f64(other) == 0.5);
        REQUIRE(dur.div_duration_f32(other) == 0.5f);
}

TEST_CASE("TimeDuration sum", "[timeCore]") {
        const TimeDuration a{1, 500'000'000u};
        const TimeDuration b{2, 700'000'000u};
        const TimeDuration c{3, 900'000'000u};
        const auto total = TimeDuration::sum({a, b, c});
        REQUIRE(total == TimeDuration(8, 100'000'000u));

        const TimeDuration parts[] = {a, b};
        const auto ranged = TimeDuration::sum(parts, parts + 2);
        REQUIRE(ranged == TimeDuration(4, 200'000'000u));

        const auto nothing = TimeDuration::sum(parts, parts);
        REQUIRE(nothing.is_zero());
}

TEST_CASE("TimeDuration constexpr evaluation", "[timeCore]") {
        static_assert(TimeDuration::from_secs(5).as_secs() == 5);
        static_assert(TimeDuration::from_millis(2'569).subsec_nanos() == 569'000'000u);
        static_assert(TimeDuration::from_nanos(1'000'000'123).as_secs() == 1);
        static_assert(TimeDuration::SECOND.as_secs() == 1);
        static_assert(TimeDuration::ZERO.is_zero());
        static_assert(TimeDuration(1, 0).checked_add(TimeDuration(0, 1)).value_or(TimeDuration::ZERO) ==
                      TimeDuration(1, 1));
        static_assert(TimeDuration::MILLISECOND.subsec_nanos() == 1'000'000u);
        REQUIRE(true);
}
