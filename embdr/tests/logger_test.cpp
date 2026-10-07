/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <regex>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

import embdr.cxxstd.logger;
import embdr.cxxstd.stringView;

using namespace embdr::cxxstd;

namespace {
        /* In-memory writer capturing both streams; the deterministic test sink. */
        struct CaptureWriter : public LogWriter {
                std::string out;
                std::string err;

                LogResult write_stdout(const SimpleStringView bytes) override {
                        this->out.append(bytes.data(), bytes.size());
                        return LogResult::ok();
                }
                LogResult write_stderr(const SimpleStringView bytes) override {
                        this->err.append(bytes.data(), bytes.size());
                        return LogResult::ok();
                }
                LogResult flush() override { return LogResult::ok(); }
        };

        /* Writer whose every stream write fails; drives the console fallback path. */
        struct FailingWriter : public LogWriter {
                LogResult write_stdout(const SimpleStringView) override {
                        return LogResult::err(LogError::io_error(5));
                }
                LogResult write_stderr(const SimpleStringView) override {
                        return LogResult::err(LogError::io_error(5));
                }
                LogResult flush() override { return LogResult::ok(); }
        };

        /* Mutex-guarded writer used by the concurrent logging test. */
        struct LockedWriter : public LogWriter {
                mutable std::mutex lock;
                std::string out;

                LogResult write_stdout(const SimpleStringView bytes) override {
                        const std::lock_guard<std::mutex> guard(this->lock);
                        this->out.append(bytes.data(), bytes.size());
                        return LogResult::ok();
                }
                LogResult write_stderr(const SimpleStringView bytes) override {
                        const std::lock_guard<std::mutex> guard(this->lock);
                        this->out.append(bytes.data(), bytes.size());
                        return LogResult::ok();
                }
                LogResult flush() override { return LogResult::ok(); }
        };

        struct ChildResult {
                std::string output;
                int status = 0;
        };

        /*
         * Runs body in a forked child with stdout and stderr both redirected
         * to a pipe and reaps it, capturing everything the body writes. The
         * body must not use REQUIRE: use exit codes for child-side failures.
         */
        template <typename Body>
        ChildResult run_child(Body&& body) {
                int fds[2];
                REQUIRE(::pipe(fds) == 0);
                const pid_t pid = ::fork();
                REQUIRE(pid >= 0);
                if (pid == 0) {
                        ::close(fds[0]);
                        if (::dup2(fds[1], STDOUT_FILENO) < 0)
                                ::_exit(90);
                        if (::dup2(fds[1], STDERR_FILENO) < 0)
                                ::_exit(91);
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

        /* Parks stdout and stderr on /dev/null for the lifetime of the object. */
        class StreamsSilencer {
            private:
                int _M_saved_out = -1;
                int _M_saved_err = -1;

            public:
                StreamsSilencer() noexcept {
                        const int devnull = ::open("/dev/null", O_WRONLY);
                        if (devnull >= 0) {
                                this->_M_saved_out = ::dup(STDOUT_FILENO);
                                this->_M_saved_err = ::dup(STDERR_FILENO);
                                if (this->_M_saved_out >= 0)
                                        (void)::dup2(devnull, STDOUT_FILENO);
                                if (this->_M_saved_err >= 0)
                                        (void)::dup2(devnull, STDERR_FILENO);
                                ::close(devnull);
                        }
                }

                ~StreamsSilencer() noexcept {
                        if (this->_M_saved_out >= 0) {
                                (void)::dup2(this->_M_saved_out, STDOUT_FILENO);
                                ::close(this->_M_saved_out);
                        }
                        if (this->_M_saved_err >= 0) {
                                (void)::dup2(this->_M_saved_err, STDERR_FILENO);
                                ::close(this->_M_saved_err);
                        }
                }

                StreamsSilencer(const StreamsSilencer&) = delete;
                StreamsSilencer& operator=(const StreamsSilencer&) = delete;
        };

        /* Runs body with both standard streams parked on /dev/null. */
        template <typename Body>
        void with_silenced_streams(Body&& body) {
                const StreamsSilencer silence;
                body();
        }

        /*
         * Restores global logger state on destruction so a failed REQUIRE in
         * one test cannot leak ansi/min-level/writer changes into the next.
         */
        struct AnsiGuard {
                ~AnsiGuard() { set_ansi_colors(AnsiColors::autoDetect); }
        };
        struct MinLevelGuard {
                ~MinLevelGuard() { set_min_log_level(LogLevel::DEBUG); }
        };
        struct WriterGuard {
                ~WriterGuard() { set_log_writer(default_log_writer()); }
        };

        /* True when text is exactly one "[<iso8601>]:<rest>" line. */
        bool timestamped_line(const std::string& text, const std::string& rest) {
                static const std::regex prefix(R"(^\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{9})?Z\]:)");
                std::smatch match;
                if (!std::regex_search(text, match, prefix))
                        return false;
                return text.substr(static_cast<size_t>(match.length())) == rest;
        }
} // namespace

TEST_CASE("log level discriminants and ordering", "[logger]") {
        REQUIRE(static_cast<uint8_t>(LogLevel::DEBUG) == 0);
        REQUIRE(static_cast<uint8_t>(LogLevel::INFO) == 1);
        REQUIRE(static_cast<uint8_t>(LogLevel::ERROR) == 2);
        REQUIRE(static_cast<uint8_t>(LogLevel::IRR) == 3);
        REQUIRE(LogLevel::DEBUG < LogLevel::INFO);
        REQUIRE(LogLevel::INFO < LogLevel::ERROR);
        REQUIRE(LogLevel::ERROR < LogLevel::IRR);
}

TEST_CASE("log level names", "[logger]") {
        REQUIRE(as_str(LogLevel::DEBUG) == "DEBUG");
        REQUIRE(as_str(LogLevel::INFO) == "INFO");
        REQUIRE(as_str(LogLevel::ERROR) == "ERROR");
        REQUIRE(as_str(LogLevel::IRR) == "IRR");
}

TEST_CASE("min log level filters enabled levels", "[logger]") {
        const MinLevelGuard guard;

        set_min_log_level(LogLevel::INFO);
        REQUIRE(min_log_level() == LogLevel::INFO);
        REQUIRE(!is_enabled(LogLevel::DEBUG));
        REQUIRE(is_enabled(LogLevel::INFO));
        REQUIRE(is_enabled(LogLevel::ERROR));
        REQUIRE(is_enabled(LogLevel::IRR));

        set_min_log_level(LogLevel::ERROR);
        REQUIRE(min_log_level() == LogLevel::ERROR);
        REQUIRE(!is_enabled(LogLevel::DEBUG));
        REQUIRE(!is_enabled(LogLevel::INFO));
        REQUIRE(is_enabled(LogLevel::ERROR));
        REQUIRE(is_enabled(LogLevel::IRR));

        set_min_log_level(LogLevel::DEBUG);
        REQUIRE(min_log_level() == LogLevel::DEBUG);
        REQUIRE(is_enabled(LogLevel::DEBUG));
}

TEST_CASE("log_error_message formats every LogError kind", "[logger]") {
        char buf[64];
        const size_t written = log_error_message(LogError::syscall_error(22), buf, sizeof(buf));
        REQUIRE(written == std::strlen(buf));
        REQUIRE(std::string(buf) == "syscall error: 22");

        REQUIRE(log_error_message(LogError::buffer_too_small(), buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "buffer too small");

        REQUIRE(log_error_message(LogError::encoding_error(), buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "encoding error");

        REQUIRE(log_error_message(LogError::handle_unavailable(), buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "console handle unavailable");

        REQUIRE(log_error_message(LogError::io_error(5), buf, sizeof(buf)) > 0);
        REQUIRE(std::string(buf) == "I/O error: 5");

        char small[8];
        REQUIRE(log_error_message(LogError::syscall_error(1234567), small, sizeof(small)) == sizeof(small) - 1);
        REQUIRE(small[sizeof(small) - 1] == '\0');
        REQUIRE(std::strlen(small) == sizeof(small) - 1);

        REQUIRE(log_error_message(LogError::buffer_too_small(), nullptr, sizeof(buf)) == 0);
        REQUIRE(log_error_message(LogError::buffer_too_small(), buf, 0) == 0);
}

TEST_CASE("log_with_timestamp accepts every level on the default writer", "[logger]") {
        bool ok_debug = false;
        bool ok_info = false;
        bool ok_error = false;
        bool ok_irr = false;
        with_silenced_streams([&] {
                ok_debug = log_with_timestamp(LogLevel::DEBUG, "debug message").has_value();
                ok_info = log_with_timestamp(LogLevel::INFO, "info message").has_value();
                ok_error = log_with_timestamp(LogLevel::ERROR, "error message").has_value();
                ok_irr = log_with_timestamp(LogLevel::IRR, "irrecoverable message").has_value();
        });
        REQUIRE(ok_debug);
        REQUIRE(ok_info);
        REQUIRE(ok_error);
        REQUIRE(ok_irr);
}

TEST_CASE("log_raw writes bytes without a timestamp", "[logger]") {
        bool ok = false;
        with_silenced_streams([&] { ok = log_raw("test raw message\n").has_value(); });
        REQUIRE(ok);
}

TEST_CASE("styled helpers log at their configured levels", "[logger]") {
        bool ok_error = false;
        bool ok_warn = false;
        bool ok_success = false;
        bool ok_info = false;
        bool ok_debug = false;
        bool ok_irr = false;
        with_silenced_streams([&] {
                ok_error = log_error("error message").has_value();
                ok_warn = log_warn("warning message").has_value();
                ok_success = log_success("success message").has_value();
                ok_info = log_info("info message").has_value();
                ok_debug = log_debug("debug message").has_value();
                ok_irr = log_irr("irrecoverable message").has_value();
        });
        REQUIRE(ok_error);
        REQUIRE(ok_warn);
        REQUIRE(ok_success);
        REQUIRE(ok_info);
        REQUIRE(ok_debug);
        REQUIRE(ok_irr);
}

TEST_CASE("custom writer captures stdout and stderr streams", "[logger]") {
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        REQUIRE(log_with_timestamp(LogLevel::INFO, "test message").has_value());
        REQUIRE(log_with_timestamp(LogLevel::ERROR, "error message").has_value());

        REQUIRE(!capture.out.empty());
        REQUIRE(!capture.err.empty());
        REQUIRE(capture.out.find("test message") != std::string::npos);
        REQUIRE(capture.err.find("error message") != std::string::npos);
        REQUIRE(capture.out.find("error message") == std::string::npos);
        REQUIRE(capture.err.find("test message") == std::string::npos);

        REQUIRE(default_log_writer() != nullptr);
        set_log_writer(default_log_writer());
        set_log_writer(nullptr);
}

TEST_CASE("filtered levels produce no output", "[logger]") {
        CaptureWriter capture;
        const WriterGuard guard;
        const MinLevelGuard levels;
        set_log_writer(&capture);
        set_min_log_level(LogLevel::ERROR);

        REQUIRE(log_with_timestamp(LogLevel::DEBUG, "filtered debug").has_value());
        REQUIRE(log_with_timestamp(LogLevel::INFO, "filtered info").has_value());
        REQUIRE(capture.out.empty());
        REQUIRE(capture.err.empty());

        REQUIRE(log_with_timestamp(LogLevel::ERROR, "visible error").has_value());
        REQUIRE(!capture.err.empty());
        REQUIRE(capture.err.find("visible error") != std::string::npos);

        // Rust log_raw is not gated by the level filter.
        REQUIRE(log_raw("raw bytes").has_value());
        REQUIRE(capture.out.find("raw bytes") != std::string::npos);
}

TEST_CASE("plain line is [<timestamp>]:<LEVEL> <message>", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        REQUIRE(log_with_timestamp(LogLevel::INFO, "hello world").has_value());
        REQUIRE(capture.out.find('\x1b') == std::string::npos);
        REQUIRE(timestamped_line(capture.out, "INFO hello world\n"));

        REQUIRE(log_with_timestamp(LogLevel::ERROR, "bad news").has_value());
        REQUIRE(timestamped_line(capture.err, "ERROR bad news\n"));
}

TEST_CASE("forced ANSI colors wrap the line", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::enabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        REQUIRE(log_with_timestamp(LogLevel::ERROR, "boom").has_value());
        REQUIRE(capture.err.starts_with("\x1b[32m["));
        REQUIRE(capture.err.ends_with("boom\n\x1b[0m"));

        REQUIRE(log_with_timestamp(LogLevel::INFO, "glow").has_value());
        REQUIRE(capture.out.starts_with("\x1b[32m["));
        REQUIRE(capture.out.ends_with("glow\n\x1b[0m"));
}

TEST_CASE("styled helpers wrap the message in preset SGR sequences", "[logger]") {
        const AnsiGuard ansi;
        // Disable line coloring so only the message presets remain.
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        REQUIRE(log_error("bang").has_value());
        REQUIRE(capture.err.find("\x1b[1;91mbang\x1b[22;39m") != std::string::npos);
        REQUIRE(timestamped_line(capture.err, "ERROR \x1b[1;91mbang\x1b[22;39m\n"));

        capture.err.clear();
        REQUIRE(log_warn("careful").has_value());
        REQUIRE(capture.err.find("\x1b[1;93mcareful\x1b[22;39m") != std::string::npos);

        capture.err.clear();
        REQUIRE(log_irr("doom").has_value());
        REQUIRE(capture.err.find("\x1b[7mdoom\x1b[27m") != std::string::npos);
        REQUIRE(capture.err.find("]:IRR ") != std::string::npos);

        REQUIRE(log_success("yay").has_value());
        REQUIRE(capture.out.find("\x1b[1;92myay\x1b[22;39m") != std::string::npos);

        capture.out.clear();
        REQUIRE(log_info("fyi").has_value());
        REQUIRE(capture.out.find("\x1b[1;94mfyi\x1b[22;39m") != std::string::npos);

        capture.out.clear();
        REQUIRE(log_debug("details").has_value());
        REQUIRE(capture.out.find("\x1b[96mdetails\x1b[39m") != std::string::npos);
        REQUIRE(capture.out.find("]:DEBUG ") != std::string::npos);
}

TEST_CASE("overlong messages are truncated to the line buffer", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        const std::string long_message(600, 'x');
        const auto result =
            log_with_timestamp(LogLevel::INFO, SimpleStringView(long_message.data(), long_message.size()));
        REQUIRE(result.has_value());
        REQUIRE(capture.out.size() == 512);
        REQUIRE(capture.out.back() == '\n');
}

TEST_CASE("a full colored line truncates instead of failing", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::enabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        const std::string long_message(600, 'y');
        const auto result =
            log_with_timestamp(LogLevel::INFO, SimpleStringView(long_message.data(), long_message.size()));
        REQUIRE(result.has_value());
        REQUIRE(capture.out.size() == 512);
        REQUIRE(capture.out.starts_with("\x1b[32m["));
        REQUIRE(capture.out.ends_with("y\n\x1b[0m"));
}

TEST_CASE("the default writer streams to the real stdio in a child", "[logger]") {
        const ChildResult child = run_child([] {
                set_ansi_colors(AnsiColors::autoDetect);
                set_min_log_level(LogLevel::DEBUG);
                if (!log_with_timestamp(LogLevel::INFO, "child stdout line").has_value())
                        ::_exit(92);
                if (!log_with_timestamp(LogLevel::ERROR, "child stderr line").has_value())
                        ::_exit(93);
                if (!log_raw("child raw line\n").has_value())
                        ::_exit(94);
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output.find("]:INFO child stdout line") != std::string::npos);
        REQUIRE(child.output.find("]:ERROR child stderr line") != std::string::npos);
        REQUIRE(child.output.find("child raw line") != std::string::npos);
        // Auto mode against a pipe: neither stream is a terminal, so no colors.
        REQUIRE(child.output.find('\x1b') == std::string::npos);
}

TEST_CASE("log calls succeed even when the installed writer fails", "[logger]") {
        FailingWriter failing;
        const WriterGuard guard;
        set_log_writer(&failing);
        const uint64_t dropped_before = log_dropped_count();
        bool info_ok = false;
        bool error_ok = false;
        bool raw_ok = false;
        with_silenced_streams([&] {
                info_ok = log_with_timestamp(LogLevel::INFO, "info via console fallback").has_value();
                error_ok = log_error("error via console fallback").has_value();
                raw_ok = log_raw("raw via console fallback\n").has_value();
        });
        REQUIRE(info_ok);
        REQUIRE(error_ok);
        REQUIRE(raw_ok);
        // The console fallback absorbed every failure, so nothing was dropped.
        REQUIRE(log_dropped_count() == dropped_before);
}

TEST_CASE("the circular queue delivers messages in order", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        constexpr int count = 64; // twice log_queue_capacity, so the ring wraps around
        for (int i = 0; i < count; ++i) {
                const std::string text = "ordered " + std::to_string(i);
                REQUIRE(log_with_timestamp(LogLevel::INFO, SimpleStringView(text.data(), text.size())).has_value());
        }
        size_t pos = 0;
        for (int i = 0; i < count; ++i) {
                const std::string text = "ordered " + std::to_string(i);
                const size_t found = capture.out.find(text, pos);
                REQUIRE(found != std::string::npos);
                pos = found + text.size();
        }
        REQUIRE(std::count(capture.out.begin(), capture.out.end(), '\n') == count);
}

TEST_CASE("inline draining keeps the queue from overflowing", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        const uint64_t overflow_before = log_overflow_count();
        for (int i = 0; i < 100; ++i)
                REQUIRE(log_with_timestamp(LogLevel::INFO, "burst").has_value());
        log_process_queue(); // explicit drain of an empty queue is a no-op
        REQUIRE(log_overflow_count() == overflow_before);
        REQUIRE(std::count(capture.out.begin(), capture.out.end(), '\n') == 100);
}

TEST_CASE("variadic overloads format only the user message", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        CaptureWriter capture;
        const WriterGuard guard;
        set_log_writer(&capture);

        REQUIRE(log_info("answer={} base={:#x}", 42, 255).has_value());
        REQUIRE(timestamped_line(capture.out, "INFO \x1b[1;94manswer=42 base=0xff\x1b[22;39m\n"));

        REQUIRE(log_with_timestamp(LogLevel::ERROR, "code={}", 13).has_value());
        REQUIRE(timestamped_line(capture.err, "ERROR code=13\n"));

        REQUIRE(log_debug("items={} label={}", 2, "two").has_value());
        REQUIRE(capture.out.find("]:DEBUG \x1b[96mitems=2 label=two\x1b[39m\n") != std::string::npos);

        REQUIRE(log_warn("warn {}", 9).has_value());
        REQUIRE(capture.err.find("\x1b[1;93mwarn 9\x1b[22;39m") != std::string::npos);

        REQUIRE(log_success("done {}", "now").has_value());
        REQUIRE(capture.out.find("\x1b[1;92mdone now\x1b[22;39m") != std::string::npos);

        REQUIRE(log_irr("lost {}", 0).has_value());
        REQUIRE(capture.err.find("]:IRR \x1b[7mlost 0\x1b[27m\n") != std::string::npos);

        // A formatted message longer than the line buffer truncates but still logs.
        capture.out.clear();
        REQUIRE(log_info("{:0>600}", 1).has_value());
        REQUIRE(capture.out.size() == 512);
        REQUIRE(capture.out.back() == '\n');
}

TEST_CASE("concurrent producers never lose a message", "[logger]") {
        const AnsiGuard ansi;
        set_ansi_colors(AnsiColors::disabled);
        LockedWriter locked;
        const WriterGuard guard;
        set_log_writer(&locked);

        constexpr int thread_count = 4;
        constexpr int per_thread = 64;
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (int t = 0; t < thread_count; ++t) {
                threads.emplace_back([t] {
                        for (int i = 0; i < per_thread; ++i)
                                (void)log_info("thread {} item {}", t, i);
                });
        }
        for (std::thread& thread : threads)
                thread.join();
        // Process anything a producer could not drain after losing the token race.
        log_process_queue();

        const std::lock_guard<std::mutex> locked_guard(locked.lock);
        REQUIRE(std::count(locked.out.begin(), locked.out.end(), '\n') == thread_count * per_thread);
}
