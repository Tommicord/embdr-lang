/*
 * Copyright(c) 2026 Tommicord
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
#include <algorithm>
#include <bit>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(_WIN32)
#        include <io.h>
#else
#        include <unistd.h>
#endif

export module embdr.cxxstd.io;
import embdr.cxxstd.memoryMaybe;
export import embdr.cxxstd.stringView;

namespace embdr::cxxstd {
        export enum class Infallible {};

        export enum class ErrorKind {
                notFound,
                permissionDenied,
                connectionRefused,
                connectionReset,
                hostUnreachable,
                networkUnreachable,
                connectionAborted,
                notConnected,
                addrInUse,
                addrNotAvailable,
                networkDown,
                brokenPipe,
                alreadyExists,
                wouldBlock,
                notADirectory,
                isADirectory,
                directoryNotEmpty,
                readOnlyFilesystem,
                filesystemLoop,
                staleNetworkFileHandle,
                invalidInput,
                invalidData,
                timedOut,
                writeZero,
                storageFull,
                notSeekable,
                fileTooLarge,
                resourceBusy,
                executableFileBusy,
                crossesDevices,
                tooManyLinks,
                invalidFilename,
                argumentListTooLong,
                interrupted,
                unsupported,
                unexpectedEof,
                outOfMemory,
                inProgress,
                other,
                uncategorized,
        };

        export constexpr SimpleStringView as_str(const ErrorKind kind) noexcept {
                switch (kind) {
                        case ErrorKind::addrInUse:
                                return "address in use";
                        case ErrorKind::addrNotAvailable:
                                return "address not available";
                        case ErrorKind::alreadyExists:
                                return "entity already exists";
                        case ErrorKind::argumentListTooLong:
                                return "argument list too long";
                        case ErrorKind::brokenPipe:
                                return "broken pipe";
                        case ErrorKind::connectionAborted:
                                return "connection aborted";
                        case ErrorKind::connectionRefused:
                                return "connection refused";
                        case ErrorKind::connectionReset:
                                return "connection reset";
                        case ErrorKind::crossesDevices:
                                return "cross-device link or rename";
                        case ErrorKind::directoryNotEmpty:
                                return "directory not empty";
                        case ErrorKind::executableFileBusy:
                                return "executable file busy";
                        case ErrorKind::fileTooLarge:
                                return "file too large";
                        case ErrorKind::filesystemLoop:
                                return "filesystem loop or indirection limit (e.g. symlink loop)";
                        case ErrorKind::hostUnreachable:
                                return "host unreachable";
                        case ErrorKind::inProgress:
                                return "in progress";
                        case ErrorKind::interrupted:
                                return "operation interrupted";
                        case ErrorKind::invalidData:
                                return "invalid data";
                        case ErrorKind::invalidFilename:
                                return "invalid filename";
                        case ErrorKind::invalidInput:
                                return "invalid input parameter";
                        case ErrorKind::isADirectory:
                                return "is a directory";
                        case ErrorKind::networkDown:
                                return "network down";
                        case ErrorKind::networkUnreachable:
                                return "network unreachable";
                        case ErrorKind::notADirectory:
                                return "not a directory";
                        case ErrorKind::notConnected:
                                return "not connected";
                        case ErrorKind::notFound:
                                return "entity not found";
                        case ErrorKind::notSeekable:
                                return "seek on unseekable file";
                        case ErrorKind::other:
                                return "other error";
                        case ErrorKind::outOfMemory:
                                return "out of memory";
                        case ErrorKind::permissionDenied:
                                return "permission denied";
                        case ErrorKind::readOnlyFilesystem:
                                return "read-only filesystem or storage medium";
                        case ErrorKind::resourceBusy:
                                return "resource busy";
                        case ErrorKind::staleNetworkFileHandle:
                                return "stale network file handle";
                        case ErrorKind::storageFull:
                                return "no storage space";
                        case ErrorKind::timedOut:
                                return "timed out";
                        case ErrorKind::tooManyLinks:
                                return "too many links";
                        case ErrorKind::uncategorized:
                                return "uncategorized error";
                        case ErrorKind::unexpectedEof:
                                return "unexpected end of file";
                        case ErrorKind::unsupported:
                                return "unsupported";
                        case ErrorKind::wouldBlock:
                                return "operation would block";
                        case ErrorKind::writeZero:
                                return "write zero";
                }
                return "uncategorized error";
        }

        export constexpr SimpleStringView debug_str(const ErrorKind kind) noexcept {
                switch (kind) {
                        case ErrorKind::addrInUse:
                                return "AddrInUse";
                        case ErrorKind::addrNotAvailable:
                                return "AddrNotAvailable";
                        case ErrorKind::alreadyExists:
                                return "AlreadyExists";
                        case ErrorKind::argumentListTooLong:
                                return "ArgumentListTooLong";
                        case ErrorKind::brokenPipe:
                                return "BrokenPipe";
                        case ErrorKind::connectionAborted:
                                return "ConnectionAborted";
                        case ErrorKind::connectionRefused:
                                return "ConnectionRefused";
                        case ErrorKind::connectionReset:
                                return "ConnectionReset";
                        case ErrorKind::crossesDevices:
                                return "CrossesDevices";
                        case ErrorKind::directoryNotEmpty:
                                return "DirectoryNotEmpty";
                        case ErrorKind::executableFileBusy:
                                return "ExecutableFileBusy";
                        case ErrorKind::fileTooLarge:
                                return "FileTooLarge";
                        case ErrorKind::filesystemLoop:
                                return "FilesystemLoop";
                        case ErrorKind::hostUnreachable:
                                return "HostUnreachable";
                        case ErrorKind::inProgress:
                                return "InProgress";
                        case ErrorKind::interrupted:
                                return "Interrupted";
                        case ErrorKind::invalidData:
                                return "InvalidData";
                        case ErrorKind::invalidFilename:
                                return "InvalidFilename";
                        case ErrorKind::invalidInput:
                                return "InvalidInput";
                        case ErrorKind::isADirectory:
                                return "IsADirectory";
                        case ErrorKind::networkDown:
                                return "NetworkDown";
                        case ErrorKind::networkUnreachable:
                                return "NetworkUnreachable";
                        case ErrorKind::notADirectory:
                                return "NotADirectory";
                        case ErrorKind::notConnected:
                                return "NotConnected";
                        case ErrorKind::notFound:
                                return "NotFound";
                        case ErrorKind::notSeekable:
                                return "NotSeekable";
                        case ErrorKind::other:
                                return "Other";
                        case ErrorKind::outOfMemory:
                                return "OutOfMemory";
                        case ErrorKind::permissionDenied:
                                return "PermissionDenied";
                        case ErrorKind::readOnlyFilesystem:
                                return "ReadOnlyFilesystem";
                        case ErrorKind::resourceBusy:
                                return "ResourceBusy";
                        case ErrorKind::staleNetworkFileHandle:
                                return "StaleNetworkFileHandle";
                        case ErrorKind::storageFull:
                                return "StorageFull";
                        case ErrorKind::timedOut:
                                return "TimedOut";
                        case ErrorKind::tooManyLinks:
                                return "TooManyLinks";
                        case ErrorKind::uncategorized:
                                return "Uncategorized";
                        case ErrorKind::unexpectedEof:
                                return "UnexpectedEof";
                        case ErrorKind::unsupported:
                                return "Unsupported";
                        case ErrorKind::wouldBlock:
                                return "WouldBlock";
                        case ErrorKind::writeZero:
                                return "WriteZero";
                }
                return "Uncategorized";
        }

        export constexpr ErrorKind error_kind_from_errno(const int code) noexcept {
                switch (code) {
                        case EBADF:
                                return ErrorKind::notFound;
                        case EPIPE:
                                return ErrorKind::brokenPipe;
                        case EAGAIN:
                                return ErrorKind::timedOut;
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
                        case EWOULDBLOCK:
                                return ErrorKind::timedOut;
#endif
                        case EINTR:
                                return ErrorKind::interrupted;
                        case ENOMEM:
                                return ErrorKind::outOfMemory;
                        default:
                                return ErrorKind::other;
                }
        }

        export class IoError {
                ErrorKind _M_kind;
                const char* _M_message;

            public:
                constexpr IoError(const ErrorKind kind) noexcept : _M_kind(kind), _M_message(nullptr) {}
                constexpr IoError(const ErrorKind kind, const char* message) noexcept :
                    _M_kind(kind), _M_message(message) {}

                [[nodiscard]] static constexpr IoError from_kind(const ErrorKind kind) noexcept {
                        return IoError(kind);
                }
                [[nodiscard]] static constexpr IoError other(const char* message) noexcept {
                        return IoError(ErrorKind::other, message);
                }
                [[nodiscard]] constexpr ErrorKind kind() const noexcept { return this->_M_kind; }
                [[nodiscard]] constexpr const char* message() const noexcept { return this->_M_message; }
                [[nodiscard]] constexpr SimpleStringView what() const noexcept {
                        if (this->_M_message != nullptr)
                                return SimpleStringView(this->_M_message);
                        return as_str(this->_M_kind);
                }
        };

        export class SeekFrom {
            public:
                enum class Origin : unsigned char { start, endPosition, currentPosition };

                [[nodiscard]] static constexpr SeekFrom Start(const uint64_t offset) noexcept {
                        return SeekFrom(Origin::start, offset);
                }
                [[nodiscard]] static constexpr SeekFrom End(const int64_t offset) noexcept {
                        return SeekFrom(Origin::endPosition, std::bit_cast<uint64_t>(offset));
                }
                [[nodiscard]] static constexpr SeekFrom Current(const int64_t offset) noexcept {
                        return SeekFrom(Origin::currentPosition, std::bit_cast<uint64_t>(offset));
                }
                [[nodiscard]] constexpr Origin origin() const noexcept { return this->_M_origin; }
                [[nodiscard]] constexpr uint64_t offset() const noexcept { return this->_M_offset; }
                [[nodiscard]] constexpr int64_t signed_offset() const noexcept {
                        return std::bit_cast<int64_t>(this->_M_offset);
                }
                [[nodiscard]] constexpr bool operator==(const SeekFrom& other) const noexcept {
                        return this->_M_origin == other._M_origin && this->_M_offset == other._M_offset;
                }

            private:
                Origin _M_origin;
                uint64_t _M_offset;

                constexpr SeekFrom(const Origin origin, const uint64_t offset) noexcept :
                    _M_origin(origin), _M_offset(offset) {}
        };

        export enum class SliceWriteError {
                full,
        };

        export constexpr SimpleStringView as_str(const SliceWriteError kind) noexcept {
                switch (kind) {
                        case SliceWriteError::full:
                                return "Full";
                }
                return "Unknown";
        }

        export [[nodiscard]] constexpr bool operator==(const ErrorKind lhs, const SliceWriteError rhs) noexcept {
                return lhs == ErrorKind::writeZero && rhs == SliceWriteError::full;
        }
        export [[nodiscard]] constexpr bool operator==(const SliceWriteError lhs, const ErrorKind rhs) noexcept {
                return rhs == lhs;
        }

        export template <typename E>
        class ReadExactError {
                bool _M_eof;
                E _M_value{};

                constexpr explicit ReadExactError(const bool eof) noexcept : _M_eof(eof) {}
                constexpr ReadExactError(const E& value, const bool eof) noexcept : _M_eof(eof), _M_value(value) {}

            public:
                using error_type = E;

                [[nodiscard]] static constexpr ReadExactError unexpected_eof() noexcept { return ReadExactError(true); }
                [[nodiscard]] static constexpr ReadExactError
                other(E value) noexcept(std::is_nothrow_copy_constructible_v<E>) {
                        return ReadExactError(value, false);
                }
                [[nodiscard]] static constexpr ReadExactError other(const ErrorKind) noexcept
                        requires(std::is_same_v<E, SliceWriteError>)
                {
                        return ReadExactError(SliceWriteError::full, false);
                }
                [[nodiscard]] static constexpr ReadExactError
                from(E value) noexcept(std::is_nothrow_copy_constructible_v<E>) {
                        return ReadExactError(value, false);
                }

                [[nodiscard]] constexpr bool is_unexpected_eof() const noexcept { return this->_M_eof; }
                [[nodiscard]] constexpr bool is_other() const noexcept { return !this->_M_eof; }
                [[nodiscard]] constexpr E& error() noexcept { return this->_M_value; }
                [[nodiscard]] constexpr const E& error() const noexcept { return this->_M_value; }
                [[nodiscard]] constexpr SimpleStringView what() const noexcept {
                        if (this->_M_eof)
                                return "UnexpectedEof";
                        if constexpr (std::is_same_v<error_type, ErrorKind>)
                                return as_str(this->_M_value);
                        else if constexpr (requires(const error_type& e) {
                                                   { e.what() } -> std::convertible_to<SimpleStringView>;
                                           })
                                return this->_M_value.what();
                        else
                                return "Other";
                }
        };

        export template <typename E>
        class WriteFmtError {
                bool _M_fmt_error;
                E _M_value{};

                constexpr explicit WriteFmtError(const bool fmt_error) noexcept : _M_fmt_error(fmt_error) {}
                constexpr WriteFmtError(const E& value, const bool fmt_error) noexcept :
                    _M_fmt_error(fmt_error), _M_value(value) {}

            public:
                using error_type = E;

                [[nodiscard]] static constexpr WriteFmtError fmt_error() noexcept { return WriteFmtError(true); }
                [[nodiscard]] static constexpr WriteFmtError
                other(E value) noexcept(std::is_nothrow_copy_constructible_v<E>) {
                        return WriteFmtError(value, false);
                }
                [[nodiscard]] static constexpr WriteFmtError other(const ErrorKind) noexcept
                        requires(std::is_same_v<E, SliceWriteError>)
                {
                        return WriteFmtError(SliceWriteError::full, false);
                }
                [[nodiscard]] static constexpr WriteFmtError
                from(E value) noexcept(std::is_nothrow_copy_constructible_v<E>) {
                        return WriteFmtError(value, false);
                }

                [[nodiscard]] constexpr bool is_fmt_error() const noexcept { return this->_M_fmt_error; }
                [[nodiscard]] constexpr bool is_other() const noexcept { return !this->_M_fmt_error; }
                [[nodiscard]] constexpr E& error() noexcept { return this->_M_value; }
                [[nodiscard]] constexpr const E& error() const noexcept { return this->_M_value; }
                [[nodiscard]] constexpr SimpleStringView what() const noexcept {
                        if (this->_M_fmt_error)
                                return "FmtError";
                        if constexpr (std::is_same_v<error_type, ErrorKind>)
                                return as_str(this->_M_value);
                        else if constexpr (requires(const error_type& e) {
                                                   { e.what() } -> std::convertible_to<SimpleStringView>;
                                           })
                                return this->_M_value.what();
                        else
                                return "Other";
                }
        };

        export {
                template <typename T, typename E = ErrorKind>
                class IoResult {
                        bool _M_ok;
                        T _M_value{};
                        Maybe<E, IoError> _M_error;

                        struct ok_tag {};
                        struct err_tag {};

                        constexpr IoResult(ok_tag, T value) noexcept(std::is_nothrow_move_constructible_v<T>) :
                            _M_ok(true), _M_value(std::move(value)) {}
                        constexpr IoResult(err_tag, E error) noexcept(std::is_nothrow_move_constructible_v<E>) :
                            _M_ok(false), _M_error(std::move(error)) {}

                    public:
                        using value_type = T;
                        using error_type = E;

                        explicit constexpr IoResult(T value) noexcept(std::is_nothrow_move_constructible_v<T>) :
                            _M_ok(true), _M_value(std::move(value)) {}

                        [[nodiscard]] static constexpr IoResult ok(T value) {
                                return IoResult(ok_tag{}, std::move(value));
                        }
                        [[nodiscard]] static constexpr IoResult err(E error) {
                                return IoResult(err_tag{}, std::move(error));
                        }
                        [[nodiscard]] static constexpr IoResult err(const SliceWriteError) noexcept
                                requires(std::is_same_v<E, ErrorKind>)
                        {
                                return IoResult(err_tag{}, ErrorKind::writeZero);
                        }

                        [[nodiscard]] constexpr bool has_value() const noexcept { return this->_M_ok; }
                        [[nodiscard]] constexpr bool has_error() const noexcept { return this->_M_error.has_value(); }
                        [[nodiscard]] constexpr explicit operator bool() const noexcept { return this->_M_ok; }
                        [[nodiscard]] constexpr T& value() noexcept { return this->_M_value; }
                        [[nodiscard]] constexpr const T& value() const noexcept { return this->_M_value; }
                        [[nodiscard]] constexpr E& error() noexcept { return this->_M_error.value(); }
                        [[nodiscard]] constexpr const E& error() const noexcept { return this->_M_error.value(); }
                };

                template <typename T>
                class IoResult<void, T> {
                        bool _M_ok;
                        Maybe<T, IoError> _M_error;

                        struct ok_tag {};
                        struct err_tag {};

                        constexpr IoResult(ok_tag) noexcept : _M_ok(true) {}
                        constexpr IoResult(err_tag, T error) noexcept(std::is_nothrow_move_constructible_v<T>) :
                            _M_ok(false), _M_error(std::move(error)) {}

                    public:
                        using value_type = void;
                        using error_type = T;

                        constexpr IoResult() = delete;
                        constexpr IoResult(T error) noexcept(std::is_nothrow_move_constructible_v<T>) :
                            _M_ok(false), _M_error(std::move(error)) {}

                        [[nodiscard]] static constexpr IoResult ok() noexcept { return IoResult(ok_tag{}); }
                        [[nodiscard]] static constexpr IoResult err(T error) {
                                return IoResult(err_tag{}, std::move(error));
                        }
                        [[nodiscard]] static constexpr IoResult err(const SliceWriteError) noexcept
                                requires(std::is_same_v<T, ErrorKind>)
                        {
                                return IoResult(err_tag{}, ErrorKind::writeZero);
                        }

                        [[nodiscard]] constexpr bool has_value() const noexcept { return this->_M_ok; }
                        [[nodiscard]] constexpr bool has_error() const noexcept { return this->_M_error.has_value(); }
                        [[nodiscard]] constexpr explicit operator bool() const noexcept { return this->_M_ok; }
                        [[nodiscard]] constexpr T& error() noexcept { return this->_M_error.value(); }
                        [[nodiscard]] constexpr const T& error() const noexcept { return this->_M_error.value(); }
                };
        }

        export template <typename T>
        concept ErrorType = requires { typename T::error_type; };

        export template <typename T>
        concept Read = ErrorType<T> && requires(T& reader, std::span<uint8_t> buf) {
                { reader.read(buf) } -> std::same_as<IoResult<size_t>>;
        };

        export template <typename T>
        concept BufRead = Read<T> && requires(T& reader, size_t amount) {
                { reader.fill_buf() } -> std::same_as<IoResult<std::span<const uint8_t>>>;
                { reader.consume(amount) };
        };

        export template <typename T>
        concept Write = ErrorType<T> && requires(T& writer, std::span<const uint8_t> buf) {
                { writer.write(buf) } -> std::same_as<IoResult<size_t>>;
                { writer.flush() } -> std::same_as<IoResult<void>>;
        };

        export template <typename T>
        concept Seek = ErrorType<T> && requires(T& seekable, SeekFrom pos) {
                { seekable.seek(pos) } -> std::same_as<IoResult<uint64_t>>;
        };

        export template <typename T>
        concept ReadReady = ErrorType<T> && requires(T& reader) {
                { reader.read_ready() } -> std::same_as<IoResult<bool>>;
        };

        export template <typename T>
        concept WriteReady = ErrorType<T> && requires(T& writer) {
                { writer.write_ready() } -> std::same_as<IoResult<bool>>;
        };

        export template <typename T>
        concept IsTerminal = requires(const T& stream) {
                { stream.is_terminal() } -> std::convertible_to<bool>;
        };

        export template <IsTerminal T>
        [[nodiscard]] bool is_terminal(const T& stream) {
                return stream.is_terminal();
        }

        export inline std::span<const uint8_t> to_bytes(const SimpleStringView text) noexcept {
                return std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size());
        }

        template <typename T>
        struct CursorTraits {
                static constexpr bool supported = false;
                static constexpr bool writable = false;
                using error_type = Infallible;
        };
        template <>
        struct CursorTraits<std::span<const uint8_t>> {
                static constexpr bool supported = true;
                static constexpr bool writable = false;
                using error_type = Infallible;
        };
        template <>
        struct CursorTraits<std::span<uint8_t>> {
                static constexpr bool supported = true;
                static constexpr bool writable = true;
                using error_type = SliceWriteError;
        };
        template <>
        struct CursorTraits<std::vector<uint8_t>> {
                static constexpr bool supported = true;
                static constexpr bool writable = true;
                using error_type = Infallible;
        };

        template <typename T>
        inline constexpr bool CursorSupported = CursorTraits<T>::supported;
        template <typename T>
        inline constexpr bool CursorWritable = CursorTraits<T>::writable;

        [[nodiscard]] constexpr uint64_t saturating_add(const uint64_t base, const uint64_t amount) noexcept {
                return (base > UINT64_MAX - amount) ? UINT64_MAX : base + amount;
        }
        [[nodiscard]] constexpr uint64_t saturating_add_signed(const uint64_t base, const int64_t amount) noexcept {
                if (amount >= 0)
                        return saturating_add(base, static_cast<uint64_t>(amount));
                const uint64_t magnitude = static_cast<uint64_t>(-(amount + 1)) + 1;
                return (base < magnitude) ? 0 : base - magnitude;
        }

        inline constexpr int stdin_fd = 0;
        inline constexpr int stdout_fd = 1;
        inline constexpr int stderr_fd = 2;

        [[nodiscard]] inline long long sys_read(const int fd, void* buffer, const size_t count) noexcept {
#if defined(_WIN32)
                return _read(fd, buffer, static_cast<unsigned int>(count));
#else
                return static_cast<long long>(::read(fd, buffer, count));
#endif
        }
        [[nodiscard]] inline long long sys_write(const int fd, const void* buffer, const size_t count) noexcept {
#if defined(_WIN32)
                return _write(fd, buffer, static_cast<unsigned int>(count));
#else
                return static_cast<long long>(::write(fd, buffer, count));
#endif
        }
        [[nodiscard]] inline bool sys_isatty(const int fd) noexcept {
#if defined(_WIN32)
                return _isatty(fd) != 0;
#else
                return ::isatty(fd) != 0;
#endif
        }

        export template <typename T>
        class Cursor {
                using traits = CursorTraits<T>;

                T _M_inner;
                uint64_t _M_pos = 0;

                [[nodiscard]] std::span<const uint8_t> _M_remaining() const noexcept
                        requires(CursorSupported<T>)
                {
                        const auto len = static_cast<uint64_t>(std::size(this->_M_inner));
                        const auto pos = std::min(this->_M_pos, len);
                        return std::span<const uint8_t>(std::data(this->_M_inner), static_cast<size_t>(len))
                            .subspan(static_cast<size_t>(pos));
                }
                [[nodiscard]] std::span<uint8_t> _M_remaining_mut() noexcept
                        requires(CursorWritable<T>)
                {
                        const auto len = static_cast<uint64_t>(std::size(this->_M_inner));
                        const auto pos = std::min(this->_M_pos, len);
                        return std::span<uint8_t>(std::data(this->_M_inner), static_cast<size_t>(len))
                            .subspan(static_cast<size_t>(pos));
                }

            public:
                using buffer_type = T;
                using error_type = typename traits::error_type;

                explicit constexpr Cursor(T inner) : _M_inner(std::move(inner)) {}

                [[nodiscard]] constexpr T into_inner() && { return std::move(this->_M_inner); }
                [[nodiscard]] constexpr const T& get_ref() const noexcept { return this->_M_inner; }
                [[nodiscard]] constexpr T& get_mut() noexcept { return this->_M_inner; }
                [[nodiscard]] constexpr uint64_t position() const noexcept { return this->_M_pos; }
                constexpr void set_position(const uint64_t pos) noexcept { this->_M_pos = pos; }

                [[nodiscard]] IoResult<size_t> read(std::span<uint8_t> buf) noexcept
                        requires(CursorSupported<T>)
                {
                        const auto remaining = this->_M_remaining();
                        const size_t amt = std::min(buf.size(), remaining.size());
                        if (amt > 0) {
                                std::copy_n(remaining.data(), amt, buf.data());
                                this->_M_pos += static_cast<uint64_t>(amt);
                        }
                        return IoResult<size_t>::ok(amt);
                }

                [[nodiscard]] IoResult<void, ReadExactError<error_type>> read_exact(std::span<uint8_t> buf) noexcept
                        requires(CursorSupported<T>)
                {
                        using result_type = IoResult<void, ReadExactError<error_type>>;
                        const auto remaining = this->_M_remaining();
                        if (remaining.size() < buf.size())
                                return result_type::err(ReadExactError<error_type>::unexpected_eof());
                        (void)this->read(buf);
                        return result_type::ok();
                }

                [[nodiscard]] IoResult<std::span<const uint8_t>> fill_buf() const
                        requires(CursorSupported<T>)
                {
                        return IoResult<std::span<const uint8_t>>::ok(this->_M_remaining());
                }

                void consume(const size_t amount) noexcept
                        requires(CursorSupported<T>)
                {
                        this->_M_pos = saturating_add(this->_M_pos, static_cast<uint64_t>(amount));
                }

                [[nodiscard]] IoResult<size_t> write(const std::span<const uint8_t> buf)
                        requires(CursorWritable<T>)
                {
                        if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                                if (buf.empty())
                                        return IoResult<size_t>::ok(0);
                                const auto pos = static_cast<size_t>(this->_M_pos);
                                if (pos + buf.size() > this->_M_inner.size())
                                        this->_M_inner.resize(pos + buf.size(), 0);
                                std::copy_n(buf.data(), buf.size(), this->_M_inner.data() + pos);
                                this->_M_pos += static_cast<uint64_t>(buf.size());
                                return IoResult<size_t>::ok(buf.size());
                        } else {
                                auto remaining = this->_M_remaining_mut();
                                const size_t amt = std::min(buf.size(), remaining.size());
                                if (!buf.empty() && amt == 0)
                                        return IoResult<size_t>::err(SliceWriteError::full);
                                if (amt > 0) {
                                        std::copy_n(buf.data(), amt, remaining.data());
                                        this->_M_pos += static_cast<uint64_t>(amt);
                                }
                                return IoResult<size_t>::ok(amt);
                        }
                }

                [[nodiscard]] IoResult<void> write_all(const std::span<const uint8_t> buf)
                        requires(CursorWritable<T>)
                {
                        if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                                (void)this->write(buf);
                                return IoResult<void>::ok();
                        } else {
                                if (this->_M_remaining_mut().size() < buf.size())
                                        return IoResult<void>::err(SliceWriteError::full);
                                (void)this->write(buf);
                                return IoResult<void>::ok();
                        }
                }

                [[nodiscard]] IoResult<void> flush() const
                        requires(CursorWritable<T>)
                {
                        return IoResult<void>::ok();
                }

                [[nodiscard]] IoResult<uint64_t> seek(const SeekFrom pos) noexcept
                        requires(CursorSupported<T>)
                {
                        const auto len = static_cast<uint64_t>(std::size(this->_M_inner));
                        uint64_t new_pos = 0;
                        switch (pos.origin()) {
                                case SeekFrom::Origin::start:
                                        new_pos = pos.offset();
                                        break;
                                case SeekFrom::Origin::endPosition:
                                        new_pos = saturating_add_signed(len, pos.signed_offset());
                                        break;
                                case SeekFrom::Origin::currentPosition:
                                        new_pos = saturating_add_signed(this->_M_pos, pos.signed_offset());
                                        break;
                        }
                        if constexpr (!std::is_same_v<T, std::vector<uint8_t>>)
                                new_pos = std::min(new_pos, len);
                        this->_M_pos = new_pos;
                        return IoResult(this->_M_pos);
                }

                [[nodiscard]] IoResult<bool> read_ready() const
                        requires(CursorSupported<T>)
                {
                        return IoResult(true);
                }

                [[nodiscard]] IoResult<bool> write_ready() const
                        requires(CursorWritable<T>)
                {
                        return IoResult(true);
                }
        };

        export template <typename T>
        Cursor(T) -> Cursor<T>;

        export class StdinLock {
            public:
                using error_type = ErrorKind;

                constexpr StdinLock() noexcept = default;

                [[nodiscard]] IoResult<size_t, ErrorKind> read(std::span<uint8_t> buf) noexcept {
                        if (buf.empty())
                                return IoResult<size_t, ErrorKind>::ok(0);
                        const long long ret = sys_read(stdin_fd, buf.data(), buf.size());
                        if (ret < 0)
                                return IoResult<size_t, ErrorKind>::err(error_kind_from_errno(errno));
                        return IoResult<size_t, ErrorKind>::ok(static_cast<size_t>(ret));
                }

                [[nodiscard]] IoResult<void, ReadExactError<ErrorKind>> read_exact(std::span<uint8_t> buf) noexcept {
                        using result_type = IoResult<void, ReadExactError<ErrorKind>>;
                        size_t total = 0;
                        while (total < buf.size()) {
                                const auto result = this->read(buf.subspan(total));
                                if (!result)
                                        return result_type::err(ReadExactError<ErrorKind>::other(result.error()));
                                if (result.value() == 0)
                                        break;
                                total += result.value();
                        }
                        if (total == buf.size())
                                return result_type::ok();
                        return result_type::err(ReadExactError<ErrorKind>::unexpected_eof());
                }

                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stdin_fd); }
        };

        export class Stdin {
            public:
                using error_type = ErrorKind;

                constexpr Stdin() noexcept = default;

                [[nodiscard]] static constexpr Stdin make() noexcept { return Stdin{}; }
                [[nodiscard]] StdinLock lock() const noexcept { return StdinLock{}; }

                [[nodiscard]] IoResult<size_t, ErrorKind> read(std::span<uint8_t> buf) noexcept {
                        return StdinLock{}.read(buf);
                }
                [[nodiscard]] IoResult<void, ReadExactError<ErrorKind>> read_exact(std::span<uint8_t> buf) noexcept {
                        return StdinLock{}.read_exact(buf);
                }
                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stdin_fd); }
        };

        export class StdoutLock {
            public:
                using error_type = ErrorKind;

                constexpr StdoutLock() noexcept = default;

                [[nodiscard]] IoResult<size_t, ErrorKind> write(const std::span<const uint8_t> buf) noexcept {
                        if (buf.empty())
                                return IoResult<size_t, ErrorKind>::ok(0);
                        const long long ret = sys_write(stdout_fd, buf.data(), buf.size());
                        if (ret < 0)
                                return IoResult<size_t, ErrorKind>::err(error_kind_from_errno(errno));
                        return IoResult<size_t, ErrorKind>::ok(static_cast<size_t>(ret));
                }

                [[nodiscard]] IoResult<void, ErrorKind> flush() noexcept { return IoResult<void, ErrorKind>::ok(); }

                [[nodiscard]] IoResult<void, ErrorKind> write_all(std::span<const uint8_t> buf) noexcept {
                        using result_type = IoResult<void, ErrorKind>;
                        while (!buf.empty()) {
                                const auto result = this->write(buf);
                                if (!result)
                                        return result_type::err(result.error());
                                if (result.value() == 0)
                                        return result_type::err(ErrorKind::writeZero);
                                buf = buf.subspan(result.value());
                        }
                        return result_type::ok();
                }

                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stdout_fd); }
        };

        export class Stdout {
            public:
                using error_type = ErrorKind;

                constexpr Stdout() noexcept = default;

                [[nodiscard]] static constexpr Stdout make() noexcept { return Stdout{}; }
                [[nodiscard]] StdoutLock lock() const noexcept { return StdoutLock{}; }

                [[nodiscard]] IoResult<size_t, ErrorKind> write(const std::span<const uint8_t> buf) noexcept {
                        return StdoutLock{}.write(buf);
                }
                [[nodiscard]] IoResult<void, ErrorKind> flush() noexcept { return StdoutLock{}.flush(); }
                [[nodiscard]] IoResult<void, ErrorKind> write_all(std::span<const uint8_t> buf) noexcept {
                        return StdoutLock{}.write_all(buf);
                }
                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stdout_fd); }
        };

        export class StderrLock {
            public:
                using error_type = ErrorKind;

                constexpr StderrLock() noexcept = default;

                [[nodiscard]] IoResult<size_t, ErrorKind> write(const std::span<const uint8_t> buf) noexcept {
                        if (buf.empty())
                                return IoResult<size_t, ErrorKind>::ok(0);
                        const long long ret = sys_write(stderr_fd, buf.data(), buf.size());
                        if (ret < 0)
                                return IoResult<size_t, ErrorKind>::err(error_kind_from_errno(errno));
                        return IoResult<size_t, ErrorKind>::ok(static_cast<size_t>(ret));
                }

                [[nodiscard]] IoResult<void, ErrorKind> flush() noexcept { return IoResult<void, ErrorKind>::ok(); }

                [[nodiscard]] IoResult<void, ErrorKind> write_all(std::span<const uint8_t> buf) noexcept {
                        using result_type = IoResult<void, ErrorKind>;
                        while (!buf.empty()) {
                                const auto result = this->write(buf);
                                if (!result)
                                        return result_type::err(result.error());
                                if (result.value() == 0)
                                        return result_type::err(ErrorKind::writeZero);
                                buf = buf.subspan(result.value());
                        }
                        return result_type::ok();
                }

                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stderr_fd); }
        };

        export class Stderr {
            public:
                using error_type = ErrorKind;

                constexpr Stderr() noexcept = default;

                [[nodiscard]] static constexpr Stderr make() noexcept { return Stderr{}; }
                [[nodiscard]] StderrLock lock() const noexcept { return StderrLock{}; }

                [[nodiscard]] IoResult<size_t, ErrorKind> write(const std::span<const uint8_t> buf) noexcept {
                        return StderrLock{}.write(buf);
                }
                [[nodiscard]] IoResult<void, ErrorKind> flush() noexcept { return StderrLock{}.flush(); }
                [[nodiscard]] IoResult<void, ErrorKind> write_all(std::span<const uint8_t> buf) noexcept {
                        return StderrLock{}.write_all(buf);
                }
                [[nodiscard]] bool is_terminal() const noexcept { return sys_isatty(stderr_fd); }
        };

        export template <Read R>
        [[nodiscard]] IoResult<void, ReadExactError<typename R::error_type>> read_exact(R& reader,
                                                                                        std::span<uint8_t> buf) {
                using error_type = typename R::error_type;
                using result_type = IoResult<void, ReadExactError<error_type>>;
                if constexpr (requires { reader.read_exact(buf); }) {
                        return reader.read_exact(buf);
                } else {
                        while (!buf.empty()) {
                                const auto result = reader.read(buf);
                                if (!result)
                                        return result_type::err(ReadExactError<error_type>::other(result.error()));
                                if (result.value() == 0)
                                        break;
                                buf = buf.subspan(result.value());
                        }
                        if (buf.empty())
                                return result_type::ok();
                        return result_type::err(ReadExactError<error_type>::unexpected_eof());
                }
        }

        export template <Write W>
        [[nodiscard]] IoResult<void> write_all(W& writer, std::span<const uint8_t> buf) {
                using error_type = typename W::error_type;
                using result_type = IoResult<void>;
                if constexpr (requires { writer.write_all(buf); }) {
                        return writer.write_all(buf);
                } else {
                        static_assert(std::is_constructible_v<error_type, ErrorKind>,
                                      "embdr::cxxstd::write_all requires an error type constructible from ErrorKind or "
                                      "a write_all "
                                      "member function on the writer");
                        while (!buf.empty()) {
                                const auto result = writer.write(buf);
                                if (!result)
                                        return result_type::err(result.error());
                                if (result.value() == 0)
                                        return result_type::err(error_type(ErrorKind::writeZero));
                                buf = buf.subspan(result.value());
                        }
                        return result_type::ok();
                }
        }

        export template <Write W>
        [[nodiscard]] IoResult<void> write_all(W& writer, const SimpleStringView buf) {
                return write_all(writer, to_bytes(buf));
        }

        export template <Write W>
        [[nodiscard]] IoResult<void, WriteFmtError<typename W::error_type>>
        write_fmt(W& writer, const SimpleStringView formatted) {
                using error_type = typename W::error_type;
                using result_type = IoResult<void, WriteFmtError<error_type>>;
                if constexpr (requires { writer.write_fmt(formatted); }) {
                        return writer.write_fmt(formatted);
                } else {
                        const auto result = write_all(writer, formatted);
                        if (!result)
                                return result_type::err(WriteFmtError<error_type>::other(result.error()));
                        return result_type::ok();
                }
        }

        export template <Seek S>
        [[nodiscard]] IoResult<void> rewind(S& seekable) {
                using error_type = typename S::error_type;
                using result_type = IoResult<void>;
                if constexpr (requires { seekable.rewind(); }) {
                        return seekable.rewind();
                } else {
                        const auto result = seekable.seek(SeekFrom::Start(0));
                        if (!result)
                                return result_type::err(result.error());
                        return result_type::ok();
                }
        }

        export template <Seek S>
        [[nodiscard]] IoResult<uint64_t> stream_position(S& seekable) {
                if constexpr (requires { seekable.stream_position(); }) {
                        return seekable.stream_position();
                } else {
                        return seekable.seek(SeekFrom::Current(0));
                }
        }

        export template <Seek S>
        [[nodiscard]] IoResult<void> seek_relative(S& seekable, const int64_t offset) {
                using error_type = typename S::error_type;
                using result_type = IoResult<void>;
                if constexpr (requires { seekable.seek_relative(offset); }) {
                        return seekable.seek_relative(offset);
                } else {
                        const auto result = seekable.seek(SeekFrom::Current(offset));
                        if (!result)
                                return result_type::err(result.error());
                        return result_type::ok();
                }
        }
} // namespace embdr::cxxstd
