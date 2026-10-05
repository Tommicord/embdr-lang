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

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

import embdr.cxxstd.timeUtil;

using namespace embdr::cxxstd;

namespace {
        struct FormatCase {
                std::int64_t seconds;
                std::uint32_t nanoseconds;
                std::string_view expected;
        };

        std::string_view format_iso8601(const Timestamp ts, const std::span<std::uint8_t> buf) noexcept {
                const auto result = format_utc_iso8601(ts, buf);
                if (!result.has_value())
                        return {};
                return std::string_view(reinterpret_cast<const char*>(buf.data()), result.value());
        }

        void check_format(const FormatCase& test_case) noexcept {
                std::array<std::uint8_t, 64> buf{};
                const std::string_view text =
                    format_iso8601(Timestamp::from_unix_seconds(test_case.seconds, test_case.nanoseconds), buf);
                REQUIRE(text == test_case.expected);
        }

        void check_utc(const std::int64_t seconds, const std::int32_t year, const Month month, const std::uint8_t day,
                       const std::uint8_t hour, const std::uint8_t minute, const std::uint8_t second) noexcept {
                const auto utc = UtcDateTime::from_unix_seconds(seconds);
                REQUIRE(utc.has_value());
                REQUIRE(utc.value().unix_timestamp() == seconds);
                const auto date = utc.value().to_calendar_date();
                REQUIRE(date.year() == year);
                REQUIRE(date.month() == month);
                REQUIRE(date.day() == day);
                const auto hms = utc.value().time().as_hms_nano();
                REQUIRE(hms.hour() == hour);
                REQUIRE(hms.minute() == minute);
                REQUIRE(hms.second() == second);
                REQUIRE(hms.nanosecond() == 0);
        }
} // namespace

TEST_CASE("utc_now returns a current timestamp in the representable range", "[timeUtil]") {
        const Timestamp now = utc_now();
        REQUIRE(now.seconds() >= 1'577'836'800); // 2020-01-01T00:00:00Z
        REQUIRE(now.seconds() <= Timestamp::MAX.seconds());
        REQUIRE(now.nanoseconds() < 1'000'000'000);
}

TEST_CASE("utc_datetime_now returns the current UTC date-time", "[timeUtil]") {
        const auto now = utc_datetime_now();
        REQUIRE(now.has_value());
        const auto date = now.value().to_calendar_date();
        REQUIRE(date.year() >= 2020);
        REQUIRE(date.year() <= 9999);
        REQUIRE(static_cast<std::uint8_t>(date.month()) >= 1);
        REQUIRE(static_cast<std::uint8_t>(date.month()) <= 12);
        REQUIRE(date.day() >= 1);
        REQUIRE(date.day() <= 31);
        const auto hms = now.value().time().as_hms_nano();
        REQUIRE(hms.hour() < 24);
        REQUIRE(hms.minute() < 60);
        REQUIRE(hms.second() < 60);
        REQUIRE(hms.nanosecond() < 1'000'000'000);
}

TEST_CASE("utc_now and utc_datetime_now describe the same moment", "[timeUtil]") {
        const Timestamp ts = utc_now();
        const auto dt = utc_datetime_now();
        REQUIRE(dt.has_value());
        const std::int64_t delta = dt.value().unix_timestamp() - ts.seconds();
        REQUIRE(delta >= -5);
        REQUIRE(delta <= 5);
}

TEST_CASE("format_utc_iso8601 formats representative instants", "[timeUtil]") {
        check_format({0, 0, "1970-01-01T00:00:00Z"});
        check_format({1, 0, "1970-01-01T00:00:01Z"});
        check_format({-1, 0, "1969-12-31T23:59:59Z"});
        check_format({86'399, 0, "1970-01-01T23:59:59Z"});
        check_format({86'400, 0, "1970-01-02T00:00:00Z"});
        check_format({981'173'106, 0, "2001-02-03T04:05:06Z"});
        check_format({951'782'400, 0, "2000-02-29T00:00:00Z"});
        check_format({1'709'164'800, 0, "2024-02-29T00:00:00Z"});
        check_format({13'574'563'200, 0, "2400-02-29T00:00:00Z"});
        check_format({-2'203'977'600, 0, "1900-02-28T00:00:00Z"});
        check_format({-2'203'891'200, 0, "1900-03-01T00:00:00Z"});
        check_format({4'107'456'000, 0, "2100-02-28T00:00:00Z"});
        check_format({4'107'542'400, 0, "2100-03-01T00:00:00Z"});
        check_format({1'577'836'800, 0, "2020-01-01T00:00:00Z"});
        check_format({1'718'454'896, 0, "2024-06-15T12:34:56Z"});
        check_format({1'735'689'599, 0, "2024-12-31T23:59:59Z"});
        check_format({-61'995'110'400, 0, "0005-06-15T00:00:00Z"});
        check_format({-30'594'326'400, 0, "1000-07-04T00:00:00Z"});
        check_format({-62'167'219'200, 0, "0000-01-01T00:00:00Z"});
        check_format({-62'198'755'200, 0, "-001-01-01T00:00:00Z"});
        check_format({-93'678'336'000, 0, "-999-06-15T00:00:00Z"});
        check_format({-77'914'224'000, 0, "-500-12-31T00:00:00Z"});
        check_format({253'370'764'800, 0, "9999-01-01T00:00:00Z"});
        check_format({253'402'300'799, 0, "9999-12-31T23:59:59Z"});
        // The Rust `write_i32_fixed` helper reserves one of the four year characters for the sign,
        // so year -9999 renders as "-999".
        check_format({-377'705'116'800, 0, "-999-01-01T00:00:00Z"});
        check_format({-377'705'030'400, 0, "-999-01-02T00:00:00Z"});
}

TEST_CASE("format_utc_iso8601 emits fractional seconds only for non-zero nanoseconds", "[timeUtil]") {
        check_format({1'718'454'896, 1, "2024-06-15T12:34:56.000000001Z"});
        check_format({1'718'454'896, 100'000'000, "2024-06-15T12:34:56.100000000Z"});
        check_format({1'718'454'896, 456'000'000, "2024-06-15T12:34:56.456000000Z"});
        check_format({1'718'454'896, 999'999'999, "2024-06-15T12:34:56.999999999Z"});
        check_format({253'402'300'799, 999'999'999, "9999-12-31T23:59:59.999999999Z"});
        // A subsecond of exactly zero omits the '.' block entirely.
        check_format({253'402'300'799, 0, "9999-12-31T23:59:59Z"});
}

TEST_CASE("format_utc_iso8601 requires at least 32 buffer bytes", "[timeUtil]") {
        std::array<std::uint8_t, 64> buf{};
        for (std::size_t size = 0; size < 32; ++size) {
                const auto result = format_utc_iso8601(Timestamp::UNIX_EPOCH, std::span(buf.data(), size));
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.has_error());
                REQUIRE(result.error() == FormatError::bufferTooSmall);
        }
        for (std::size_t size = 32; size <= 40; ++size) {
                const auto result = format_utc_iso8601(Timestamp::UNIX_EPOCH, std::span(buf.data(), size));
                REQUIRE(result.has_value());
                REQUIRE(result.value() == 20);
        }
}

TEST_CASE("format_utc_iso8601 rejects invalid timestamps", "[timeUtil]") {
        std::array<std::uint8_t, 64> buf{};
        {
                const auto result = format_utc_iso8601(Timestamp::from_unix_seconds(Timestamp::MIN.seconds() - 1), buf);
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.has_error());
                REQUIRE(result.error() == FormatError::invalidTimestamp);
        }
        {
                const auto result = format_utc_iso8601(Timestamp::from_unix_seconds(Timestamp::MAX.seconds() + 1), buf);
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.has_error());
                REQUIRE(result.error() == FormatError::invalidTimestamp);
        }
        {
                const auto result = format_utc_iso8601(Timestamp::from_unix_seconds(0, 1'000'000'000), buf);
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.has_error());
                REQUIRE(result.error() == FormatError::invalidTimestamp);
        }
        {
                const auto result = format_utc_iso8601(Timestamp::from_unix_seconds(0, 2'000'000'000), buf);
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.has_error());
                REQUIRE(result.error() == FormatError::invalidTimestamp);
        }
}

TEST_CASE("the current timestamp formats as a valid ISO 8601 instant", "[timeUtil]") {
        std::array<std::uint8_t, 64> buf{};
        const auto result = format_utc_iso8601(utc_now(), buf);
        REQUIRE(result.has_value());
        const std::string_view text(reinterpret_cast<const char*>(buf.data()), result.value());
        REQUIRE(text.size() >= 20);
        REQUIRE(text.size() <= 30);
        REQUIRE(text.back() == 'Z');
        REQUIRE(text.find('T') == 10);
}

TEST_CASE("Timestamp and UtcDateTime constants match the representable range", "[timeUtil]") {
        REQUIRE(Timestamp::UNIX_EPOCH.seconds() == 0);
        REQUIRE(Timestamp::UNIX_EPOCH.nanoseconds() == 0);
        REQUIRE(Timestamp::MIN.seconds() == -377'705'116'800);
        REQUIRE(Timestamp::MIN.nanoseconds() == 0);
        REQUIRE(Timestamp::MAX.seconds() == 253'402'300'799);
        REQUIRE(Timestamp::MAX.nanoseconds() == 999'999'999);
        REQUIRE(Timestamp::MIN.seconds() == UtcDateTime::MIN_UNIX_SECONDS);
        REQUIRE(Timestamp::MAX.seconds() == UtcDateTime::MAX_UNIX_SECONDS);
        REQUIRE(Timestamp::UNIX_EPOCH == Timestamp::from_unix_seconds(0));
        REQUIRE(Timestamp::MIN == Timestamp::from_unix_seconds(-377'705'116'800, 0));
        REQUIRE(Timestamp::MAX == Timestamp::from_unix_seconds(253'402'300'799, 999'999'999));
        REQUIRE(Timestamp::MIN < Timestamp::UNIX_EPOCH);
        REQUIRE(Timestamp::UNIX_EPOCH < Timestamp::MAX);
}

TEST_CASE("UtcDateTime boundary values mirror the Rust assertions", "[timeUtil]") {
        REQUIRE((UtcDateTime::UNIX_EPOCH.to_calendar_date() == CalendarDate{1970, Month::january, 1}));
        REQUIRE((UtcDateTime::MIN.to_calendar_date() == CalendarDate{-9999, Month::january, 1}));
        REQUIRE((UtcDateTime::MAX.to_calendar_date() == CalendarDate{9999, Month::december, 31}));
        // The Rust crate asserts `Timestamp::MIN.time() == MIDNIGHT` and `Timestamp::MAX.time() == Time::MAX`.
        REQUIRE(UtcDateTime::MIN.time() == TimeOfDay::MIDNIGHT);
        REQUIRE(UtcDateTime::MAX.time() == TimeOfDay::MAX);
        const auto epoch = Timestamp::UNIX_EPOCH.to_utc();
        REQUIRE(epoch.has_value());
        REQUIRE(epoch.value() == UtcDateTime::UNIX_EPOCH);
        const auto min = Timestamp::MIN.to_utc();
        REQUIRE(min.has_value());
        REQUIRE(min.value() == UtcDateTime::MIN);
        const auto max = Timestamp::MAX.to_utc();
        REQUIRE(max.has_value());
        REQUIRE(max.value() == UtcDateTime::MAX);
        REQUIRE(UtcDateTime::MIN < UtcDateTime::UNIX_EPOCH);
        REQUIRE(UtcDateTime::UNIX_EPOCH < UtcDateTime::MAX);
        REQUIRE(TimeOfDay::MIDNIGHT < TimeOfDay::MAX);
}

TEST_CASE("from_unix_seconds converts known instants", "[timeUtil]") {
        check_utc(0, 1970, Month::january, 1, 0, 0, 0);
        check_utc(-1, 1969, Month::december, 31, 23, 59, 59);
        check_utc(86'399, 1970, Month::january, 1, 23, 59, 59);
        check_utc(86'400, 1970, Month::january, 2, 0, 0, 0);
        check_utc(981'173'106, 2001, Month::february, 3, 4, 5, 6);
        check_utc(951'782'400, 2000, Month::february, 29, 0, 0, 0);
        check_utc(-2'203'977'600, 1900, Month::february, 28, 0, 0, 0);
        check_utc(4'107'456'000, 2100, Month::february, 28, 0, 0, 0);
        check_utc(-61'995'110'400, 5, Month::june, 15, 0, 0, 0);
        check_utc(-62'198'755'200, -1, Month::january, 1, 0, 0, 0);
        check_utc(-77'914'224'000, -500, Month::december, 31, 0, 0, 0);
        check_utc(-377'705'116'800, -9999, Month::january, 1, 0, 0, 0);
        check_utc(253'402'300'799, 9999, Month::december, 31, 23, 59, 59);
        check_utc(1'718'454'896, 2024, Month::june, 15, 12, 34, 56);
}

TEST_CASE("from_unix_seconds rejects out-of-range timestamps", "[timeUtil]") {
        const auto below = UtcDateTime::from_unix_seconds(UtcDateTime::MIN_UNIX_SECONDS - 1);
        REQUIRE_FALSE(below.has_value());
        REQUIRE(below.has_error());
        REQUIRE(below.error() == FormatError::invalidTimestamp);
        const auto above = UtcDateTime::from_unix_seconds(UtcDateTime::MAX_UNIX_SECONDS + 1);
        REQUIRE_FALSE(above.has_value());
        REQUIRE(above.has_error());
        REQUIRE(above.error() == FormatError::invalidTimestamp);
        REQUIRE(UtcDateTime::from_unix_seconds(UtcDateTime::MIN_UNIX_SECONDS).has_value());
        REQUIRE(UtcDateTime::from_unix_seconds(UtcDateTime::MAX_UNIX_SECONDS).has_value());
}

TEST_CASE("to_utc validates seconds and nanoseconds", "[timeUtil]") {
        const auto epoch = Timestamp::UNIX_EPOCH.to_utc();
        REQUIRE(epoch.has_value());
        REQUIRE(epoch.value() == UtcDateTime::UNIX_EPOCH);

        const auto bad_nanos = Timestamp::from_unix_seconds(0, 1'000'000'000).to_utc();
        REQUIRE_FALSE(bad_nanos.has_value());
        REQUIRE(bad_nanos.has_error());
        REQUIRE(bad_nanos.error() == FormatError::invalidTimestamp);

        const auto below = Timestamp::from_unix_seconds(Timestamp::MIN.seconds() - 1).to_utc();
        REQUIRE_FALSE(below.has_value());
        REQUIRE(below.has_error());
        REQUIRE(below.error() == FormatError::invalidTimestamp);

        const auto above = Timestamp::from_unix_seconds(Timestamp::MAX.seconds() + 1).to_utc();
        REQUIRE_FALSE(above.has_value());
        REQUIRE(above.has_error());
        REQUIRE(above.error() == FormatError::invalidTimestamp);

        REQUIRE(Timestamp::MIN.to_utc().has_value());
        REQUIRE(Timestamp::MAX.to_utc().has_value());
}

TEST_CASE("date conversion round-trips across the whole representable range", "[timeUtil]") {
        constexpr std::int64_t day = 86'400;
        constexpr std::int64_t stride = 2921 * day; // ~8 years: sweeps the full ±9999 window
        for (std::int64_t seconds = Timestamp::MIN.seconds(); seconds <= Timestamp::MAX.seconds(); seconds += stride) {
                const auto utc = UtcDateTime::from_unix_seconds(seconds);
                REQUIRE(utc.has_value());
                REQUIRE(utc.value().unix_timestamp() == seconds);
        }
}

TEST_CASE("Month values are 1-based like the Rust Month enum", "[timeUtil]") {
        REQUIRE(static_cast<std::uint8_t>(Month::january) == 1);
        REQUIRE(static_cast<std::uint8_t>(Month::february) == 2);
        REQUIRE(static_cast<std::uint8_t>(Month::march) == 3);
        REQUIRE(static_cast<std::uint8_t>(Month::april) == 4);
        REQUIRE(static_cast<std::uint8_t>(Month::may) == 5);
        REQUIRE(static_cast<std::uint8_t>(Month::june) == 6);
        REQUIRE(static_cast<std::uint8_t>(Month::july) == 7);
        REQUIRE(static_cast<std::uint8_t>(Month::august) == 8);
        REQUIRE(static_cast<std::uint8_t>(Month::september) == 9);
        REQUIRE(static_cast<std::uint8_t>(Month::october) == 10);
        REQUIRE(static_cast<std::uint8_t>(Month::november) == 11);
        REQUIRE(static_cast<std::uint8_t>(Month::december) == 12);
}

TEST_CASE("value types expose their components", "[timeUtil]") {
        const TimeOfDay time{1, 2, 3, 456'000'000};
        REQUIRE(time.hour() == 1);
        REQUIRE(time.minute() == 2);
        REQUIRE(time.second() == 3);
        REQUIRE(time.nanosecond() == 456'000'000);
        REQUIRE((time.as_hms_nano() == HmsNano{1, 2, 3, 456'000'000}));
        REQUIRE((TimeOfDay{0, 0, 0, 0} == TimeOfDay::MIDNIGHT));

        const CalendarDate date{2026, Month::october, 5};
        REQUIRE(date.year() == 2026);
        REQUIRE(date.month() == Month::october);
        REQUIRE(date.day() == 5);
        REQUIRE((date == CalendarDate{2026, Month::october, 5}));

        const Timestamp ts = Timestamp::from_unix_seconds(42, 7);
        REQUIRE(ts.seconds() == 42);
        REQUIRE(ts.nanoseconds() == 7);
        REQUIRE(ts == Timestamp::from_unix_seconds(42, 7));
        REQUIRE(ts != Timestamp::from_unix_seconds(42, 8));
}
