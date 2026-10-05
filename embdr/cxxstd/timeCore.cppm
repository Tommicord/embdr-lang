/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


module;
#include <cerrno>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <initializer_list>

// glibc defines these as macros; they collide with the SystemTime::CLOCK_* members below.
#undef CLOCK_MONOTONIC
#undef CLOCK_MONOTONIC_RAW
#undef CLOCK_REALTIME

export module embdr.cxxstd.timeCore;
import embdr.cxxstd.memoryMaybe;

namespace embdr::cxxstd {
        // Resolves the `clock_gettime` syscall via libc (module-internal, mirrors the
        // private Rust `call_clock_gettime` helper).
        [[nodiscard]] inline int call_clock_gettime(const int clock_id, ::timespec* ts) noexcept {
                return ::clock_gettime(static_cast<clockid_t>(clock_id), ts);
        }

        // The `timespec` structure holding seconds and nanoseconds.
        // Platform-agnostic representation, converted to/from `::timespec` at syscall boundaries.
        export struct Timespec {
                // Seconds since the epoch (realtime) or boot (monotonic).
                std::uint64_t _M_tv_sec{};
                // Additional nanoseconds.
                std::uint64_t _M_tv_nsec{};

                // Converts this `Timespec` to `::timespec` for syscall use.
                [[nodiscard]] constexpr ::timespec into_libc() const noexcept {
                        ::timespec ts{};
                        ts.tv_sec = static_cast<::time_t>(this->_M_tv_sec);
                        ts.tv_nsec = static_cast<long>(this->_M_tv_nsec);
                        return ts;
                }

                // Converts a `::timespec` to our `Timespec`.
                [[nodiscard]] static constexpr Timespec from_libc(const ::timespec& ts) noexcept {
                        return Timespec{static_cast<std::uint64_t>(ts.tv_sec),
                                        static_cast<std::uint64_t>(ts.tv_nsec)};
                }

                friend constexpr bool operator==(const Timespec& lhs, const Timespec& rhs) noexcept {
                        return lhs._M_tv_sec == rhs._M_tv_sec && lhs._M_tv_nsec == rhs._M_tv_nsec;
                }
        };

        // The result of a time query, carrying seconds and nanoseconds.
        export struct TimeVal {
                // Seconds component.
                std::uint64_t _M_secs{};
                // Nanoseconds component.
                std::uint64_t _M_nsecs{};

                [[nodiscard]] constexpr std::uint64_t secs() const noexcept { return this->_M_secs; }

                [[nodiscard]] constexpr std::uint64_t nsecs() const noexcept { return this->_M_nsecs; }

                friend constexpr bool operator==(const TimeVal& lhs, const TimeVal& rhs) noexcept {
                        return lhs._M_secs == rhs._M_secs && lhs._M_nsecs == rhs._M_nsecs;
                }
        };

        // Error type for time syscalls (port of the Rust `TimeError` enum).
        export struct TimeError {
                enum class Kind : std::uint8_t {
                        // The syscall returned an error code.
                        SyscallError,
                        // The clock ID is not supported on this platform.
                        UnsupportedClock,
                        // The clock is not available (e.g., no OS timer on bare-metal).
                        NotAvailable,
                };

                Kind _M_kind{Kind::NotAvailable};
                int _M_code{0};

                [[nodiscard]] static constexpr TimeError syscall_error(const int rc) noexcept {
                        return TimeError{Kind::SyscallError, rc};
                }

                [[nodiscard]] static constexpr TimeError unsupported_clock() noexcept {
                        return TimeError{Kind::UnsupportedClock, 0};
                }

                [[nodiscard]] static constexpr TimeError not_available() noexcept {
                        return TimeError{Kind::NotAvailable, 0};
                }

                [[nodiscard]] constexpr Kind kind() const noexcept { return this->_M_kind; }

                // Raw syscall return code carried by `SyscallError` (0 for the other variants).
                [[nodiscard]] constexpr int code() const noexcept { return this->_M_code; }

                friend constexpr bool operator==(const TimeError& lhs, const TimeError& rhs) noexcept {
                        return lhs._M_kind == rhs._M_kind && lhs._M_code == rhs._M_code;
                }

                friend constexpr bool operator!=(const TimeError& lhs, const TimeError& rhs) noexcept {
                        return !(lhs == rhs);
                }
        };

        // A syscall-based time provider for both monotonic and realtime clocks.
        //
        // `SystemTime` issues raw syscalls via `clock_gettime`. All methods are static so no
        // allocation or runtime state is required. Rust `Result<T, TimeError>` returns are ported
        // as `Maybe<T>` (an empty `Maybe` is the error case); the `TimeError&` overloads expose
        // the diagnostic (error-out pattern) and leave `error` untouched on success.
        export struct SystemTime {
                // Clock ID for `CLOCK_MONOTONIC`: monotonically increasing time.
                static constexpr int CLOCK_MONOTONIC = 1;
                // Clock ID for `CLOCK_REALTIME`: wall-clock time.
                static constexpr int CLOCK_REALTIME = 0;
                // Clock ID for `CLOCK_MONOTONIC_RAW`: monotonic time not subject to NTP corrections.
                static constexpr int CLOCK_MONOTONIC_RAW = 4;

                // Returns the current monotonic time (equivalent to now_with_clock(CLOCK_MONOTONIC)).
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> now() noexcept { return now_with_clock(CLOCK_MONOTONIC); }

                // Error-out overload of `now()`.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> now(TimeError& error) noexcept {
                        return now_with_clock(CLOCK_MONOTONIC, error);
                }

                // Returns the current time for the given clock. Use `now()` for CLOCK_MONOTONIC.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> now_with_clock(const int clock_id) noexcept {
                        TimeError error{};
                        return now_with_clock(clock_id, error);
                }

                // Error-out overload of `now_with_clock()`; on failure `error` describes the fault.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> now_with_clock(const int clock_id, TimeError& error) noexcept {
                        ::timespec ts{};
                        const int rc = call_clock_gettime(clock_id, &ts);
                        if (rc != 0) {
                                error = TimeError::syscall_error(rc);
                                return {};
                        }
                        return TimeVal{static_cast<std::uint64_t>(ts.tv_sec),
                                       static_cast<std::uint64_t>(ts.tv_nsec)};
                }

                // Returns the current monotonic time. Not affected by system clock adjustments.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> monotonic() noexcept { return now_with_clock(CLOCK_MONOTONIC); }

                // Error-out overload of `monotonic()`.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> monotonic(TimeError& error) noexcept {
                        return now_with_clock(CLOCK_MONOTONIC, error);
                }

                // Returns the current realtime (wall-clock) time. May jump due to NTP/manual changes.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> realtime() noexcept { return now_with_clock(CLOCK_REALTIME); }

                // Error-out overload of `realtime()`.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> realtime(TimeError& error) noexcept {
                        return now_with_clock(CLOCK_REALTIME, error);
                }

                // Returns the current monotonic raw time (like `monotonic()` without NTP corrections).
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> monotonic_raw() noexcept {
                        return now_with_clock(CLOCK_MONOTONIC_RAW);
                }

                // Error-out overload of `monotonic_raw()`.
                [[nodiscard]] static Maybe<TimeVal, TimeError::Kind> monotonic_raw(TimeError& error) noexcept {
                        return now_with_clock(CLOCK_MONOTONIC_RAW, error);
                }

                // Returns monotonic nanoseconds since an arbitrary reference point (0 on error).
                [[nodiscard]] static std::uint64_t monotonic_nanos() noexcept {
                        const auto tv = now();
                        return tv.has_value() ? nsecs_from_val(tv.value()) : 0;
                }

                // Returns realtime nanoseconds since the Unix epoch (0 on error).
                [[nodiscard]] static std::uint64_t realtime_nanos() noexcept {
                        const auto tv = now_with_clock(CLOCK_REALTIME);
                        return tv.has_value() ? nsecs_from_val(tv.value()) : 0;
                }

                // Returns the elapsed nanoseconds since `start` measured on the monotonic clock.
                [[nodiscard]] static Maybe<std::uint64_t, TimeError::Kind> elapsed_since(const TimeVal start) noexcept {
                        const auto current = monotonic();
                        if (!current.has_value())
                                return {};
                        return diff_nsecs(current.value(), start);
                }

                // Error-out overload of `elapsed_since()`.
                [[nodiscard]] static Maybe<std::uint64_t, TimeError::Kind> elapsed_since(const TimeVal start,
                                                                       TimeError& error) noexcept {
                        const auto current = monotonic(error);
                        if (!current.has_value())
                                return {};
                        return diff_nsecs(current.value(), start);
                }

                // Returns the duration between `earlier` and `later` in nanoseconds (0 if reversed).
                [[nodiscard]] static std::uint64_t duration_since(const TimeVal earlier,
                                                                 const TimeVal later) noexcept {
                        return diff_nsecs(later, earlier);
                }

                // Converts a `TimeVal` to total nanoseconds.
                [[nodiscard]] static constexpr std::uint64_t nsecs_from_val(const TimeVal& val) noexcept {
                        return val._M_secs * 1'000'000'000ull + val._M_nsecs;
                }

                // Converts a `TimeVal` to total microseconds.
                [[nodiscard]] static constexpr std::uint64_t micros_from_val(const TimeVal& val) noexcept {
                        return val._M_secs * 1'000'000ull + val._M_nsecs / 1'000ull;
                }

                // Converts a `TimeVal` to total milliseconds.
                [[nodiscard]] static constexpr std::uint64_t millis_from_val(const TimeVal& val) noexcept {
                        return val._M_secs * 1'000ull + val._M_nsecs / 1'000'000ull;
                }

                // Converts a `TimeVal` to total seconds (truncating).
                [[nodiscard]] static constexpr std::uint64_t secs_from_val(const TimeVal& val) noexcept {
                        return val._M_secs;
                }

                // Converts seconds to nanoseconds.
                [[nodiscard]] static constexpr std::uint64_t secs_to_nanos(const std::uint64_t secs) noexcept {
                        return secs * 1'000'000'000ull;
                }

                // Converts milliseconds to nanoseconds.
                [[nodiscard]] static constexpr std::uint64_t millis_to_nanos(const std::uint64_t millis) noexcept {
                        return millis * 1'000'000ull;
                }

                // Converts microseconds to nanoseconds.
                [[nodiscard]] static constexpr std::uint64_t micros_to_nanos(const std::uint64_t micros) noexcept {
                        return micros * 1'000ull;
                }

                // Converts nanoseconds to seconds (truncating).
                [[nodiscard]] static constexpr std::uint64_t nanos_to_secs(const std::uint64_t nanos) noexcept {
                        return nanos / 1'000'000'000ull;
                }

                // Converts nanoseconds to milliseconds (truncating).
                [[nodiscard]] static constexpr std::uint64_t nanos_to_millis(const std::uint64_t nanos) noexcept {
                        return nanos / 1'000'000ull;
                }

                // Converts nanoseconds to microseconds (truncating).
                [[nodiscard]] static constexpr std::uint64_t nanos_to_micros(const std::uint64_t nanos) noexcept {
                        return nanos / 1'000ull;
                }

                // Adds two `TimeVal` instances, carrying nanoseconds into seconds once.
                [[nodiscard]] static constexpr TimeVal add_timeval(const TimeVal a, const TimeVal b) noexcept {
                        std::uint64_t nsecs = a._M_nsecs + b._M_nsecs;
                        std::uint64_t secs = a._M_secs + b._M_secs;
                        if (nsecs >= 1'000'000'000ull) {
                                nsecs -= 1'000'000'000ull;
                                secs += 1;
                        }
                        return TimeVal{secs, nsecs};
                }

                // Subtracts `b` from `a`. Returns a zero `TimeVal` if `b > a`.
                [[nodiscard]] static constexpr TimeVal sub_timeval(const TimeVal a, const TimeVal b) noexcept {
                        if (a._M_secs < b._M_secs || (a._M_secs == b._M_secs && a._M_nsecs < b._M_nsecs))
                                return TimeVal{0, 0};
                        std::uint64_t a_nsecs = a._M_nsecs;
                        std::uint64_t a_secs = a._M_secs;
                        if (a_nsecs < b._M_nsecs) {
                                a_nsecs += 1'000'000'000ull;
                                a_secs -= 1;
                        }
                        return TimeVal{a_secs - b._M_secs, a_nsecs - b._M_nsecs};
                }

                // Returns the difference `later - earlier` in nanoseconds (saturating at 0).
                [[nodiscard]] static constexpr std::uint64_t diff_nsecs(const TimeVal& later,
                                                                       const TimeVal& earlier) noexcept {
                        const std::uint64_t later_nsecs = nsecs_from_val(later);
                        const std::uint64_t earlier_nsecs = nsecs_from_val(earlier);
                        return later_nsecs >= earlier_nsecs ? later_nsecs - earlier_nsecs : 0;
                }

                // Compares two `TimeVal` instances: -1 if a < b, 0 if equal, 1 if a > b.
                [[nodiscard]] static constexpr std::int8_t compare(const TimeVal a, const TimeVal b) noexcept {
                        const std::uint64_t a_nsecs = nsecs_from_val(a);
                        const std::uint64_t b_nsecs = nsecs_from_val(b);
                        if (a_nsecs < b_nsecs)
                                return -1;
                        if (a_nsecs > b_nsecs)
                                return 1;
                        return 0;
                }
        };

        // A time span of whole seconds plus a subsecond nanosecond part.
        //
        // Field layout mirrors the Rust `TimeDuration { secs: u64, nanos: TimeNanos(u32) }`:
        // a 64-bit seconds counter followed by a 32-bit nanoseconds counter in [0, 1e9).
        export struct TimeDuration {
                // The duration of one second.
                static const TimeDuration SECOND;
                // The duration of one millisecond.
                static const TimeDuration MILLISECOND;
                // The duration of one microsecond.
                static const TimeDuration MICROSECOND;
                // The duration of one nanosecond.
                static const TimeDuration NANOSECOND;
                // A duration of zero time.
                static const TimeDuration ZERO;
                // The maximum duration (~584,942,417,355 years).
                static const TimeDuration MAX;

                constexpr TimeDuration() noexcept = default;

                // Rust `TimeDuration::new(secs, nanos)`: nanoseconds >= 1e9 carry into the
                // seconds component (saturating at u64::MAX).
                constexpr TimeDuration(const std::uint64_t secs, const std::uint32_t nanos) noexcept {
                        if (nanos < NANOS_PER_SEC) {
                                this->_M_secs = secs;
                                this->_M_nanos = nanos;
                        } else {
                                const std::uint64_t extra = nanos / NANOS_PER_SEC;
                                this->_M_secs = extra > ~std::uint64_t{0} - secs ? ~std::uint64_t{0} : secs + extra;
                                this->_M_nanos = nanos % NANOS_PER_SEC;
                        }
                }

                // Creates a duration from a whole number of seconds.
                [[nodiscard]] static constexpr TimeDuration from_secs(const std::uint64_t secs) noexcept {
                        return TimeDuration{secs, 0u};
                }

                // Creates a duration from a whole number of milliseconds.
                [[nodiscard]] static constexpr TimeDuration from_millis(const std::uint64_t millis) noexcept {
                        const std::uint64_t secs = millis / MILLIS_PER_SEC;
                        const auto subsec_millis = static_cast<std::uint32_t>(millis % MILLIS_PER_SEC);
                        return TimeDuration{secs, subsec_millis * NANOS_PER_MILLI};
                }

                // Creates a duration from a whole number of microseconds.
                [[nodiscard]] static constexpr TimeDuration from_micros(const std::uint64_t micros) noexcept {
                        const std::uint64_t secs = micros / MICROS_PER_SEC;
                        const auto subsec_micros = static_cast<std::uint32_t>(micros % MICROS_PER_SEC);
                        return TimeDuration{secs, subsec_micros * NANOS_PER_MICRO};
                }

                // Creates a duration from a whole number of nanoseconds.
                [[nodiscard]] static constexpr TimeDuration from_nanos(const std::uint64_t nanos) noexcept {
                        return TimeDuration{nanos / NANOS_PER_SEC, static_cast<std::uint32_t>(nanos % NANOS_PER_SEC)};
                }

                // Creates a duration from a 128-bit nanosecond count; empty if it exceeds MAX.
                [[nodiscard]] static constexpr Maybe<TimeDuration, TimeError::Kind>
                from_nanos_u128(const unsigned __int128 nanos) noexcept {
                        const unsigned __int128 secs = nanos / NANOS_PER_SEC;
                        if (secs > ~std::uint64_t{0})
                                return {};
                        const auto subsec_nanos = static_cast<std::uint32_t>(nanos % NANOS_PER_SEC);
                        return TimeDuration{static_cast<std::uint64_t>(secs), subsec_nanos};
                }

                // Returns true if this duration spans no time.
                [[nodiscard]] constexpr bool is_zero() const noexcept {
                        return this->_M_secs == 0 && this->_M_nanos == 0;
                }

                // Returns the number of whole seconds contained by this duration.
                [[nodiscard]] constexpr std::uint64_t as_secs() const noexcept { return this->_M_secs; }

                // Returns the fractional part of this duration, in whole milliseconds.
                [[nodiscard]] constexpr std::uint32_t subsec_millis() const noexcept {
                        return this->_M_nanos / NANOS_PER_MILLI;
                }

                // Returns the fractional part of this duration, in whole microseconds.
                [[nodiscard]] constexpr std::uint32_t subsec_micros() const noexcept {
                        return this->_M_nanos / NANOS_PER_MICRO;
                }

                // Returns the fractional part of this duration, in nanoseconds.
                [[nodiscard]] constexpr std::uint32_t subsec_nanos() const noexcept { return this->_M_nanos; }

                // Returns the total number of whole milliseconds contained by this duration.
                [[nodiscard]] constexpr unsigned __int128 as_millis() const noexcept {
                        return static_cast<unsigned __int128>(this->_M_secs) * MILLIS_PER_SEC +
                               this->_M_nanos / NANOS_PER_MILLI;
                }

                // Returns the total number of whole microseconds contained by this duration.
                [[nodiscard]] constexpr unsigned __int128 as_micros() const noexcept {
                        return static_cast<unsigned __int128>(this->_M_secs) * MICROS_PER_SEC +
                               this->_M_nanos / NANOS_PER_MICRO;
                }

                // Returns the total number of nanoseconds contained by this duration.
                [[nodiscard]] constexpr unsigned __int128 as_nanos() const noexcept {
                        return static_cast<unsigned __int128>(this->_M_secs) * NANOS_PER_SEC + this->_M_nanos;
                }

                // Computes the absolute difference between `this` and `other`.
                [[nodiscard]] constexpr TimeDuration abs_diff(const TimeDuration& other) const noexcept;

                // Checked addition: empty on overflow.
                [[nodiscard]] constexpr Maybe<TimeDuration, TimeError::Kind> checked_add(const TimeDuration& rhs) const noexcept;

                // Saturating addition: MAX on overflow.
                [[nodiscard]] constexpr TimeDuration saturating_add(const TimeDuration& rhs) const noexcept {
                        const auto result = this->checked_add(rhs);
                        return result.has_value() ? result.value() : MAX;
                }

                // Checked subtraction: empty if the result would be negative or overflow.
                [[nodiscard]] constexpr Maybe<TimeDuration, TimeError::Kind> checked_sub(const TimeDuration& rhs) const noexcept;

                // Saturating subtraction: ZERO if the result would be negative or overflow.
                [[nodiscard]] constexpr TimeDuration saturating_sub(const TimeDuration& rhs) const noexcept {
                        const auto result = this->checked_sub(rhs);
                        return result.has_value() ? result.value() : ZERO;
                }

                // Checked multiplication by a u32 scalar: empty on overflow.
                [[nodiscard]] constexpr Maybe<TimeDuration, TimeError::Kind> checked_mul(std::uint32_t rhs) const noexcept;

                // Saturating multiplication by a u32 scalar: MAX on overflow.
                [[nodiscard]] constexpr TimeDuration saturating_mul(std::uint32_t rhs) const noexcept {
                        const auto result = this->checked_mul(rhs);
                        return result.has_value() ? result.value() : MAX;
                }

                // Checked division by a u32 scalar: empty if `rhs == 0`.
                [[nodiscard]] constexpr Maybe<TimeDuration, TimeError::Kind> checked_div(std::uint32_t rhs) const noexcept;

                // Returns the seconds contained by this duration as f64 (including the fraction).
                [[nodiscard]] constexpr double as_secs_f64() const noexcept {
                        return static_cast<double>(this->_M_secs) +
                               static_cast<double>(this->_M_nanos) / static_cast<double>(NANOS_PER_SEC);
                }

                // Returns the seconds contained by this duration as f32 (including the fraction).
                [[nodiscard]] constexpr float as_secs_f32() const noexcept {
                        return static_cast<float>(this->_M_secs) +
                               static_cast<float>(this->_M_nanos) / static_cast<float>(NANOS_PER_SEC);
                }

                // Returns the milliseconds contained by this duration as f64.
                [[nodiscard]] constexpr double as_millis_f64() const noexcept {
                        return static_cast<double>(this->_M_secs) * static_cast<double>(MILLIS_PER_SEC) +
                               static_cast<double>(this->_M_nanos) / static_cast<double>(NANOS_PER_MILLI);
                }

                // Returns the milliseconds contained by this duration as f32.
                [[nodiscard]] constexpr float as_millis_f32() const noexcept {
                        return static_cast<float>(this->_M_secs) * static_cast<float>(MILLIS_PER_SEC) +
                               static_cast<float>(this->_M_nanos) / static_cast<float>(NANOS_PER_MILLI);
                }

                // Divides this duration by `rhs` and returns the ratio as f64.
                [[nodiscard]] constexpr double div_duration_f64(const TimeDuration& rhs) const noexcept {
                        const double self_nanos =
                            static_cast<double>(this->_M_secs) * NANOS_PER_SEC + static_cast<double>(this->_M_nanos);
                        const double rhs_nanos =
                            static_cast<double>(rhs._M_secs) * NANOS_PER_SEC + static_cast<double>(rhs._M_nanos);
                        return self_nanos / rhs_nanos;
                }

                // Divides this duration by `rhs` and returns the ratio as f32.
                [[nodiscard]] constexpr float div_duration_f32(const TimeDuration& rhs) const noexcept {
                        const float self_nanos =
                            static_cast<float>(this->_M_secs) * NANOS_PER_SEC + static_cast<float>(this->_M_nanos);
                        const float rhs_nanos =
                            static_cast<float>(rhs._M_secs) * NANOS_PER_SEC + static_cast<float>(rhs._M_nanos);
                        return self_nanos / rhs_nanos;
                }

                // Rust `Sum`/`Sum<&TimeDuration>`: sums a range of durations.
                // Overflow aborts (the Rust implementation panics with "overflow in iter::sum").
                template <typename InputIt>
                [[nodiscard]] static TimeDuration sum(InputIt first, InputIt last) noexcept {
                        std::uint64_t total_secs = 0;
                        std::uint64_t total_nanos = 0;
                        for (; first != last; ++first) {
                                const TimeDuration entry = *first;
                                if (__builtin_add_overflow(total_secs, entry._M_secs, &total_secs))
                                        std::abort();
                                std::uint64_t next = 0;
                                if (__builtin_add_overflow(total_nanos,
                                                           static_cast<std::uint64_t>(entry._M_nanos), &next)) {
                                        if (__builtin_add_overflow(total_secs, total_nanos / NANOS_PER_SEC,
                                                                   &total_secs))
                                                std::abort();
                                        total_nanos = total_nanos % NANOS_PER_SEC + entry._M_nanos;
                                } else {
                                        total_nanos = next;
                                }
                        }
                        if (__builtin_add_overflow(total_secs, total_nanos / NANOS_PER_SEC, &total_secs))
                                std::abort();
                        total_nanos %= NANOS_PER_SEC;
                        return TimeDuration{total_secs, static_cast<std::uint32_t>(total_nanos)};
                }

                [[nodiscard]] static TimeDuration sum(const std::initializer_list<TimeDuration> durations) noexcept {
                        return sum(durations.begin(), durations.end());
                }

                // Rust `impl Add`: checked addition, saturating to ZERO on overflow.
                friend constexpr TimeDuration operator+(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        const auto result = lhs.checked_add(rhs);
                        return result.has_value() ? result.value() : ZERO;
                }

                friend constexpr TimeDuration& operator+=(TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        lhs = lhs + rhs;
                        return lhs;
                }

                // Rust `impl Sub`: checked subtraction, saturating to ZERO on underflow.
                friend constexpr TimeDuration operator-(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        const auto result = lhs.checked_sub(rhs);
                        return result.has_value() ? result.value() : ZERO;
                }

                friend constexpr TimeDuration& operator-=(TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        lhs = lhs - rhs;
                        return lhs;
                }

                // Rust `impl Mul<u32>`: checked multiplication, saturating to ZERO on overflow.
                friend constexpr TimeDuration operator*(const TimeDuration& lhs, const std::uint32_t rhs) noexcept {
                        const auto result = lhs.checked_mul(rhs);
                        return result.has_value() ? result.value() : ZERO;
                }

                // Rust `impl Mul<TimeDuration> for u32`.
                friend constexpr TimeDuration operator*(const std::uint32_t lhs, const TimeDuration& rhs) noexcept {
                        return rhs * lhs;
                }

                friend constexpr TimeDuration& operator*=(TimeDuration& lhs, const std::uint32_t rhs) noexcept {
                        lhs = lhs * rhs;
                        return lhs;
                }

                // Rust `impl Div<u32>`: checked division, ZERO when dividing by zero.
                friend constexpr TimeDuration operator/(const TimeDuration& lhs, const std::uint32_t rhs) noexcept {
                        const auto result = lhs.checked_div(rhs);
                        return result.has_value() ? result.value() : ZERO;
                }

                friend constexpr TimeDuration& operator/=(TimeDuration& lhs, const std::uint32_t rhs) noexcept {
                        lhs = lhs / rhs;
                        return lhs;
                }

                friend constexpr bool operator==(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return lhs._M_secs == rhs._M_secs && lhs._M_nanos == rhs._M_nanos;
                }

                friend constexpr bool operator!=(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return !(lhs == rhs);
                }

                // Rust `PartialOrd`/`Ord` derive: order by seconds first, then nanoseconds.
                friend constexpr bool operator<(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return lhs._M_secs < rhs._M_secs ||
                               (lhs._M_secs == rhs._M_secs && lhs._M_nanos < rhs._M_nanos);
                }

                friend constexpr bool operator>(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return rhs < lhs;
                }

                friend constexpr bool operator<=(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return !(rhs < lhs);
                }

                friend constexpr bool operator>=(const TimeDuration& lhs, const TimeDuration& rhs) noexcept {
                        return !(lhs < rhs);
                }

            private:
                static constexpr std::uint32_t NANOS_PER_SEC = 1'000'000'000u;
                static constexpr std::uint32_t NANOS_PER_MILLI = 1'000'000u;
                static constexpr std::uint32_t NANOS_PER_MICRO = 1'000u;
                static constexpr std::uint64_t MILLIS_PER_SEC = 1'000ull;
                static constexpr std::uint64_t MICROS_PER_SEC = 1'000'000ull;
                std::uint64_t _M_secs{};
                std::uint32_t _M_nanos{};
        };
        constexpr TimeDuration TimeDuration::SECOND{1, 0};
        constexpr TimeDuration TimeDuration::MILLISECOND{0, 1'000'000u};
        constexpr TimeDuration TimeDuration::MICROSECOND{0, 1'000u};
        constexpr TimeDuration TimeDuration::NANOSECOND{0, 1u};
        constexpr TimeDuration TimeDuration::ZERO{0, 0};
        constexpr TimeDuration TimeDuration::MAX{~std::uint64_t{0}, 999'999'999u};

        constexpr TimeDuration TimeDuration::abs_diff(const TimeDuration& other) const noexcept {
                const auto self_diff = this->checked_sub(other);
                if (self_diff.has_value())
                        return self_diff.value();
                const auto other_diff = other.checked_sub(*this);
                return other_diff.has_value() ? other_diff.value() : TimeDuration{};
        }

        constexpr Maybe<TimeDuration, TimeError::Kind> TimeDuration::checked_add(const TimeDuration& rhs) const noexcept {
                std::uint64_t secs = 0;
                if (__builtin_add_overflow(this->_M_secs, rhs._M_secs, &secs))
                        return {};
                std::uint64_t nanos = static_cast<std::uint64_t>(this->_M_nanos) + rhs._M_nanos;
                if (nanos >= NANOS_PER_SEC) {
                        nanos -= NANOS_PER_SEC;
                        if (__builtin_add_overflow(secs, std::uint64_t{1}, &secs))
                                return {};
                }
                return TimeDuration{secs, static_cast<std::uint32_t>(nanos)};
        }

        constexpr Maybe<TimeDuration, TimeError::Kind> TimeDuration::checked_sub(const TimeDuration& rhs) const noexcept {
                std::uint64_t secs = 0;
                if (__builtin_sub_overflow(this->_M_secs, rhs._M_secs, &secs))
                        return {};
                std::uint32_t nanos = 0;
                if (this->_M_nanos >= rhs._M_nanos) {
                        nanos = this->_M_nanos - rhs._M_nanos;
                } else {
                        std::uint64_t borrowed = 0;
                        if (__builtin_sub_overflow(secs, std::uint64_t{1}, &borrowed))
                                return {};
                        secs = borrowed;
                        nanos = this->_M_nanos + NANOS_PER_SEC - rhs._M_nanos;
                }
                return TimeDuration{secs, nanos};
        }

        constexpr Maybe<TimeDuration, TimeError::Kind> TimeDuration::checked_mul(const std::uint32_t rhs) const noexcept {
                // Multiply nanoseconds as u64, because it cannot overflow that way.
                const std::uint64_t total_nanos = static_cast<std::uint64_t>(this->_M_nanos) * rhs;
                const std::uint64_t extra_secs = total_nanos / NANOS_PER_SEC;
                const auto nanos = static_cast<std::uint32_t>(total_nanos % NANOS_PER_SEC);
                std::uint64_t secs = 0;
                if (__builtin_mul_overflow(this->_M_secs, static_cast<std::uint64_t>(rhs), &secs))
                        return {};
                if (__builtin_add_overflow(secs, extra_secs, &secs))
                        return {};
                return TimeDuration{secs, nanos};
        }

        constexpr Maybe<TimeDuration, TimeError::Kind> TimeDuration::checked_div(const std::uint32_t rhs) const noexcept {
                if (rhs == 0)
                        return {};
                const std::uint64_t divisor = rhs;
                const std::uint64_t secs = this->_M_secs / divisor;
                const std::uint64_t extra_secs = this->_M_secs % divisor;
                std::uint64_t nanos = this->_M_nanos / rhs;
                const std::uint64_t extra_nanos = this->_M_nanos % rhs;
                nanos += (extra_secs * NANOS_PER_SEC + extra_nanos) / divisor;
                return TimeDuration{secs, static_cast<std::uint32_t>(nanos)};
        }
} // namespace embdr::cxxstd
