/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

import embdr.cxxstd.consoleUtil;
import embdr.cxxstd.stringView;
import embdr.cxxstd.unwindPrettify;

using namespace embdr::cxxstd;

namespace {
        struct ChildResult {
                std::string output;
                int status = 0;
        };

        /*
         * Runs body in a forked child with fd redirected to a pipe and reaps it,
         * capturing everything the body writes to that descriptor. The body must
         * not use REQUIRE: use exit codes for child-side failures.
         */
        template <typename Body>
        ChildResult run_child(const int fd, Body&& body) {
                int fds[2];
                REQUIRE(::pipe(fds) == 0);
                const pid_t pid = ::fork();
                REQUIRE(pid >= 0);
                if (pid == 0) {
                        ::close(fds[0]);
                        if (::dup2(fds[1], fd) < 0)
                                ::_exit(90);
                        ::close(fds[1]);
                        body();
                        ::_exit(0);
                }
                ::close(fds[1]);
                ChildResult result;
                char buf[4096];
                for (;;) {
                        const ssize_t n = ::read(fds[0], buf, sizeof(buf));
                        if (n > 0) {
                                result.output.append(buf, static_cast<size_t>(n));
                                continue;
                        }
                        if (n < 0 && errno == EINTR)
                                continue;
                        break;
                }
                ::close(fds[0]);
                pid_t waited = -1;
                do {
                        waited = ::waitpid(pid, &result.status, 0);
                } while (waited < 0 && errno == EINTR);
                REQUIRE(waited == pid);
                return result;
        }

        /* Restores an environment variable on destruction. */
        class EnvVarGuard {
            private:
                std::string _M_name;
                std::string _M_value;
                bool _M_had = false;

            public:
                explicit EnvVarGuard(const char* const name) : _M_name(name) {
                        const char* const current = ::getenv(name);
                        this->_M_had = current != nullptr;
                        if (this->_M_had)
                                this->_M_value = current;
                }

                ~EnvVarGuard() {
                        if (this->_M_had)
                                (void)::setenv(this->_M_name.c_str(), this->_M_value.c_str(), 1);
                        else
                                (void)::unsetenv(this->_M_name.c_str());
                }

                EnvVarGuard(const EnvVarGuard&) = delete;
                EnvVarGuard& operator=(const EnvVarGuard&) = delete;

                void set(const char* const value) const { (void)::setenv(this->_M_name.c_str(), value, 1); }
                void clear() const { (void)::unsetenv(this->_M_name.c_str()); }
        };
} // namespace

TEST_CASE("console_strip_ansi strips CSI sequences like the Rust reference", "[consoleUtil]") {
        FrameSink<128> out;
        REQUIRE(console_strip_ansi("\x1b[31mRed\x1b[0m", out) == 3);
        REQUIRE(out.view() == "Red");

        out.clear();
        REQUIRE(console_strip_ansi("Normal text", out) == 11);
        REQUIRE(out.view() == "Normal text");

        out.clear();
        REQUIRE(console_strip_ansi("\x1b[1;32mBold Green\x1b[0m", out) == 10);
        REQUIRE(out.view() == "Bold Green");

        out.clear();
        REQUIRE(console_strip_ansi("\x1b[2J\x1b[H", out) == 0);
        REQUIRE(out.empty());

        // A CSI without a final byte is dropped along with its parameters.
        out.clear();
        REQUIRE(console_strip_ansi("\x1b[31Red", out) == 2);
        REQUIRE(out.view() == "ed");

        // A non-'[' byte after ESC ends the escape and is itself dropped.
        out.clear();
        REQUIRE(console_strip_ansi("\x1bXtail", out) == 4);
        REQUIRE(out.view() == "tail");

        // ESC at the very end appends nothing beyond the pending text.
        out.clear();
        REQUIRE(console_strip_ansi("ab\x1b", out) == 2);
        REQUIRE(console_strip_ansi("", out) == 0);
        REQUIRE(out.view() == "ab");
}

TEST_CASE("console_strip_ansi passes multi-byte text through untouched", "[consoleUtil]") {
        FrameSink<64> out;
        const SimpleStringView input("\x1b[0m\xe2\x80\x94\xc3\xa9", 9);
        REQUIRE(console_strip_ansi(input, out) == 5);
        REQUIRE(out.view() == SimpleStringView("\xe2\x80\x94\xc3\xa9", 5));
}

TEST_CASE("console_strip_ansi truncates into a fixed sink", "[consoleUtil]") {
        FrameSink<4> small;
        REQUIRE(console_strip_ansi("abcdefgh", small) == 4);
        REQUIRE(small.view() == "abcd");
        REQUIRE(small.size() == 4);

        FrameSink<4> escaped;
        REQUIRE(console_strip_ansi("\x1b[31m12345", escaped) == 4);
        REQUIRE(escaped.view() == "1234");
        REQUIRE(escaped.size() == 4);
}

TEST_CASE("console ANSI state flags round-trip", "[consoleUtil]") {
        console_reset_ansi_state();
        REQUIRE(!console_is_ansi_forced());
        REQUIRE(!console_is_ansi_disabled());

        console_force_ansi(true);
        REQUIRE(console_is_ansi_forced());
        REQUIRE(!console_is_ansi_disabled());
        console_force_ansi(false);
        REQUIRE(!console_is_ansi_forced());

        console_disable_ansi(true);
        REQUIRE(console_is_ansi_disabled());
        REQUIRE(!console_is_ansi_forced());

        console_reset_ansi_state();
        REQUIRE(!console_is_ansi_forced());
        REQUIRE(!console_is_ansi_disabled());

        // Enabling VT processing must not flip the force/disable flags.
        console_init_ansi_support();
        REQUIRE(!console_is_ansi_forced());
        REQUIRE(!console_is_ansi_disabled());
}

TEST_CASE("console_supports_ansi honors flags and COLORTERM", "[consoleUtil]") {
        const EnvVarGuard colorterm("COLORTERM");
        console_reset_ansi_state();
        colorterm.clear();
        REQUIRE(!console_supports_ansi());

        colorterm.set("truecolor");
        REQUIRE(console_supports_ansi());
        colorterm.set("24bit");
        REQUIRE(console_supports_ansi());
        colorterm.set("TrueColor");
        REQUIRE(console_supports_ansi());
        colorterm.set("yes");
        REQUIRE(!console_supports_ansi());

        colorterm.set("truecolor");
        console_disable_ansi(true);
        REQUIRE(!console_supports_ansi());

        // The force flag is checked before the disable flag.
        console_force_ansi(true);
        REQUIRE(console_supports_ansi());

        console_reset_ansi_state();
        REQUIRE(console_supports_ansi());

        colorterm.clear();
        REQUIRE(!console_supports_ansi());
        console_reset_ansi_state();
}

TEST_CASE("ConsoleResult carries ok and error states", "[consoleUtil]") {
        const ConsoleResult ok = ConsoleResult::ok();
        REQUIRE(ok.has_value());
        REQUIRE(!ok.has_error());
        REQUIRE(static_cast<bool>(ok));

        const ConsoleError detail{ConsoleErrorKind::bufferTooSmall, 7};
        const ConsoleResult err = ConsoleResult::err(detail);
        REQUIRE(!err.has_value());
        REQUIRE(err.has_error());
        REQUIRE(!static_cast<bool>(err));
        REQUIRE(err.error()._M_kind == ConsoleErrorKind::bufferTooSmall);
        REQUIRE(err.error()._M_code == 7);
}

TEST_CASE("console_error_message formats every error kind", "[consoleUtil]") {
        char buf[64];
        const ConsoleError syscall_error{ConsoleErrorKind::syscallError, 22};
        size_t written = console_error_message(syscall_error, buf, sizeof(buf));
        REQUIRE(std::string(buf) == "syscall error: 22");
        REQUIRE(written == std::strlen(buf));

        written = console_error_message(ConsoleError{ConsoleErrorKind::syscallError, -22}, buf, sizeof(buf));
        REQUIRE(std::string(buf) == "syscall error: -22");
        REQUIRE(written == std::strlen(buf));

        REQUIRE(console_error_message(ConsoleError{ConsoleErrorKind::ioError, 5}, buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "I/O error");
        REQUIRE(console_error_message(ConsoleError{ConsoleErrorKind::encodingError, 0}, buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "encoding error");
        REQUIRE(console_error_message(ConsoleError{ConsoleErrorKind::handleUnavailable, 0}, buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "console handle unavailable");
        REQUIRE(console_error_message(ConsoleError{ConsoleErrorKind::bufferTooSmall, 0}, buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "buffer too small");

        char small[8];
        REQUIRE(console_error_message(syscall_error, small, sizeof(small)) == sizeof(small) - 1);
        REQUIRE(small[sizeof(small) - 1] == '\0');
        REQUIRE(std::strlen(small) == sizeof(small) - 1);

        REQUIRE(console_error_message(syscall_error, nullptr, sizeof(buf)) == 0);
        REQUIRE(console_error_message(syscall_error, buf, 0) == 0);
}

TEST_CASE("console_write_stderr delivers bytes to the descriptor", "[consoleUtil]") {
        const ChildResult child = run_child(STDERR_FILENO, [] {
                const SimpleStringView line("embdr console\n");
                if (!console_write_stderr(line).has_value())
                        ::_exit(91);
                const uint8_t payload[] = {'o', 'k', '\n'};
                if (!console_write_stderr(std::span<const uint8_t>(payload, sizeof(payload))).has_value())
                        ::_exit(92);
                if (console_write_stderr(std::span<const uint8_t>{}).has_error())
                        ::_exit(93);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output == "embdr console\nok\n");
}

TEST_CASE("console_write_stderr loops over partial writes", "[consoleUtil]") {
        const ChildResult child = run_child(STDERR_FILENO, [] {
                static char chunk[1000];
                for (size_t i = 0; i < sizeof(chunk); ++i)
                        chunk[i] = 'a';
                const std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(chunk), sizeof(chunk));
                for (int i = 0; i < 70; ++i)
                        if (!console_write_stderr(span).has_value())
                                ::_exit(91);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output.size() == 70000);
        REQUIRE(child.output.find_first_not_of('a') == std::string::npos);
}

TEST_CASE("console_write_stderr reports a closed descriptor", "[consoleUtil]") {
        const ChildResult child = run_child(STDERR_FILENO, [] {
                ::close(STDERR_FILENO);
                const ConsoleResult result = console_write_stderr(SimpleStringView("lost"));
                if (result.has_value())
                        ::_exit(91);
                if (result.error()._M_kind != ConsoleErrorKind::ioError)
                        ::_exit(92);
                if (result.error()._M_code != EBADF)
                        ::_exit(93);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output.empty());
}

TEST_CASE("console_write_stdout delivers bytes to the descriptor", "[consoleUtil]") {
        const ChildResult child = run_child(STDOUT_FILENO, [] {
                const SimpleStringView text("stdout payload\n");
                if (!console_write_stdout(text).has_value())
                        ::_exit(91);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output == "stdout payload\n");
}

TEST_CASE("console terminal width cache clamps updates to 255", "[consoleUtil]") {
        console_update_terminal_width(80);
        REQUIRE(console_terminal_width() == 80);
        console_update_terminal_width(300);
        REQUIRE(console_terminal_width() == 255);
        console_update_terminal_width(65535);
        REQUIRE(console_terminal_width() == 255);
        console_update_terminal_width(0);
        REQUIRE(console_terminal_width() == 0);
        console_update_terminal_width(100);
        REQUIRE(console_terminal_width() == 100);
        console_update_terminal_width(80);
        REQUIRE(console_terminal_width() == 80);
}

TEST_CASE("console_detect_terminal_width prefers the COLUMNS environment variable", "[consoleUtil]") {
        const EnvVarGuard columns("COLUMNS");
        columns.set("132");
        const ChildResult child = run_child(STDOUT_FILENO, [] {
                console_update_terminal_width(99);
                if (console_detect_terminal_width() != 132)
                        ::_exit(91);
                if (console_terminal_width() != 132)
                        ::_exit(92);
                // The probed value is reported whole even though the cache clamps.
                (void)::setenv("COLUMNS", "400", 1);
                if (console_detect_terminal_width() != 400)
                        ::_exit(93);
                if (console_terminal_width() != 255)
                        ::_exit(94);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
}

TEST_CASE("console_detect_terminal_width falls back when COLUMNS is not a u16", "[consoleUtil]") {
        const EnvVarGuard columns("COLUMNS");
        // The child's stdout is the capture pipe, so TIOCGWINSZ fails here.
        columns.set("not-a-number");
        const ChildResult first = run_child(STDOUT_FILENO, [] {
                console_update_terminal_width(99);
                if (console_detect_terminal_width() != 80)
                        ::_exit(91);
                if (console_terminal_width() != 99)
                        ::_exit(92);
        });
        REQUIRE(WIFEXITED(first.status));
        REQUIRE(WEXITSTATUS(first.status) == 0);

        columns.set("70000");
        const ChildResult second = run_child(STDOUT_FILENO, [] {
                console_update_terminal_width(99);
                if (console_detect_terminal_width() != 80)
                        ::_exit(91);
                if (console_terminal_width() != 99)
                        ::_exit(92);
        });
        REQUIRE(WIFEXITED(second.status));
        REQUIRE(WEXITSTATUS(second.status) == 0);
}

TEST_CASE("console terminal height cache clamps updates to 255", "[consoleUtil]") {
        console_update_terminal_height(24);
        REQUIRE(console_terminal_height() == 24);
        console_update_terminal_height(300);
        REQUIRE(console_terminal_height() == 255);
        console_update_terminal_height(65535);
        REQUIRE(console_terminal_height() == 255);
        console_update_terminal_height(0);
        REQUIRE(console_terminal_height() == 0);
        console_update_terminal_height(50);
        REQUIRE(console_terminal_height() == 50);
        console_update_terminal_height(24);
        REQUIRE(console_terminal_height() == 24);
}

TEST_CASE("console_detect_terminal_height prefers the LINES environment variable", "[consoleUtil]") {
        const EnvVarGuard lines("LINES");
        lines.set("45");
        const ChildResult child = run_child(STDOUT_FILENO, [] {
                console_update_terminal_height(33);
                if (console_detect_terminal_height() != 45)
                        ::_exit(91);
                if (console_terminal_height() != 45)
                        ::_exit(92);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
}

TEST_CASE("console_detect_terminal_height falls back when LINES is invalid", "[consoleUtil]") {
        const EnvVarGuard lines("LINES");
        lines.set("abc");
        const ChildResult child = run_child(STDOUT_FILENO, [] {
                console_update_terminal_height(33);
                if (console_detect_terminal_height() != 24)
                        ::_exit(91);
                if (console_terminal_height() != 33)
                        ::_exit(92);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
}
