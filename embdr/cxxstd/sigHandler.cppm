/*
 * Copyright (c) 2026, Tommicord
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

module;
#include <atomic>
#include <cstdint>
#if !defined(_WIN32)
#        include <cerrno>
#        include <signal.h>
#        include <ucontext.h>
#        include <unistd.h>
#endif

export module embdr.cxxstd.sigHandler;
import embdr.cxxstd.unwind;
import embdr.cxxstd.unwindPrettify;
import embdr.cxxstd.stringView;

/*
 * Crash reporting signal handlers.
 *
 * Installing registers a SA_SIGINFO | SA_ONSTACK handler for the common
 * fault, termination and job-control signals. When one fires the handler
 * formats a short report (signal name, fault address, fault IP and a
 * backtrace) with the async-signal-safe helpers from
 * embdr.cxxstd.unwindPrettify, then restores the default disposition and
 * re-raises so the process still dies by the original signal. The whole
 * path runs on a static alternate signal stack, shares the unwinder's
 * reentrancy guard and never allocates. There is no Windows backend yet:
 * the install entry points report SigInstallErrorKind::unsupported.
 */
namespace embdr::cxxstd {
        export enum class SigInstallErrorKind { unsupported, syscall };

        export struct SigInstallError {
                SigInstallErrorKind _M_kind = SigInstallErrorKind::unsupported;
                const char* _M_op = "";
                int _M_errno_value = 0;
        };
        /* Maximum signals tracked by install/uninstall (23 used today). */
        inline constexpr size_t sig_max_saved = 0x20;

        /* Alternate signal stack installed by sig_install. */
        export inline constexpr size_t sig_default_alt_stack_size = 0x10000;

        /* Minimum accepted size for a user-supplied alternate signal stack. */
        export inline constexpr size_t sig_min_alt_stack_size = 0x2000;

        /* Maximum frames captured by a backtrace dump. */
        export inline constexpr size_t sig_dump_frames_max = 0x40;

        static std::atomic sig_installed{false};
        /* Returns a short symbolic name for sig, or "SIGUNKNOWN". */
        export [[nodiscard]] SimpleStringView sig_signal_name(const int sig) noexcept {
#if defined(_WIN32)
                (void)sig;
                return SimpleStringView("SIGUNKNOWN");
#else
                switch (sig) {
                        case SIGHUP:
                                return SimpleStringView("SIGHUP");
                        case SIGINT:
                                return SimpleStringView("SIGINT");
                        case SIGQUIT:
                                return SimpleStringView("SIGQUIT");
                        case SIGILL:
                                return SimpleStringView("SIGILL");
                        case SIGTRAP:
                                return SimpleStringView("SIGTRAP");
                        case SIGABRT:
                                return SimpleStringView("SIGABRT");
                        case SIGBUS:
                                return SimpleStringView("SIGBUS");
                        case SIGFPE:
                                return SimpleStringView("SIGFPE");
                        case SIGKILL:
                                return SimpleStringView("SIGKILL");
                        case SIGUSR1:
                                return SimpleStringView("SIGUSR1");
                        case SIGSEGV:
                                return SimpleStringView("SIGSEGV");
                        case SIGUSR2:
                                return SimpleStringView("SIGUSR2");
                        case SIGPIPE:
                                return SimpleStringView("SIGPIPE");
                        case SIGALRM:
                                return SimpleStringView("SIGALRM");
                        case SIGTERM:
                                return SimpleStringView("SIGTERM");
                        case SIGCHLD:
                                return SimpleStringView("SIGCHLD");
                        case SIGCONT:
                                return SimpleStringView("SIGCONT");
                        case SIGSTOP:
                                return SimpleStringView("SIGSTOP");
                        case SIGTSTP:
                                return SimpleStringView("SIGTSTP");
                        case SIGTTIN:
                                return SimpleStringView("SIGTTIN");
                        case SIGTTOU:
                                return SimpleStringView("SIGTTOU");
                        case SIGURG:
                                return SimpleStringView("SIGURG");
                        case SIGXCPU:
                                return SimpleStringView("SIGXCPU");
                        case SIGXFSZ:
                                return SimpleStringView("SIGXFSZ");
                        case SIGVTALRM:
                                return SimpleStringView("SIGVTALRM");
                        case SIGPROF:
                                return SimpleStringView("SIGPROF");
                        case SIGWINCH:
                                return SimpleStringView("SIGWINCH");
                        case SIGSYS:
                                return SimpleStringView("SIGSYS");
#        ifdef SIGSTKFLT
                        case SIGSTKFLT:
                                return SimpleStringView("SIGSTKFLT");
#        endif
#        ifdef SIGPWR
                        case SIGPWR:
                                return SimpleStringView("SIGPWR");
#        endif
#        ifdef SIGIO
                        case SIGIO:
                                return SimpleStringView("SIGIO");
#        endif
                        default:
                                return SimpleStringView("SIGUNKNOWN");
                }
#endif
        }

        /*
         * Returns true for synchronous fault signals (where si_addr is
         * meaningful and the faulting instruction pointer is worth
         * reporting).
         */
        export [[nodiscard]] bool sig_is_fault_signal(const int sig) noexcept {
#if defined(_WIN32)
                (void)sig;
                return false;
#else
                switch (sig) {
                        case SIGILL:
                        case SIGTRAP:
                        case SIGABRT:
                        case SIGBUS:
                        case SIGFPE:
                        case SIGSEGV:
                        case SIGSYS:
                        case SIGXCPU:
                        case SIGXFSZ:
                                return true;
#        ifdef SIGSTKFLT
                        case SIGSTKFLT:
                                return true;
#        endif
                        default:
                                return false;
                }
#endif
        }

        /*
         * Formats err into buf without a trailing newline, truncating to
         * cap - 1 bytes plus the terminator. Returns the byte count written
         * (excluding the terminator); buf must hold at least cap bytes.
         */
        export [[nodiscard]] size_t sig_error_message(const SigInstallError& err, char* const buf,
                                                      const size_t cap) noexcept {
                if (buf == nullptr || cap == 0)
                        return 0;
                FrameSink<256> line;
                if (err._M_kind == SigInstallErrorKind::unsupported)
                        line.write("<signal handling not supported on this target>");
                else {
                        line.write("`");
                        if (err._M_op != nullptr)
                                line.write(SimpleStringView(err._M_op));
                        line.write("` failed with errno ");
                        long long value = err._M_errno_value;
                        if (value < 0) {
                                line.write("-");
                                value = -value;
                        }
                        write_uint(line, static_cast<uint64_t>(value), 0);
                }
                const SimpleStringView text = line.view();
                const size_t count = text.size() < cap - 1 ? text.size() : cap - 1;
                for (size_t i = 0; i < count; ++i)
                        buf[i] = text.data()[i];
                buf[count] = '\0';
                return count;
        }

        /* True between a successful sig_install and the matching sig_uninstall. */
        export [[nodiscard]] bool sig_is_installed() noexcept { return sig_installed.load(std::memory_order_acquire); }

        /* True while executing inside a dump or signal handler. */
        export [[nodiscard]] bool sig_is_in_handler() noexcept { return unwind_in_handler(); }

#if !defined(_WIN32)
        /* Signals covered by sig_install (fault + termination + job control). */
        static int sig_handled_signals[] = {
            SIGILL,  SIGTRAP, SIGABRT, SIGBUS,  SIGFPE,  SIGSEGV, SIGSYS,  SIGXCPU,   SIGXFSZ,
            SIGHUP,  SIGINT,  SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2, SIGALRM, SIGVTALRM, SIGPROF,
#        ifdef SIGPWR
            SIGPWR,
#        endif
#        ifdef SIGIO
            SIGIO,
#        endif
            SIGTSTP, SIGTTIN, SIGTTOU,
        };

        static constexpr size_t sig_handled_count = sizeof(sig_handled_signals) / sizeof(sig_handled_signals[0]);
        static_assert(sig_handled_count <= sig_max_saved);

        /*
         * Saved previous dispositions for sig_uninstall. Touched only from
         * install/uninstall context (never from a signal handler); callers
         * must not race those entry points.
         */
        static int sig_saved_signals[sig_max_saved] = {};
        static struct sigaction sig_saved_actions[sig_max_saved] = {};
        static size_t sig_saved_count = 0;

        /* Default alternate signal stack (16-byte aligned, process lifetime). */
        alignas(16) static char sig_default_alt_stack[sig_default_alt_stack_size] = {};

        static void sig_save_prev(const int sig, const struct sigaction& act) noexcept {
                if (sig_saved_count < sig_max_saved) {
                        sig_saved_signals[sig_saved_count] = sig;
                        sig_saved_actions[sig_saved_count] = act;
                        ++sig_saved_count;
                }
        }

        /* Restores every saved disposition in reverse order and clears the table. */
        static void sig_restore_prev() noexcept {
                for (size_t i = sig_saved_count; i > 0; --i)
                        ::sigaction(sig_saved_signals[i - 1], &sig_saved_actions[i - 1], nullptr);
                sig_saved_count = 0;
        }

        static void sig_write_raw(const SimpleStringView text) noexcept {
                size_t off = 0;
                while (off < text.size()) {
                        const ssize_t written = ::write(STDERR_FILENO, text.data() + off, text.size() - off);
                        if (written > 0) {
                                off += static_cast<size_t>(written);
                                continue;
                        }
                        if (written < 0 && errno == EINTR)
                                continue;
                        break;
                }
        }

        /* Appends a newline, writes the line to stderr and resets the sink. */
        template <unsigned int N>
        static void sig_flush_line(FrameSink<N>& line) noexcept {
                line.write("\n");
                sig_write_raw(line.view());
                line.clear();
        }

        /* Resets sig to SIG_DFL, unblocks it and re-raises the default action. */
        static void sig_reset_and_raise(const int sig) noexcept {
                struct sigaction dfl{};
                dfl.sa_handler = SIG_DFL;
                ::sigaction(sig, &dfl, nullptr);

                sigset_t set;
                ::sigemptyset(&set);
                ::sigaddset(&set, sig);
                ::sigprocmask(SIG_UNBLOCK, &set, nullptr);
                ::raise(sig);
        }

        /* Extracts the faulting instruction pointer from a kernel ucontext. */
        static uintptr_t sig_fault_ip(void* const uc) noexcept {
                if (uc == nullptr)
                        return 0;
#        if defined(__linux__) && defined(__x86_64__)
                const ucontext_t* const ctx = static_cast<const ucontext_t*>(uc);
                return static_cast<uintptr_t>(ctx->uc_mcontext.gregs[REG_RIP]);
#        elif defined(__linux__) && defined(__aarch64__)
                const ucontext_t* const ctx = static_cast<const ucontext_t*>(uc);
                return static_cast<uintptr_t>(ctx->uc_mcontext.pc);
#        else
                return 0;
#        endif
        }

        static void sig_dump_frames() noexcept {
                UnwindFrame frames[sig_dump_frames_max];
                const size_t count = unwind_capture_frames(frames, sig_dump_frames_max);
                for (size_t i = 0; i < count; ++i) {
                        FrameSink<512> line;
                        write_frame(line, frames[i], i);
                        sig_flush_line(line);
                }
        }

        extern "C" void embdr_sig_handler(const int sig, siginfo_t* const info, void* const uc) noexcept {
                // Reentrancy: never wait, never nest; a held guard means a
                // recursive fault while another report was being written.
                if (!unwind_try_enter()) {
                        sig_write_raw(SimpleStringView("recursive signal during handling\n"));
                        sig_reset_and_raise(sig);
                        return;
                }
                const uintptr_t addr =
                    (sig_is_fault_signal(sig) && info != nullptr) ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;
                FrameSink<512> line;
                line.write("Caught signal ");
                write_uint(line, static_cast<uint64_t>(sig), 0);
                line.write(" (");
                line.write(sig_signal_name(sig));
                line.write(")");
                sig_flush_line(line);
                if (addr != 0) {
                        line.write("fault address: 0x");
                        write_hex(line, addr, 16);
                        sig_flush_line(line);
                }
                const uintptr_t ip = sig_fault_ip(uc);
                if (ip != 0) {
                        line.write("fault IP:      0x");
                        write_hex(line, ip, 16);
                        sig_flush_line(line);
                }
                line.write("Backtrace: ");
                sig_flush_line(line);
                sig_dump_frames();

                sig_reset_and_raise(sig);
                // Only reachable for job-control stops resumed with SIGCONT
                unwind_leave();
        }

        static bool sig_install_impl(SigInstallError* const err) noexcept {
                stack_t ss{};
                ss.ss_sp = sig_default_alt_stack;
                ss.ss_size = sizeof(sig_default_alt_stack);
                ss.ss_flags = 0;
                if (::sigaltstack(&ss, nullptr) != 0) {
                        if (err != nullptr)
                                *err = SigInstallError{SigInstallErrorKind::syscall, "sigaltstack", errno};
                        return false;
                }

                struct sigaction act{};
                act.sa_sigaction = embdr_sig_handler;
                act.sa_flags = SA_SIGINFO | SA_ONSTACK;
                ::sigemptyset(&act.sa_mask);
                for (size_t i = 0; i < sig_handled_count; ++i)
                        ::sigaddset(&act.sa_mask, sig_handled_signals[i]);
                // Fresh save table for this install attempt
                sig_saved_count = 0;
                for (size_t i = 0; i < sig_handled_count; ++i) {
                        struct sigaction old{};
                        if (::sigaction(sig_handled_signals[i], &act, &old) != 0) {
                                const int saved_errno = errno;
                                // Roll back to the dispositions seen before
                                // this attempt, not SIG_DFL: an unrelated
                                // handler must not be clobbered by a failure.
                                sig_restore_prev();
                                if (err != nullptr)
                                        *err = SigInstallError{SigInstallErrorKind::syscall, "sigaction", saved_errno};
                                return false;
                        }
                        sig_save_prev(sig_handled_signals[i], old);
                }
                return true;
        }

        /*
         * Installs the crash-reporting handlers and the default alternate
         * signal stack. Returns true when installed (calling this again
         * while installed is a no-op). On failure err receives the
         * operation name and OS error; see SigInstallError.
         */
        export [[nodiscard]] bool sig_install(SigInstallError* const err = nullptr) noexcept {
                if (err != nullptr)
                        *err = SigInstallError{};
                if (sig_installed.load(std::memory_order_acquire))
                        return true;
                if (!sig_install_impl(err))
                        return false;
                sig_installed.store(true, std::memory_order_release);
                return true;
        }

        /*
         * Restores the dispositions sig_install replaced. No-op when
         * nothing is installed; does not touch the alternate signal stack.
         */
        export [[nodiscard]] bool sig_uninstall() noexcept {
                if (!sig_installed.load(std::memory_order_acquire))
                        return true;
                sig_restore_prev();
                sig_installed.store(false, std::memory_order_release);
                return true;
        }

        /*
         * Sets buf as the alternate signal stack of the current thread.
         * buf must stay alive while installed; sizes below
         * sig_min_alt_stack_size are rejected with errno EINVAL.
         */
        export [[nodiscard]] bool sig_install_alt_stack(void* const buf, const size_t size,
                                                        SigInstallError* const err = nullptr) noexcept {
                if (err != nullptr)
                        *err = SigInstallError{};
                if (size < sig_min_alt_stack_size) {
                        if (err != nullptr)
                                *err = SigInstallError{SigInstallErrorKind::syscall, "sigaltstack:size", EINVAL};
                        return false;
                }
                stack_t ss{};
                ss.ss_sp = buf;
                ss.ss_size = size;
                ss.ss_flags = 0;
                if (::sigaltstack(&ss, nullptr) != 0) {
                        if (err != nullptr)
                                *err = SigInstallError{SigInstallErrorKind::syscall, "sigaltstack", errno};
                        return false;
                }
                return true;
        }

        /*
         * Writes a backtrace of the current thread to stderr. Silent when
         * a dump or handler is already in progress (shared reentrancy
         * guard); never throws and never allocates.
         */
        export void sig_dump_backtrace() noexcept {
                if (!unwind_try_enter())
                        return;
                sig_dump_frames();
                unwind_leave();
        }
#else
        export [[nodiscard]] bool sig_install(SigInstallError* const err = nullptr) noexcept {
                if (err != nullptr)
                        *err = SigInstallError{};
                return false;
        }

        export [[nodiscard]] bool sig_uninstall() noexcept { return true; }

        export [[nodiscard]] bool sig_install_alt_stack(void* const buf, const size_t size,
                                                        SigInstallError* const err = nullptr) noexcept {
                (void)buf;
                (void)size;
                if (err != nullptr)
                        *err = SigInstallError{};
                return false;
        }

        export void sig_dump_backtrace() noexcept {}
#endif
} // namespace embdr::cxxstd
