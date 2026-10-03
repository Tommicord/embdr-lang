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
#include <cinttypes>
#include <concepts>
#include <memory>
#include <new>
#include <type_traits>

export module embdr.cxxstd.memoryAllocator;
import embdr.cxxstd.memoryIterator;

namespace embdr::cxxstd {
        export template <typename T, int Sz>
        struct BackingStorage {
                static_assert(Sz > 0, "embdr::cxxstd::BackingStorage invalid capacity");
                static_assert(!std::is_reference_v<T>,
                              "embdr::cxxstd::BackingStorage required a non-reference value type");
                static_assert(!std::is_void_v<T>, "embdr::cxxstd::BackingStorage cannot allocate void");
                static_assert(std::is_destructible_v<T>,
                              "embdr::cxxstd::BackingStorage required a destructible value type");
                static_assert(std::is_object_v<T>, "embdr::cxxstd::BackingStorage required an object type");
                static_assert(std::is_trivially_copyable_v<T>,
                              "embdr::cxxstd::BackingStorage required a trivially copyable value type");
                static_assert(Sz > 0, "embdr::cxxstd::BackingStorage capacity must be valid");
                alignas(alignof(T)) T _M_storage[Sz]{};

                BackingStorage() noexcept;
                BackingStorage(const BackingStorage& other) noexcept = default;
                BackingStorage(BackingStorage&&) noexcept = delete;
                ~BackingStorage() noexcept;

                using value_type = T;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using size_type = size_t;
                using difference_type = ptrdiff_t;

                static constexpr size_type capacity_value = Sz;
                pointer data() noexcept { return this->_M_storage; }
                const_pointer data() const noexcept { return this->_M_storage; }
                reference operator[](const size_type index) noexcept { return this->_M_storage[index]; }
                const_reference operator[](const size_type index) const noexcept { return this->_M_storage[index]; }
        };
        export template <typename T>
        struct BackingStorageSingle {
                static_assert(!std::is_reference_v<T>,
                              "embdr::cxxstd::BackingStorageSingle required a non-reference value type");
                static_assert(!std::is_void_v<T>, "embdr::cxxstd::BackingStorageSingle cannot allocate void");
                static_assert(std::is_destructible_v<T>,
                              "embdr::cxxstd::BackingStorageSingle required a destructible value type");
                static_assert(std::is_object_v<T>, "embdr::cxxstd::BackingStorageSingle required an object type");
                static_assert(std::is_trivially_copyable_v<T>,
                              "embdr::cxxstd::BackingStorageSingle required a trivially copyable value type");
                alignas(alignof(T)) T _M_storage{};

                BackingStorageSingle() noexcept;
                BackingStorageSingle(const BackingStorageSingle& other) noexcept = default;
                BackingStorageSingle(BackingStorageSingle&&) noexcept = delete;
                ~BackingStorageSingle() noexcept;

                using value_type = T;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                pointer data() noexcept { return this->_M_storage; }
                const_pointer data() const noexcept { return this->_M_storage; }
                reference operator[](const size_type index) noexcept { return this->_M_storage[index]; }
                const_reference operator[](const size_type index) const noexcept { return this->_M_storage[index]; }
        };

        template <class T, class Alloc, unsigned int Sz>
        concept ContainerAllocator = requires {
                Sz > 0 &&
                            requires(Alloc & alloc, typename Alloc::pointer p)
                {
                        typename Alloc::value_type;
                        typename Alloc::pointer;
                        typename Alloc::size_type;
                        requires std::same_as<typename Alloc::value_type, T>;
                        {alloc.template allocate<Sz>()}->std::same_as<typename Alloc::pointer>;
                        {
                                alloc.deallocate(p)
                        } noexcept;
                };
        };
        export template <class Alloc>
                requires ContainerAllocator<typename Alloc::value_type, Alloc, Alloc::capacity_value>
        struct AllocatorTraits {
                using allocator_type = Alloc;
                using value_type = typename Alloc::value_type;
                using pointer = typename Alloc::pointer;
                using const_pointer = typename Alloc::const_pointer;
                using reference = typename Alloc::reference;
                using const_reference = typename Alloc::const_reference;
                using size_type = typename Alloc::size_type;
                using difference_type = typename Alloc::difference_type;
                using propagate_on_container_copy_assignment = std::false_type;
                using propagate_on_container_move_assignment = std::true_type;
                using propagate_on_container_swap = std::false_type;
                using is_always_equal = std::true_type;

                struct rebind {
                        using other = AllocatorTraits;
                };
                static void on_swap(allocator_type& a, allocator_type& b) {
                        if (propagate_on_swap()) {
                                auto temp = a;
                                a = b;
                                b = temp;
                        }
                }

                template <typename _T, typename... Args>
                        requires(!std::is_unbounded_array_v<_T>) &&
                                requires { ::new (static_cast<void*>(nullptr)) _T(std::declval<Args>()...); }
                static _T* _S_construct_at(_T* location,
                                               Args&&... args) noexcept(noexcept(::new (static_cast<void*>(nullptr))
                                                                                     _T(std::declval<Args>()...))) {
                        void* loc = location;
                        if (std::is_array_v<_T>) {
                                static_assert(sizeof...(Args) == 0,
                                              "embdr::cxxstd::container::alloc_traits::_S_construct_at for array "
                                              "types must not use any arguments to initialize the "
                                              "array");
                                return ::new (loc) _T[1]();
                        } else
                                return ::new (loc) _T(std::forward<Args>(args)...);
                }
                template <typename _T>
                static void _S_destroy_at(_T* location) {
                        if (std::is_array_v<_T>) {
                                for (auto& x : *location)
                                        _S_destroy_at(std::addressof(x));
                        } else
                                location->~_T();
                }
                template <typename ValUp, typename... Args>
                static void construct(ValUp* p,
                                      Args&&... args) noexcept(std::is_nothrow_constructible_v<ValUp, Args...>) {
                        _S_construct_at(p, std::forward<Args>(args)...);
                }

                static allocator_type select_on_container_copy_construction(const allocator_type& alloc) noexcept {
                        return alloc;
                }

                template <typename ValUp>
                static void destroy(ValUp* p) noexcept(std::is_nothrow_destructible_v<ValUp>) {
                        _S_destroy_at(p);
                }
                static size_type max_size() noexcept { return static_cast<size_type>(-1) / sizeof(value_type); }

                static bool propagate_on_copy_assign() { return propagate_on_container_copy_assignment::value; }

                static bool propagate_on_move_assign() { return propagate_on_container_move_assignment::value; }

                static bool propagate_on_swap() { return propagate_on_container_swap::value; }

                static bool always_equal() { return is_always_equal::value; }

                static pointer allocate(Alloc& alloc, size_type count) noexcept { return alloc.allocate(count); }
                template <size_type N>
                static pointer allocate(Alloc& alloc) noexcept {
                        if (requires { alloc.template allocate<N>(); })
                                return alloc.template allocate<N>();
                        else
                                return alloc.allocate(N);
                }
                static void deallocate(Alloc& alloc, pointer p) noexcept { alloc.deallocate(p); }
                template <class T, class... Args>
                        requires std::constructible_from<T, Args...>
                static void construct(Alloc&, T* p,
                                      Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
                        _S_construct_at(p, std::forward<Args>(args)...);
                }
                template <class T>
                static void destroy(Alloc&, T* p) noexcept(std::is_nothrow_destructible_v<T>) {
                        _S_destroy_at(p);
                }
        };

        export template <class T, unsigned int Sz>
        class BumpAllocator {
            public:
                static_assert(!std::is_reference_v<T>,
                              "embdr::cxxstd::BumpAllocator required a non-reference value type");
                static_assert(!std::is_void_v<T>, "embdr::cxxstd::BumpAllocator cannot allocate void");
                static_assert(std::is_destructible_v<T>,
                              "embdr::cxxstd::BumpAllocator required a destructible value type");
                static_assert(std::is_object_v<T>, "embdr::cxxstd::BumpAllocator required an object type");
                static_assert(std::is_trivially_copyable_v<T>,
                              "embdr::cxxstd::BumpAllocator required a trivially copyable value type");
                static_assert(Sz > 0, "embdr::cxxstd::BumpAllocator capacity must be valid");

                using value_type = T;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                using iterator = BasicIterator<value_type>;
                using const_iterator = const BasicIterator<value_type>;
                using storage_type = BackingStorage<T, Sz>;
                static constexpr size_type capacity_value = Sz;

                BumpAllocator& operator=(const BumpAllocator&) noexcept = delete;
                BumpAllocator(const BumpAllocator& other) noexcept = default;
                BumpAllocator(BumpAllocator&& other) noexcept = delete;
                BumpAllocator& operator=(BumpAllocator&& other) noexcept = delete;
                BumpAllocator() noexcept : _M_offset(0) {}

                template <unsigned int N>
                        requires(N > 0 && N <= Sz)
                pointer allocate() noexcept;
                pointer allocate(const size_type count) noexcept;
                void deallocate(pointer) noexcept;
                ~BumpAllocator() noexcept;
                iterator begin() const noexcept { return iterator(this->_M_storage.data()); }
                const_iterator begin() noexcept { return iterator(this->_M_storage.data()); }
                iterator end() noexcept { return iterator(this->_M_storage.data() + Sz); }
                const_iterator end() const noexcept { return iterator(this->_M_storage.data() + Sz); }
                storage_type raw() const noexcept { return this->_M_storage; }
                const_iterator cbegin() const noexcept { return static_cast<const_iterator>(this->begin()); }
                const_iterator cend() const noexcept { return static_cast<const_iterator>(this->end()); }
                size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(value_type); }
                size_type used() const noexcept { return this->_M_offset * sizeof(value_type); }
                size_type capacity() const noexcept { return static_cast<size_type>(Sz) * sizeof(value_type); }
                bool is_empty() const noexcept { return this->_M_offset == 0; }
                void reset() const;

            private:
                storage_type _M_storage{};
                mutable size_type _M_offset = 0;
        };

        export template <class T, unsigned int Sz>
        class BitmapAllocator {
            public:
                static_assert(!std::is_reference_v<T>,
                              "embdr::cxxstd::BitmapAllocator required a non-reference value type");
                static_assert(!std::is_void_v<T>, "embdr::cxxstd::BitmapAllocator cannot allocate void");
                static_assert(std::is_destructible_v<T>,
                              "embdr::cxxstd::BitmapAllocator required a destructible value type");
                static_assert(std::is_object_v<T>, "embdr::cxxstd::BitmapAllocator required an object type");
                static_assert(std::is_trivially_copyable_v<T>,
                              "embdr::cxxstd::BitmapAllocator required a trivially copyable value type");
                static_assert(Sz > 0, "embdr::cxxstd::BitmapAllocator capacity must be valid");

                using value_type = T;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                using iterator = BasicIterator<value_type>;
                using const_iterator = const BasicIterator<value_type>;
                using storage_type = BackingStorage<T, Sz>;
                static constexpr size_type capacity_value = Sz;
                static constexpr size_type bitmap_words = ((Sz + 63) >> 6);
                static constexpr size_type max_bitmaps = Sz;

                BitmapAllocator& operator=(const BitmapAllocator&) noexcept = delete;
                BitmapAllocator(const BitmapAllocator& other) noexcept = default;
                BitmapAllocator(BitmapAllocator&& other) noexcept = delete;
                BitmapAllocator& operator=(BitmapAllocator&& other) noexcept = delete;
                BitmapAllocator() noexcept;

                template <unsigned int N>
                pointer allocate() noexcept;
                void deallocate(pointer ptr) noexcept;
                ~BitmapAllocator() noexcept;
                iterator begin() const noexcept { return iterator(this->_M_storage.data()); }
                const_iterator begin() noexcept { return iterator(this->_M_storage.data()); }
                iterator end() noexcept { return iterator(this->_M_storage.data() + Sz); }
                const_iterator end() const noexcept { return iterator(this->_M_storage.data() + Sz); }
                storage_type raw() const noexcept { return this->_M_storage; }
                const_iterator cbegin() const noexcept { return static_cast<const_iterator>(this->begin()); }
                const_iterator cend() const noexcept { return static_cast<const_iterator>(this->end()); }
                size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(value_type); }
                size_type used() const noexcept { return this->_M_bitmap_count * sizeof(value_type); }
                size_type capacity() const noexcept { return static_cast<size_type>(Sz) * sizeof(value_type); }
                bool is_empty() const noexcept { return this->_M_bitmap_count == 0; }
                void reset() const;

            private:
                template <unsigned int N>
                pointer _S_handle_needed(size_type word_idx, size_type next_word, size_type start_bit,
                                         size_type bits_needed, size_type available) noexcept;
                storage_type _M_storage{};
                mutable uint64_t _M_allocation_bitmap[bitmap_words]{};
                mutable size_type _M_bitmap_count = 0;
        };

        export template <class T, unsigned int Sz>
        class BuddyAllocator {
            public:
                static_assert(!std::is_reference_v<T>,
                              "embdr::cxxstd::BuddyAllocator required a non-reference value type");
                static_assert(!std::is_void_v<T>, "embdr::cxxstd::BuddyAllocator cannot allocate void");
                static_assert(std::is_destructible_v<T>,
                              "embdr::cxxstd::BuddyAllocator required a destructible value type");
                static_assert(std::is_object_v<T>, "embdr::cxxstd::BuddyAllocator required an object type");
                static_assert(std::is_trivially_copyable_v<T>,
                              "embdr::cxxstd::BuddyAllocator required a trivially copyable value type");
                static_assert(Sz > 0, "embdr::cxxstd::BuddyAllocator capacity must be valid");

                using value_type = T;
                using pointer = value_type*;
                using const_pointer = const value_type*;
                using reference = value_type&;
                using const_reference = const value_type&;
                using size_type = size_t;
                using difference_type = ptrdiff_t;
                using iterator = BasicIterator<value_type>;
                using const_iterator = const BasicIterator<value_type>;
                using storage_type = BackingStorage<T, Sz>;
                using index_type = uint16_t;
                static constexpr size_type capacity_value = Sz;
                static constexpr index_type uninit = static_cast<index_type>(-1);

                BuddyAllocator& operator=(const BuddyAllocator&) noexcept = delete;
                BuddyAllocator(const BuddyAllocator& other) noexcept = default;
                BuddyAllocator(BuddyAllocator&& other) noexcept = delete;
                BuddyAllocator& operator=(BuddyAllocator&& other) noexcept = delete;
                BuddyAllocator() noexcept;

                template <unsigned int N>
                pointer allocate() noexcept;
                pointer allocate(size_type count) noexcept;
                void deallocate(pointer p) noexcept;
                ~BuddyAllocator() noexcept;
                iterator begin() const noexcept { return iterator(this->_M_storage.data()); }
                const_iterator begin() noexcept { return iterator(this->_M_storage.data()); }
                iterator end() noexcept { return iterator(this->_M_storage.data() + Sz); }
                const_iterator end() const noexcept { return iterator(this->_M_storage.data() + Sz); }
                storage_type raw() const noexcept { return this->_M_storage; }
                const_iterator cbegin() const noexcept { return static_cast<const_iterator>(this->begin()); }
                const_iterator cend() const noexcept { return static_cast<const_iterator>(this->end()); }
                size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(value_type); }
                size_type used() const noexcept { return this->_M_used_bytes; }
                size_type capacity() const noexcept { return static_cast<size_type>(Sz) * sizeof(value_type); }
                bool is_empty() const noexcept { return this->_M_used_bytes == 0; }
                void reset() const;

            private:
                static constexpr size_type _S_min_block_size() noexcept {
                        size_type value_size = sizeof(value_type);
                        size_type align = (63 << 1) + 32;
                        if (value_size < align)
                                return align;
                        else
                                return value_size;
                }
                static constexpr size_type _S_max_order() noexcept {
                        size_type min_block_size = _S_min_block_size();
                        size_type size = Sz * sizeof(value_type);
                        size_type max_order = 0;
                        while ((static_cast<size_type>(1) << max_order) * min_block_size < size)
                                ++max_order;
                        return max_order;
                }
                static constexpr size_type _S_max_block_count() noexcept  {
                        const size_type order = _S_max_order();
                        if (constexpr size_type size_bits = sizeof(size_type) << 3; order >= size_bits - 1)
                                return static_cast<size_type>(-1);
                        else
                                return static_cast<size_type>(1) << order;
                }
                static constexpr size_type _S_max_blocks = _S_max_block_count();
                static constexpr size_type _S_free_lists = _S_max_order() + 1;

                storage_type _M_storage{};
                struct block_owner {
                        index_type _M_begin;
                        index_type _M_end;
                        index_type _M_next_block;

                        block_owner() = default;
                        block_owner(block_owner&&) = delete;
                        block_owner(const block_owner& other) :
                            _M_begin(other._M_begin), _M_end(other._M_end), _M_next_block(other._M_next_block) {};
                        block_owner& operator=(block_owner&&) = delete;
                        block_owner& operator=(const block_owner&) = default;
                        explicit block_owner(const size_type begin, const size_type end) noexcept :
                            _M_begin(static_cast<index_type>(begin)), _M_end(static_cast<index_type>(end)),
                            _M_next_block(uninit) {}
                        size_type _S_size() const noexcept {
                                return static_cast<size_type>(this->_M_end - this->_M_begin) * sizeof(value_type);
                        }
                        bool _S_is_valid() const noexcept { return this->_M_begin != uninit && this->_M_end != uninit; }
                        bool _S_is_allocated() const noexcept { return this->_S_is_valid(); }
                        void _S_mark_deallocated() noexcept { this->_M_begin = this->_M_end = uninit; }
                        void _S_initialize(const size_type begin, const size_type end) noexcept {
                                this->_M_begin = static_cast<index_type>(begin);
                                this->_M_end = static_cast<index_type>(end);
                                this->_M_next_block = uninit;
                        }
                };
                struct buddy_state {
                        static constexpr index_type uninit = static_cast<index_type>(-1);
                        block_owner _M_block_storage[_S_max_blocks]{};
                        index_type _M_free_heads[_S_free_lists]{};
                        index_type _M_block_count;
                        index_type _M_free_head;

                        buddy_state() noexcept : _M_free_head(0) { this->_S_init_free_lists<0, _S_free_lists>(); }
                        template <size_type Start, size_type End>
                        void _S_init_free_lists() noexcept {
                                if constexpr (Start < End) {
                                        this->_M_free_heads[Start] = uninit;
                                        this->_S_init_free_lists<Start + 1, End>();
                                }
                        }
                        buddy_state(buddy_state&& other) noexcept = default;
                        buddy_state(const buddy_state& other) noexcept {
                                for (size_type i = 0; i < _S_max_blocks; ++i)
                                        this->_M_block_storage[i] = other._M_block_storage[i];
                                for (size_type i = 0; i < _S_free_lists; ++i)
                                        this->_M_free_heads[i] = other._M_free_heads[i];
                                this->_M_block_count = other._M_block_count;
                                this->_M_free_head = other._M_free_head;
                        }
                        buddy_state operator=(const buddy_state&) noexcept = delete;
                        buddy_state operator=(const buddy_state&&) noexcept = delete;
                        ~buddy_state() noexcept = default;
                };
                void _S_construct_allocator() noexcept;
                mutable size_type _M_used_bytes = 0;
                mutable buddy_state _M_buddy{};

                static bool _S_is_valid_block(const block_owner* block) noexcept;
                static bool _S_is_block_allocated(const block_owner* block) noexcept;
                template <size_type Size>
                static size_type _S_order_of_size() noexcept {
                        static_assert(Size > 0, "embdr::cxxstd::BuddyAllocator invalid size");
                        size_type order = 0;
                        size_type min_block_size = _S_min_block_size();
                        const size_type max_order = _S_max_order();
                        size_type block_size = min_block_size;
                        while (block_size < Size && order < max_order) {
                                block_size <<= 1;
                                ++order;
                        }
                        return order;
                }
                template <size_type Start, size_type End, size_type Order, size_type BlockIndex = 0>
                size_type _S_do_find_free_block() noexcept {
                        if (Start < End) {
                                size_type curr_order = End - 1 - Start;
                                if (const size_type free_head = this->_M_buddy._M_free_heads[Start];
                                    free_head == BlockIndex) {
                                        this->_S_remove_from_free_list<BlockIndex, curr_order>();
                                        if (curr_order > Order)
                                                this->_S_split_block<BlockIndex, curr_order, Order>();
                                        return BlockIndex;
                                }
                                if (BlockIndex + 1 < _S_max_blocks)
                                        return this->_S_do_find_free_block<Start, End, Order, BlockIndex + 1>();
                                else
                                        return this->_S_do_find_free_block<Start + 1, End, Order, 0>();
                        }
                        return buddy_state::uninit;
                }
                template <size_type Order>
                index_type _S_find_free_block() noexcept {
                        size_type max_order = this->_S_max_order();
                        return this->_S_do_find_free_block<0, max_order + 1, Order>();
                }
                index_type _S_find_free_block(size_type order) noexcept;
                index_type _S_do_find_free_block(size_type start, size_type end, size_type order,
                                                 index_type block_index) noexcept;
                template <size_type BlockIndex, size_type CurrentOrder, size_type TargetOrder>
                void _S_do_split_block() noexcept {
                        if (CurrentOrder > TargetOrder) {
                                block_owner& block = this->_M_buddy._M_block_storage[BlockIndex];
                                const auto new_size = block._S_size() >> 1;
                                const size_type new_block_index = this->_M_buddy._S_block_count;
                                ++this->_M_buddy._M_block_count;
                                block_owner& buddy = this->_M_buddy._M_block_storage[new_block_index];
                                auto split_point = block._M_begin + (new_size / sizeof(value_type));
                                buddy._S_initialize(split_point, block._M_end);
                                block._M_end = split_point;
                                this->_S_add_to_free_list<CurrentOrder - 1>(new_block_index);
                                this->_S_do_split_block<BlockIndex, CurrentOrder - 1, TargetOrder>();
                        }
                }

                template <size_type BlockIndex, size_type CurrentOrder, size_type TargetOrder>
                void _S_split_block() noexcept {
                        static_assert(BlockIndex < _S_max_blocks, "embdr::cxxstd::BuddyAllocator index out of range");
                        static_assert(TargetOrder <= this->_S_max_order(),
                                      "embdr::cxxstd::BuddyAllocator target order exceeds max order");
                        if (!this->_M_buddy._M_block_storage[BlockIndex]._S_is_valid())
                                return;
                        else
                                this->_S_do_split_block<BlockIndex, CurrentOrder, TargetOrder>();
                }
                void _S_split_block(index_type block_index, size_type current_order, size_type target_order) noexcept;
                void _S_do_split_block(index_type block_index, size_type current_order,
                                       size_type target_order) noexcept;
                template <size_type Start, size_type End, size_type Order, size_type BlockIndex>
                void _S_do_coalesce_block() noexcept {
                        if (Start < End) {
                                block_owner& block = this->_M_buddy._M_block_storage[BlockIndex];
                                if (!this->_S_is_valid_block(&block))
                                        return;
                                const uintptr_t block_addr =
                                    reinterpret_cast<uintptr_t>(static_cast<size_type>(block._M_begin));
                                const size_type block_size = block._S_size();
                                if (block_size == 0)
                                        return;
                                const uintptr_t buddy_addr = block_addr ^ block_size;
                                const index_type buddy_idx = this->_S_find_block_pointer(buddy_addr);
                                if (buddy_idx == buddy_state::uninit)
                                        return;
                                if (block_owner& buddy = this->_M_buddy._M_block_storage[buddy_idx];
                                    buddy._S_size() != block_size)
                                        return;
                                this->_S_remove_from_free_list<buddy_idx, Order>();
                                const size_type new_size = block_size << 1;
                                block._M_end = block._M_begin + static_cast<index_type>(new_size / sizeof(value_type));
                                this->_S_do_coalesce_block<Start + 1, End, Order + 1, BlockIndex>();
                        }
                }
                template <size_type BlockIndex>
                void _S_coalesce_block() noexcept {
                        static_assert(BlockIndex < _S_max_blocks, "embdr::cxxstd::BuddyAllocator index out of range");
                        this->_S_coalesce_block(BlockIndex);
                }
                void _S_coalesce_block(const index_type block_index) noexcept { this->_S_coalesce_block(block_index); }
                template <size_type BlockIndex, size_type Order>
                void _S_add_to_free_list() noexcept {
                        block_owner& block = this->_M_buddy._M_block_storage[BlockIndex];
                        static_assert(!std::is_reference_v<block_owner>,
                                      "embdr::cxxstd::BuddyAllocator invalid block state");
                        size_type free_list_index = _S_max_order() - Order;
                        block._M_next_block = this->_M_buddy._M_free_heads[free_list_index];
                        this->_M_buddy._M_free_heads[free_list_index] = BlockIndex;
                }
                template <size_type Order>
                void _S_add_to_free_list(const size_type block_index) noexcept {
                        block_owner& block = this->_M_buddy._M_block_storage[block_index];
                        static_assert(!std::is_reference_v<block_owner>,
                                      "embdr::cxxstd::BuddyAllocator invalid block state");
                        size_type free_list_index = this->_S_max_order() - Order;
                        block._M_next_block = this->_M_buddy._M_free_heads[free_list_index];
                        this->_M_buddy._M_free_heads[free_list_index] = block_index;
                }
                template <uintptr_t BlockPtr>
                size_type _S_find_block_pointer() const noexcept;
                template <size_type BlockIndex, size_type Order>
                void _S_remove_from_free_list() const noexcept {
                        block_owner& block = this->_M_buddy._M_block_storage[BlockIndex];
                        size_type free_list_index = _S_max_order() - Order;
                        if (this->_M_buddy._M_free_heads[free_list_index] == BlockIndex)
                                this->_M_buddy._M_free_heads[free_list_index] = block._M_next_block;
                        else {
                                index_type prev = this->_M_buddy._M_free_heads[free_list_index];
                                while (prev != buddy_state::uninit &&
                                       this->_M_buddy._M_block_storage[prev]._M_next_block != BlockIndex)
                                        prev = this->_M_buddy._M_block_storage[prev]._M_next_block;
                                if (prev != buddy_state::uninit)
                                        this->_M_buddy._M_block_storage[prev]._M_next_block = block._M_next_block;
                        }
                }
                void _S_remove_from_free_list(index_type block_index, const size_type order) noexcept;
                void _S_add_to_free_list(index_type block_index, const size_type order) noexcept;
        };
        template <typename T, int Sz>
        BackingStorage<T, Sz>::BackingStorage() noexcept = default;

        template <typename T, int Sz>
        BackingStorage<T, Sz>::~BackingStorage() noexcept = default;

        template <typename T>
        BackingStorageSingle<T>::BackingStorageSingle() noexcept = default;

        template <typename T>
        BackingStorageSingle<T>::~BackingStorageSingle() noexcept = default;

        template <class T, unsigned int Sz>
        template <unsigned int N>
                requires(N > 0 && N <= Sz)
        BumpAllocator<T, Sz>::pointer BumpAllocator<T, Sz>::allocate() noexcept {
                static_assert(N <= Sz, "embdr::cxxstd::BumpAllocator size cannot exceed capacity");
                const size_type offset = this->_M_offset;
                this->_M_offset += N;
                if (offset + N > Sz)
                        return nullptr;
                return this->_M_storage.data() + offset;
        }
        template <class T, unsigned int Sz>
        BumpAllocator<T, Sz>::pointer BumpAllocator<T, Sz>::allocate(const size_type count) noexcept {
                if (count != 0) {
                        if (count > static_cast<size_type>(Sz - this->_M_offset)) {
                                return nullptr;
                        }
                        const size_type offset = this->_M_offset;
                        this->_M_offset += count;
                        return this->_M_storage.data() + offset;
                } else {
                        return nullptr;
                }
        }
        template <class T, unsigned int Sz>
        void BumpAllocator<T, Sz>::deallocate(pointer) noexcept {};

        template <class T, unsigned int Sz>
        BumpAllocator<T, Sz>::~BumpAllocator() noexcept = default;

        template <class T, unsigned int Sz>
        void BumpAllocator<T, Sz>::reset() const {
                this->~BumpAllocator();
                ::new (static_cast<void*>(const_cast<BumpAllocator*>(this))) BumpAllocator();
        };
        template <class T, unsigned int Sz>
        template <unsigned int N>
        BitmapAllocator<T, Sz>::pointer BitmapAllocator<T, Sz>::allocate() noexcept {
                static_assert(N > 0 && N < Sz, "embdr::cxxstd::BitmapAllocator invalid size");
                for (size_type word_idx = 0; word_idx < bitmap_words; ++word_idx) {
                        uint64_t word = this->_M_allocation_bitmap[word_idx];
                        if (word == UINT64_MAX)
                                continue;
                        const size_type start_bit = __builtin_ctzll(~word);
                        size_type bits_needed = N;
                        const size_type available = 64 - start_bit;
                        if (available >= bits_needed) {
                                uint64_t mask = 0;
                                if (bits_needed >= 64)
                                        mask = UINT64_MAX << start_bit;
                                else
                                        mask = ((UINT64_C(1) << bits_needed) - 1) << start_bit;
                                this->_M_allocation_bitmap[word_idx] |= mask;
                                this->_M_bitmap_count += N;
                                size_type element_idx = word_idx * 64 + start_bit;
                                return this->_M_storage.data() + element_idx;
                        }
                        size_type consecutive = available;
                        for (size_type next_word = word_idx + 1; next_word < bitmap_words && consecutive < bits_needed;
                             ++next_word) {
                                const uint64_t next_word_val = this->_M_allocation_bitmap[next_word];
                                size_type next_bits = __builtin_ctzll(~next_word_val);
                                if (next_bits == 0) {
                                        consecutive += 64;
                                        if (consecutive >= bits_needed) {
                                                return this->_S_handle_needed<N>(word_idx, next_word, start_bit,
                                                                                 bits_needed, available);
                                        }
                                        continue;
                                }
                                consecutive += next_bits;
                                if (consecutive >= bits_needed)
                                        return this->_S_handle_needed<N>(word_idx, next_word, start_bit, bits_needed,
                                                                         available);
                        }
                }
                return nullptr;
        }
        template <class T, unsigned int Sz>
        void BitmapAllocator<T, Sz>::deallocate(pointer ptr) noexcept {
                const uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
                const uintptr_t base_addr = reinterpret_cast<uintptr_t>(this->_M_storage.data());
                if (ptr_addr < base_addr)
                        return;
                const size_type element_idx = (ptr_addr - base_addr) / sizeof(value_type);
                if (element_idx >= max_bitmaps)
                        return;
                const size_type word_idx = element_idx >> 6;
                const size_type bit_idx = element_idx & 63;
                const uint64_t word = this->_M_allocation_bitmap[word_idx];
                if ((word & (UINT64_C(1) << bit_idx)) == 0)
                        return;
                size_type count = 1;
                while (bit_idx + count < 64 && (word & (UINT64_C(1) << (bit_idx + count))) != 0)
                        ++count;
                if (bit_idx + count == 64) {
                        for (size_type w = word_idx + 1; w < bitmap_words; ++w) {
                                const uint64_t next_word = this->_M_allocation_bitmap[w];
                                size_type next_count = 0;
                                while (next_count < 64 && (next_word & (UINT64_C(1) << next_count)) != 0)
                                        ++next_count;
                                count += next_count;
                                if (next_count < 64)
                                        break;
                        }
                }
                const uint64_t mask = (count == 64) ? UINT64_MAX : ((UINT64_C(1) << count) - 1) << bit_idx;
                this->_M_allocation_bitmap[word_idx] &= ~mask;
                if (bit_idx + count > 64) {
                        size_type remaining = bit_idx + count - 64;
                        for (size_type w = word_idx + 1; w < bitmap_words && remaining > 0; ++w) {
                                const uint64_t next_mask =
                                    (remaining >= 64) ? UINT64_MAX : ((UINT64_C(1) << remaining) - 1);
                                this->_M_allocation_bitmap[w] &= ~next_mask;
                                if (remaining > 64)
                                        remaining -= 64;
                                else
                                        remaining = 0;
                        }
                }
                this->_M_bitmap_count -= count;
        }
        template <class T, unsigned int Sz>
        BitmapAllocator<T, Sz>::~BitmapAllocator() noexcept = default;

        template <class T, unsigned int Sz>
        template <unsigned int N>
        BitmapAllocator<T, Sz>::pointer
        BitmapAllocator<T, Sz>::_S_handle_needed(const size_type word_idx, const size_type next_word,
                                                     const size_type start_bit, const size_type bits_needed,
                                                     const size_type available) noexcept {
                uint64_t first_mask = UINT64_MAX << start_bit;
                this->_M_allocation_bitmap[word_idx] |= first_mask;
                for (size_type w = word_idx + 1; w < next_word; ++w)
                        this->_M_allocation_bitmap[w] = UINT64_MAX;
                const size_type remaining_bits = bits_needed - available;
                uint64_t last_mask = 0;
                if (remaining_bits >= 64)
                        last_mask = UINT64_MAX;
                else
                        last_mask = (UINT64_C(1) << remaining_bits) - 1;
                this->_M_allocation_bitmap[next_word] |= last_mask;
                this->_M_bitmap_count += N;
                size_type element_idx = (word_idx << 6) + start_bit;
                return this->_M_storage.data() + element_idx;
        }
        template <class T, unsigned int Sz>
        BuddyAllocator<T, Sz>::BuddyAllocator() noexcept {
                this->_S_construct_allocator();
        }

        template <class T, unsigned int Sz>
        template <unsigned int N>
        BuddyAllocator<T, Sz>::pointer BuddyAllocator<T, Sz>::allocate() noexcept {
                static_assert(N <= Sz, "embdr::cxxstd::BuddyAllocator invalid size");
                return this->allocate(static_cast<size_type>(N));
        }

        template <class T, unsigned int Sz>
        BuddyAllocator<T, Sz>::pointer BuddyAllocator<T, Sz>::allocate(const size_type count) noexcept {
                if (count != 0) {
                        if (count > Sz)
                                return nullptr;
                        const size_type min_block_size = _S_min_block_size();
                        size_type max_order = _S_max_order();
                        size_type order = 0;
                        size_type block_size = min_block_size;
                        while (block_size < count * sizeof(value_type) && order < max_order) {
                                block_size <<= 1;
                                ++order;
                        }
                        if (order > max_order)
                                return nullptr;
                        index_type block_idx = this->_S_find_free_block(order);
                        if (block_idx == buddy_state::uninit)
                                return nullptr;
                        block_owner& block = this->_M_buddy._M_block_storage[block_idx];
                        if (!_S_is_valid_block(&block))
                                return nullptr;
                        this->_M_used_bytes += block._S_size();
                        this->_S_remove_from_free_list(block_idx, order);
                        return this->_M_storage.data() + static_cast<size_type>(block._M_begin);
                }
                return nullptr;
        }

        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::deallocate(pointer p) noexcept {
                if (p == nullptr)
                        return;
                const uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(p);
                const uintptr_t base_addr = reinterpret_cast<uintptr_t>(this->_M_storage.data());
                for (size_type i = 0; i < this->_M_buddy._S_block_count; ++i) {
                        block_owner& block = this->_M_buddy._M_block_storage[i];
                        if (!block._S_is_allocated())
                                continue;
                        const uintptr_t block_start = base_addr + (block._M_begin * sizeof(value_type));
                        const uintptr_t block_end = base_addr + (block._M_end * sizeof(value_type));
                        if (ptr_addr >= block_start && ptr_addr < block_end) {
                                const size_type block_size = block._S_size();
                                block._S_mark_deallocated();
                                this->_M_used_bytes -= block_size;
                                this->_S_coalesce_block(i);
                                return;
                        }
                }
        }

        template <class T, unsigned int Sz>
        BuddyAllocator<T, Sz>::~BuddyAllocator() noexcept = default;

        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::reset() const {
                this->~BuddyAllocator();
                ::new (static_cast<void*>(const_cast<BuddyAllocator*>(this))) BuddyAllocator();
        }
        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::_S_construct_allocator() noexcept {
                size_type initial_idx = 0;
                this->_M_buddy._M_block_count = initial_idx + 1;
                block_owner& initial_block = this->_M_buddy._M_block_storage[initial_idx];
                initial_block._S_initialize(0, Sz);
                initial_block._M_next_block = buddy_state::uninit;
                this->_S_add_to_free_list<0, _S_max_order()>();
        }
        template <class T, unsigned int Sz>
        bool BuddyAllocator<T, Sz>::_S_is_valid_block(const block_owner* block) noexcept {
                if (block == nullptr)
                        return false;
                else
                        return block->_S_is_valid();
        }
        template <class T, unsigned int Sz>
        bool BuddyAllocator<T, Sz>::_S_is_block_allocated(const block_owner* block) noexcept {
                if (!_S_is_valid_block(block))
                        return false;
                else
                        return block->_S_is_allocated();
        }
        template <class T, unsigned int Sz>
        BuddyAllocator<T, Sz>::index_type
        BuddyAllocator<T, Sz>::_S_find_free_block(const size_type order) noexcept {
                size_type max_order = this->_S_max_order();
                return this->_S_do_find_free_block(0, max_order + 1, order, 0);
        }
        template <class T, unsigned int Sz>
        BuddyAllocator<T, Sz>::index_type
        BuddyAllocator<T, Sz>::_S_do_find_free_block(size_type start, const size_type end, const size_type order,
                                                         const index_type block_index) noexcept {
                if (start < end) {
                        size_type curr_order = end - 1 - start;
                        if (const index_type free_head = this->_M_buddy._M_free_heads[start];
                            free_head == block_index) {
                                this->_S_remove_from_free_list(block_index, curr_order);
                                if (curr_order > order)
                                        this->_S_split_block(block_index, curr_order, order);
                                return block_index;
                        }
                        if (block_index + 1 < _S_max_blocks)
                                return this->_S_do_find_free_block(start, end, order, block_index + 1);
                        else
                                return this->_S_do_find_free_block(start + 1, end, order, 0);
                }
                return buddy_state::uninit;
        }
        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::_S_split_block(index_type block_index, size_type current_order,
                                                       size_type target_order) noexcept {
                if (block_index >= _S_max_blocks || target_order > this->_S_max_order())
                        return;
                if (!this->_M_buddy._M_block_storage[block_index]._S_is_valid())
                        return;
                this->_S_do_split_block(block_index, current_order, target_order);
        }
        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::_S_do_split_block(index_type block_index, size_type current_order,
                                                          size_type target_order) noexcept {
                while (current_order > target_order) {
                        block_owner& block = this->_M_buddy._M_block_storage[block_index];
                        const auto new_size = block._S_size() >> 1;
                        const index_type new_block_index = this->_M_buddy._M_block_count;
                        ++this->_M_buddy._M_block_count;
                        block_owner& buddy = this->_M_buddy._M_block_storage[new_block_index];
                        auto split_point = block._M_begin + static_cast<index_type>(new_size / sizeof(value_type));
                        buddy._S_initialize(split_point, block._M_end);
                        block._M_end = split_point;
                        this->_S_add_to_free_list(new_block_index, current_order - 1);
                        current_order--;
                }
        }
        template <class T, unsigned int Sz>
        template <uintptr_t BlockPtr>
        BuddyAllocator<T, Sz>::size_type BuddyAllocator<T, Sz>::_S_find_block_pointer() const noexcept {
                for (size_type i = 0; i < this->_M_buddy._S_block_count; ++i) {
                        const block_owner& block = this->_M_buddy._M_block_storage[i];
                        if (!block._S_is_allocated())
                                continue;
                        if (const uintptr_t block_start = reinterpret_cast<uintptr_t>(block._M_begin);
                            BlockPtr == block_start)
                                return i;
                }
                return buddy_state::uninit;
        }
        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::_S_remove_from_free_list(index_type block_index,
                                                                 const size_type order) noexcept {
                block_owner& block = this->_M_buddy._M_block_storage[block_index];
                const size_type free_list_index = _S_max_order() - order;
                if (this->_M_buddy._M_free_heads[free_list_index] == block_index)
                        this->_M_buddy._M_free_heads[free_list_index] = block._M_next_block;
                else {
                        index_type prev = this->_M_buddy._M_free_heads[free_list_index];
                        while (prev != buddy_state::uninit &&
                               this->_M_buddy._M_block_storage[prev]._M_next_block != block_index)
                                prev = this->_M_buddy._M_block_storage[prev]._M_next_block;
                        if (prev != buddy_state::uninit)
                                this->_M_buddy._M_block_storage[prev]._M_next_block = block._M_next_block;
                }
        }
        template <class T, unsigned int Sz>
        void BuddyAllocator<T, Sz>::_S_add_to_free_list(index_type block_index, const size_type order) noexcept {
                block_owner& block = this->_M_buddy._M_block_storage[block_index];
                const size_type free_list_index = _S_max_order() - order;
                block._M_next_block = this->_M_buddy._M_free_heads[free_list_index];
                this->_M_buddy._M_free_heads[free_list_index] = block_index;
        }
} // namespace embdr::cxxstd
