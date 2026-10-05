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
#include <span>

export module embdr.cxxstd.timeUtil;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.timeCore;

namespace embdr::cxxstd {
        inline constexpr std::int64_t SECS_PER_MINUTE = 60;
        inline constexpr std::int64_t SECS_PER_HOUR = 3'600;
        inline constexpr std::int64_t SECS_PER_DAY = 86'400;
        inline constexpr std::uint32_t NANOS_PER_SEC = 1'000'000'000u;

        // Rust `div_floor!`: rounds the quotient towards negative infinity (C++ `/` truncates), so
        // timestamps before the epoch map onto the preceding calendar day.
        [[nodiscard]] constexpr std::int64_t div_floor(const std::int64_t numerator, const std::int64_t denominator) noexcept {
                const std::int64_t quotient = numerator / denominator;
                if (numerator % denominator != 0 && ((numerator < 0) != (denominator < 0)))
                        return quotient - 1;
                return quotient;
        }

        // Proleptic Gregorian date to days since the Unix epoch (Hinnant's `days_from_civil`;
        // the Rust crate performs the same conversion through Julian days).
        [[nodiscard]] constexpr std::int64_t days_from_civil(const std::int32_t year, const std::uint32_t month,
                                                             const std::uint32_t day) noexcept {
                std::int64_t adjusted_year = year;
                adjusted_year -= month <= 2 ? 1 : 0;
                const std::int64_t era = (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
                const std::int64_t year_of_era = adjusted_year - era * 400;
                const std::int64_t month_shift = month > 2 ? -3 : 9;
                const std::int64_t day_of_year =
                    (153 * (static_cast<std::int64_t>(month) + month_shift) + 2) / 5 + static_cast<std::int64_t>(day) - 1;
                const std::int64_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
                return era * 146'097 + day_of_era - 719'468;
        }

        // Days since the Unix epoch to a proleptic Gregorian date (Hinnant's `civil_from_days`).
        constexpr void civil_from_days(const std::int64_t days, std::int32_t& year, std::uint32_t& month,
                                       std::uint32_t& day) noexcept {
                const std::int64_t shifted = days + 719'468;
                const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146'096) / 146'097;
                const std::int64_t day_of_era = shifted - era * 146'097;
                const std::int64_t year_of_era =
                    (day_of_era - day_of_era / 1'460 + day_of_era / 36'524 - day_of_era / 146'096) / 365;
                const std::int64_t decoded_year = year_of_era + era * 400;
                const std::int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
                const std::int64_t month_index = (5 * day_of_year + 2) / 153;
                const std::int64_t decoded_month = month_index + (month_index < 10 ? 3 : -9);
                day = static_cast<std::uint32_t>(day_of_year - (153 * month_index + 2) / 5 + 1);
                month = static_cast<std::uint32_t>(decoded_month);
                year = static_cast<std::int32_t>(decoded_year + (decoded_month <= 2 ? 1 : 0));
        }

        // Fixed-width zero-padded writer for `u32` values (port of Rust `write_u32_fixed`); digits are
        // emitted least-significant first, so values wider than `width` keep only their trailing digits.
        [[nodiscard]] constexpr bool write_u32_fixed(const std::span<std::uint8_t> buf, std::uint32_t val,
                                                     const std::size_t width) noexcept {
                if (buf.size() < width)
                        return false;
                for (std::size_t i = width; i-- > 0;) {
                        buf[i] = static_cast<std::uint8_t>('0' + (val % 10));
                        val /= 10;
                }
                return true;
        }

        // Fixed-width zero-padded writer for `u8` values (port of Rust `write_u8_fixed`).
        [[nodiscard]] constexpr bool write_u8_fixed(const std::span<std::uint8_t> buf, std::uint8_t val,
                                                    const std::size_t width) noexcept {
                if (buf.size() < width)
                        return false;
                for (std::size_t i = width; i-- > 0;) {
                        buf[i] = static_cast<std::uint8_t>('0' + (val % 10));
                        val = static_cast<std::uint8_t>(val / 10);
                }
                return true;
        }

        // Fixed-width writer for `i32` values (port of Rust `write_i32_fixed`); a negative value writes
        // `-` first, so the magnitude is padded into the remaining `width - 1` characters.
        [[nodiscard]] constexpr bool write_i32_fixed(const std::span<std::uint8_t> buf, const std::int32_t val,
                                                     const std::size_t width) noexcept {
                if (buf.size() < width)
                        return false;
                if (val < 0) {
                        buf[0] = static_cast<std::uint8_t>('-');
                        const auto magnitude = static_cast<std::uint32_t>(-static_cast<std::int64_t>(val));
                        return write_u32_fixed(buf.subspan(1), magnitude, width - 1);
                }
                return write_u32_fixed(buf, static_cast<std::uint32_t>(val), width);
        }

        // The error type returned by `format_utc_iso8601` (port of the Rust `FormatError`).
        export enum class FormatError {
                // The output buffer is too small to hold the formatted timestamp.
                bufferTooSmall,
                // The timestamp could not be converted to a UTC date-time.
                invalidTimestamp,
        };

        // Display text for `FormatError` (port of the Rust `impl core::fmt::Display`).
        export constexpr SimpleStringView as_str(const FormatError error) noexcept {
                switch (error) {
                        case FormatError::bufferTooSmall:
                                return "the output buffer is too small";
                        case FormatError::invalidTimestamp:
                                return "the timestamp is not a valid UTC date-time";
                }
                return "the timestamp is not a valid UTC date-time";
        }

        // Debug text for `FormatError` (port of the Rust `#[derive(Debug)]`).
        export constexpr SimpleStringView debug_str(const FormatError error) noexcept {
                switch (error) {
                        case FormatError::bufferTooSmall:
                                return "BufferTooSmall";
                        case FormatError::invalidTimestamp:
                                return "InvalidTimestamp";
                }
                return "InvalidTimestamp";
        }

        // Calendar month (port of the Rust `Month`; numeric values are 1-based, matching `month as u8`).
        export enum class Month : std::uint8_t {
                january = 1,
                february,
                march,
                april,
                may,
                june,
                july,
                august,
                september,
                october,
                november,
                december,
        };

        // A year-month-day triple (the Rust `(i32, Month, u8)` tuple returned by `to_calendar_date`).
        export struct CalendarDate {
                // Year in the proleptic Gregorian calendar.
                std::int32_t _M_year{};
                // Month of the year.
                Month _M_month{Month::january};
                // Day of the month.
                std::uint8_t _M_day{};

                [[nodiscard]] constexpr std::int32_t year() const noexcept { return this->_M_year; }
                [[nodiscard]] constexpr Month month() const noexcept { return this->_M_month; }
                [[nodiscard]] constexpr std::uint8_t day() const noexcept { return this->_M_day; }

                [[nodiscard]] constexpr bool operator==(const CalendarDate& other) const noexcept {
                        return this->_M_year == other._M_year && this->_M_month == other._M_month &&
                               this->_M_day == other._M_day;
                }
        };

        // An hour/minute/second/nanosecond breakdown (the Rust `(u8, u8, u8, u32)` tuple returned by
        // `Time::as_hms_nano`).
        export struct HmsNano {
                // Hour of the day, 0..=23.
                std::uint8_t _M_hour{};
                // Minute of the hour, 0..=59.
                std::uint8_t _M_minute{};
                // Second of the minute, 0..=59.
                std::uint8_t _M_second{};
                // Nanosecond of the second, 0..=999'999'999.
                std::uint32_t _M_nanosecond{};

                [[nodiscard]] constexpr std::uint8_t hour() const noexcept { return this->_M_hour; }
                [[nodiscard]] constexpr std::uint8_t minute() const noexcept { return this->_M_minute; }
                [[nodiscard]] constexpr std::uint8_t second() const noexcept { return this->_M_second; }
                [[nodiscard]] constexpr std::uint32_t nanosecond() const noexcept { return this->_M_nanosecond; }

                [[nodiscard]] constexpr bool operator==(const HmsNano& other) const noexcept {
                        return this->_M_hour == other._M_hour && this->_M_minute == other._M_minute &&
                               this->_M_second == other._M_second && this->_M_nanosecond == other._M_nanosecond;
                }
        };

        // A wall-clock time of day with nanosecond precision (port of the Rust `Time` type).
        // Components are stored unchecked; they are produced in range by `UtcDateTime`.
        export struct TimeOfDay {
                // Midnight (00:00:00.0), the Rust `Time::MIDNIGHT`.
                static const TimeOfDay MIDNIGHT;
                // The latest time of day (23:59:59.999999999), the Rust `Time::MAX`.
                static const TimeOfDay MAX;

                // Hour of the day, 0..=23.
                std::uint8_t _M_hour{};
                // Minute of the hour, 0..=59.
                std::uint8_t _M_minute{};
                // Second of the minute, 0..=59.
                std::uint8_t _M_second{};
                // Nanosecond of the second, 0..=999'999'999.
                std::uint32_t _M_nanosecond{};

                [[nodiscard]] constexpr std::uint8_t hour() const noexcept { return this->_M_hour; }
                [[nodiscard]] constexpr std::uint8_t minute() const noexcept { return this->_M_minute; }
                [[nodiscard]] constexpr std::uint8_t second() const noexcept { return this->_M_second; }
                [[nodiscard]] constexpr std::uint32_t nanosecond() const noexcept { return this->_M_nanosecond; }

                // The hour, minute, second, and nanosecond as a single value (Rust `as_hms_nano`).
                [[nodiscard]] constexpr HmsNano as_hms_nano() const noexcept {
                        return HmsNano{this->_M_hour, this->_M_minute, this->_M_second, this->_M_nanosecond};
                }

                [[nodiscard]] constexpr bool operator==(const TimeOfDay& other) const noexcept {
                        return this->_M_hour == other._M_hour && this->_M_minute == other._M_minute &&
                               this->_M_second == other._M_second && this->_M_nanosecond == other._M_nanosecond;
                }

                // Earliest time of day first (Rust `Ord` derive on `Time`).
                [[nodiscard]] constexpr bool operator<(const TimeOfDay& other) const noexcept {
                        if (this->_M_hour != other._M_hour)
                                return this->_M_hour < other._M_hour;
                        if (this->_M_minute != other._M_minute)
                                return this->_M_minute < other._M_minute;
                        if (this->_M_second != other._M_second)
                                return this->_M_second < other._M_second;
                        return this->_M_nanosecond < other._M_nanosecond;
                }
        };
        constexpr TimeOfDay TimeOfDay::MIDNIGHT{0, 0, 0, 0};
        constexpr TimeOfDay TimeOfDay::MAX{23, 59, 59, 999'999'999};

        // A date-time in UTC with nanosecond precision (port of the Rust `UtcDateTime`, `large-dates`
        // disabled: years -9999..=9999 are representable).
        export struct UtcDateTime {
                // Midnight, 1 January 1970 (the Rust `UtcDateTime::UNIX_EPOCH`).
                static const UtcDateTime UNIX_EPOCH;
                // -9999-01-01 00:00:00.0 (the Rust `UtcDateTime::MIN`).
                static const UtcDateTime MIN;
                // 9999-12-31 23:59:59.999999999 (the Rust `UtcDateTime::MAX`).
                static const UtcDateTime MAX;

                // The smallest Unix timestamp accepted by `from_unix_seconds` (-9999-01-01 00:00:00 UTC).
                static constexpr std::int64_t MIN_UNIX_SECONDS = -377'705'116'800;
                // The largest Unix timestamp accepted by `from_unix_seconds` (9999-12-31 23:59:59 UTC).
                static constexpr std::int64_t MAX_UNIX_SECONDS = 253'402'300'799;

                // Year in the proleptic Gregorian calendar, -9999..=9999.
                std::int32_t _M_year{1970};
                // Month of the year, 1..=12.
                std::uint8_t _M_month{1};
                // Day of the month, 1..=31.
                std::uint8_t _M_day{1};
                // Time of day.
                TimeOfDay _M_time{};

                // Creates a `UtcDateTime` from whole seconds since the Unix epoch (Rust
                // `from_unix_timestamp`); empty with `FormatError::invalidTimestamp` when `timestamp`
                // falls outside the representable ±9999 range.
                [[nodiscard]] static constexpr Maybe<UtcDateTime, FormatError>
                from_unix_seconds(std::int64_t timestamp) noexcept;

                [[nodiscard]] constexpr std::int32_t year() const noexcept { return this->_M_year; }
                [[nodiscard]] constexpr Month month() const noexcept { return static_cast<Month>(this->_M_month); }
                [[nodiscard]] constexpr std::uint8_t day() const noexcept { return this->_M_day; }
                [[nodiscard]] constexpr std::uint8_t hour() const noexcept { return this->_M_time.hour(); }
                [[nodiscard]] constexpr std::uint8_t minute() const noexcept { return this->_M_time.minute(); }
                [[nodiscard]] constexpr std::uint8_t second() const noexcept { return this->_M_time.second(); }
                [[nodiscard]] constexpr std::uint32_t nanosecond() const noexcept {
                        return this->_M_time.nanosecond();
                }

                // The year, month, and day (Rust `to_calendar_date`).
                [[nodiscard]] constexpr CalendarDate to_calendar_date() const noexcept {
                        return CalendarDate{this->_M_year, this->month(), this->_M_day};
                }

                // The time of day (Rust `time`).
                [[nodiscard]] constexpr TimeOfDay time() const noexcept { return this->_M_time; }

                // Whole seconds since the Unix epoch, flooring the subsecond part (Rust `unix_timestamp`).
                [[nodiscard]] constexpr std::int64_t unix_timestamp() const noexcept {
                        return days_from_civil(this->_M_year, this->_M_month, this->_M_day) * SECS_PER_DAY +
                               static_cast<std::int64_t>(this->_M_time.hour()) * SECS_PER_HOUR +
                               static_cast<std::int64_t>(this->_M_time.minute()) * SECS_PER_MINUTE +
                               static_cast<std::int64_t>(this->_M_time.second());
                }

                [[nodiscard]] constexpr bool operator==(const UtcDateTime& other) const noexcept {
                        return this->_M_year == other._M_year && this->_M_month == other._M_month &&
                               this->_M_day == other._M_day && this->_M_time == other._M_time;
                }

                // Earliest moment first (Rust `Ord` derive on `UtcDateTime`).
                [[nodiscard]] constexpr bool operator<(const UtcDateTime& other) const noexcept {
                        if (this->_M_year != other._M_year)
                                return this->_M_year < other._M_year;
                        if (this->_M_month != other._M_month)
                                return this->_M_month < other._M_month;
                        if (this->_M_day != other._M_day)
                                return this->_M_day < other._M_day;
                        return this->_M_time < other._M_time;
                }
        };
        constexpr UtcDateTime UtcDateTime::UNIX_EPOCH{1970, 1, 1, TimeOfDay::MIDNIGHT};
        constexpr UtcDateTime UtcDateTime::MIN{-9999, 1, 1, TimeOfDay::MIDNIGHT};
        constexpr UtcDateTime UtcDateTime::MAX{9999, 12, 31, TimeOfDay::MAX};

        constexpr Maybe<UtcDateTime, FormatError> UtcDateTime::from_unix_seconds(const std::int64_t timestamp) noexcept {
                if (timestamp < MIN_UNIX_SECONDS || timestamp > MAX_UNIX_SECONDS)
                        return Maybe<UtcDateTime, FormatError>(FormatError::invalidTimestamp);
                const std::int64_t days = div_floor(timestamp, SECS_PER_DAY);
                const std::int64_t seconds_within_day = timestamp - days * SECS_PER_DAY;
                std::int32_t year = 0;
                std::uint32_t month = 0;
                std::uint32_t day = 0;
                civil_from_days(days, year, month, day);
                UtcDateTime result{};
                result._M_year = year;
                result._M_month = static_cast<std::uint8_t>(month);
                result._M_day = static_cast<std::uint8_t>(day);
                result._M_time = TimeOfDay{static_cast<std::uint8_t>(seconds_within_day / SECS_PER_HOUR),
                                           static_cast<std::uint8_t>((seconds_within_day % SECS_PER_HOUR) / SECS_PER_MINUTE),
                                           static_cast<std::uint8_t>(seconds_within_day % SECS_PER_MINUTE), 0u};
                return Maybe<UtcDateTime, FormatError>(result);
        }

        // A Unix timestamp with nanosecond precision (port of the Rust `Timestamp`).
        //
        // The Rust type enforces `seconds ∈ [MIN, MAX]` and `nanoseconds ∈ [0, 1e9)` through ranged
        // integers; this port stores both unchecked (like Rust `Timestamp::new_unchecked`) and
        // validates them in `to_utc()` and `format_utc_iso8601`.
        export struct Timestamp {
                // A `Timestamp` representing the Unix epoch (1970-01-01 00:00:00 UTC).
                static const Timestamp UNIX_EPOCH;
                // The minimum valid `Timestamp` (-9999-01-01 00:00:00 UTC).
                static const Timestamp MIN;
                // The maximum valid `Timestamp` (9999-12-31 23:59:59.999999999 UTC).
                static const Timestamp MAX;

                // Seconds since (or, when negative, before) the Unix epoch.
                std::int64_t _M_seconds{};
                // Subsecond part, in `[0, 1'000'000'000)`.
                std::uint32_t _M_nanoseconds{};

                // Creates a `Timestamp` for the current moment (Rust `Timestamp::now`): reads the
                // realtime clock and clamps to `MIN`/`MAX`, falling back to `UNIX_EPOCH` when the
                // clock cannot be read (the Rust `.unwrap_or(Self::UNIX_EPOCH)`).
                [[nodiscard]] static Timestamp now() noexcept;

                // Creates a `Timestamp` from components without validating their ranges (the Rust
                // `new_unchecked`); `to_utc()` reports invalid components.
                [[nodiscard]] static constexpr Timestamp
                from_unix_seconds(const std::int64_t seconds, const std::uint32_t nanoseconds = 0) noexcept {
                        return Timestamp{seconds, nanoseconds};
                }

                // Seconds since the Unix epoch; negative before the epoch (Rust `as_seconds`).
                [[nodiscard]] constexpr std::int64_t seconds() const noexcept { return this->_M_seconds; }

                // The subsecond part in nanoseconds (Rust `Timestamp` `nanoseconds` field).
                [[nodiscard]] constexpr std::uint32_t nanoseconds() const noexcept { return this->_M_nanoseconds; }

                // Converts the `Timestamp` to a `UtcDateTime` (Rust `to_utc`); empty with
                // `FormatError::invalidTimestamp` when the seconds or nanoseconds are out of range.
                [[nodiscard]] constexpr Maybe<UtcDateTime, FormatError> to_utc() const noexcept;

                [[nodiscard]] constexpr bool operator==(const Timestamp& other) const noexcept {
                        return this->_M_seconds == other._M_seconds && this->_M_nanoseconds == other._M_nanoseconds;
                }

                // Earliest moment first (Rust `Ord` derive on `Timestamp`).
                [[nodiscard]] constexpr bool operator<(const Timestamp& other) const noexcept {
                        if (this->_M_seconds != other._M_seconds)
                                return this->_M_seconds < other._M_seconds;
                        return this->_M_nanoseconds < other._M_nanoseconds;
                }
        };
        constexpr Timestamp Timestamp::UNIX_EPOCH{0, 0};
        constexpr Timestamp Timestamp::MIN{UtcDateTime::MIN_UNIX_SECONDS, 0};
        constexpr Timestamp Timestamp::MAX{UtcDateTime::MAX_UNIX_SECONDS, 999'999'999};

        Timestamp Timestamp::now() noexcept {
                const auto realtime = SystemTime::realtime();
                if (!realtime.has_value())
                        return UNIX_EPOCH;
                const TimeVal& tv = realtime.value();
                if (tv.secs() > static_cast<std::uint64_t>(MAX._M_seconds))
                        return MAX;
                const auto seconds = static_cast<std::int64_t>(tv.secs());
                if (seconds < MIN._M_seconds)
                        return MIN;
                const std::uint64_t nanos = tv.nsecs() < NANOS_PER_SEC ? tv.nsecs() : NANOS_PER_SEC - 1;
                return Timestamp{seconds, static_cast<std::uint32_t>(nanos)};
        }

        constexpr Maybe<UtcDateTime, FormatError> Timestamp::to_utc() const noexcept {
                if (this->_M_nanoseconds >= NANOS_PER_SEC)
                        return Maybe<UtcDateTime, FormatError>(FormatError::invalidTimestamp);
                const auto utc = UtcDateTime::from_unix_seconds(this->_M_seconds);
                if (!utc.has_value())
                        return Maybe<UtcDateTime, FormatError>(FormatError::invalidTimestamp);
                UtcDateTime result = utc.value();
                result._M_time._M_nanosecond = this->_M_nanoseconds;
                return Maybe<UtcDateTime, FormatError>(result);
        }

        // Returns the current UTC timestamp using the system realtime clock (Rust `utc_now`).
        export [[nodiscard]] inline Timestamp utc_now() noexcept { return Timestamp::now(); }

        // Returns the current UTC date-time (Rust `utc_datetime_now`).
        export [[nodiscard]] inline Maybe<UtcDateTime, FormatError> utc_datetime_now() noexcept {
                return Timestamp::now().to_utc();
        }

        // Formats a UTC timestamp as an ISO 8601 string (`YYYY-MM-DDTHH:MM:SS.sssssssssZ`, with the
        // fractional seconds omitted when the nanosecond part is zero; Rust `format_utc_iso8601`).
        // The buffer must hold at least 32 bytes (the Rust requirement, even though at most 30 bytes
        // are ever written); returns the number of bytes written, or the failing `FormatError`.
        export [[nodiscard]] Maybe<std::size_t, FormatError>
        format_utc_iso8601(const Timestamp ts, std::span<std::uint8_t> buf) noexcept {
                if (buf.size() < 32)
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                const auto utc = ts.to_utc();
                if (!utc.has_value())
                        return Maybe<std::size_t, FormatError>(FormatError::invalidTimestamp);
                const CalendarDate date = utc.value().to_calendar_date();
                const HmsNano hms = utc.value().time().as_hms_nano();

                std::size_t idx = 0;
                if (!write_i32_fixed(buf.subspan(idx), date.year(), 4))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 4;
                buf[idx] = static_cast<std::uint8_t>('-');
                idx += 1;
                if (!write_u8_fixed(buf.subspan(idx), static_cast<std::uint8_t>(date.month()), 2))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 2;
                buf[idx] = static_cast<std::uint8_t>('-');
                idx += 1;
                if (!write_u8_fixed(buf.subspan(idx), date.day(), 2))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 2;
                buf[idx] = static_cast<std::uint8_t>('T');
                idx += 1;
                if (!write_u8_fixed(buf.subspan(idx), hms.hour(), 2))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 2;
                buf[idx] = static_cast<std::uint8_t>(':');
                idx += 1;
                if (!write_u8_fixed(buf.subspan(idx), hms.minute(), 2))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 2;
                buf[idx] = static_cast<std::uint8_t>(':');
                idx += 1;
                if (!write_u8_fixed(buf.subspan(idx), hms.second(), 2))
                        return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                idx += 2;
                if (hms.nanosecond() != 0) {
                        buf[idx] = static_cast<std::uint8_t>('.');
                        idx += 1;
                        if (!write_u32_fixed(buf.subspan(idx), hms.nanosecond(), 9))
                                return Maybe<std::size_t, FormatError>(FormatError::bufferTooSmall);
                        idx += 9;
                }
                buf[idx] = static_cast<std::uint8_t>('Z');
                idx += 1;
                return Maybe<std::size_t, FormatError>(idx);
        }
} // namespace embdr::cxxstd
