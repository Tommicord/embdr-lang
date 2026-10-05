/*
 * Copyright(c) 2026 Tommicord
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
#include <cstdint>
#include <cstddef>
#include <cinttypes>
#include <utility>

export module embdr.cxxstd.stringFormat;
import embdr.cxxstd.stringView;

namespace embdr::cxxstd {
        template <typename T>
        struct FormatArg {
                T v;
                constexpr FormatArg(T val) : v(val) {}
        };
        template <unsigned int N, typename CharTy>
        struct FormatArg<const CharTy[N]> {
                using value_type = CharTy*;
                BasicTemplatedStringView<N, CharTy> v;
                constexpr FormatArg(const CharTy (&str)[N]) noexcept : v(str) {}
                constexpr operator value_type() const noexcept { return v.data(); }
        };
        template <typename T>
        FormatArg(T) -> FormatArg<T>;
        template <unsigned int N, typename CharTy>
        FormatArg(const CharTy (&)[N]) -> FormatArg<const CharTy[N]>;

        export template <const BasicTemplatedStringView Fmt, FormatArg... Args>
        consteval auto format() {}
        export template <unsigned int DstSize, typename CharTy, typename First, typename... Rest>
        constexpr auto format(CharTy (&dst)[DstSize], const CharTy* fmt, First&& first, Rest&&... rest) {}
        export template <unsigned int DstSize, typename CharTy, typename... Args>
        constexpr auto parse(CharTy (&dst)[DstSize], const CharTy* fmt, Args&&... args) {}
        export template <unsigned int DstSize, typename CharTy>
        constexpr auto parse(CharTy (&dst)[DstSize], const CharTy* fmt) {}
} // namespace embdr::cxxstd
