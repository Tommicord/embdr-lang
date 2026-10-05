/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


module;
#include <atomic>
#include <cstddef>
#include <cstdint>

export module embdr.cxxstd.logger;
import embdr.cxxstd.io;
import embdr.cxxstd.stringView;
import embdr.cxxstd.timeCore;
import embdr.cxxstd.unwindPrettify;

/*
 * Zero-allocation console logging (port of the Rust codevar-logger crate).
 *
 * Four levels (debug/info/error/irr) are filtered through an atomic minimum
 * level; enabled messages are formatted as "[<utc-iso8601>]:<LEVEL> <message>\n"
 * into a fixed 512-byte stack buffer and handed to a pluggable LogWriter.
 * ERROR and IRR lines go to stderr, everything else to stdout. The default
 * writer streams through embdr.cxxstd.io (terminal detection included); a
 * custom writer can be installed for capture. ANSI colors around the bracket
 * are tri-state (off / on / auto via terminal detection). The styled helpers
 * log_error..log_debug wrap the message in the console style presets exactly
 * like the Rust helpers do. No exceptions, no RTTI, no heap.
 */
namespace embdr::cxxstd {
        /* Log severity, ordered from most to least verbose (Rust `LogLevel`, repr(u8)). */
        export enum class LogLevel : uint8_t {
                debug = 0,
                info = 1,
                error = 2,
                irr = 3,
        };

        /* Returns the level name as it appears in log lines ("DEBUG", ...). */
        export constexpr SimpleStringView as_str(const LogLevel level) noexcept {
                switch (level) {
                        case LogLevel::debug:
                                return "DEBUG";
                        case LogLevel::info:
                                return "INFO";
                        case LogLevel::error:
                                return "ERROR";
                        case LogLevel::irr:
                                return "IRR";
                }
                return "IRR";
        }

        /* Global minimum level; messages below it are dropped (relaxed atomics, like Rust). */
        static std::atomic<uint8_t> log_min_level{0};

        export void set_min_log_level(const LogLevel level) noexcept {
                log_min_level.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
        }

        export [[nodiscard]] LogLevel min_log_level() noexcept {
                return static_cast<LogLevel>(log_min_level.load(std::memory_order_relaxed));
        }

        export [[nodiscard]] bool is_enabled(const LogLevel level) noexcept {
                return static_cast<uint8_t>(level) >= log_min_level.load(std::memory_order_relaxed);
        }

        /* Tri-state ANSI color switch (Rust `Option<bool>` argument of set_ansi_colors). */
        export enum class AnsiColors : uint8_t {
                disabled,
                enabled,
                autoDetect,
        };

        static std::atomic<uint8_t> log_ansi_colors{2}; // 0=disabled, 1=enabled, 2=auto

        export void set_ansi_colors(const AnsiColors mode) noexcept {
                log_ansi_colors.store(static_cast<uint8_t>(mode), std::memory_order_relaxed);
        }

        /* Resolves the mode against the terminal state of the target stream. */
        static bool log_should_use_ansi(const bool stream_is_terminal) noexcept {
                switch (log_ansi_colors.load(std::memory_order_relaxed)) {
                        case 0:
                                return false;
                        case 1:
                                return true;
                        default:
                                return stream_is_terminal;
                }
        }

        /* Error returned by logging operations (Rust `LogError`). */
        export class LogError {
            public:
                enum class Kind : uint8_t {
                        syscallError,
                        bufferTooSmall,
                        encodingError,
                        handleUnavailable,
                        ioError,
                };

                constexpr LogError() noexcept = default;

                [[nodiscard]] static constexpr LogError syscall_error(const int code) noexcept {
                        return LogError(Kind::syscallError, code);
                }

                [[nodiscard]] static constexpr LogError buffer_too_small() noexcept {
                        return LogError(Kind::bufferTooSmall, 0);
                }

                [[nodiscard]] static constexpr LogError encoding_error() noexcept {
                        return LogError(Kind::encodingError, 0);
                }

                [[nodiscard]] static constexpr LogError handle_unavailable() noexcept {
                        return LogError(Kind::handleUnavailable, 0);
                }

                [[nodiscard]] static constexpr LogError io_error(const int code) noexcept {
                        return LogError(Kind::ioError, code);
                }

                [[nodiscard]] constexpr Kind kind() const noexcept { return this->_M_kind; }

                /* errno for syscallError, ErrorKind discriminant for ioError, 0 otherwise. */
                [[nodiscard]] constexpr int code() const noexcept { return this->_M_code; }

                [[nodiscard]] constexpr bool operator==(const LogError& other) const noexcept {
                        return this->_M_kind == other._M_kind && this->_M_code == other._M_code;
                }

            private:
                Kind _M_kind = Kind::handleUnavailable;
                int _M_code = 0;

                constexpr LogError(const Kind kind, const int code) noexcept : _M_kind(kind), _M_code(code) {}
        };

        static void log_write_code(FrameSink<64>& sink, const int code) noexcept {
                long long value = code;
                if (value < 0) {
                        sink.write("-");
                        value = -value;
                }
                write_uint(sink, static_cast<uint64_t>(value), 0);
        }

        /*
         * Formats err like Rust's `Display for LogError` into buf without a
         * trailing newline, truncating to cap - 1 bytes plus the terminator.
         * Returns the byte count written (excluding the terminator).
         */
        export [[nodiscard]] size_t log_error_message(const LogError& err, char* const buf, const size_t cap) noexcept {
                if (buf == nullptr || cap == 0)
                        return 0;
                FrameSink<64> text;
                switch (err.kind()) {
                        case LogError::Kind::syscallError:
                                text.write("syscall error: ");
                                log_write_code(text, err.code());
                                break;
                        case LogError::Kind::bufferTooSmall:
                                text.write("buffer too small");
                                break;
                        case LogError::Kind::encodingError:
                                text.write("encoding error");
                                break;
                        case LogError::Kind::handleUnavailable:
                                text.write("console handle unavailable");
                                break;
                        case LogError::Kind::ioError:
                                text.write("I/O error: ");
                                log_write_code(text, err.code());
                                break;
                }
                const SimpleStringView text_view = text.view();
                const size_t count = text_view.size() < cap - 1 ? text_view.size() : cap - 1;
                for (size_t i = 0; i < count; ++i)
                        buf[i] = text_view.data()[i];
                buf[count] = '\0';
                return count;
        }

        /* Result of a logging call: ok when the message was handed to a writer (or filtered out). */
        export using LogResult = IoResult<void, LogError>;

        /* Converts an io write result into a LogResult (Rust `From<ConsoleError> for LogError`). */
        static LogResult log_from_io(const IoResult<void, ErrorKind>& result) noexcept {
                if (result.has_value())
                        return LogResult::ok();
                return LogResult::err(LogError::io_error(static_cast<int>(result.error())));
        }

        /* Sink for formatted log bytes (Rust `LogWriter` trait). */
        export class LogWriter {
            public:
                LogWriter() noexcept = default;
                virtual ~LogWriter() noexcept = default;

                /* Appends bytes to the stdout stream. */
                virtual LogResult write_stdout(SimpleStringView bytes) = 0;
                /* Appends bytes to the stderr stream. */
                virtual LogResult write_stderr(SimpleStringView bytes) = 0;
                /* Flushes any buffered output. */
                virtual LogResult flush() = 0;
        };

        /* Console writer streaming through embdr.cxxstd.io (Rust `DefaultLogWriter`). */
        export class DefaultLogWriter final : public LogWriter {
            public:
                DefaultLogWriter() noexcept = default;

                LogResult write_stdout(const SimpleStringView bytes) override {
                        Stdout out;
                        const auto written = out.write_all(to_bytes(bytes));
                        if (!written.has_value())
                                return log_from_io(written);
                        return log_from_io(out.flush());
                }

                LogResult write_stderr(const SimpleStringView bytes) override {
                        Stderr out;
                        const auto written = out.write_all(to_bytes(bytes));
                        if (!written.has_value())
                                return log_from_io(written);
                        return log_from_io(out.flush());
                }

                LogResult flush() override {
                        Stdout out;
                        return log_from_io(out.flush());
                }
        };

        static DefaultLogWriter log_default_writer{};
        static std::atomic<LogWriter*> log_writer{nullptr};

        /* Writer set by set_log_writer, or the built-in console writer. */
        static LogWriter* log_get_writer() noexcept {
                LogWriter* const writer = log_writer.load(std::memory_order_relaxed);
                return writer != nullptr ? writer : &log_default_writer;
        }

        /* Installs a custom global writer; it must outlive all logging, nullptr restores the default. */
        export void set_log_writer(LogWriter* const writer) noexcept {
                log_writer.store(writer, std::memory_order_relaxed);
        }

        /* The built-in console writer; feed it to set_log_writer to restore default output. */
        export [[nodiscard]] LogWriter* default_log_writer() noexcept { return &log_default_writer; }

        /* Maximum size of a single log line (Rust LOG_LINE_BUFFER_SIZE). */
        static constexpr size_t log_line_buffer_size = 512;

        static size_t log_copy(char* const buffer, const size_t offset, const SimpleStringView text) noexcept {
                for (size_t i = 0; i < text.size(); ++i)
                        buffer[offset + i] = text.data()[i];
                return text.size();
        }

        /* Writes value as exactly width decimal digits (keeps the low digits), advancing idx. */
        static void log_write_fixed(char* const buf, size_t& idx, uint64_t value, const size_t width) noexcept {
                size_t pos = idx + width;
                for (size_t i = 0; i < width; ++i) {
                        buf[--pos] = static_cast<char>('0' + (value % 10));
                        value /= 10;
                }
                idx += width;
        }

        /* Converts days since the Unix epoch to a proleptic Gregorian UTC date. */
        static void log_civil_from_days(const int64_t days, int64_t& year, uint32_t& month, uint32_t& day) noexcept {
                const int64_t z = days + 719468;
                const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
                const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
                const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
                const int64_t y = static_cast<int64_t>(yoe) + era * 400;
                const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
                const uint64_t mp = (5 * doy + 2) / 153;
                day = static_cast<uint32_t>(doy - (153 * mp + 2) / 5 + 1);
                month = static_cast<uint32_t>(mp < 10 ? mp + 3 : mp - 9);
                year = y + (month <= 2 ? 1 : 0);
        }

        /*
         * Formats secs.nanos as UTC ISO8601 "YYYY-MM-DDTHH:MM:SS[.fffffffff]Z"
         * (fraction only when nanos != 0, like Rust format_utc_iso8601).
         * Needs cap >= 32; returns false when the buffer is smaller.
         */
        static bool log_format_utc_iso8601(const uint64_t secs, const uint32_t nanos, char* const buf,
                                           const size_t cap, size_t& out_len) noexcept {
                if (cap < 32)
                        return false;
                int64_t year = 0;
                uint32_t month = 0;
                uint32_t day = 0;
                log_civil_from_days(static_cast<int64_t>(secs / 86400ull), year, month, day);
                const uint64_t rem = secs % 86400ull;
                char year_digits[24];
                size_t year_count = 0;
                uint64_t year_value = static_cast<uint64_t>(year);
                do {
                        year_digits[year_count++] = static_cast<char>('0' + (year_value % 10));
                        year_value /= 10;
                } while (year_value != 0 && year_count < sizeof(year_digits));
                while (year_count < 4)
                        year_digits[year_count++] = '0';
                size_t idx = 0;
                for (size_t i = year_count; i > 0; --i)
                        buf[idx++] = year_digits[i - 1];
                buf[idx++] = '-';
                log_write_fixed(buf, idx, month, 2);
                buf[idx++] = '-';
                log_write_fixed(buf, idx, day, 2);
                buf[idx++] = 'T';
                log_write_fixed(buf, idx, rem / 3600, 2);
                buf[idx++] = ':';
                log_write_fixed(buf, idx, (rem % 3600) / 60, 2);
                buf[idx++] = ':';
                log_write_fixed(buf, idx, rem % 60, 2);
                if (nanos != 0) {
                        buf[idx++] = '.';
                        log_write_fixed(buf, idx, nanos, 9);
                }
                buf[idx++] = 'Z';
                out_len = idx;
                return true;
        }

        /* Appends the current UTC timestamp at offset; false when it does not fit. */
        static bool log_format_timestamp(char* const buffer, const size_t cap, const size_t offset,
                                         size_t& written) noexcept {
                const auto now = SystemTime::realtime();
                uint64_t secs = 0;
                uint32_t nanos = 0;
                if (now.has_value()) {
                        secs = now.value().secs();
                        nanos = static_cast<uint32_t>(now.value().nsecs());
                }
                char ts[40];
                size_t ts_len = 0;
                if (!log_format_utc_iso8601(secs, nanos, ts, sizeof(ts), ts_len))
                        return false;
                if (offset + ts_len >= cap)
                        return false;
                written = log_copy(buffer, offset, SimpleStringView(ts, ts_len));
                return true;
        }

        /* Appends "<LEVEL> " (level plus a trailing space) at offset. */
        static bool log_format_level(char* const buffer, const size_t cap, const size_t offset, const LogLevel level,
                                     size_t& written) noexcept {
                const SimpleStringView level_str = as_str(level);
                const size_t required = level_str.size() + 1;
                if (offset + required >= cap)
                        return false;
                const size_t copied = log_copy(buffer, offset, level_str);
                buffer[offset + copied] = ' ';
                written = copied + 1;
                return true;
        }

        /* Appends "message\n", truncating the message to the remaining space minus the newline. */
        static bool log_format_message(char* const buffer, const size_t cap, const size_t offset,
                                       const SimpleStringView message, size_t& written) noexcept {
                if (offset >= cap)
                        return false;
                const size_t remaining = cap - offset;
                const size_t msg_len = message.size() < remaining - 1 ? message.size() : remaining - 1;
                if (msg_len == 0 && !message.empty())
                        return false;
                const size_t copied = log_copy(buffer, offset, message.substr(0, msg_len));
                buffer[offset + copied] = '\n';
                written = copied + 1;
                return true;
        }

        /*
         * Writes the message as "[<timestamp>]:<LEVEL> <message>\n". ERROR and
         * IRR lines go to stderr, the rest to stdout; return value is ok when
         * the level is filtered out. The line is built in a fixed 512-byte
         * stack buffer, long messages are truncated. ANSI colors around the
         * bracket follow the ansi-colors mode resolved against the target
         * stream's terminal state.
         */
        export [[nodiscard]] LogResult log_with_timestamp(const LogLevel level, const SimpleStringView message) {
                if (!is_enabled(level))
                        return LogResult::ok();
                LogWriter* const writer = log_get_writer();
                char buffer[log_line_buffer_size]{};
                size_t idx = 0;
                const bool use_color = level >= LogLevel::error ? log_should_use_ansi(Stderr{}.is_terminal())
                                                                 : log_should_use_ansi(Stdout{}.is_terminal());
                if (use_color) {
                        constexpr SimpleStringView color = "\x1b[32m"; // AnsiColor::Green.fg()
                        constexpr SimpleStringView reset = "\x1b[0m";  // AnsiStyle::Reset.sequence()
                        constexpr size_t required = color.size() + reset.size() + 1;
                        if (idx + required >= log_line_buffer_size)
                                return LogResult::err(LogError::buffer_too_small());
                        idx += log_copy(buffer, idx, color);
                        buffer[idx++] = '[';
                        size_t part = 0;
                        if (!log_format_timestamp(buffer, log_line_buffer_size, idx, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                        buffer[idx++] = ']';
                        buffer[idx++] = ':';
                        if (!log_format_level(buffer, log_line_buffer_size, idx, level, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                        if (!log_format_message(buffer, log_line_buffer_size, idx, message, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                        // Rust appends the reset after the newline and panics when
                        // the line filled the buffer; report that as an error.
                        if (idx + reset.size() > log_line_buffer_size)
                                return LogResult::err(LogError::buffer_too_small());
                        idx += log_copy(buffer, idx, reset);
                } else {
                        buffer[idx++] = '[';
                        size_t part = 0;
                        if (!log_format_timestamp(buffer, log_line_buffer_size, idx, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                        buffer[idx++] = ']';
                        buffer[idx++] = ':';
                        if (!log_format_level(buffer, log_line_buffer_size, idx, level, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                        if (!log_format_message(buffer, log_line_buffer_size, idx, message, part))
                                return LogResult::err(LogError::buffer_too_small());
                        idx += part;
                }
                const SimpleStringView output(buffer, idx);
                if (level >= LogLevel::error)
                        return writer->write_stderr(output);
                return writer->write_stdout(output);
        }

        /* Writes raw bytes to stdout with no timestamp or level (Rust `log_raw`). */
        export [[nodiscard]] LogResult log_raw(const SimpleStringView bytes) {
                return log_get_writer()->write_stdout(bytes);
        }

        /*
         * Message style presets: the exact SGR sequences the Rust consoleutil
         * presets build (fg code after the attribute, selective reset).
         */
        constexpr SimpleStringView log_style_error_open = "\x1b[1;91m";
        constexpr SimpleStringView log_style_error_close = "\x1b[22;39m";
        constexpr SimpleStringView log_style_warning_open = "\x1b[1;93m";
        constexpr SimpleStringView log_style_warning_close = "\x1b[22;39m";
        constexpr SimpleStringView log_style_highlight_open = "\x1b[7m";
        constexpr SimpleStringView log_style_highlight_close = "\x1b[27m";
        constexpr SimpleStringView log_style_success_open = "\x1b[1;92m";
        constexpr SimpleStringView log_style_success_close = "\x1b[22;39m";
        constexpr SimpleStringView log_style_info_open = "\x1b[1;94m";
        constexpr SimpleStringView log_style_info_close = "\x1b[22;39m";
        constexpr SimpleStringView log_style_debug_open = "\x1b[96m";
        constexpr SimpleStringView log_style_debug_close = "\x1b[39m";

        /* Wraps message in the preset sequences, then logs it at level. */
        static LogResult log_styled(const LogLevel level, const SimpleStringView open, const SimpleStringView close,
                                    const SimpleStringView message) {
                FrameSink<512> styled;
                styled.write(open);
                styled.write(message);
                styled.write(close);
                return log_with_timestamp(level, styled.view());
        }

        /* Logs message at ERROR level with the error preset (bright red, bold). */
        export [[nodiscard]] LogResult log_error(const SimpleStringView message) {
                if (!is_enabled(LogLevel::error))
                        return LogResult::ok();
                return log_styled(LogLevel::error, log_style_error_open, log_style_error_close, message);
        }

        /* Logs message at ERROR level with the warning preset (bright yellow, bold). */
        export [[nodiscard]] LogResult log_warn(const SimpleStringView message) {
                if (!is_enabled(LogLevel::error))
                        return LogResult::ok();
                return log_styled(LogLevel::error, log_style_warning_open, log_style_warning_close, message);
        }

        /* Logs message at IRR level with the highlight preset (reverse). */
        export [[nodiscard]] LogResult log_irr(const SimpleStringView message) {
                if (!is_enabled(LogLevel::irr))
                        return LogResult::ok();
                return log_styled(LogLevel::irr, log_style_highlight_open, log_style_highlight_close, message);
        }

        /* Logs message at INFO level with the success preset (bright green, bold). */
        export [[nodiscard]] LogResult log_success(const SimpleStringView message) {
                if (!is_enabled(LogLevel::info))
                        return LogResult::ok();
                return log_styled(LogLevel::info, log_style_success_open, log_style_success_close, message);
        }

        /* Logs message at INFO level with the info preset (bright blue, bold). */
        export [[nodiscard]] LogResult log_info(const SimpleStringView message) {
                if (!is_enabled(LogLevel::info))
                        return LogResult::ok();
                return log_styled(LogLevel::info, log_style_info_open, log_style_info_close, message);
        }

        /* Logs message at DEBUG level with the debug preset (bright cyan). */
        export [[nodiscard]] LogResult log_debug(const SimpleStringView message) {
                if (!is_enabled(LogLevel::debug))
                        return LogResult::ok();
                return log_styled(LogLevel::debug, log_style_debug_open, log_style_debug_close, message);
        }
} // namespace embdr::cxxstd
