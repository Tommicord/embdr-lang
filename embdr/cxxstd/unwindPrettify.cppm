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
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#if !defined(_WIN32)
#        include <dlfcn.h>
#endif

export module embdr.cxxstd.unwindPrettify;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.unwind;
import embdr.cxxstd.stringView;

/*
 * Pretty-printing for stack unwind frames.
 *
 * Every helper writes into a caller-provided sink, so this module performs no
 * heap allocation and stays usable from async-signal-safe contexts (for
 * example writing into a fixed stack buffer inside a signal handler). Symbol
 * names are reported mangled: demangling would allocate.
 */
namespace embdr::cxxstd {
        export template <unsigned int N>
        class FrameSink {
            private:
                static_assert(N > 0);
                char _M_buf[N]{};
                size_t _M_len = 0;

            public:
                constexpr FrameSink() noexcept = default;

                [[nodiscard]] constexpr size_t size() const noexcept { return this->_M_len; }
                [[nodiscard]] constexpr bool empty() const noexcept { return this->_M_len == 0; }
                [[nodiscard]] constexpr const char* data() const noexcept { return this->_M_buf; }
                [[nodiscard]] constexpr SimpleStringView view() const noexcept {
                        return SimpleStringView(this->_M_buf, this->_M_len);
                }

                constexpr size_t write(const SimpleStringView text) noexcept {
                        const size_t avail = N - this->_M_len;
                        const size_t take = text.size() < avail ? text.size() : avail;
                        for (size_t i = 0; i < take; ++i)
                                this->_M_buf[this->_M_len + i] = text.data()[i];
                        this->_M_len += take;
                        return take;
                }

                /* Resets the sink to empty so its buffer can be reused for the next line. */
                constexpr void clear() noexcept { this->_M_len = 0; }
        };

        /*
         * Resolves the nearest symbol name for addr via dladdr.
         *
         * Returns the mangled symbol name when dladdr finds one, the module
         * path when the address belongs to a module without a symbol, and an
         * empty view when the address cannot be resolved at all.
         */
        export SimpleStringView resolve_symbol(const uintptr_t addr) noexcept {
#if defined(_WIN32)
                (void)addr;
                return SimpleStringView{};
#else
                Dl_info info{};
                if (::dladdr(reinterpret_cast<const void*>(addr), &info) == 0)
                        return SimpleStringView{};
                if (info.dli_sname != nullptr && info.dli_sname[0] != '\0')
                        return SimpleStringView(info.dli_sname);
                if (info.dli_fname != nullptr)
                        return SimpleStringView(info.dli_fname);
                return SimpleStringView{};
#endif
        }

        /* Resolves the module path for addr via dladdr; empty view when unresolved. */
        export SimpleStringView resolve_module(const uintptr_t addr) noexcept {
#if defined(_WIN32)
                (void)addr;
                return SimpleStringView{};
#else
                Dl_info info{};
                if (::dladdr(reinterpret_cast<const void*>(addr), &info) == 0)
                        return SimpleStringView{};
                if (info.dli_fname != nullptr)
                        return SimpleStringView(info.dli_fname);
                return SimpleStringView{};
#endif
        }

        inline constexpr char prettify_hex_digits[] = "0123456789abcdef";

        /* Writes value as decimal, left-padded with spaces to width columns. */
        export template <typename Sink>
        constexpr void write_uint(Sink& sink, uint64_t value, const size_t width) noexcept {
                char digits[20];
                size_t count = 0;
                do {
                        digits[count++] = static_cast<char>('0' + (value % 10));
                        value /= 10;
                } while (value != 0);
                char buf[24];
                size_t pos = 0;
                const size_t spaces = width > count ? width - count : 0;
                for (size_t i = 0; i < spaces && pos < sizeof(buf); ++i)
                        buf[pos++] = ' ';
                while (count > 0 && pos < sizeof(buf))
                        buf[pos++] = digits[--count];
                sink.write(SimpleStringView(buf, pos));
        }

        /*
         * Writes value as lowercase hex with at least min_digits digits
         * (zero-padded on the left), without a "0x" prefix.
         */
        export template <typename Sink>
        constexpr void write_hex(Sink& sink, const uint64_t value, const size_t min_digits) noexcept {
                char digits[16];
                size_t count = 0;
                uint64_t v = value;
                do {
                        digits[count++] = prettify_hex_digits[v & 0xF];
                        v >>= 4;
                } while (v != 0);
                char buf[16];
                size_t pos = 0;
                size_t pad = count > min_digits ? count : min_digits;
                if (pad > sizeof(buf))
                        pad = sizeof(buf);
                for (size_t i = count; i < pad; ++i)
                        buf[pos++] = '0';
                while (count > 0)
                        buf[pos++] = digits[--count];
                sink.write(SimpleStringView(buf, pos));
        }

        /*
         * Formats a single frame as:
         * "   {index:>3}: ip=0x{ip:016x} sp=0x{sp:016x}[+0x{offset:x}] [<(sym)>] [(mod)]"
         *
         * The offset and symbol parts appear only when the frame carries a
         * module base; the module part is resolved independently.
         */
        export template <typename Sink>
        void write_frame(Sink& sink, const UnwindFrame& frame, const size_t index) noexcept {
                sink.write("   ");
                write_uint(sink, index, 3);
                sink.write(": ip=0x");
                write_hex(sink, frame._M_ip, 16);
                sink.write(" sp=0x");
                write_hex(sink, frame._M_sp, 16);
                if (frame._M_module_base.has_value()) {
                        const uintptr_t base = frame._M_module_base.value();
                        sink.write("+0x");
                        write_hex(sink, frame._M_ip - base, 0);
                        const SimpleStringView symbol = resolve_symbol(frame._M_ip);
                        if (!symbol.empty()) {
                                sink.write(" <");
                                sink.write(symbol);
                                sink.write(">");
                        }
                }
                const SimpleStringView module = resolve_module(frame._M_ip);
                if (!module.empty()) {
                        sink.write(" (");
                        sink.write(module);
                        sink.write(")");
                }
        }

        /* Formats frames as '#' prefixed lines separated by '\n' (no trailing newline). */
        export template <typename Sink>
        void write_frames(Sink& sink, const std::span<const UnwindFrame> frames) noexcept {
                for (size_t i = 0; i < frames.size(); ++i) {
                        if (i > 0)
                                sink.write("\n");
                        sink.write("#");
                        write_frame(sink, frames[i], i);
                }
        }

        /* Writes the frame's symbol address as a 16-digit hex string. */
        export template <typename Sink>
        void write_symbol_address(Sink& sink, const UnwindFrame& frame) noexcept {
                write_hex(sink, frame.symbol_address(), 16);
        }
} // namespace embdr::cxxstd
