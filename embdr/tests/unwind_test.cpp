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

import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.unwind;

namespace
{
    using namespace embdr::cxxstd;

    __attribute__((noinline)) size_t unwind_capture_bottom(UnwindFrame* const out, const size_t cap)
    {
        return unwind_capture_frames(out, cap);
    }

    __attribute__((noinline)) size_t unwind_capture_middle(UnwindFrame* const out, const size_t cap)
    {
        return unwind_capture_bottom(out, cap);
    }

    __attribute__((noinline)) size_t unwind_capture_top(UnwindFrame* const out, const size_t cap)
    {
        return unwind_capture_middle(out, cap);
    }
} // namespace

TEST_CASE("unwind_capture_frames captures the nested call chain", "[unwind]")
{
    UnwindFrame frames[32];
    const size_t count = unwind_capture_top(frames, 32);
    REQUIRE(count >= 3);
    for (size_t i = 0; i < count; ++i)
        {
            REQUIRE(frames[i]._M_ip != 0);
            REQUIRE(frames[i]._M_sp != 0);
            REQUIRE(frames[i].symbol_address() == frames[i]._M_ip);
        }
    // The walk only emits a frame after the stack pointer advanced
    // towards the caller, so the captured values strictly increase.
    for (size_t i = 1; i < count; ++i)
        REQUIRE(frames[i]._M_sp > frames[i - 1]._M_sp);
    REQUIRE(frames[0]._M_module_base.has_value());
    REQUIRE(!frames[0]._M_module_base.has_error());
}

TEST_CASE("unwind_capture_frames honours the frame cap", "[unwind]")
{
    UnwindFrame frames[8];
    REQUIRE(unwind_capture_frames(frames, 0) == 0);
    REQUIRE(unwind_capture_frames(nullptr, 8) == 0);
    REQUIRE(unwind_capture_frames(frames, 1) == 1);
    REQUIRE(frames[0]._M_ip != 0);
}

TEST_CASE("unwind_trace visits frames with consecutive indices", "[unwind]")
{
    size_t calls = 0;
    bool ordered = true;
    unwind_trace(
        [&calls, &ordered](const UnwindFrame& frame, const size_t index) noexcept
            {
                if (index != calls)
                    ordered = false;
                if (frame._M_ip == 0)
                    ordered = false;
                ++calls;
            });
    REQUIRE(ordered);
    REQUIRE(calls >= 3);
    REQUIRE(calls <= unwind_max_frames);
}

TEST_CASE("unwind_module_base resolves a code address", "[unwind]")
{
    const uintptr_t self = reinterpret_cast<uintptr_t>(&unwind_capture_frames);
    const Maybe<uintptr_t, FrameUnwindError> base = unwind_module_base(self);
    REQUIRE(base.has_value());
    REQUIRE(base.value() != 0);
    REQUIRE(self >= base.value());

    const SimpleStringView name = unwind_module_name(base.value());
    REQUIRE(!name.empty());
}

TEST_CASE("unwind_module_base rejects an unmapped address", "[unwind]")
{
    const Maybe<uintptr_t, FrameUnwindError> bad = unwind_module_base(1);
    REQUIRE(bad.has_error());
    REQUIRE(bad.error() == FrameUnwindError::invalidAddr);
}

TEST_CASE("the unwind reentrancy guard does not nest", "[unwind]")
{
    REQUIRE(!unwind_in_handler());
    REQUIRE(unwind_try_enter());
    REQUIRE(unwind_in_handler());
    REQUIRE(!unwind_try_enter());
    unwind_leave();
    REQUIRE(!unwind_in_handler());
    // A second leave keeps the guard released instead of underflowing.
    unwind_leave();
    REQUIRE(!unwind_in_handler());
}
