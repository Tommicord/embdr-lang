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
#include <cstddef>
#include <cstdint>

export module embdr.cxxstd.stringNumber;

namespace embdr::cxxstd {
        export enum class NumberBase : unsigned int { binary = 2, octal = 8, decimal = 10, hexadecimal = 16 };

        export constexpr unsigned long long uitoa(char* buff, unsigned long long value, const unsigned int base,
                                                  const bool upper_case) {
                if (buff == nullptr)
                        return 0;
                const unsigned int radix = (base < 2 || base > 36) ? 10u : base;
                char tmp[64];
                unsigned int len = 0;
                do {
                        const unsigned int digit = static_cast<unsigned int>(value % radix);
                        if (digit < 10)
                                tmp[len++] = static_cast<char>('0' + digit);
                        else
                                tmp[len++] = static_cast<char>((upper_case ? 'A' : 'a') + digit - 10);
                        value /= radix;
                } while (value != 0);
                for (unsigned int i = 0; i < len; ++i)
                        buff[i] = tmp[len - i - 1];
                buff[len] = '\0';
                return len;
        }
        export constexpr unsigned long long uitoa(char* buff, const unsigned long long value, const unsigned int base) {
                return uitoa(buff, value, base, false);
        }
        export constexpr unsigned long long uitoa(char* buff, const unsigned long long value) {
                return uitoa(buff, value, 10, false);
        }
        export constexpr unsigned long long itoa(char* buff, const long long value, const unsigned int base,
                                                 const bool upper_case) {
                if (buff == nullptr)
                        return 0;
                else {
                        if (value < 0) {
                                buff[0] = '-';
                                return 1ull + uitoa(buff + 1, 0ull - static_cast<unsigned long long>(value), base, upper_case);
                        }
                }
                return uitoa(buff, static_cast<unsigned long long>(value), base, upper_case);
        }
        export constexpr unsigned long long itoa(char* buff, const long long value, const unsigned int base) {
                return itoa(buff, value, base, false);
        }
        export constexpr unsigned long long itoa(char* buff, const long long value) {
                return itoa(buff, value, 10, false);
        }
        export constexpr unsigned long long uitoa(char* buff, const unsigned long long value, const NumberBase base,
                                                  const bool upper_case) {
                return uitoa(buff, value, static_cast<unsigned int>(base), upper_case);
        }
        export constexpr unsigned long long uitoa(char* buff, const unsigned long long value, const NumberBase base) {
                return uitoa(buff, value, base, false);
        }
        export constexpr unsigned long long itoa(char* buff, const long long value, const NumberBase base,
                                                 const bool upper_case) {
                return itoa(buff, value, static_cast<unsigned int>(base), upper_case);
        }
        export constexpr unsigned long long itoa(char* buff, const long long value, const NumberBase base) {
                return itoa(buff, value, base, false);
        }
} // namespace embdr::cxxstd
