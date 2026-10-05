/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#include <catch2/catch_all.hpp>

#include <array>
#include <cerrno>
#include <cstdint>
#include <span>
#include <string_view>
#include <unistd.h>
#include <vector>

import embdr.cxxstd.io;

using namespace embdr::cxxstd;

namespace {
        std::string_view as_string(const std::span<const uint8_t> bytes) noexcept {
                return std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
} // namespace

TEST_CASE("SeekFrom variants compare equal", "[io]") {
        const auto start = SeekFrom::Start(100);
        REQUIRE(start == SeekFrom::Start(100));
        REQUIRE(start != SeekFrom::Start(101));
        const auto end = SeekFrom::End(-50);
        REQUIRE(end == SeekFrom::End(-50));
        const auto current = SeekFrom::Current(10);
        REQUIRE(current == SeekFrom::Current(10));
        REQUIRE(current != end);
        REQUIRE(start.origin() == SeekFrom::Origin::start);
        REQUIRE(start.offset() == 100);
        REQUIRE(end.signed_offset() == -50);
}

TEST_CASE("ErrorKind equality, display and debug strings", "[io]") {
        REQUIRE(ErrorKind::other == ErrorKind::other);
        REQUIRE(ErrorKind::notFound == ErrorKind::notFound);
        REQUIRE(ErrorKind::other != ErrorKind::notFound);
        REQUIRE(debug_str(ErrorKind::notFound) == "NotFound");
        REQUIRE(debug_str(ErrorKind::permissionDenied) == "PermissionDenied");
        REQUIRE(as_str(ErrorKind::permissionDenied) == "permission denied");
        REQUIRE(as_str(ErrorKind::notFound) == "entity not found");
        REQUIRE(as_str(ErrorKind::unexpectedEof) == "unexpected end of file");
        REQUIRE(as_str(ErrorKind::writeZero) == "write zero");
}

TEST_CASE("Error carries a kind and an optional message", "[io]") {
        const IoError simple = IoError::from_kind(ErrorKind::notFound);
        REQUIRE(simple.kind() == ErrorKind::notFound);
        REQUIRE(simple.message() == nullptr);
        REQUIRE(simple.what() == "entity not found");

        const IoError custom = IoError::other("oh no!");
        REQUIRE(custom.kind() == ErrorKind::other);
        REQUIRE(custom.message() == std::string_view("oh no!"));
        REQUIRE(custom.what() == "oh no!");
}

TEST_CASE("ReadExactError, SliceWriteError and WriteFmtError wrappers", "[io]") {
        const auto converted = ReadExactError<ErrorKind>::from(ErrorKind::timedOut);
        REQUIRE(converted.is_other());
        REQUIRE(!converted.is_unexpected_eof());
        REQUIRE(converted.error() == ErrorKind::timedOut);

        const ReadExactError<ErrorKind> eof = ReadExactError<ErrorKind>::unexpected_eof();
        REQUIRE(eof.is_unexpected_eof());
        REQUIRE(!eof.is_other());
        REQUIRE(eof.what() == "UnexpectedEof");

        const SliceWriteError slice = SliceWriteError::full;
        REQUIRE(slice == SliceWriteError::full);
        REQUIRE(as_str(slice) == "Full");

        const auto fmt_other = WriteFmtError<ErrorKind>::from(ErrorKind::other);
        REQUIRE(fmt_other.is_other());
        REQUIRE(fmt_other.error() == ErrorKind::other);

        const WriteFmtError<ErrorKind> fmt_failure = WriteFmtError<ErrorKind>::fmt_error();
        REQUIRE(fmt_failure.is_fmt_error());
        REQUIRE(!fmt_failure.is_other());
        REQUIRE(fmt_failure.what() == "FmtError");
}

TEST_CASE("Cursor basic construction and accessors", "[io][cursor]") {
        Cursor cursor(std::vector<uint8_t>{1, 2, 3});
        REQUIRE(cursor.position() == 0);
        REQUIRE(cursor.get_ref() == (std::vector<uint8_t>{1, 2, 3}));
        REQUIRE(std::move(cursor).into_inner() == (std::vector<uint8_t>{1, 2, 3}));

        Cursor mutable_cursor(std::vector<uint8_t>{1, 2, 3});
        mutable_cursor.get_mut()[0] = 10;
        REQUIRE(mutable_cursor.get_ref()[0] == 10);
        mutable_cursor.set_position(2);
        REQUIRE(mutable_cursor.position() == 2);
}

TEST_CASE("Cursor read from slices", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        std::array<uint8_t, 5> buf{};

        const auto first = cursor.read(buf);
        REQUIRE(first.has_value());
        REQUIRE(first.value() == 5);
        REQUIRE(as_string(buf) == "Hello");

        const auto second = cursor.read(buf);
        REQUIRE(second.has_value());
        REQUIRE(second.value() == 5);
        REQUIRE(as_string(buf) == ", Wor");

        const auto third = cursor.read(buf);
        REQUIRE(third.has_value());
        REQUIRE(third.value() == 3);
        REQUIRE(as_string(std::span<const uint8_t>(buf).first(3)) == "ld!");

        const auto fourth = cursor.read(buf);
        REQUIRE(fourth.has_value());
        REQUIRE(fourth.value() == 0);

        std::array<uint8_t, 10> source{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        Cursor mut_cursor{std::span<uint8_t>(source)};
        std::array<uint8_t, 5> dest{};
        const auto n = mut_cursor.read(dest);
        REQUIRE(n.has_value());
        REQUIRE(n.value() == 5);
        const std::array<uint8_t, 5> expected{1, 2, 3, 4, 5};
        REQUIRE(dest == expected);
}

TEST_CASE("Cursor read_exact and EOF handling", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        std::array<uint8_t, 5> buf{};

        const auto first = cursor.read_exact(buf);
        REQUIRE(first.has_value());
        REQUIRE(as_string(buf) == "Hello");

        const auto second = read_exact(cursor, std::span<uint8_t>(buf));
        REQUIRE(second.has_value());
        REQUIRE(as_string(buf) == ", Wor");

        std::array<uint8_t, 3> rest{};
        const auto third = cursor.read_exact(rest);
        REQUIRE(third.has_value());
        REQUIRE(as_string(rest) == "ld!");

        std::array<uint8_t, 1> past_end{};
        const auto eof = cursor.read_exact(past_end);
        REQUIRE(!eof.has_value());
        REQUIRE(eof.error().is_unexpected_eof());
}

TEST_CASE("Cursor write into a mutable slice", "[io][cursor]") {
        std::array<uint8_t, 20> buf{};
        Cursor cursor{std::span<uint8_t>(buf)};
        const auto first = cursor.write(to_bytes(std::string_view("Hello")));
        REQUIRE(first.has_value());
        REQUIRE(first.value() == 5);
        const auto second = cursor.write(to_bytes(std::string_view(" World")));
        REQUIRE(second.has_value());
        REQUIRE(second.value() == 6);
        REQUIRE(as_string(std::span<const uint8_t>(buf).first(11)) == "Hello World");
}

TEST_CASE("Cursor write_all reports a full slice", "[io][cursor]") {
        std::array<uint8_t, 10> buf{};
        {
                Cursor cursor{std::span<uint8_t>(buf)};
                const auto written = write_all(cursor, std::string_view("Hello"));
                REQUIRE(written.has_value());
                REQUIRE(!written.has_error());
                REQUIRE(cursor.position() == 5);
        }
        REQUIRE(as_string(buf).substr(0, 5) == "Hello");

        Cursor cursor{std::span<uint8_t>(buf)};
        cursor.set_position(5);
        const auto full = write_all(cursor, std::string_view(" World!"));
        REQUIRE(!full.has_value());
        REQUIRE(full.error() == SliceWriteError::full);
}

TEST_CASE("Cursor write into a vector", "[io][cursor]") {
        Cursor cursor(std::vector<uint8_t>{});
        const auto written = cursor.write(to_bytes(std::string_view("Hello")));
        REQUIRE(written.has_value());
        REQUIRE(written.value() == 5);
        const std::vector<uint8_t> expected{'H', 'e', 'l', 'l', 'o'};
        REQUIRE(std::move(cursor).into_inner() == expected);

        Cursor overwrite(std::vector<uint8_t>{1, 2, 3, 4, 5});
        const auto seeked = overwrite.seek(SeekFrom::Start(0));
        REQUIRE(seeked.has_value());
        REQUIRE(seeked.value() == 0);
        const std::array<uint8_t, 2> patch{6, 7};
        const auto patched = write_all(overwrite, std::span<const uint8_t>(patch));
        REQUIRE(patched.has_value());
        const std::vector<uint8_t> patched_bytes{6, 7, 3, 4, 5};
        REQUIRE(std::move(overwrite).into_inner() == patched_bytes);
}

TEST_CASE("Cursor seek from start", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        const auto seeked = cursor.seek(SeekFrom::Start(7));
        REQUIRE(seeked.has_value());
        REQUIRE(seeked.value() == 7);
        std::array<uint8_t, 5> buf{};
        REQUIRE(cursor.read_exact(buf).has_value());
        REQUIRE(as_string(buf) == "World");
}

TEST_CASE("Cursor seek from current position", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        const auto start = cursor.seek(SeekFrom::Start(7));
        REQUIRE(start.has_value());
        const auto current = cursor.seek(SeekFrom::Current(-5));
        REQUIRE(current.has_value());
        REQUIRE(current.value() == 2);
        std::array<uint8_t, 5> buf{};
        REQUIRE(cursor.read_exact(buf).has_value());
        REQUIRE(as_string(buf) == "llo, ");
}

TEST_CASE("Cursor seek from end", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        const auto end = cursor.seek(SeekFrom::End(-6));
        REQUIRE(end.has_value());
        REQUIRE(end.value() == 7);
        std::array<uint8_t, 5> buf{};
        REQUIRE(cursor.read_exact(buf).has_value());
        REQUIRE(as_string(buf) == "World");
}

TEST_CASE("Cursor rewind, stream_position and seek_relative", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        auto position = stream_position(cursor);
        REQUIRE(position.has_value());
        REQUIRE(position.value() == 0);

        std::array<uint8_t, 5> buf{};
        const auto read = cursor.read(buf);
        REQUIRE(read.has_value());
        REQUIRE(read.value() == 5);
        position = stream_position(cursor);
        REQUIRE(position.value() == 5);

        const auto far = cursor.seek(SeekFrom::Start(10));
        REQUIRE(far.has_value());
        position = stream_position(cursor);
        REQUIRE(position.value() == 10);

        const auto rewound = rewind(cursor);
        REQUIRE(rewound.has_value());
        REQUIRE(cursor.position() == 0);

        const auto seeked = cursor.seek(SeekFrom::Start(5));
        REQUIRE(seeked.has_value());
        const auto relative = seek_relative(cursor, 3);
        REQUIRE(relative.has_value());
        REQUIRE(cursor.position() == 8);
        const auto backwards = seek_relative(cursor, -4);
        REQUIRE(backwards.has_value());
        REQUIRE(cursor.position() == 4);
}

TEST_CASE("Cursor seek clamps slices but extends vectors", "[io][cursor]") {
        Cursor slice(to_bytes(std::string_view("Hello")));
        const auto clamped = slice.seek(SeekFrom::Start(10));
        REQUIRE(clamped.has_value());
        REQUIRE(clamped.value() == 5);

        Cursor vec(std::vector<uint8_t>{1, 2, 3});
        const auto beyond = vec.seek(SeekFrom::Start(10));
        REQUIRE(beyond.has_value());
        REQUIRE(beyond.value() == 10);
        const std::array<uint8_t, 2> tail{4, 5};
        const auto written = vec.write(tail);
        REQUIRE(written.has_value());
        const std::vector<uint8_t> expected{1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 4, 5};
        REQUIRE(std::move(vec).into_inner() == expected);
}

TEST_CASE("Cursor buffered reads and readiness flags", "[io][cursor]") {
        Cursor cursor(to_bytes(std::string_view("Hello, World!")));
        const auto buffered = cursor.fill_buf();
        REQUIRE(buffered.has_value());
        REQUIRE(as_string(buffered.value()) == "Hello, World!");

        cursor.consume(7);
        const auto rest = cursor.fill_buf();
        REQUIRE(rest.has_value());
        REQUIRE(as_string(rest.value()) == "World!");

        cursor.consume(6);
        const auto end = cursor.fill_buf();
        REQUIRE(end.has_value());
        REQUIRE(end.value().empty());

        const auto ready = cursor.read_ready();
        REQUIRE(ready.has_value());
        REQUIRE(ready.value());

        std::array<uint8_t, 10> out{};
        Cursor writable{std::span<uint8_t>(out)};
        const auto write_ready = writable.write_ready();
        REQUIRE(write_ready.has_value());
        REQUIRE(write_ready.value());
}

TEST_CASE("Cursor and standard streams satisfy the sync IO concepts", "[io]") {
        using ReadCursor = Cursor<std::span<const uint8_t>>;
        using MutableCursor = Cursor<std::span<uint8_t>>;
        using VectorCursor = Cursor<std::vector<uint8_t>>;

        static_assert(ErrorType<ReadCursor>);
        static_assert(Read<ReadCursor>);
        static_assert(BufRead<ReadCursor>);
        static_assert(Seek<ReadCursor>);
        static_assert(ReadReady<ReadCursor>);
        static_assert(!Write<ReadCursor>);

        static_assert(Read<MutableCursor>);
        static_assert(Write<MutableCursor>);
        static_assert(Seek<MutableCursor>);
        static_assert(BufRead<MutableCursor>);
        static_assert(WriteReady<MutableCursor>);

        static_assert(Read<VectorCursor>);
        static_assert(Write<VectorCursor>);
        static_assert(BufRead<VectorCursor>);
        static_assert(Seek<VectorCursor>);
        static_assert(ReadReady<VectorCursor>);
        static_assert(WriteReady<VectorCursor>);

        static_assert(ErrorType<Stdin> && Read<Stdin>);
        static_assert(ErrorType<Stdout> && Write<Stdout>);
        static_assert(ErrorType<Stderr> && Write<Stderr>);
        static_assert(Read<StdinLock> && Write<StdoutLock> && Write<StderrLock>);
        static_assert(IsTerminal<Stdin> && IsTerminal<Stdout> && IsTerminal<Stderr>);
        SUCCEED();
}

TEST_CASE("write_fmt writes a formatted payload", "[io][cursor]") {
        std::array<uint8_t, 16> buf{};
        Cursor cursor{std::span<uint8_t>(buf)};
        const auto written = write_fmt(cursor, std::string_view("Test"));
        REQUIRE(written.has_value());
        REQUIRE(as_string(buf).substr(0, 4) == "Test");

        std::span<uint8_t> empty_span;
        Cursor empty_cursor(empty_span);
        const auto failed = write_fmt(empty_cursor, std::string_view("Test"));
        REQUIRE(!failed.has_value());
        REQUIRE(failed.error().is_other());
        REQUIRE(failed.error().error() == SliceWriteError::full);
}

TEST_CASE("is_terminal matches isatty", "[io]") {
        const Stdin in = Stdin::make();
        const Stdout out = Stdout::make();
        const Stderr err = Stderr::make();
        REQUIRE(in.is_terminal() == (isatty(STDIN_FILENO) != 0));
        REQUIRE(out.is_terminal() == (isatty(STDOUT_FILENO) != 0));
        REQUIRE(err.is_terminal() == (isatty(STDERR_FILENO) != 0));
        REQUIRE(is_terminal(out) == out.is_terminal());

        const auto in_lock = in.lock();
        REQUIRE(is_terminal(in_lock) == in.is_terminal());
        const auto out_lock = out.lock();
        REQUIRE(is_terminal(out_lock) == out.is_terminal());
}

TEST_CASE("stdout write smoke", "[io]") {
        Stdout out;
        StdoutLock locked = out.lock();
        const std::string_view line = "embdr io smoke\n";
        const auto written = locked.write(to_bytes(line));
        REQUIRE(written.has_value());
        REQUIRE(written.value() == line.size());
        REQUIRE(locked.flush().has_value());

        const auto all = write_all(out, line);
        REQUIRE(all.has_value());
        REQUIRE(out.flush().has_value());
}

TEST_CASE("errno mapping", "[io]") {
        REQUIRE(error_kind_from_errno(EBADF) == ErrorKind::notFound);
        REQUIRE(error_kind_from_errno(EPIPE) == ErrorKind::brokenPipe);
        REQUIRE(error_kind_from_errno(EAGAIN) == ErrorKind::timedOut);
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        REQUIRE(error_kind_from_errno(EWOULDBLOCK) == ErrorKind::timedOut);
#endif
        REQUIRE(error_kind_from_errno(EINTR) == ErrorKind::interrupted);
        REQUIRE(error_kind_from_errno(ENOMEM) == ErrorKind::outOfMemory);
        REQUIRE(error_kind_from_errno(9999) == ErrorKind::other);
}

TEST_CASE("standard stream handles create locks", "[io]") {
        const Stdin in{};
        const Stdout out{};
        const Stderr err{};
        [[maybe_unused]] const auto in_lock = in.lock();
        [[maybe_unused]] const auto out_lock = out.lock();
        [[maybe_unused]] const auto err_lock = err.lock();
        SUCCEED();
}
