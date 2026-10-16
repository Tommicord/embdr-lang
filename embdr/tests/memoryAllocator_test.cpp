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

#include <catch2/catch_all.hpp>

#include <cstdint>

import embdr.cxxstd.memoryAllocator;
import embdr.cxxstd.memoryMaybe;

using namespace embdr::cxxstd;

namespace
{
  struct alignas (32) OverAlignedWord
  {
    uint64_t _M_value;
  };

  template <typename T, unsigned int Sz>
  bool
  in_pool (const TlsfAllocator<T, Sz>& alloc, const T* p) noexcept
  {
    const auto begin = reinterpret_cast<uintptr_t> (alloc.begin ().get ());
    const auto end = begin + alloc.capacity ();
    const auto addr = reinterpret_cast<uintptr_t> (p);
    return addr >= begin && addr < end;
  }
} // namespace

TEST_CASE ("TlsfAllocator serves aligned allocations from an empty pool", "[memoryAllocator]")
{
  TlsfAllocator<uint64_t, 256> alloc;
  REQUIRE (alloc.is_empty ());
  REQUIRE (alloc.capacity () == 256 * sizeof (uint64_t));

  auto first = alloc.allocate (4);
  REQUIRE (first.has_value ());
  REQUIRE (reinterpret_cast<uintptr_t> (first.value ()) % alignof (uint64_t) == 0);
  REQUIRE (in_pool (alloc, first.value ()));
  REQUIRE_FALSE (alloc.is_empty ());
  REQUIRE (alloc.used () >= 4 * sizeof (uint64_t));

  auto second = alloc.allocate (8);
  REQUIRE (second.has_value ());
  REQUIRE (second.value () != first.value ());

  alloc.deallocate (first.value ());
  alloc.deallocate (second.value ());
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator keeps over-aligned value types aligned", "[memoryAllocator]")
{
  TlsfAllocator<OverAlignedWord, 64> alloc;
  auto slot = alloc.allocate (2);
  REQUIRE (slot.has_value ());
  REQUIRE (reinterpret_cast<uintptr_t> (slot.value ()) % alignof (OverAlignedWord) == 0);
  alloc.deallocate (slot.value ());
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator reports invalid requests as error values", "[memoryAllocator]")
{
  TlsfAllocator<uint64_t, 64> alloc;

  auto zero = alloc.allocate (0);
  REQUIRE (zero.has_error ());
  REQUIRE (zero.error ().kind () == AllocError::Kind::ZERO_SIZE);

  auto too_large = alloc.allocate (alloc.capacity ());
  REQUIRE (too_large.has_error ());
  REQUIRE (too_large.error ().kind () == AllocError::Kind::TOO_LARGE);
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator reports exhaustion and is fully reusable after frees", "[memoryAllocator]")
{
  TlsfAllocator<uint8_t, 128> alloc;
  Maybe<uint8_t*, AllocError> slots[8];
  size_t successes = 0;
  while (successes < 8)
    {
      auto slot = alloc.allocate (1);
      if (slot.has_error ())
        {
          REQUIRE (slot.error ().kind () == AllocError::Kind::OUT_OF_MEMORY);
          break;
        }
      slots[successes] = slot;
      ++successes;
    }
  REQUIRE (successes > 0);

  for (size_t i = 0; i < successes; ++i)
    alloc.deallocate (slots[i].value ());
  REQUIRE (alloc.is_empty ());

  // The pool must be fully reusable once everything is freed.
  auto again = alloc.allocate (1);
  REQUIRE (again.has_value ());
}

TEST_CASE ("TlsfAllocator splits blocks and coalesces neighbours on free", "[memoryAllocator]")
{
  TlsfAllocator<uint32_t, 128> alloc;
  auto a = alloc.allocate (4);
  auto b = alloc.allocate (4);
  auto c = alloc.allocate (4);
  REQUIRE (a.has_value ());
  REQUIRE (b.has_value ());
  REQUIRE (c.has_value ());

  // Free the middle block; a smaller request must reuse the hole.
  alloc.deallocate (b.value ());
  const auto hole_used = alloc.used ();
  auto reuse = alloc.allocate (1);
  REQUIRE (reuse.has_value ());
  REQUIRE (in_pool (alloc, reuse.value ()));
  REQUIRE (alloc.used () > hole_used);

  alloc.deallocate (reuse.value ());
  alloc.deallocate (a.value ());
  alloc.deallocate (c.value ());
  REQUIRE (alloc.is_empty ());

  // A request that only fits a fully coalesced pool proves the four
  // neighbours (a, the reused hole, c and the tail) merged into one block.
  auto big = alloc.allocate (96);
  REQUIRE (big.has_value ());
  alloc.deallocate (big.value ());
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator drops foreign and double frees without state changes", "[memoryAllocator]")
{
  TlsfAllocator<uint64_t, 64> alloc;
  auto slot = alloc.allocate (4);
  REQUIRE (slot.has_value ());
  const auto used_after_alloc = alloc.used ();

  alignas (8) static uint64_t outside[4] = {1, 2, 3, 4};
  alloc.deallocate (outside);
  alloc.deallocate (slot.value () + 1);
  REQUIRE (alloc.used () == used_after_alloc);

  alloc.deallocate (slot.value ());
  REQUIRE (alloc.is_empty ());
  alloc.deallocate (slot.value ());
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator supports compile-time sized allocations", "[memoryAllocator]")
{
  TlsfAllocator<uint64_t, 128> alloc;
  auto slot = alloc.allocate<8> ();
  REQUIRE (slot.has_value ());
  REQUIRE (reinterpret_cast<uintptr_t> (slot.value ()) % alignof (uint64_t) == 0);
  REQUIRE (alloc.used () >= 8 * sizeof (uint64_t));
  alloc.deallocate (slot.value ());
  REQUIRE (alloc.is_empty ());
}

TEST_CASE ("TlsfAllocator reset restores the empty pool", "[memoryAllocator]")
{
  TlsfAllocator<uint64_t, 64> alloc;
  auto slot = alloc.allocate (4);
  REQUIRE (slot.has_value ());
  REQUIRE_FALSE (alloc.is_empty ());

  alloc.reset ();
  REQUIRE (alloc.is_empty ());
  REQUIRE (alloc.used () == 0);

  auto again = alloc.allocate (4);
  REQUIRE (again.has_value ());
  alloc.deallocate (again.value ());
  REQUIRE (alloc.is_empty ());
}
