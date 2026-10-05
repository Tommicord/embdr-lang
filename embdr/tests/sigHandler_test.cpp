/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

import embdr.cxxstd.sigHandler;
import embdr.cxxstd.stringView;
import embdr.cxxstd.unwind;

using namespace embdr::cxxstd;

namespace {
        struct ChildResult {
                std::string output;
                int status = 0;
                bool signaled = false;
                int term_signal = 0;
        };

        /*
         * Runs body in a forked child with stderr redirected to a pipe and
         * reaps it, capturing everything the body (or a signal handler it
         * installs) writes. The body must not use REQUIRE: use exit codes
         * for child-side failures.
         */
        template <typename Body>
        ChildResult run_child(Body&& body) {
                int fds[2];
                REQUIRE(::pipe(fds) == 0);
                const pid_t pid = ::fork();
                REQUIRE(pid >= 0);
                if (pid == 0) {
                        ::close(fds[0]);
                        if (::dup2(fds[1], STDERR_FILENO) < 0)
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
                result.signaled = WIFSIGNALED(result.status);
                result.term_signal = result.signaled ? WTERMSIG(result.status) : 0;
                return result;
        }

        /* Uninstalls on destruction so a failed REQUIRE cannot leak handlers. */
        struct InstallGuard {
                ~InstallGuard() {
                        if (sig_is_installed())
                                (void)sig_uninstall();
                }
        };

        /* Redirects stderr to /dev/null for the lifetime of the object. */
        class StderrSilencer {
            private:
                int _M_saved = -1;

            public:
                StderrSilencer() noexcept {
                        const int devnull = ::open("/dev/null", O_WRONLY);
                        this->_M_saved = ::dup(STDERR_FILENO);
                        if (this->_M_saved >= 0 && devnull >= 0)
                                (void)::dup2(devnull, STDERR_FILENO);
                        if (devnull >= 0)
                                ::close(devnull);
                }

                ~StderrSilencer() noexcept {
                        if (this->_M_saved >= 0) {
                                (void)::dup2(this->_M_saved, STDERR_FILENO);
                                ::close(this->_M_saved);
                        }
                }

                StderrSilencer(const StderrSilencer&) = delete;
                StderrSilencer& operator=(const StderrSilencer&) = delete;
        };
} // namespace

TEST_CASE("sig_signal_name reports known and unknown signals", "[sigHandler]") {
        REQUIRE(sig_signal_name(SIGTERM) == "SIGTERM");
        REQUIRE(sig_signal_name(SIGSEGV) == "SIGSEGV");
        REQUIRE(sig_signal_name(SIGINT) == "SIGINT");
        REQUIRE(sig_signal_name(INT_MAX) == "SIGUNKNOWN");
}

TEST_CASE("sig_is_fault_signal classifies signals", "[sigHandler]") {
        REQUIRE(sig_is_fault_signal(SIGILL));
        REQUIRE(sig_is_fault_signal(SIGTRAP));
        REQUIRE(sig_is_fault_signal(SIGABRT));
        REQUIRE(sig_is_fault_signal(SIGBUS));
        REQUIRE(sig_is_fault_signal(SIGFPE));
        REQUIRE(sig_is_fault_signal(SIGSEGV));
        REQUIRE(sig_is_fault_signal(SIGSYS));
        REQUIRE(sig_is_fault_signal(SIGXCPU));
        REQUIRE(sig_is_fault_signal(SIGXFSZ));
#ifdef SIGSTKFLT
        REQUIRE(sig_is_fault_signal(SIGSTKFLT));
#endif
        REQUIRE(!sig_is_fault_signal(SIGTERM));
        REQUIRE(!sig_is_fault_signal(SIGINT));
        REQUIRE(!sig_is_fault_signal(SIGUSR1));
        REQUIRE(!sig_is_fault_signal(SIGKILL));
        REQUIRE(!sig_is_fault_signal(INT_MAX));
}

TEST_CASE("sig_error_message formats syscall and unsupported errors", "[sigHandler]") {
        char buf[96];
        const SigInstallError syscall_error{SigInstallErrorKind::syscall, "sigaction", 22};
        const size_t written = sig_error_message(syscall_error, buf, sizeof(buf));
        REQUIRE(std::string(buf) == "`sigaction` failed with errno 22");
        REQUIRE(written == std::strlen(buf));

        const SigInstallError unsupported{};
        (void)sig_error_message(unsupported, buf, sizeof(buf));
        REQUIRE(std::string(buf) == "<signal handling not supported on this target>");

        char small[8];
        REQUIRE(sig_error_message(syscall_error, small, sizeof(small)) == sizeof(small) - 1);
        REQUIRE(small[sizeof(small) - 1] == '\0');
        REQUIRE(std::strlen(small) == sizeof(small) - 1);
}

TEST_CASE("sig_install lifecycle installs and restores dispositions", "[sigHandler]") {
        struct sigaction before{};
        REQUIRE(::sigaction(SIGUSR1, nullptr, &before) == 0);
        REQUIRE(!sig_is_installed());

        InstallGuard guard;
        REQUIRE(sig_install());
        REQUIRE(sig_is_installed());
        // Installing again is a documented no-op.
        REQUIRE(sig_install());
        REQUIRE(sig_is_installed());

        struct sigaction during{};
        REQUIRE(::sigaction(SIGUSR1, nullptr, &during) == 0);
        REQUIRE(during.sa_sigaction != before.sa_sigaction);
        REQUIRE((during.sa_flags & SA_SIGINFO) != 0);

        // sig_install also selects the default alternate signal stack.
        stack_t ss{};
        REQUIRE(::sigaltstack(nullptr, &ss) == 0);
        REQUIRE(ss.ss_size == sig_default_alt_stack_size);
        REQUIRE((ss.ss_flags & SS_DISABLE) == 0);

        REQUIRE(sig_uninstall());
        REQUIRE(!sig_is_installed());
        // Uninstalling with nothing installed is a no-op.
        REQUIRE(sig_uninstall());

        struct sigaction after{};
        REQUIRE(::sigaction(SIGUSR1, nullptr, &after) == 0);
        REQUIRE(after.sa_sigaction == before.sa_sigaction);
}

TEST_CASE("sig_install_alt_stack rejects buffers below the minimum", "[sigHandler]") {
        alignas(16) static char small[sig_min_alt_stack_size / 2];
        SigInstallError err;
        REQUIRE(!sig_install_alt_stack(small, sizeof(small), &err));
        REQUIRE(err._M_kind == SigInstallErrorKind::syscall);
        REQUIRE(SimpleStringView(err._M_op) == "sigaltstack:size");
        REQUIRE(err._M_errno_value == EINVAL);
}

TEST_CASE("sig_install_alt_stack accepts a large enough buffer", "[sigHandler]") {
        alignas(16) static char big[sig_min_alt_stack_size * 2];
        SigInstallError err;
        REQUIRE(sig_install_alt_stack(big, sizeof(big), &err));

        stack_t ss{};
        REQUIRE(::sigaltstack(nullptr, &ss) == 0);
        REQUIRE(ss.ss_sp == static_cast<void*>(big));
        REQUIRE(ss.ss_size == sizeof(big));
        REQUIRE((ss.ss_flags & SS_DISABLE) == 0);
}

TEST_CASE("sig_dump_backtrace writes frames in a child process", "[sigHandler]") {
        const ChildResult child = run_child([] { sig_dump_backtrace(); });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output.find("ip=0x") != std::string::npos);
        REQUIRE(child.output.find("sp=0x") != std::string::npos);
}

TEST_CASE("sig_dump_backtrace releases the reentrancy guard", "[sigHandler]") {
        {
                const StderrSilencer silence;
                sig_dump_backtrace();
                REQUIRE(!sig_is_in_handler());
                sig_dump_backtrace();
        }
        REQUIRE(!sig_is_in_handler());
}

TEST_CASE("recursive dumps are suppressed while the guard is held", "[sigHandler]") {
        const ChildResult child = run_child([] {
                if (!unwind_try_enter())
                        ::_exit(91);
                sig_dump_backtrace();
                unwind_leave();
        });
        REQUIRE(WIFEXITED(child.status));
        REQUIRE(WEXITSTATUS(child.status) == 0);
        REQUIRE(child.output.empty());

        REQUIRE(!sig_is_in_handler());
        REQUIRE(unwind_try_enter());
        REQUIRE(sig_is_in_handler());
        // A nested enter fails instead of waiting; the guard is shared.
        REQUIRE(!unwind_try_enter());
        unwind_leave();
        REQUIRE(!sig_is_in_handler());
}

TEST_CASE("the handler reports a fatal signal and re-raises it", "[sigHandler]") {
        const ChildResult child = run_child([] {
                struct rlimit core_limit{};
                core_limit.rlim_cur = 0;
                core_limit.rlim_max = 0;
                (void)::setrlimit(RLIMIT_CORE, &core_limit);
                if (!sig_install())
                        ::_exit(92);
                ::raise(SIGSEGV);
                ::_exit(93);
        });
        REQUIRE(child.signaled);
        REQUIRE(child.term_signal == SIGSEGV);
        REQUIRE(child.output.find("Caught signal " + std::to_string(SIGSEGV) + " (SIGSEGV)") != std::string::npos);
        REQUIRE(child.output.find("fault IP:      0x") != std::string::npos);
        REQUIRE(child.output.find("Backtrace: ") != std::string::npos);
        REQUIRE(child.output.find("ip=0x") != std::string::npos);
}
