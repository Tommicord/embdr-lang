/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.cxxstd.unwind;
import embdr.cxxstd.unwindPrettify;

namespace {
        using namespace embdr::cxxstd;

        UnwindFrame make_frame(const uintptr_t ip, const uintptr_t sp) {
                UnwindFrame frame;
                frame._M_ip = ip;
                frame._M_sp = sp;
                return frame;
        }

        UnwindFrame make_frame(const uintptr_t ip, const uintptr_t sp, const uintptr_t base) {
                UnwindFrame frame = make_frame(ip, sp);
                frame._M_module_base = Maybe<uintptr_t, FrameUnwindError>(base);
                return frame;
        }
} // namespace

TEST_CASE("write_frame includes module base offset", "[unwindPrettify]") {
        const UnwindFrame frame = make_frame(0x12345678, 0x9abcdef0, 0x12345000);
        FrameSink<256> out;
        write_frame(out, frame, 0);
        REQUIRE(out.view().contains("ip=0x0000000012345678"));
        REQUIRE(out.view().contains("sp=0x000000009abcdef0"));
        REQUIRE(out.view().contains("+0x678"));
}

TEST_CASE("write_frame omits offset without module base", "[unwindPrettify]") {
        const UnwindFrame frame = make_frame(0xdeadbeef, 0xcafebabe);
        FrameSink<256> out;
        write_frame(out, frame, 1);
        REQUIRE(out.view().contains("ip=0x00000000deadbeef"));
        REQUIRE(out.view().contains("sp=0x00000000cafebabe"));
        REQUIRE(!out.view().contains('+'));
}

TEST_CASE("write_frames separates frames with newlines", "[unwindPrettify]") {
        const UnwindFrame frames[] = {make_frame(0x1000, 0x2000), make_frame(0x3000, 0x4000)};
        FrameSink<512> out;
        write_frames(out, std::span<const UnwindFrame>(frames, 2));
        const SimpleStringView formatted = out.view();
        const size_t first_nl = formatted.find("\n");
        REQUIRE(first_nl != SimpleStringView::not_found);
        REQUIRE(formatted.find("\n", first_nl + 1) == SimpleStringView::not_found);
        const SimpleStringView first_line = formatted.substr(0, first_nl);
        const SimpleStringView second_line = formatted.substr(first_nl + 1);
        REQUIRE(first_line.starts_with("#"));
        REQUIRE(first_line.contains("ip=0x0000000000001000"));
        REQUIRE(second_line.contains("ip=0x0000000000003000"));
}

TEST_CASE("write_symbol_address formats 16 hex digits", "[unwindPrettify]") {
        const UnwindFrame frame = make_frame(0x1000, 0x2000);
        FrameSink<64> out;
        write_symbol_address(out, frame);
        REQUIRE(out.view().contains("0000000000001000"));
}

TEST_CASE("write_frame pads the index to three columns", "[unwindPrettify]") {
        const UnwindFrame frame = make_frame(0x1, 0x2);
        FrameSink<128> out;
        write_frame(out, frame, 42);
        REQUIRE(out.view().contains("   42:"));
}

TEST_CASE("write_frame truncates safely when the sink is full", "[unwindPrettify]") {
        const UnwindFrame frame = make_frame(0x1, 0x2);
        FrameSink<4> out;
        write_frame(out, frame, 0);
        REQUIRE(out.size() == 4);
}

TEST_CASE("write_frames formats captured frames", "[unwindPrettify]") {
        UnwindFrame frames[8];
        const size_t count = unwind_capture_frames(frames, 8);
        REQUIRE(count >= 3);
        FrameSink<1024> out;
        write_frames(out, std::span<const UnwindFrame>(frames, count));
        REQUIRE(out.view().contains("#"));
        REQUIRE(out.view().contains("ip=0x"));
        REQUIRE(out.view().contains("sp=0x"));
}
