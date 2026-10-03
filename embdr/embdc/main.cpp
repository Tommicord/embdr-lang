#include <cstdio>

import embdr.cxxstd.memoryAllocator;
import embdr.cxxstd.unwind;

using namespace embdr;

#if defined(__GNUC__) || defined(__clang__)
#        define EMBDR_TEST_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#        define EMBDR_TEST_NOINLINE __declspec(noinline)
#else
#        define EMBDR_TEST_NOINLINE
#endif

namespace {

int g_failures = 0;

void check(const bool condition, const char* what) {
        if (!condition) {
                std::printf("FAIL: %s\n", what);
                ++g_failures;
        }
}

EMBDR_TEST_NOINLINE size_t level3(cxxstd::UnwindFrame* frames, const size_t cap) {
        return cxxstd::unwind_capture_frames(frames, cap);
}

EMBDR_TEST_NOINLINE size_t level2(cxxstd::UnwindFrame* frames, const size_t cap) { return level3(frames, cap); }

EMBDR_TEST_NOINLINE size_t level1(cxxstd::UnwindFrame* frames, const size_t cap) { return level2(frames, cap); }

} // namespace

int main() {
        check(!cxxstd::unwind_in_handler(), "guard starts clear");
        check(cxxstd::unwind_try_enter(), "guard acquisition succeeds");
        check(cxxstd::unwind_in_handler(), "guard set after enter");
        check(!cxxstd::unwind_try_enter(), "guard rejects reentry");
        cxxstd::unwind_leave();
        check(!cxxstd::unwind_in_handler(), "guard clear after leave");

        cxxstd::UnwindFrame frames[cxxstd::unwind_max_frames] = {};
        const size_t count = level1(frames, cxxstd::unwind_max_frames);
        std::printf("captured %zu frames\n", count);
        check(count >= 3, "at least three frames captured");
        for (size_t i = 0; i < count; ++i) {
                check(frames[i]._M_ip != 0, "frame ip nonzero");
                check(frames[i]._M_sp != 0, "frame sp nonzero");
                if (i > 0)
                        check(frames[i]._M_sp > frames[i - 1]._M_sp, "stack pointer grows toward caller");
        }
        if (count > 0)
                check(frames[0]._M_module_base.has_value(), "module base resolved for innermost frame");

        size_t traced = 0;
        cxxstd::unwind_trace([&traced](const cxxstd::UnwindFrame&, size_t) noexcept { ++traced; });
        std::printf("traced %zu frames\n", traced);
        check(traced >= 3, "trace yields multiple frames");

        if (g_failures == 0)
                std::printf("unwinder smoke test: OK\n");
        return g_failures == 0 ? 0 : 1;
}