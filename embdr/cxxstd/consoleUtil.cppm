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
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#if defined(_WIN32)
#        include <io.h>
#else
#        include <sys/ioctl.h>
#        include <unistd.h>
#endif

export module embdr.cxxstd.consoleUtil;
import embdr.cxxstd.stringView;
import embdr.cxxstd.unwindPrettify;

/*
 * Console utilities: ANSI state flags, escape-sequence stripping, raw
 * writes to the standard streams and terminal geometry detection.
 *
 * Nothing here allocates or throws: the write paths are plain ::write
 * retry loops so they stay usable from async-signal-safe contexts (see
 * embdr.cxxstd.sigHandler), and string processing runs into
 * caller-provided fixed sinks. The geometry cache is a u8 per axis, so
 * updates are clamped to 255 while detection reports the full probed
 * value. There is no Windows console backend yet: init_ansi_support is a
 * no-op and the write helpers use the CRT file descriptors directly.
 */
namespace embdr::cxxstd {
        export enum class ConsoleErrorKind {
                syscallError,
                ioError,
                encodingError,
                handleUnavailable,
                bufferTooSmall,
        };

        /* Payload of a failed console operation; _M_code carries errno-style detail. */
        export struct ConsoleError {
                ConsoleErrorKind _M_kind = ConsoleErrorKind::handleUnavailable;
                int _M_code = 0;
        };

        /*
         * Formats err into buf without a trailing newline, truncating to cap - 1
         * bytes plus the terminator. Returns the byte count written (excluding the
         * terminator); buf must hold at least cap bytes.
         */
        export [[nodiscard]] size_t console_error_message(const ConsoleError& err, char* const buf,
                                                          const size_t cap) noexcept {
                if (buf == nullptr || cap == 0)
                        return 0;
                FrameSink<64> line;
                if (err._M_kind == ConsoleErrorKind::syscallError) {
                        line.write("syscall error: ");
                        long long value = err._M_code;
                        if (value < 0) {
                                line.write("-");
                                value = -value;
                        }
                        write_uint(line, static_cast<uint64_t>(value), 0);
                } else if (err._M_kind == ConsoleErrorKind::ioError)
                        line.write("I/O error");
                else if (err._M_kind == ConsoleErrorKind::encodingError)
                        line.write("encoding error");
                else if (err._M_kind == ConsoleErrorKind::handleUnavailable)
                        line.write("console handle unavailable");
                else
                        line.write("buffer too small");
                const SimpleStringView text = line.view();
                const size_t count = text.size() < cap - 1 ? text.size() : cap - 1;
                for (size_t i = 0; i < count; ++i)
                        buf[i] = text.data()[i];
                buf[count] = '\0';
                return count;
        }

        /* Outcome of a console operation: ok, or the ConsoleError that stopped it. */
        export class ConsoleResult {
                bool _M_ok;
                ConsoleError _M_error;

                constexpr ConsoleResult(const bool ok, const ConsoleError& error) noexcept :
                    _M_ok(ok), _M_error(error) {}

            public:
                constexpr ConsoleResult() noexcept : _M_ok(true), _M_error{} {}

                [[nodiscard]] static constexpr ConsoleResult ok() noexcept { return ConsoleResult{}; }
                [[nodiscard]] static constexpr ConsoleResult err(const ConsoleError& error) noexcept {
                        return ConsoleResult(false, error);
                }

                [[nodiscard]] constexpr bool has_value() const noexcept { return this->_M_ok; }
                [[nodiscard]] constexpr bool has_error() const noexcept { return !this->_M_ok; }
                [[nodiscard]] constexpr explicit operator bool() const noexcept { return this->_M_ok; }
                [[nodiscard]] constexpr const ConsoleError& error() const noexcept { return this->_M_error; }
        };

        /* POSIX descriptor numbering for the standard streams. */
        inline constexpr int console_stdout_fd = 1;
        inline constexpr int console_stderr_fd = 2;

        static std::atomic<bool> _S_console_ansi_forced{false};
        static std::atomic<bool> _S_console_ansi_disabled{false};
        static std::atomic<uint8_t> _S_console_width_cache{80};
        static std::atomic<uint8_t> _S_console_height_cache{24};

        /* Writes every byte of bytes to fd, retrying on EINTR and short writes. */
        static ConsoleResult console_write_all(const int fd, const std::span<const uint8_t> bytes) noexcept {
                size_t off = 0;
                while (off < bytes.size()) {
#if defined(_WIN32)
                        const int written =
                            ::_write(fd, bytes.data() + off, static_cast<unsigned int>(bytes.size() - off));
#else
                        const ssize_t written = ::write(fd, bytes.data() + off, bytes.size() - off);
#endif
                        if (written > 0) {
                                off += static_cast<size_t>(written);
                                continue;
                        }
                        if (written < 0 && errno == EINTR)
                                continue;
                        // write() never completes a non-empty buffer with 0; if it ever
                        // did, report the pending errno (0 when there is none).
                        return ConsoleResult::err(ConsoleError{ConsoleErrorKind::ioError, written < 0 ? errno : 0});
                }
                return ConsoleResult::ok();
        }

        /* Case-insensitive ASCII equality so COLORTERM probing stays allocation-free. */
        static bool console_ascii_ieq(const SimpleStringView lhs, const SimpleStringView rhs) noexcept {
                if (lhs.size() != rhs.size())
                        return false;
                for (size_t i = 0; i < lhs.size(); ++i) {
                        char a = lhs.data()[i];
                        char b = rhs.data()[i];
                        if (a >= 'A' && a <= 'Z')
                                a = static_cast<char>(a - 'A' + 'a');
                        if (b >= 'A' && b <= 'Z')
                                b = static_cast<char>(b - 'A' + 'a');
                        if (a != b)
                                return false;
                }
                return true;
        }

        /* Parses text as a u16 the way Rust does: optional '+', digits only, whole string. */
        static bool console_parse_u16(const char* const text, uint16_t& out) noexcept {
                if (text == nullptr)
                        return false;
                size_t i = 0;
                if (text[i] == '+')
                        ++i;
                if (text[i] == '\0')
                        return false;
                uint32_t value = 0;
                for (; text[i] != '\0'; ++i) {
                        const char digit = text[i];
                        if (digit < '0' || digit > '9')
                                return false;
                        value = value * 10 + static_cast<uint32_t>(digit - '0');
                        if (value > 0xFFFF)
                                return false;
                }
                out = static_cast<uint16_t>(value);
                return true;
        }

        /* Force ANSI output even when the terminal is not detected as capable. */
        export void console_force_ansi(const bool enable) noexcept {
                _S_console_ansi_forced.store(enable, std::memory_order_relaxed);
        }

        /* Disable ANSI output globally (checked after the force flag). */
        export void console_disable_ansi(const bool enable) noexcept {
                _S_console_ansi_disabled.store(enable, std::memory_order_relaxed);
        }

        /* True while console_force_ansi(true) is in effect. */
        export [[nodiscard]] bool console_is_ansi_forced() noexcept {
                return _S_console_ansi_forced.load(std::memory_order_relaxed);
        }

        /* True while console_disable_ansi(true) is in effect. */
        export [[nodiscard]] bool console_is_ansi_disabled() noexcept {
                return _S_console_ansi_disabled.load(std::memory_order_relaxed);
        }

        /* Reset both flags so output follows auto-detection again. */
        export void console_reset_ansi_state() noexcept {
                _S_console_ansi_forced.store(false, std::memory_order_relaxed);
                _S_console_ansi_disabled.store(false, std::memory_order_relaxed);
        }

        /*
         * Enables virtual-terminal processing on Windows. This is a no-op on
         * Unix, and the Win32 path is not ported yet, so it never changes
         * state today.
         */
        export void console_init_ansi_support() noexcept {}

        /*
         * True when ANSI output is forced, or when it is neither disabled nor
         * undetected: COLORTERM=truecolor/24bit marks a capable terminal.
         */
        export [[nodiscard]] bool console_supports_ansi() noexcept {
                if (_S_console_ansi_forced.load(std::memory_order_relaxed))
                        return true;
                if (_S_console_ansi_disabled.load(std::memory_order_relaxed))
                        return false;
                const char* const colorterm = std::getenv("COLORTERM");
                if (colorterm != nullptr) {
                        const SimpleStringView value(colorterm);
                        if (console_ascii_ieq(value, "truecolor") || console_ascii_ieq(value, "24bit"))
                                return true;
                }
                return false;
        }

        /*
         * Appends input to out with every CSI escape sequence removed and
         * returns the byte count appended. The scan is byte-oriented: UTF-8
         * payload bytes never equal the ASCII CSI terminators, so this matches
         * a char-wise scan of the same input. Sinks that fill up truncate
         * silently.
         */
        export template <typename Sink>
        [[nodiscard]] size_t console_strip_ansi(const SimpleStringView input, Sink& out) noexcept {
                size_t written = 0;
                bool in_escape = false;
                bool in_csi = false;
                for (size_t i = 0; i < input.size(); ++i) {
                        const char ch = input.data()[i];
                        if (!in_escape) {
                                if (ch == '\x1b')
                                        in_escape = true;
                                else
                                        written += out.write(ch);
                        } else if (!in_csi) {
                                if (ch == '[')
                                        in_csi = true;
                                else {
                                        in_escape = false;
                                        in_csi = false;
                                }
                        } else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '~') {
                                in_escape = false;
                                in_csi = false;
                        }
                }
                return written;
        }

        /* Writes every byte of bytes to stdout; async-signal-safe. */
        export [[nodiscard]] ConsoleResult console_write_stdout(const std::span<const uint8_t> bytes) noexcept {
                return console_write_all(console_stdout_fd, bytes);
        }

        /* Convenience overload writing the bytes of text to stdout. */
        export [[nodiscard]] ConsoleResult console_write_stdout(const SimpleStringView text) noexcept {
                return console_write_all(console_stdout_fd, std::span<const uint8_t>(
                                                                 reinterpret_cast<const uint8_t*>(text.data()),
                                                                 text.size()));
        }

        /* Writes every byte of bytes to stderr; async-signal-safe. */
        export [[nodiscard]] ConsoleResult console_write_stderr(const std::span<const uint8_t> bytes) noexcept {
                return console_write_all(console_stderr_fd, bytes);
        }

        /* Convenience overload writing the bytes of text to stderr. */
        export [[nodiscard]] ConsoleResult console_write_stderr(const SimpleStringView text) noexcept {
                return console_write_all(console_stderr_fd, std::span<const uint8_t>(
                                                                 reinterpret_cast<const uint8_t*>(text.data()),
                                                                 text.size()));
        }

        /* Cached terminal width in columns (starts at 80). */
        export [[nodiscard]] uint16_t console_terminal_width() noexcept {
                return _S_console_width_cache.load(std::memory_order_relaxed);
        }

        /* Stores width in the cache, clamping to the u8 range (255 columns). */
        export void console_update_terminal_width(const uint16_t width) noexcept {
                const uint16_t clamped = width < 255 ? width : 255;
                _S_console_width_cache.store(static_cast<uint8_t>(clamped), std::memory_order_relaxed);
        }

        /*
         * Detects width from $COLUMNS, then the TIOCGWINSZ ioctl on stdout,
         * updating the cache. Returns 80 when neither reports a value; the
         * cache is left untouched on that fallback.
         */
        export [[nodiscard]] uint16_t console_detect_terminal_width() noexcept {
                uint16_t columns = 0;
                if (console_parse_u16(std::getenv("COLUMNS"), columns)) {
                        console_update_terminal_width(columns);
                        return columns;
                }
#if !defined(_WIN32)
                struct winsize ws{};
                if (::ioctl(console_stdout_fd, TIOCGWINSZ, &ws) >= 0 && ws.ws_col > 0) {
                        console_update_terminal_width(ws.ws_col);
                        return ws.ws_col;
                }
#endif
                return 80;
        }

        /* Cached terminal height in lines (starts at 24). */
        export [[nodiscard]] uint16_t console_terminal_height() noexcept {
                return _S_console_height_cache.load(std::memory_order_relaxed);
        }

        /* Stores height in the cache, clamping to the u8 range (255 lines). */
        export void console_update_terminal_height(const uint16_t height) noexcept {
                const uint16_t clamped = height < 255 ? height : 255;
                _S_console_height_cache.store(static_cast<uint8_t>(clamped), std::memory_order_relaxed);
        }

        /*
         * Detects height from $LINES, then the TIOCGWINSZ ioctl on stdout,
         * updating the cache. Returns 24 when neither reports a value; the
         * cache is left untouched on that fallback.
         */
        export [[nodiscard]] uint16_t console_detect_terminal_height() noexcept {
                uint16_t lines = 0;
                if (console_parse_u16(std::getenv("LINES"), lines)) {
                        console_update_terminal_height(lines);
                        return lines;
                }
#if !defined(_WIN32)
                struct winsize ws{};
                if (::ioctl(console_stdout_fd, TIOCGWINSZ, &ws) >= 0 && ws.ws_row > 0) {
                        console_update_terminal_height(ws.ws_row);
                        return ws.ws_row;
                }
#endif
                return 24;
        }
} // namespace embdr::cxxstd
