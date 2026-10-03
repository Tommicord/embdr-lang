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
#include <cstdint>
#include <cstddef>
#include <cinttypes>
#include <utility>

export module embdr.cxxstd.stringFormat;
import embdr.cxxstd.stringView;

namespace embdr::cxxstd {
        export template <typename T>
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
        consteval auto FormatTo() {}
        export template <unsigned int DstSize, typename CharTy, typename First, typename... Rest>
        constexpr auto FormatTo(CharTy (&dst)[DstSize], const CharTy* fmt, First&& first, Rest&&... rest) {}
        export template <unsigned int DstSize, typename CharTy, typename... Args>
        constexpr auto Parse(CharTy (&dst)[DstSize], const CharTy* fmt, Args&&... args) {}
        export template <unsigned int DstSize, typename CharTy>
        constexpr auto Parse(CharTy (&dst)[DstSize], const CharTy* fmt) {}
} // namespace embdr::cxxstd
