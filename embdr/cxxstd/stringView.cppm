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
#include <type_traits>
#include <limits>
#include <string>

export module embdr.cxxstd.stringView;
import embdr.cxxstd.memoryMaybe;

template <typename T>
constexpr int compare(T n1, T n2) noexcept {
        using traits = std::numeric_limits<int>;
        const auto diff = n1 - n2;
        if (diff > traits::max())
                return traits::max();
        if (diff < traits::min())
                return traits::min();
        return static_cast<int>(diff);
}
template <typename CharTy, unsigned int N>
constexpr auto sv_check(const unsigned long size, unsigned long pos, CharTy (&s)[N]) {
        if (pos > size)
                return embdr::cxxstd::make_err<unsigned long>(s);
        else
                return embdr::cxxstd::make_some(pos);
}

constexpr size_t sv_limit(const unsigned long size, const unsigned long pos, const unsigned long off) noexcept {
        const bool testoff = off < size - pos;
        return testoff ? off : size - pos;
}

namespace embdr::cxxstd {
        export template <typename CharTy, typename Traits = std::char_traits<std::remove_cv_t<CharTy>>>
        class BasicStringView {
                static_assert(!std::is_array_v<CharTy>);
                static_assert(std::is_trivially_copyable_v<CharTy> &&
                              std::is_trivially_default_constructible_v<CharTy> && std::is_standard_layout_v<CharTy>);
                static_assert(std::is_same_v<std::remove_cv_t<CharTy>, typename Traits::char_type>);

            public:
                using traits_type = Traits;
                using value_type = CharTy;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using const_iterator = const value_type*;
                using iterator = const_iterator;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                static constexpr size_type not_found = static_cast<size_type>(-1);

                constexpr BasicStringView() noexcept : len{0}, str{nullptr} {}

                constexpr BasicStringView(const BasicStringView&) noexcept = default;
                constexpr BasicStringView(const CharTy* str) noexcept : len{traits_type::length(str)}, str{str} {}
                constexpr BasicStringView(const CharTy* str, const size_type len) noexcept : len{len}, str{str} {}
                template <std::contiguous_iterator Iter, std::sized_sentinel_for<Iter> End>
                        requires std::same_as<std::iter_value_t<Iter>, CharTy> && (!std::convertible_to<End, size_type>)
                constexpr BasicStringView(Iter first, End last) noexcept(noexcept(last - first)) :
                    len(last - first), str(std::to_address(first)) {}
                template <typename Rg, typename DRg = std::remove_cvref_t<Rg>>
                        requires(!std::is_same_v<DRg, BasicStringView>) && std::ranges::contiguous_range<Rg> &&
                                    std::ranges::sized_range<Rg> &&
                                    std::is_same_v<std::ranges::range_value_t<Rg>, CharTy> &&
                                    (!std::is_convertible_v<Rg, const CharTy*>) &&
                                    (!requires(DRg& d) { d.operator BasicStringView(); })
                constexpr explicit BasicStringView(Rg&& r) noexcept(noexcept(std::ranges::size(r)) &&
                                                                    noexcept(std::ranges::data(r))) :
                    len(std::ranges::size(r)), str(std::ranges::data(r)) {}
                BasicStringView(nullptr_t) = delete;
                constexpr BasicStringView& operator=(const BasicStringView&) noexcept = default;

                [[nodiscard]]
                constexpr const_iterator begin() const noexcept {
                        return this->str;
                }

                [[nodiscard]]
                constexpr const_iterator end() const noexcept {
                        return this->str + this->len;
                }

                [[nodiscard]]
                constexpr const_iterator cbegin() const noexcept {
                        return this->str;
                }

                [[nodiscard]]
                constexpr const_iterator cend() const noexcept {
                        return this->str + this->len;
                }
                [[nodiscard]]
                constexpr size_type size() const noexcept {
                        return this->len;
                }

                [[nodiscard]]
                constexpr size_type max_size() const noexcept {
                        return (not_found - sizeof(size_type) - sizeof(void*)) / sizeof(value_type) / 4;
                }

                [[nodiscard]]
                constexpr bool empty() const noexcept {
                        return this->len == 0;
                }
                [[nodiscard]]
                constexpr const_reference operator[](size_type pos) const noexcept {
                        return *(this->str + pos);
                }

                [[nodiscard]]
                constexpr const_reference at(size_type pos) const {
                        return *(this->str + pos);
                }

                [[nodiscard]]
                constexpr const_reference front() const noexcept {
                        return *this->str;
                }

                [[nodiscard]]
                constexpr const_reference back() const noexcept {
                        return *(this->str + this->len - 1);
                }

                [[nodiscard]]
                constexpr const_pointer data() const noexcept {
                        return this->str;
                }

                constexpr void remove_prefix(size_type n) noexcept {
                        this->str += n;
                        this->len -= n;
                }

                constexpr void remove_suffix(size_type n) noexcept { this->len -= n; }

                constexpr void swap(BasicStringView& sv) noexcept {
                        auto tmp = *this;
                        *this = sv;
                        sv = tmp;
                }

                constexpr size_type copy(CharTy* str, size_type n, size_type pos = 0) const noexcept {
                        auto pos_checked = sv_check(this->size(), pos, "embdr::cxxstd::BasicStringView::copy");
                        if (!pos_checked.has_value())
                                return 0;
                        pos = pos_checked.value();
                        const size_type rlen = std::min<size_t>(n, len - pos);
                        traits_type::copy(str, data() + pos, rlen);
                        return rlen;
                }

                [[nodiscard]]
                constexpr BasicStringView substr(size_type pos = 0, const size_type n = not_found) const noexcept {
                        auto pos_checked = sv_check(this->size(), pos, "embdr::cxxstd::BasicStringView::substr");
                        if (!pos_checked.has_value())
                                return BasicStringView();
                        pos = pos_checked.value();
                        const size_type rlen = std::min<size_type>(n, len - pos);
                        return BasicStringView{str + pos, rlen};
                }

                [[nodiscard]]
                constexpr int32_t compare(BasicStringView str) const noexcept {
                        const size_type rlen = std::min(this->len, str.len);
                        int ret = traits_type::compare(this->str, str.str, rlen);
                        if (ret == 0)
                                ret = compare(this->len, str.len);
                        return ret;
                }

                [[nodiscard]]
                constexpr int32_t compare(const size_type pos, const size_type n1, BasicStringView str) const noexcept {
                        return this->substr(pos, n1).compare(str);
                }

                [[nodiscard]]
                constexpr int32_t compare(const size_type pos1, const size_type n1, BasicStringView str,
                                          const size_type pos2, const size_type n2) const noexcept {
                        return this->substr(pos1, n1).compare(str.substr(pos2, n2));
                }

                [[nodiscard]]
                constexpr int32_t compare(const CharTy* str) const noexcept {
                        return this->compare(BasicStringView{str});
                }

                [[nodiscard]]
                constexpr int32_t compare(const size_type pos, const size_type n1, const CharTy* str) const noexcept {
                        return this->substr(pos, n1).compare(BasicStringView{str});
                }

                [[nodiscard]]
                constexpr int32_t compare(const size_type pos, const size_type n1, const CharTy* str,
                                          size_type n2) const noexcept {
                        return this->substr(pos, n1).compare(BasicStringView(str, n2));
                }
                [[nodiscard]]
                constexpr bool starts_with(BasicStringView x) const noexcept {
                        return len >= x.len && traits_type::compare(str, x.str, x.len) == 0;
                }

                [[nodiscard]]
                constexpr bool starts_with(CharTy x) const noexcept {
                        return !this->empty() && traits_type::eq(this->front(), x);
                }

                [[nodiscard]]
                constexpr bool starts_with(const CharTy* x) const noexcept {
                        return this->starts_with(BasicStringView(x));
                }

                [[nodiscard]]
                constexpr bool ends_with(const BasicStringView x) const noexcept {
                        const auto len = this->size();
                        const auto xlen = x.size();
                        return len >= xlen && traits_type::compare(end() - xlen, x.data(), xlen) == 0;
                }

                [[nodiscard]]
                constexpr bool ends_with(const CharTy x) const noexcept {
                        return !this->empty() && traits_type::eq(this->back(), x);
                }

                [[nodiscard]]
                constexpr bool ends_with(const CharTy* x) const noexcept {
                        return this->ends_with(BasicStringView(x));
                }
                [[nodiscard]]
                constexpr bool contains(BasicStringView x) const noexcept {
                        return this->find(x) != not_found;
                }

                [[nodiscard]]
                constexpr bool contains(CharTy x) const noexcept {
                        for (size_type i = 0; i < this->len; ++i) {
                                if (this->str[i] == x)
                                        return true;
                        }
                        return false;
                }

                [[nodiscard]]
                constexpr bool contains(const CharTy* x) const noexcept {
                        return this->find(x) != not_found;
                }

                [[nodiscard]]
                constexpr size_type find(BasicStringView str, const size_type pos = 0) const noexcept {
                        return this->find(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type find(const CharTy* str, size_type pos = 0) const noexcept {
                        const size_type len = traits_type::length(str);
                        if (pos > this->len)
                                return not_found;
                        if (len == 0)
                                return pos;
                        const CharTy elem0 = str[0];
                        const CharTy* const data = this->str;
                        const CharTy* first = data + pos;
                        const CharTy* const last = data + this->len;
                        size_type remaining = this->len - pos;

                        while (remaining >= len) {
                                first = traits_type::find(first, remaining - len + 1, elem0);
                                if (!first)
                                        return not_found;
                                if (traits_type::compare(first, str, len) == 0)
                                        return first - data;
                                remaining = last - ++first;
                        }
                        return not_found;
                }

                [[nodiscard]]
                constexpr size_type rfind(BasicStringView str, size_type pos = not_found) const noexcept {
                        return this->rfind(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type rfind(const CharTy* str, size_type pos = not_found) const noexcept {
                        return this->rfind(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(BasicStringView str, size_type pos = 0) const noexcept {
                        return this->find_first_of(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(CharTy c, size_type pos = 0) const noexcept {
                        return this->find(c, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(const CharTy* str, size_type pos = 0) const noexcept {
                        return this->find_first_of(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(BasicStringView str, size_type pos = not_found) const noexcept {
                        return this->find_last_of(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(CharTy c, size_type pos = not_found) const noexcept {
                        return this->rfind(c, pos);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(const CharTy* str, size_type pos = not_found) const noexcept {
                        return this->find_last_of(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_not_of(BasicStringView str, size_type pos = 0) const noexcept {
                        return this->find_first_not_of(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type find_first_not_of(const CharTy* str, size_type pos = 0) const noexcept {
                        return this->find_first_not_of(str, pos, traits_type::length(str));
                }

                [[nodiscard]]
                constexpr size_type find_last_not_of(BasicStringView str, size_type pos = not_found) const noexcept {
                        return this->find_last_not_of(str.str, pos, str.len);
                }

                [[nodiscard]]
                constexpr size_type find_last_not_of(const CharTy* str, size_type pos = not_found) const noexcept {
                        return this->find_last_not_of(str, pos, traits_type::length(str));
                }

            private:
                size_type len;
                const CharTy* str;
        };

        export template <unsigned int N, typename CharTy, typename Traits = std::char_traits<std::remove_cv_t<CharTy>>>
        struct BasicTemplatedStringView {
                static_assert(!std::is_array_v<CharTy>);
                static_assert(std::is_trivially_copyable_v<CharTy> &&
                              std::is_trivially_default_constructible_v<CharTy> && std::is_standard_layout_v<CharTy>);
                static_assert(std::is_same_v<std::remove_cv_t<CharTy>, typename Traits::char_type>);

                using traits_type = Traits;
                using value_type = CharTy;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using const_iterator = const value_type*;
                using iterator = const_iterator;
                using size_type = size_t;
                using difference_type = ptrdiff_t;

                static constexpr size_type nfound = static_cast<size_type>(-1);
                static constexpr size_type capacity_value = N;

                constexpr BasicTemplatedStringView() noexcept = default;
                constexpr BasicTemplatedStringView(const BasicTemplatedStringView&) noexcept = default;
                constexpr BasicTemplatedStringView(const CharTy (&str)[N]) noexcept :
                    BasicTemplatedStringView(str, std::make_index_sequence<N>{}) {}
                template <size_t... Ts>
                constexpr BasicTemplatedStringView(const CharTy (&str)[N], std::index_sequence<Ts...>) noexcept :
                    str{str[Ts]...} {}
                template <typename Rg, typename DRg = std::remove_cvref_t<Rg>>
                        requires(!std::is_same_v<DRg, BasicTemplatedStringView>) && std::ranges::contiguous_range<Rg> &&
                                std::ranges::sized_range<Rg> &&
                                std::is_same_v<std::ranges::range_value_t<Rg>, CharTy> &&
                                (!std::is_convertible_v<Rg, const CharTy*>) &&
                                (!requires(DRg& d) { d.operator BasicTemplatedStringView(); })
                constexpr explicit BasicTemplatedStringView(Rg&& r) noexcept(noexcept(std::ranges::size(r)) &&
                                                                             noexcept(std::ranges::data(r))) :
                    str(std::ranges::data(r)) {}
                BasicTemplatedStringView(nullptr_t) = delete;
                constexpr BasicTemplatedStringView& operator=(const BasicTemplatedStringView&) noexcept = default;

                [[nodiscard]]
                constexpr const_iterator begin() const noexcept {
                        return this->str;
                }

                [[nodiscard]]
                constexpr const_iterator end() const noexcept {
                        return this->str + N;
                }

                [[nodiscard]]
                constexpr const_iterator cbegin() const noexcept {
                        return this->str;
                }

                [[nodiscard]]
                constexpr const_iterator cend() const noexcept {
                        return this->str + N;
                }
                [[nodiscard]]
                constexpr size_type size() const noexcept {
                        return N;
                }
                [[nodiscard]]
                constexpr operator BasicStringView<CharTy>() const noexcept {
                        return BasicStringView<CharTy>(str, N);
                }

                [[nodiscard]]
                constexpr size_type max_size() const noexcept {
                        return (nfound - sizeof(size_type) - sizeof(void*)) / sizeof(value_type) / 4;
                }
                [[nodiscard]]
                constexpr const_reference operator[](size_type pos) const noexcept {
                        return *(this->str + pos);
                }

                [[nodiscard]]
                constexpr const_reference at(size_type pos) const {
                        return *(this->str + pos);
                }

                [[nodiscard]]
                constexpr const_reference front() const noexcept {
                        return *this->str;
                }

                [[nodiscard]]
                constexpr const_reference back() const noexcept {
                        return *(this->str + N - 1);
                }

                [[nodiscard]]
                constexpr const_pointer data() const noexcept {
                        return this->str;
                }

                constexpr void swap(BasicTemplatedStringView& sv) noexcept {
                        auto tmp = *this;
                        *this = sv;
                        sv = tmp;
                }

                constexpr size_type copy(CharTy* str, size_type n, size_type pos = 0) const noexcept {
                        auto pos_checked = sv_check(this->size(), pos, "embdr::cxxstd::BasicTemplatedStringView::copy");
                        if (!pos_checked.has_value())
                                return 0;
                        pos = pos_checked.value();
                        const size_type rlen = std::min<size_t>(n, N - pos);
                        traits_type::copy(str, data() + pos, rlen);
                        return rlen;
                }

                [[nodiscard]]
                constexpr int32_t compare(const BasicTemplatedStringView str) const noexcept {
                        const size_type rlen = std::min(this->capacity_value, str.capacity_value);
                        int ret = traits_type::compare(this->str, str.str, rlen);
                        if (ret == 0)
                                ret = compare(this->size(), str.size());
                        return ret;
                }
                [[nodiscard]]
                constexpr bool starts_with(BasicTemplatedStringView x) const noexcept {
                        return this->size() >= x.size() && traits_type::compare(str, x.str, x.size()) == 0;
                }

                [[nodiscard]]
                constexpr bool starts_with(CharTy x) const noexcept {
                        return N > 0 && traits_type::eq(this->front(), x);
                }

                [[nodiscard]]
                constexpr bool starts_with(const CharTy* x) const noexcept {
                        return this->starts_with(BasicStringView(x));
                }

                [[nodiscard]]
                constexpr bool ends_with(const BasicTemplatedStringView x) const noexcept {
                        const auto len = this->size();
                        const auto xlen = x.size();
                        return len >= xlen && traits_type::compare(end() - xlen, x.data(), xlen) == 0;
                }

                [[nodiscard]]
                constexpr bool ends_with(const CharTy x) const noexcept {
                        return N > 0 && traits_type::eq(this->back(), x);
                }

                [[nodiscard]]
                constexpr bool ends_with(const CharTy* x) const noexcept {
                        return this->ends_with(BasicStringView(x));
                }
                [[nodiscard]]
                constexpr bool contains(BasicTemplatedStringView x) const noexcept {
                        return this->find(x) != nfound;
                }

                [[nodiscard]]
                constexpr bool contains(CharTy x) const noexcept {
                        for (size_type i = 0; i < N; ++i) {
                                if (this->str[i] == x)
                                        return true;
                        }
                        return false;
                }

                [[nodiscard]]
                constexpr bool contains(const CharTy* x) const noexcept {
                        return this->find(x) != nfound;
                }

                [[nodiscard]]
                constexpr size_type find(BasicTemplatedStringView str, const size_type pos = 0) const noexcept {
                        return this->find(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type find(const CharTy* str, size_type pos = 0) const noexcept {
                        const size_type len = traits_type::length(str);
                        if (pos > N)
                                return nfound;
                        if (len == 0)
                                return pos;
                        const CharTy elem0 = str[0];
                        const CharTy* const data = this->str;
                        const CharTy* first = data + pos;
                        const CharTy* const last = data + N;
                        size_type remaining = N - pos;
                        while (remaining >= len) {
                                first = traits_type::find(first, remaining - len + 1, elem0);
                                if (!first)
                                        return nfound;
                                if (traits_type::compare(first, str, len) == 0)
                                        return first - data;
                                remaining = last - ++first;
                        }
                        return nfound;
                }

                [[nodiscard]]
                constexpr size_type rfind(BasicTemplatedStringView str, size_type pos = nfound) const noexcept {
                        return this->rfind(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type rfind(const CharTy* str, size_type pos = nfound) const noexcept {
                        return this->rfind(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(BasicTemplatedStringView str, size_type pos = 0) const noexcept {
                        return this->find_first_of(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(CharTy c, size_type pos = 0) const noexcept {
                        return this->find(c, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_of(const CharTy* str, size_type pos = 0) const noexcept {
                        return this->find_first_of(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(BasicTemplatedStringView str, size_type pos = nfound) const noexcept {
                        return this->find_last_of(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(CharTy c, size_type pos = nfound) const noexcept {
                        return this->rfind(c, pos);
                }

                [[nodiscard]]
                constexpr size_type find_last_of(const CharTy* str, size_type pos = nfound) const noexcept {
                        return this->find_last_of(str, pos);
                }

                [[nodiscard]]
                constexpr size_type find_first_not_of(BasicTemplatedStringView str, size_type pos = 0) const noexcept {
                        return this->find_first_not_of(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type find_first_not_of(const CharTy* str, size_type pos = 0) const noexcept {
                        return this->find_first_not_of(str, pos, traits_type::length(str));
                }

                [[nodiscard]]
                constexpr size_type find_last_not_of(BasicTemplatedStringView str,
                                                     size_type pos = nfound) const noexcept {
                        return this->find_last_not_of(str.str, pos, N);
                }

                [[nodiscard]]
                constexpr size_type find_last_not_of(const CharTy* str, size_type pos = nfound) const noexcept {
                        return this->find_last_not_of(str, pos, traits_type::length(str));
                }
                value_type str[N]{};
        };

        export template <typename T>
        struct IsStruturalStringView : std::false_type {};
        template <unsigned int N, typename CharTy, typename Traits>
        struct IsStruturalStringView<BasicTemplatedStringView<N, CharTy, Traits>> : std::true_type {};

        export template <typename, typename>
        struct IsStringConvertible : std::false_type {};
        template <typename T>
        struct IsStringConvertible<T, char>
            : std::bool_constant<std::is_convertible_v<T, BasicStringView<char>>> {};
        template <typename T>
        struct IsStringConvertible<T, char8_t>
            : std::bool_constant<std::is_convertible_v<T, BasicStringView<char>>> {};
        template <typename T>
        struct IsStringConvertible<T, char16_t>
            : std::bool_constant<std::is_convertible_v<T, BasicStringView<char>>> {};
        export template <typename, typename>
        struct IsStringConstructible : std::false_type {};
        template <typename T>
        struct IsStringConstructible<T, char>
            : std::bool_constant<std::is_constructible_v<BasicStringView<char>, T>> {};
        export template <typename, typename>
        struct IsNothrowStringConvertible : std::false_type {};
        template <typename T>
        struct IsNothrowStringConvertible<T, char>
            : std::bool_constant<std::is_nothrow_convertible_v<T, BasicStringView<char>>> {};
        template <typename T>
        struct IsNothrowStringConvertible<T, char8_t>
            : std::bool_constant<std::is_nothrow_convertible_v<T, BasicStringView<char8_t>>> {};
        template <typename T>
        struct IsNothrowStringConvertible<T, char16_t>
            : std::bool_constant<std::is_nothrow_convertible_v<T, BasicStringView<char16_t>>> {};
        export template <typename, typename>
        struct IsNothrowStringConstructible : std::false_type {};
        template <typename T>
        struct IsNothrowStringConstructible<T, char>
            : std::bool_constant<std::is_nothrow_constructible_v<T, char>> {};
        template <typename T>
        struct IsNothrowStringConstructible<T, char8_t>
            : std::bool_constant<std::is_nothrow_constructible_v<T, char8_t>> {};
        template <typename T>
        struct IsNothrowStringConstructible<T, char16_t>
            : std::bool_constant<std::is_nothrow_constructible_v<T, char16_t>> {};
        export template <typename T, typename CharTy>
        using IsStringNothrowConvertible = IsNothrowStringConvertible<T, CharTy>;
        export template <typename T, typename CharTy>
        using is_string_nothrow_constructible = IsNothrowStringConstructible<T, CharTy>;

        template <typename T, typename CharTy>
        inline constexpr bool is_string_convertible_ty = IsStringConvertible<T, CharTy>::value;
        template <typename T, typename CharTy>
        inline constexpr bool is_string_constructible_ty = IsStringConstructible<T, CharTy>::value;
        template <typename T, typename CharTy>
        inline constexpr bool is_nothrow_string_convertible_ty = IsNothrowStringConvertible<T, CharTy>::value;
        template <typename T, typename CharTy>
        inline constexpr bool is_nothrow_string_constructible_ty = IsNothrowStringConstructible<T, CharTy>::value;
        template <typename T, typename CharTy>
        inline constexpr bool is_string_nothrow_convertible_ty = is_nothrow_string_convertible_ty<T, CharTy>;
        template <typename T, typename CharTy>
        inline constexpr bool is_string_nothrow_constructible_ty = is_nothrow_string_constructible_ty<T, CharTy>;

        template <unsigned int N1, unsigned int N2, typename CharTy, typename Traits>
        [[nodiscard]]
        constexpr auto operator+(const BasicTemplatedStringView<N1, CharTy, Traits>& lhs,
                                 const BasicTemplatedStringView<N2, CharTy, Traits>& rhs) noexcept {
                BasicTemplatedStringView<N1 + N2, CharTy, Traits> result;
                for (size_t i = 0; i < N1; ++i) {
                        result.str[i] = lhs[i];
                }
                for (size_t i = 0; i < N2; ++i) {
                        result.str[N1 + i] = rhs[i];
                }
                return result;
        }

        template <typename CharTraits>
        constexpr auto char_traits_cmp(const int cmp) noexcept {
                if constexpr (requires { typename CharTraits::comparison_category; }) {
                        using cat = CharTraits::comparison_category;
                        static_assert(!std::is_void_v<std::common_comparison_category_t<cat>>);
                        return static_cast<cat>(cmp <=> 0);
                } else
                        return static_cast<std::weak_ordering>(cmp <=> 0);
        }

        template <std::contiguous_iterator Iter, std::sized_sentinel_for<Iter> End>
        BasicStringView(Iter, End) -> BasicStringView<std::iter_value_t<Iter>>;

        template <std::ranges::contiguous_range Rg>
        BasicStringView(Rg&&) -> BasicStringView<std::ranges::range_value_t<Rg>>;

        template <typename CharTy, typename Traits>
        [[nodiscard]]
        constexpr bool operator==(BasicStringView<CharTy, Traits> x,
                                  std::type_identity_t<BasicStringView<CharTy, Traits>> y) noexcept {
                return x.size() == y.size() && x.compare(y) == 0;
        }

        template <typename CharTy, typename Traits>
        [[nodiscard]]
        constexpr auto operator<=>(BasicStringView<CharTy, Traits> x,
                                   std::type_identity_t<BasicStringView<CharTy, Traits>> y) noexcept
            -> decltype(char_traits_cmp<Traits>(0)) {
                return char_traits_cmp<Traits>(x.compare(y));
        }
        export using SimpleStringView = BasicStringView<char>;
        export using U8StringView = BasicStringView<char8_t>;
        export using U16StringView = BasicStringView<char16_t>;
        export template <unsigned int N>
        using SimpleTemplatedStringView = BasicTemplatedStringView<N, char>;
        export template <unsigned int N>
        using U8TemplatedStringView = BasicTemplatedStringView<N, char8_t>;
        export template <unsigned int N>
        using U16TemplatedStringView = BasicTemplatedStringView<N, char16_t>;
} // namespace embdr::cxxstd
