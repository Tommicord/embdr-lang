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
#include <functional>
#include <utility>

export module embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.memoryAllocator;

template <typename T>
struct MaybePayloadBase {
        using value_type = std::remove_const_t<T>;
        MaybePayloadBase() noexcept : _M_payload(), _M_engaged(false) {}
        ~MaybePayloadBase() noexcept = default;

        template <typename... Args>
        constexpr explicit MaybePayloadBase(std::in_place_t, Args&&... args) noexcept :
            _M_payload(std::in_place, std::forward<Args>(args)...), _M_engaged(true) {}

        template <typename ValUp, typename... Args>
        constexpr explicit MaybePayloadBase(std::initializer_list<ValUp> il, Args&&... args) noexcept :
            _M_payload(il, std::forward<Args>(args)...), _M_engaged(true) {}
        constexpr MaybePayloadBase(const bool engaged_flag, const MaybePayloadBase& other) noexcept :
            _M_payload(), _M_engaged(false) {
                if (engaged_flag)
                        _M_construct(other._M_get());
        }
        constexpr MaybePayloadBase(const bool engaged_flag, MaybePayloadBase&& other) noexcept :
            _M_payload(), _M_engaged(false) {
                if (engaged_flag)
                        _M_construct(std::move(other._M_get()));
        }

        MaybePayloadBase(const MaybePayloadBase&) = default;
        MaybePayloadBase(MaybePayloadBase&&) = default;
        MaybePayloadBase& operator=(const MaybePayloadBase&) = default;
        MaybePayloadBase& operator=(MaybePayloadBase&&) = default;

        constexpr void _S_copy_assign(const MaybePayloadBase& other) noexcept {
                if (this->_M_engaged && other._M_engaged)
                        this->_M_get() = other._M_get();
                else if (other._M_engaged)
                        _M_construct(other._M_get());
                else
                        reset();
        }
        constexpr void _S_move_assign(MaybePayloadBase&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                         std::is_nothrow_move_assignable_v<T>) {
                if (this->_M_engaged && other._M_engaged)
                        this->_M_get() = std::move(other._M_get());
                else if (other._M_engaged)
                        _M_construct(std::move(other._M_get()));
                else
                        reset();
        }
        struct _M_empty_byte {};

        template <typename ValUp, bool = std::is_trivially_destructible_v<ValUp>>
        union MaybeStorage {
                constexpr MaybeStorage() noexcept : _M_empty() {}

                template <typename... Args>
                constexpr explicit MaybeStorage(std::in_place_t, Args&&... args) : val(std::forward<Args>(args)...) {}

                template <typename V, typename... Args>
                constexpr explicit MaybeStorage(std::initializer_list<V> il, Args&&... args) :
                    val(il, std::forward<Args>(args)...) {}
                ~MaybeStorage() = default;

                ~MaybeStorage()
                        requires(!std::is_trivially_destructible_v<ValUp>)
                {}
                MaybeStorage(const MaybeStorage&) = default;
                MaybeStorage(MaybeStorage&&) = default;
                MaybeStorage& operator=(const MaybeStorage&) = default;
                MaybeStorage& operator=(MaybeStorage&&) = default;
                _M_empty_byte _M_empty;
                ValUp val;
        };
        template <typename... Args>
        constexpr void _M_construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<value_type, Args...>) {
                ::new (static_cast<void*>(std::addressof(this->_M_payload.val)))
                    value_type(std::forward<Args>(args)...);
                this->_M_engaged = true;
        }
        constexpr void _M_destruct() noexcept { this->_M_payload.val.~value_type(); }
        constexpr T& _M_get() noexcept { return this->_M_payload.val; }
        constexpr const T& _M_get() const noexcept { return this->_M_payload.val; }
        constexpr void reset() noexcept {
                if (this->_M_engaged) {
                        this->_M_destruct();
                        this->_M_engaged = false;
                }
        }
        MaybeStorage<value_type> _M_payload;
        bool _M_engaged;
};
template <typename T, bool trivial_destruct = std::is_trivially_destructible_v<T>,
          bool trivial_copy = std::is_trivially_copy_assignable_v<T> && std::is_trivially_copy_constructible_v<T>,
          bool trivial_move = std::is_trivially_move_assignable_v<T> && std::is_trivially_move_constructible_v<T>>
struct maybe_payload;

template <typename T>
struct maybe_payload<T, true, true, true> : MaybePayloadBase<T> {
        using MaybePayloadBase<T>::MaybePayloadBase;
        maybe_payload() = default;
};

template <typename T>
struct maybe_payload<T, true, false, true> : MaybePayloadBase<T> {
        using MaybePayloadBase<T>::MaybePayloadBase;
        maybe_payload() = default;
        ~maybe_payload() = default;
        maybe_payload(const maybe_payload&) = default;
        maybe_payload(maybe_payload&&) = default;
        maybe_payload& operator=(maybe_payload&&) = default;

        constexpr maybe_payload& operator=(const maybe_payload& other) {
                this->_S_copy_assign(other);
                return *this;
        }
};

template <typename T>
struct maybe_payload<T, true, true, false> : MaybePayloadBase<T> {
        using MaybePayloadBase<T>::MaybePayloadBase;
        maybe_payload() = default;
        ~maybe_payload() = default;
        maybe_payload(const maybe_payload&) = default;
        maybe_payload(maybe_payload&&) = default;
        maybe_payload& operator=(const maybe_payload&) = default;

        constexpr maybe_payload& operator=(maybe_payload&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                           std::is_nothrow_move_assignable_v<T>) {
                this->_S_move_assign(std::move(other));
                return *this;
        }
};
template <typename T>
struct maybe_payload<T, true, false, false> : MaybePayloadBase<T> {
        using MaybePayloadBase<T>::MaybePayloadBase;
        maybe_payload() = default;
        ~maybe_payload() = default;
        maybe_payload(const maybe_payload&) = default;
        maybe_payload(maybe_payload&&) = default;

        constexpr maybe_payload& operator=(const maybe_payload& other) {
                this->_S_copy_assign(other);
                return *this;
        }

        constexpr maybe_payload& operator=(maybe_payload&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                           std::is_nothrow_move_assignable_v<T>) {
                this->_S_move_assign(std::move(other));
                return *this;
        }
};
template <typename T, bool copy, bool move>
struct maybe_payload<T, false, copy, move> : maybe_payload<T, true, false, false> {
        using maybe_payload<T, true, false, false>::maybe_payload;
        maybe_payload() = default;
        maybe_payload(const maybe_payload&) = default;
        maybe_payload(maybe_payload&&) = default;
        maybe_payload& operator=(const maybe_payload&) = default;
        maybe_payload& operator=(maybe_payload&&) = default;
        constexpr ~maybe_payload() { this->reset(); }
};
template <typename T, bool = std::is_trivially_copy_constructible_v<T>,
          bool = std::is_trivially_move_constructible_v<T>>
struct MaybeBase {
        using value_type = T;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        constexpr MaybeBase() = default;

        template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
        constexpr explicit MaybeBase(std::in_place_t, Args&&... args) :
            _M_payload(std::in_place, std::forward<Args>(args)...) {}

        template <typename U, typename... Args,
                  typename = std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>&, Args...>>>
        constexpr explicit MaybeBase(std::in_place_t, std::initializer_list<U> il, Args&&... args) :
            _M_payload(std::in_place, il, std::forward<Args>(args)...) {}

        constexpr MaybeBase(const MaybeBase& other) noexcept(std::is_nothrow_copy_constructible_v<T>) :
            _M_payload(other._M_payload._M_engaged, other._M_payload) {}

        constexpr MaybeBase(MaybeBase&& other) noexcept(std::is_nothrow_move_constructible_v<T>) :
            _M_payload(other._M_payload._M_engaged, std::move(other._M_payload)) {}
        constexpr MaybeBase(const MaybeBase&)
                requires std::is_trivially_copy_constructible_v<T>
        = default;
        constexpr MaybeBase(MaybeBase&&)
                requires std::is_trivially_move_constructible_v<T>
        = default;
        MaybeBase& operator=(const MaybeBase&) = default;
        MaybeBase& operator=(MaybeBase&&) = default;
        maybe_payload<T> _M_payload;

        template <typename... Args>
        constexpr void
        construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<value_type>, Args...>) {
                _M_payload._M_construct(std::forward<Args>(args)...);
        }
        constexpr void destruct() noexcept { _M_payload._M_destruct(); }
        constexpr void reset() noexcept { _M_payload.reset(); }
        constexpr bool is_engaged() const noexcept { return _M_payload._M_engaged; }
        constexpr T& get() noexcept { return _M_payload._M_get(); }
        constexpr const T& get() const noexcept { return _M_payload._M_get(); }
        constexpr bool engaged() const noexcept { return is_engaged(); }
        constexpr T& stored() noexcept { return get(); }
        constexpr const T& stored() const noexcept { return get(); }
};

template <typename Signature>
struct result_of;

struct invoke_memfun_ref {};
struct invoke_memfun_deref {};
struct invoke_memobj_ref {};
struct invoke_memobj_deref {};
struct invoke_other {};

template <typename T, typename ValUp>
constexpr ValUp&& invfwd(std::remove_reference_t<T>& t) noexcept {
        return static_cast<ValUp&&>(t);
}

template <typename Res, typename Fn, typename... Args>
constexpr Res invoke_impl(invoke_other, Fn&& f, Args&&... args) {
        return std::forward<Fn>(f)(std::forward<Args>(args)...);
}

template <typename Res, typename MemFun, typename T, typename... Args>
constexpr Res invoke_impl(invoke_memfun_ref, MemFun&& f, T&& t, Args&&... args) {
        return (invfwd<T>(t).*f)(std::forward<Args>(args)...);
}

template <typename Res, typename MemFun, typename T, typename... Args>
constexpr Res invoke_impl(invoke_memfun_deref, MemFun&& f, T&& t, Args&&... args) {
        return ((*std::forward<T>(t)).*f)(std::forward<Args>(args)...);
}

template <typename Res, typename MemPtr, typename T>
constexpr Res invoke_impl(invoke_memobj_ref, MemPtr&& f, T&& t) {
        return invfwd<T>(t).*f;
}

template <typename Res, typename MemPtr, typename T>
constexpr Res invoke_impl(invoke_memobj_deref, MemPtr&& f, T&& t) {
        return (*std::forward<T>(t)).*f;
}

template <typename Callable, typename... Args>
constexpr std::invoke_result_t<Callable, Args...>
invoke(Callable&& fn, Args&&... args) noexcept(std::is_nothrow_invocable_v<Callable, Args...>) {
        using result_type = std::invoke_result<Callable, Args...>;
        using type = result_type::type;
        using tag = result_type::invoke_type;
        return invoke_impl<type>(tag{}, std::forward<Callable>(fn), std::forward<Args>(args)...);
}

namespace embdr::cxxstd {
        export template <typename CharTy, unsigned int N, typename T>
        class MaybeHandleMessaged;

        export template <typename T>
        class MaybeHandle;

        export template <typename T>
        class Maybe : MaybeBase<T> {
            public:
                using base_type = MaybeBase<T>;
                using value_type = T;
                using reference = value_type&;
                using const_reference = const value_type&;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using size_type = size_t;
                using difference_type = ptrdiff_t;

                static_assert(!std::is_same_v<std::remove_cv_t<value_type>, std::in_place_t>);
                static_assert(std::is_object_v<value_type> && !std::is_array_v<value_type>);

                constexpr Maybe() noexcept = default;
                constexpr Maybe(nullptr_t) noexcept {}

                template <typename ValUp = std::remove_cv_t<value_type>>
                        requires(!std::is_same_v<Maybe, std::remove_cvref_t<ValUp>>) &&
                                (!std::is_same_v<std::in_place_t, std::remove_cvref_t<ValUp>>) &&
                                std::is_constructible_v<value_type, ValUp>
                constexpr explicit(!std::is_convertible_v<ValUp, value_type>)
                    Maybe(ValUp&& value) noexcept(std::is_nothrow_constructible_v<value_type, ValUp>) :
                    base_type(std::in_place, std::forward<ValUp>(value)) {}

                template <typename ValUp>
                        requires(!std::is_same_v<value_type, ValUp>) &&
                                std::is_constructible_v<value_type, const ValUp&>
                constexpr explicit(!std::is_convertible_v<const ValUp&, value_type>) Maybe(
                    const Maybe<ValUp>& other) noexcept(std::is_nothrow_constructible_v<value_type, const ValUp&>) :
                    base_type(other._M_payload._M_engaged, other._M_payload) {}

                template <typename ValUp>
                        requires(!std::is_same_v<value_type, ValUp>) && std::is_constructible_v<value_type, ValUp>
                constexpr explicit(!std::is_convertible_v<ValUp, value_type>)
                    Maybe(Maybe<ValUp>&& other) noexcept(std::is_nothrow_constructible_v<value_type, ValUp>) :
                    base_type(other._M_payload._M_engaged, std::move(other._M_payload)) {}

                template <typename... Args>
                        requires std::is_constructible_v<value_type, Args...>
                explicit constexpr Maybe(std::in_place_t, Args&&... args) noexcept(
                    std::is_nothrow_constructible_v<value_type, Args...>) :
                    base_type(std::in_place, std::forward<Args>(args)...) {}

                constexpr Maybe(const Maybe& other) noexcept(std::is_nothrow_copy_constructible_v<value_type>)
                        requires std::is_copy_constructible_v<value_type>
                = default;
                constexpr Maybe(Maybe&& other) noexcept(std::is_nothrow_move_constructible_v<value_type>)
                        requires std::is_move_constructible_v<value_type>
                = default;

                constexpr Maybe& operator=(const Maybe& other) noexcept(std::is_nothrow_copy_assignable_v<value_type>)
                        requires std::is_copy_assignable_v<value_type>
                = default;
                constexpr Maybe& operator=(Maybe&& other) noexcept(std::is_nothrow_move_assignable_v<value_type>)
                        requires std::is_move_assignable_v<value_type>
                = default;
                constexpr ~Maybe() = default;

                [[nodiscard]] constexpr bool has_value() const noexcept { return base_type::is_engaged(); }

                [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

                constexpr value_type& value() & noexcept { return base_type::get(); }

                constexpr const value_type& value() const& noexcept { return base_type::get(); }

                constexpr value_type&& value() && noexcept { return std::move(base_type::get()); }

                constexpr const value_type&& value() const&& noexcept { return std::move(base_type::get()); }

                template <typename U>
                        requires std::is_convertible_v<U, value_type>
                constexpr value_type value_or(U&& default_value) const& {
                        return has_value() ? value() : static_cast<value_type>(std::forward<U>(default_value));
                }

                template <typename U>
                        requires std::is_convertible_v<U, value_type>
                constexpr value_type value_or(U&& default_value) && {
                        return has_value() ? std::move(value())
                                           : static_cast<value_type>(std::forward<U>(default_value));
                }

                template <typename F>
                        requires(std::invocable<F, value_type&> &&
                                 !std::is_void_v<std::invoke_result_t<F, value_type&>>)
                constexpr auto map(F&& f) & -> Maybe<std::remove_cvref_t<std::invoke_result_t<F, value_type&>>> {
                        if (!has_value())
                                return {};
                        else
                                return std::invoke(std::forward<F>(f), value());
                }

                template <typename F>
                        requires(std::invocable<F, const value_type&> &&
                                 !std::is_void_v<std::invoke_result_t<F, const value_type&>>)
                constexpr auto
                map(F&& f) const& -> Maybe<std::remove_cvref_t<std::invoke_result_t<F, const value_type&>>> {
                        if (!has_value())
                                return {};
                        else
                                return std::invoke(std::forward<F>(f), value());
                }

                template <typename F>
                        requires(std::invocable<F, value_type &&> &&
                                 !std::is_void_v<std::invoke_result_t<F, value_type &&>>)
                constexpr auto map(F&& f) && -> Maybe<std::remove_cvref_t<std::invoke_result_t<F, value_type&&>>> {
                        if (!has_value())
                                return {};
                        else
                                return std::invoke(std::forward<F>(f), std::move(value()));
                }

                template <typename F>
                constexpr auto transform(F&& f) & -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) const& -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) && -> decltype(std::move(*this).map(std::forward<F>(f))) {
                        return std::move(*this).map(std::forward<F>(f));
                }

                template <typename F>
                        requires std::invocable<F, value_type&> &&
                                 requires(std::invoke_result_t<F, value_type&> result) {
                                         { result.has_value() } -> std::convertible_to<bool>;
                                 }
                constexpr auto and_then(F&& f) & -> std::remove_cvref_t<std::invoke_result_t<F, value_type&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, value_type&>>;
                        if (!this->has_value())
                                return result_type{};
                        else
                                return std::invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, const value_type&> &&
                                 requires(std::invoke_result_t<F, const value_type&> result) {
                                         { result.has_value() } -> std::convertible_to<bool>;
                                 }
                constexpr auto
                and_then(F&& f) const& -> std::remove_cvref_t<std::invoke_result_t<F, const value_type&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, const value_type&>>;
                        if (!this->has_value())
                                return result_type{};
                        else
                                return std::invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, value_type&&> &&
                                 requires(std::invoke_result_t<F, value_type&&> result) {
                                         { result.has_value() } -> std::convertible_to<bool>;
                                 }
                constexpr auto and_then(F&& f) && -> std::remove_cvref_t<std::invoke_result_t<F, value_type&&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, value_type&&>>;
                        if (!this->has_value())
                                return result_type{};
                        else
                                return std::invoke(std::forward<F>(f), std::move(this->value()));
                }

                template <typename U>
                        requires std::constructible_from<value_type, U>
                constexpr Maybe or_value(U&& value) const& {
                        return this->has_value() ? Maybe(value()) : Maybe(std::forward<U>(value));
                }

                template <typename U>
                        requires std::constructible_from<value_type, U>
                constexpr auto or_value(U&& value) && {
                        return this->has_value() ? Maybe(std::move(value())) : Maybe(std::forward<U>(value));
                }

                constexpr auto or_value(Maybe other) const& {
                        return this->has_value() ? Maybe(this->value()) : std::move(other);
                }

                constexpr auto or_value(Maybe other) && {
                        return this->has_value() ? Maybe(std::move(this->value())) : std::move(other);
                }

                template <typename F>
                        requires std::invocable<F> && std::constructible_from<value_type, std::invoke_result_t<F>>
                constexpr auto or_else(F&& f) const& {
                        if (this->has_value())
                                return Maybe<value_type>(this->value());
                        else
                                return Maybe<value_type>(std::invoke(std::forward<F>(f)));
                }

                template <typename U>
                constexpr auto and_other(Maybe<U> other) const& {
                        return this->has_value() ? std::move(other) : Maybe<U>{};
                }

                template <typename U>
                constexpr auto and_other(Maybe<U> other) && {
                        return this->has_value() ? std::move(other) : Maybe<U>{};
                }

                [[nodiscard]] constexpr bool ok() const noexcept { return this->has_value(); }

                template <typename E>
                        requires(std::is_enum_v<E>)
                constexpr auto ok_or(E&& err) const& {
                        using maybe_type = MaybeHandle<value_type>;
                        if (this->has_value())
                                return maybe_type(this->value());
                        else
                                return maybe_type(err);
                }

                template <typename E>
                constexpr auto ok_or(E&& err) && {
                        using maybe_type = MaybeHandle<value_type>;
                        if (this->has_value())
                                return maybe_type(std::move(this->value()));
                        else
                                return maybe_type(std::forward<E>(err));
                }

                constexpr void reset() noexcept { base_type::reset(); }
        };

        export template <typename T>
        struct MaybeErrorStorage {
                using value_type = T;
                using reference = value_type&;
                using base_type = MaybeBase<T>;
                using const_reference = const value_type&;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using size_type = size_t;
                using difference_type = ptrdiff_t;

                bool _M_engaged = false;
                BackingStorageSingle<T> _M_storage;
                constexpr MaybeErrorStorage() noexcept = default;
                explicit constexpr MaybeErrorStorage(const T v) noexcept : _M_engaged(true) {
                        struct Uint8TBytes {
                                uint8_t b[sizeof(T)];
                        };
                        auto valty_bytes = std::bit_cast<Uint8TBytes>(v);
                        for (size_type i = 0; i < sizeof(T); ++i)
                                this->_M_storage.data()[i] = valty_bytes.b[i];
                        this->_M_storage.data()[sizeof(T)] = '\0';
                }

                constexpr MaybeErrorStorage(const MaybeErrorStorage& other) noexcept : _M_engaged(other._M_engaged) {
                        this->_M_copy_from(other);
                }

                constexpr MaybeErrorStorage(MaybeErrorStorage&& other) noexcept : _M_engaged(other._M_engaged) {
                        this->_M_copy_bytes(other);
                        other._M_engaged = false;
                }
                constexpr MaybeErrorStorage& operator=(const MaybeErrorStorage& other) noexcept {
                        if (std::addressof(other) != this)
                                this->_M_copy_from(other);
                        return *this;
                }
                constexpr MaybeErrorStorage& operator=(MaybeErrorStorage&& other) noexcept {
                        if (std::addressof(other) != this) {
                                this->_M_engaged = other._M_engaged;
                                this->_M_copy_bytes(other);
                                other._M_engaged = false;
                        }
                        return *this;
                }
                constexpr ~MaybeErrorStorage() noexcept = default;
                constexpr char* get() noexcept { return this->_M_engaged ? this->_M_storage.data() : nullptr; }
                constexpr const char* get() const noexcept {
                        return this->_M_engaged ? this->_M_storage.data() : nullptr;
                }
                constexpr bool error_state() const noexcept { return this->_M_engaged; }

            private:
                constexpr void _M_copy_bytes(const MaybeErrorStorage& other) noexcept {
                        for (size_type i = 0; i < this->_M_storage.capacity_value; ++i)
                                this->_M_storage.data()[i] = other._M_storage.data()[i];
                }
                constexpr void _M_copy_from(const MaybeErrorStorage& other) noexcept {
                        this->_M_engaged = other._M_engaged;
                        this->_M_copy_bytes(other);
                }
        };

        export template <typename T>
        class MaybeHandle {
                static_assert(std::is_object_v<T> && !std::is_array_v<T>,
                              "embdr::cxxstd::MaybeHandle must hold an object type");
                template <typename ValUp>
                using not_self = std::negation<std::is_same<MaybeHandle, std::remove_cv_t<ValUp>>>;

            public:
                using value_type = T;
                using reference = value_type&;
                using base_type = MaybeBase<T>;
                using const_reference = const value_type&;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                base_type _M_base_handle;
                MaybeErrorStorage<T> _M_storage;

                constexpr MaybeHandle() noexcept : _M_base_handle(), _M_storage() {}
                explicit constexpr MaybeHandle(const T value) noexcept(std::is_nothrow_move_constructible_v<T> ||
                                                                       std::is_nothrow_copy_constructible_v<T>) :
                    _M_base_handle(std::in_place, std::move(value)), _M_storage() {}

                template <typename... Args>
                        requires std::is_constructible_v<T, Args...>
                constexpr explicit MaybeHandle(std::in_place_t,
                                               Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
                    _M_base_handle(std::in_place, std::forward<Args>(args)...), _M_storage() {}
                template <typename Fmt>
                constexpr explicit MaybeHandle(Fmt msg) noexcept : _M_base_handle(), _M_storage(msg) {}

                constexpr MaybeHandle(const MaybeHandle& other) noexcept
                        requires std::is_copy_constructible_v<T>
                    : _M_base_handle(other._M_base_handle), _M_storage(other._M_storage) {}

                constexpr MaybeHandle(MaybeHandle&& other) noexcept
                        requires std::is_move_constructible_v<T>
                    : _M_base_handle(std::move(other._M_base_handle)), _M_storage(std::move(other._M_storage)) {}

                constexpr ~MaybeHandle() noexcept = default;

                constexpr MaybeHandle& operator=(const MaybeHandle& other) noexcept
                        requires std::is_copy_assignable_v<T>
                {
                        if (std::addressof(other) != this) {
                                this->_M_base_handle = other._M_base_handle;
                                this->_M_storage = other._M_storage;
                        }
                        return *this;
                }

                constexpr MaybeHandle& operator=(MaybeHandle&& other) noexcept
                        requires std::is_move_assignable_v<T>
                {
                        if (std::addressof(other) != this) {
                                this->_M_base_handle = std::move(other._M_base_handle);
                                this->_M_storage = std::move(other._M_storage);
                        }
                        return *this;
                }

                [[nodiscard]] constexpr bool has_value() const noexcept {
                        return this->_M_base_handle.engaged() && !this->_M_storage.error_state();
                }

                constexpr T& value() & noexcept { return this->_M_base_handle.stored(); }

                constexpr const T& value() const& noexcept { return this->_M_base_handle.stored(); }

                constexpr T&& value() && noexcept { return std::move(this->_M_base_handle.stored()); }

                constexpr const T&& value() const&& noexcept { return std::move(this->_M_base_handle.stored()); }

                template <typename F>
                        requires(std::invocable<F, T&> && !std::is_void_v<std::invoke_result_t<F, T&>>)
                constexpr auto map(F&& f) & -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, T&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), this->value()));
                }

                template <typename F>
                        requires(std::invocable<F, const T&> && !std::is_void_v<std::invoke_result_t<F, const T&>>)
                constexpr auto
                map(F&& f) const& -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, const T&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), this->value()));
                }

                template <typename F>
                        requires(std::invocable<F, T &&> && !std::is_void_v<std::invoke_result_t<F, T &&>>)
                constexpr auto map(F&& f) && -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, T&&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), std::move(this->value())));
                }

                template <typename F>
                constexpr auto transform(F&& f) & -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) const& -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) && -> decltype(std::move(*this).map(std::forward<F>(f))) {
                        return std::move(*this).map(std::forward<F>(f));
                }

                template <typename F>
                        requires std::invocable<F, T&> && requires(std::invoke_result_t<F, T&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) & -> std::remove_cvref_t<std::invoke_result_t<F, T&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, const T&> && requires(std::invoke_result_t<F, const T&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) const& -> std::remove_cvref_t<std::invoke_result_t<F, const T&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, T&&> && requires(std::invoke_result_t<F, T&&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) && -> std::remove_cvref_t<std::invoke_result_t<F, T&&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), std::move(this->value()));
                }

                template <typename U>
                        requires std::constructible_from<T, U>
                constexpr auto or_value(U&& value) const& {
                        return has_value() ? MaybeHandle(value()) : MaybeHandle(std::forward<U>(value));
                }

                template <typename F>
                        requires std::invocable<F> && std::constructible_from<T, std::invoke_result_t<F>>
                constexpr auto or_else(F&& f) const& {
                        if (this->has_value())
                                return MaybeHandle(this->value());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f)));
                }

                template <typename U>
                constexpr auto and_other(MaybeHandle<U> other) const& {
                        return this->has_value() ? std::move(other) : MaybeHandle{};
                }

                [[nodiscard]] constexpr const char* what() const noexcept { return _M_storage.get(); }

                template <typename E>
                constexpr auto ok_or(E&& err) const& {
                        if (this->has_value())
                                return Maybe(this->value());
                        else
                                return err;
                }

                template <typename U>
                        requires std::is_convertible_v<U, T>
                constexpr T unroll_or(U&& default_value) const& {
                        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                                      "T must be copy or move constructible for unroll_or");
                        return this->has_value() ? this->value() : static_cast<T>(std::forward<U>(default_value));
                }

                template <typename ValUp>
                        requires std::is_convertible_v<ValUp, T>
                constexpr T unroll_or(ValUp&& default_value) && {
                        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                                      "T must be copy or move constructible for unroll_or");
                        return this->has_value() ? std::move(this->value())
                                                 : static_cast<T>(std::forward<ValUp>(default_value));
                }

                template <typename F>
                        requires std::invocable<F>
                constexpr void unroll_or_else(F&& f) const& {
                        if (!this->has_value())
                                invoke(std::forward<F>(f));
                }

                template <typename F>
                        requires std::invocable<F>
                constexpr void unroll_or_else(F&& f) && {
                        if (!this->has_value())
                                invoke(std::forward<F>(f));
                }

                template <typename ValFn>
                        requires std::invocable<ValFn> && std::is_convertible_v<decltype(std::declval<ValFn>()()), T>
                constexpr T unroll_or_else(ValFn&& fn) const& {
                        if (this->has_value())
                                return this->value();
                        else
                                return std::forward<ValFn>(fn)();
                }

                template <typename Fn>
                        requires std::invocable<Fn> && std::is_convertible_v<decltype(std::declval<Fn>()()), T>
                constexpr T unroll_or_else(Fn&& fn) && {
                        if (this->has_value())
                                return std::move(this->value());
                        else
                                return std::forward<Fn>(fn)();
                }

                template <typename ValEn>
                        requires(std::is_enum_v<ValEn> || std::is_base_of_v<ValEn, MaybeHandle<T>>)
                constexpr auto unroll_map() const noexcept -> MaybeHandle<ValEn> {
                        static_assert(std::is_enum_v<ValEn> || std::is_class_v<ValEn>,
                                      "embdr::cxxstd::MaybeHandle target type must be an enum or class type");
                        if (has_value())
                                return MaybeHandle(static_cast<ValEn>(this->value()));
                        else
                                return MaybeHandle(this->what());
                }
        };
        template <typename CharTy, unsigned int N, typename T>
        class MaybeHandleMessaged {
                bool _M_has_value = true;
                union {
                        BackingStorage<CharTy, N> _M_error_storage{};
                        BackingStorageSingle<T> _M_value_storage{};
                };

            public:
                using value_type = T;
                using reference = value_type&;
                using base_type = MaybeBase<T>;
                using const_reference = const value_type&;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                base_type _M_base_handle;
                MaybeErrorStorage<T> _M_storage;
                constexpr MaybeHandleMessaged() noexcept = default;

                template <typename Fmt>
                constexpr explicit MaybeHandleMessaged(Fmt msg) noexcept : _M_has_value(false) {
                        this->_M_copy_message(msg);
                }

                constexpr MaybeHandleMessaged(const MaybeHandleMessaged& other) noexcept :
                    _M_has_value(other._M_has_value) {
                        this->_M_copy_storage(other);
                }

                constexpr MaybeHandleMessaged(MaybeHandleMessaged&& other) noexcept : _M_has_value(other._M_has_value) {
                        this->_M_copy_storage(other);
                        other._M_has_value = true;
                        other._M_error_storage[0] = '\0';
                }

                constexpr ~MaybeHandleMessaged() noexcept = default;

                constexpr MaybeHandleMessaged& operator=(const MaybeHandleMessaged& other) noexcept {
                        if (std::addressof(other) != this) {
                                this->_M_has_value = other._M_has_value;
                                this->_M_copy_storage(other);
                        }
                        return *this;
                }

                constexpr MaybeHandleMessaged& operator=(MaybeHandleMessaged&& other) noexcept {
                        if (std::addressof(other) != this) {
                                this->_M_has_value = other._M_has_value;
                                this->_M_copy_storage(other);
                                other._M_has_value = true;
                                other._M_error_storage[0] = '\0';
                        }
                        return *this;
                }

                [[nodiscard]] constexpr bool has_value() const noexcept { return this->_M_has_value; }

                [[nodiscard]] constexpr explicit operator bool() const noexcept { return this->has_value(); }

                constexpr T& value() & noexcept { return this->_M_base_handle.stored(); }

                constexpr const T& value() const& noexcept { return this->_M_base_handle.stored(); }

                constexpr T&& value() && noexcept { return std::move(this->_M_base_handle.stored()); }

                constexpr const T&& value() const&& noexcept { return std::move(this->_M_base_handle.stored()); }

                template <typename F>
                        requires(std::invocable<F, T&> && !std::is_void_v<std::invoke_result_t<F, T&>>)
                constexpr auto map(F&& f) & -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, T&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), this->value()));
                }

                template <typename F>
                        requires(std::invocable<F, const T&> && !std::is_void_v<std::invoke_result_t<F, const T&>>)
                constexpr auto
                map(F&& f) const& -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, const T&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), this->value()));
                }

                template <typename F>
                        requires(std::invocable<F, T &&> && !std::is_void_v<std::invoke_result_t<F, T &&>>)
                constexpr auto map(F&& f) && -> MaybeHandle<std::remove_cvref_t<std::invoke_result_t<F, T&&>>> {
                        if (!this->has_value())
                                return MaybeHandle(this->what());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f), std::move(this->value())));
                }

                template <typename F>
                constexpr auto transform(F&& f) & -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) const& -> decltype(this->map(std::forward<F>(f))) {
                        return this->map(std::forward<F>(f));
                }

                template <typename F>
                constexpr auto transform(F&& f) && -> decltype(std::move(*this).map(std::forward<F>(f))) {
                        return std::move(*this).map(std::forward<F>(f));
                }

                template <typename F>
                        requires std::invocable<F, T&> && requires(std::invoke_result_t<F, T&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) & -> std::remove_cvref_t<std::invoke_result_t<F, T&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, const T&> && requires(std::invoke_result_t<F, const T&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) const& -> std::remove_cvref_t<std::invoke_result_t<F, const T&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), this->value());
                }

                template <typename F>
                        requires std::invocable<F, T&&> && requires(std::invoke_result_t<F, T&&> result) {
                                { result.has_value() } -> std::convertible_to<bool>;
                        }
                constexpr auto and_then(F&& f) && -> std::remove_cvref_t<std::invoke_result_t<F, T&&>> {
                        using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
                        if (!this->has_value())
                                return result_type(this->what());
                        else
                                return invoke(std::forward<F>(f), std::move(this->value()));
                }

                template <typename U>
                        requires std::constructible_from<T, U>
                constexpr auto or_value(U&& value) const& {
                        return has_value() ? MaybeHandle(value()) : MaybeHandle(std::forward<U>(value));
                }

                template <typename F>
                        requires std::invocable<F> && std::constructible_from<T, std::invoke_result_t<F>>
                constexpr auto or_else(F&& f) const& {
                        if (this->has_value())
                                return MaybeHandle(this->value());
                        else
                                return MaybeHandle(invoke(std::forward<F>(f)));
                }

                template <typename CharTy1, unsigned int N1, typename U>
                constexpr auto and_other(MaybeHandleMessaged<CharTy1, N1, U> other) const& {
                        return this->has_value() ? std::move(other) : MaybeHandleMessaged<CharTy1, N1, U>{};
                }

                template <typename U>
                constexpr auto and_other(MaybeHandle<U> other) const& {
                        return this->has_value() ? MaybeHandle<U>(other.value()) : MaybeHandle<U>{};
                }

                [[nodiscard]] constexpr const char* what() const noexcept { return this->_M_storage.get(); }

                template <typename E>
                constexpr auto ok_or(E&& err) const& {
                        if (this->has_value())
                                return Maybe(this->value());
                        else
                                return err;
                }

                template <typename U>
                        requires std::is_convertible_v<U, T>
                constexpr T unroll_or(U&& default_value) const& {
                        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                                      "T must be copy or move constructible for unroll_or");
                        return this->has_value() ? this->value() : static_cast<T>(std::forward<U>(default_value));
                }

                template <typename ValUp>
                        requires std::is_convertible_v<ValUp, T>
                constexpr T unroll_or(ValUp&& default_value) && {
                        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                                      "T must be copy or move constructible for unroll_or");
                        return this->has_value() ? std::move(this->value())
                                                 : static_cast<T>(std::forward<ValUp>(default_value));
                }

                template <typename F>
                        requires std::invocable<F>
                constexpr void unroll_or_else(F&& f) const& {
                        if (!this->has_value())
                                invoke(std::forward<F>(f));
                }

                template <typename F>
                        requires std::invocable<F>
                constexpr void unroll_or_else(F&& f) && {
                        if (!this->has_value())
                                invoke(std::forward<F>(f));
                }

                template <typename ValFn>
                        requires std::invocable<ValFn> && std::is_convertible_v<decltype(std::declval<ValFn>()()), T>
                constexpr T unroll_or_else(ValFn&& fn) const& {
                        if (this->has_value())
                                return this->value();
                        else
                                return std::forward<ValFn>(fn)();
                }

                template <typename Fn>
                        requires std::invocable<Fn> && std::is_convertible_v<decltype(std::declval<Fn>()()), T>
                constexpr T unroll_or_else(Fn&& fn) && {
                        if (this->has_value())
                                return std::move(this->value());
                        else
                                return std::forward<Fn>(fn)();
                }

                template <typename ValEn>
                        requires(std::is_enum_v<ValEn> || std::is_base_of_v<ValEn, MaybeHandle<T>>)
                constexpr auto unroll_map() const noexcept -> MaybeHandle<ValEn> {
                        static_assert(std::is_enum_v<ValEn> || std::is_class_v<ValEn>,
                                      "embdr::cxxstd::MaybeHandle target type must be an enum or class type");
                        if (has_value())
                                return MaybeHandle(static_cast<ValEn>(this->value()));
                        else
                                return MaybeHandle(this->what());
                }
            private:
                template <unsigned int N1, typename Fmt>
                constexpr void _M_copy_message(const Fmt (&msg)[N1]) noexcept {
                        for (size_type i = 0; i < N1 && i < N; ++i)
                                this->_M_error_storage[i] = msg[i];
                        this->_M_error_storage[N1] = '\0';
                }
                constexpr void _M_copy_storage(const MaybeHandleMessaged& other) noexcept {
                        for (size_type i = 0; i < this->_M_error_storage.capacity_value; ++i)
                                this->_M_error_storage[i] = other._M_error_storage[i];
                }
        };
        export template <typename T>
        constexpr auto make_some(T& value) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>) {
                return MaybeHandle<T>(std::in_place, value);
        }
        export template <typename T>
        constexpr auto make_some(T&& value) noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>) {
                return MaybeHandle<T>(std::in_place, std::forward<T>(value));
        }

        export template <unsigned int N, typename CharTy, typename T, typename... Args>
        constexpr auto make_some(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
                return MaybeHandleMessaged<CharTy, N, T>(std::in_place, std::forward<Args>(args)...);
        }

        export template <unsigned int N, typename CharTy, typename T>
        constexpr auto make_err(CharTy (&fmt)[N]) noexcept {
                return MaybeHandleMessaged<CharTy, N, T>(fmt);
        }

        export template <unsigned int N, typename CharTy, typename T, typename... Args>
        constexpr auto make_err(CharTy (&fmt)[N], const Args&... args) noexcept {
                return MaybeHandleMessaged<CharTy, N, T>(fmt, args...);
        }
} // namespace embdr::cxxstd
