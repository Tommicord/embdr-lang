# CODE_QUALITY.md

**embedrite** — strict C++26 coding standard for `embdc` (compiler) and `embdr`
(runtime library).

These rules are **hard constraints**, not suggestions. A change that violates
them is rejected, regardless of how correct it is. The priority order of the
project (binary size → compile-time evaluation → correctness → readability)
decides every conflict.

> Scope: `embdr/**` and `embdc/**` (production code).
> `embdr/tests/**` and `third-party/**` are **exempt** from rules marked
> `[tests-ok]` (Catch2 requires the standard library's allocating types).

---

## 1. Language subset (compiler-enforced)

The whole codebase must build with these flags. They are not "release-only
optimizations" — they define the language we write.

| Rule                          | Flag / mechanism                                                                  |
|-------------------------------|-----------------------------------------------------------------------------------|
| No exceptions                 | `-fno-exceptions` / `/EHs-c-`                                                     |
| No RTTI                       | `-fno-rtti` / `/GR-`                                                              |
| No unwind tables              | `-fno-unwind-tables -fno-asynchronous-unwind-tables`                              |
| Frame pointers kept           | `-fno-omit-frame-pointer` / `/Oy-` (**never remove**: the unwinder depends on it) |
| Dead-section elimination      | `-ffunction-sections -fdata-sections -Wl,--gc-sections`                           |
| No static destructors at exit | `-fno-use-cxa-atexit`                                                             |

### 1.1 No exceptions — ever

- **Forbidden:** `throw`, `try`, `catch`, `std::exception`, `std::runtime_error`,
  `std::*_error`, `assert`-via-throw, any `noexcept(false)` function that can
  terminate.
- Errors are **values**: `Maybe<T, E>`, `IoResult<T, E>`, `ConsoleResult`,
  `LogResult`, or a dedicated `*Error` type.
- Propagate with explicit checks (`has_error()` / `operator bool`), never by
  unwinding.
- A failing invariant **terminates** (`std::abort` / `EMBDR_ASSERT` in Debug) or
  returns an error value. It never throws.

### 1.2 No RTTI

- **Forbidden:** `dynamic_cast`, `typeid`, `std::type_info` comparisons.
- **Allowed:** virtual dispatch (e.g. `LogWriter`) — it needs no RTTI. Any
  polymorphic type must be used only through a reference/pointer obtained where
  the static type is already known.
- Prefer `enum class Kind` + `static_cast`, concepts, or tag types over
  runtime type queries.

### 1.3 No heap allocation

Production code performs **zero** dynamic allocation. There is no allocator
underneath `embdr` and no OOM path to handle.

- **Forbidden:** `operator new`, `operator delete`, `malloc`/`calloc`/`realloc`/
  `free`, `std::make_unique`, `std::make_shared`, `std::shared_ptr`,
  `std::vector`, `std::string`, `std::function`, `std::any`, `std::optional` of
  over-aligned types, iostreams (`std::cout`/`std::ostringstream`), `<regex>`.
  `[tests-ok]`
- **Use instead:**
    - fixed capacity: `BackingStorage<T, N>`, `BackingStorageSingle<T>`
    - allocator-aware static storage: `AllocatorTraits` / `memoryAllocator`
    - views over caller memory: `BasicStringView` (`SimpleStringView`)
    - value sums: `Maybe<T, E>`, `IoResult`
- Placement `::new (ptr)` into owned storage is the **only** legal `new`.
- Any buffer whose size is not provable at compile time must come from the
  caller (out-parameter or caller-provided span) — never from an internal
  allocation.
- Containers must be statically bounded: bounds are checked, overflow returns
  an error value, never grows.

### 1.4 No UB

- Pointers are never null, never dangling, always point to initialized memory. **`nullptr` may not be used to signal
  absence** — never `return nullptr;` on
  a failure path, never keep an optional pointer a `nullptr` "means empty".
  Use `Maybe<T*, E>` or a valid pointer into a static/stack object.
  Comparisons against `nullptr` are allowed only when validating a pointer
  that came from a C API (`dladdr`, `sigaction`, ...).
- No `reinterpret_cast` outside `embdr/cxxstd/unwind.cppm`,
  `sigHandler.cppm` and other low-level modules that document why; every such
  cast carries a comment justifying it.
- No signed overflow: use unsigned or checked helpers (`compare()` in
  `stringView.cppm`).
- `volatile` is the only opt-in to implementation-defined hardware access; a
  `volatile` block must be explicit and commented.
- No `const_cast` to remove `volatile`. No strict-aliasing violations (`std::memcpy`/`std::bit_cast` for type punning).

### 1.5 No exceptions-throwing standard library

`<string>`, `<vector>`, `<iostream>`, `<functional>`, `<locale>`,
`<filesystem>` are banned in `embdr/**` `[tests-ok]`. Allowed headers are
limited to what the modules already use:

```text
<cassert> <cinttypes> <cstddef> <cstdint> <cstdio> <cstdlib> <cstring>
<concepts> <cstdarg> <limits> <memory> <new> <type_traits> <utility>
<functional> (only as a constrained callable concept, never std::function)
<atomic> <mutex> <thread> (only inside explicitly documented concurrency code)
```

Every `#include` lives in the **global module fragment**, before
`export module ...;`.

---

### 1.6 Known legacy debt (do not copy, do not extend)

These exist today and violate the spirit of §1. They are migration targets, **not** precedent:

- `BumpAllocator::allocate()` / `BitmapAllocator::allocate()` return `nullptr`
  on exhaustion instead of `Maybe<T*, E>` (`memoryAllocator.cppm`).
- `IteratorDeleter::operator()` does a plain `delete` (`memoryIterator.cppm`).
- `FormatSink::dst = nullptr` is a "no output buffer" counting-mode sentinel (`stringFormat.cppm`);
  `stringifyFloat.cppm` mirrors `memchr`'s
  "not found == `nullptr`" convention.
- `unwind.cppm` / `sigHandler.cppm` / `unwindPrettify.cppm` legitimately hold
  `nullptr` locals while parsing C-ABI structures; new code elsewhere must not
  imitate that pattern.

Never widen an existing violation when touching a file.

---

## 2. `noexcept` discipline

- Every function in `embdr/**` is `noexcept`, or conditionally
  `noexcept(noexcept(expr))`.
- **All destructors are `noexcept`** (implicitly, and explicitly where the type
  is exported).
- A function that can fail returns `Maybe`/`IoResult`/`*Result` — it does not
  throw, and it is still `noexcept`.
- Violating a precondition in a `noexcept` function terminates the process.
  That is intended: fail fast, no unwinding, no binary bloat.

---

## 3. Error handling

- One error type per failure domain (`SvError`, `IoError`, `LogError`,
  `ConsoleError`, `SigError`, ...). Errors carry enough data to be formatted
  later (`console_error_message`, logger).
- Success and failure are distinguishable **without** inspecting payload bytes:
  `explicit operator bool()`, `has_value()`, `has_error()`.
- Accessors that can fail are `[[nodiscard]]`.
- Never return a sentinel (`-1`, `0`, `nullptr`) to signal failure.
- Never `std::abort` on a recoverable condition; never propagate a fatal
  condition silently — log it (`log_error`) before returning.

---

## 4. Compile-time first

- Anything computable at compile time **must** be `constexpr` (this is the
  project's #1 design goal).
- Constants are `static constexpr` members, never mutable globals.
- No function-local `static` with a non-trivial constructor (no dynamic init,
  no guard variables, no thread-safe statics — `-fno-use-cxa-atexit`).
- Prefer `if constexpr`, templates, and `static_assert` over runtime branches.
- `static_assert` guards the entry of every template: validate `Sz > 0`,
  value category, trivially-copyable, destructible (see `memoryAllocator.cppm`).

---

## 5. Naming and formatting

Enforced by `.clang-format` (LLVM base). Format before committing:

```bash
clang-format -i $(git ls-files '*.cppm' '*.cpp' '*.h')
```

| Element                  | Style                                      | Example           |
|--------------------------|--------------------------------------------|-------------------|
| Indent                   | 4 spaces, no tabs                          | `    value = 0;`  |
| Column limit             | 120                                        | —                 |
| Pointer/ref              | left-aligned                               | `char* const buf` |
| Class members            | `_M_` prefix                               | `_M_engaged`      |
| Static members / helpers | `_S_` prefix                               | `_S_move_assign`  |
| Types                    | `PascalCase`                               | `BackingStorage`  |
| Functions / variables    | `snake_case`                               | `has_error()`     |
| Constants                | `kLowerCamel` or `UPPER_SNAKE` for macros  | `capacity_value`  |
| Namespaces               | `snake_case`, always qualified             | `embdr::cxxstd`   |
| Enumerators              | `SCREAMING_SNAKE_CASE` inside `enum class` | `NETWORK_ERROR`   |
| Template params          | `PascalCase`                               | `typename T`      |

- Braces attached (except after function/struct/enum definitions — see
  `.clang-format` `BraceWrapping`).
- `SortIncludes: Never` — do not reorder includes manually.
- `public:`/`private:` indented at class-body level, not extra-indented.
- One declaration per line; no `using namespace` in headers/modules (it is fine
  in a `.cpp`/`main.cpp`).

---

## 6. Module conventions

- File extension `.cppm`, one module per file, file name == module leaf name.
- Declaration order, strictly:

  ```cpp
  /* MIT license header */

  module;                       // global module fragment
  #include <cstdint>            // only the C/STL headers actually used

  export module embdr.cxxstd.stringView;

  import embdr.cxxstd.memoryMaybe;

  namespace embdr::cxxstd {
          export ... // 8-space indent inside namespace
  }
  ```

- Module names: `embdr.cxxstd.<name>` (runtime) / `embdr.lex.<name>` (front
  end) / `embdc.<name>` (driver). Never invent a new top-level prefix.
- Export the minimum: internal helpers stay unexported in an inner namespace
  or an unnamed namespace.
- `import` after `export module`, one per line, project modules before standard
  library (the standard library is reached through the global module fragment
  `#include`s).
- The MIT header comment is copied verbatim at the top of every source file.

---

## 7. API rules

- All public entry points: `export`, `[[nodiscard]]` when a discarded result
  loses information, `noexcept`, and `constexpr` when possible.
- No global mutable state; if unavoidable, it must be `static` +
  documented + thread-safe via `std::atomic`.
- No raw owning pointers. Non-owning views are `T*`/`BasicStringView` and must
  document lifetime ("valid for the duration of the call").
- Virtual functions only where the vtable cost is justified (sink interfaces
  such as `LogWriter`); no virtual bases, no CRTP-for-RTTI-substitutes without
  a size justification comment.
- Prefer `explicit` single-argument constructors (always, unless conversion is
  the documented intent).
- Deleted special members where copying is wrong: `= delete` on move for
  storage types (see `BackingStorage`).

---

## 8. Binary size

- No code that only exists for convenience if it can be `constexpr`.
- Avoid templates instantiated for a single call site where a plain function
  would do (each instantiation is code size).
- No exceptions, no RTTI, no static initializers — see §1.
- Functions that must not be inlined are marked; everything else follows the
  compiler. Do not add blanket `__attribute__((always_inline))` without a size
  justification comment.

---

## 9. Tests

- Location: `embdr/tests/<module>_test.cpp`, framework **Catch2**
  (`catch2/catch_all.hpp`, already vendored in `third-party/catch2`).
- Every new module ships at least one test file, registered in
  `CMakeLists.txt` inside the `embdr-tests` target.
- Tests **may** use `std::string`, `std::vector`, threads and `<regex>`
  (`[tests-ok]`), but the code under test must not.
- Tests must be deterministic: no wall-clock sleeps beyond what
  `timeCore_test` already does, no unseeded randomness, no reliance on test
  order, no leaked fds.
- Cover the failure path of every `Maybe`/`IoResult` returning function, not
  only the success path.
- Run: `ctest --test-dir build --output-on-failure`.

---

## 10. Enforcement

Before considering any change done:

```bash
# 1. Format
clang-format -i $(git ls-files '*.cppm' '*.cpp')

# 2. Build both profiles (Release enforces -fno-exceptions/-fno-rtti)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/debug -j"$(nproc)"

# 3. Tests (+ ASan in Debug)
ctest --test-dir build --output-on-failure
ctest --test-dir build/debug --output-on-failure

# 4. Static gates — these MUST print nothing
#    (no exceptions / RTTI anywhere in embdr, tests included)
grep -rnE '\bthrow\b|\btry\b|\bcatch[[:space:]]*\(|dynamic_cast|typeid' embdr --include='*.cppm' --include='*.cpp'

#    (no heap / allocating std lib in production code)
grep -rnE '(^|[^:[:alnum:]_])(malloc|calloc|realloc|free)[[:space:]]*\(|std::(vector|string|function|shared_ptr|make_shared|cout|ostringstream)' \
  embdr/cxxstd embdr/lex embdr/embdc --include='*.cppm' --include='*.cpp'

#    (no new-expressions or delete-expressions; placement new, `= delete`,
#     <new> includes and memoryIterator's legacy OwnedIterator excepted)
grep -rnE '\bdelete[[:space:]]+[A-Za-z_&*(]|(^|[^:_[:alnum:]])new[[:space:]]*[(]&?[A-Za-z_]' \
  embdr/cxxstd embdr/lex embdr/embdc --include='*.cppm' --include='*.cpp' | grep -v memoryIterator.cppm

#    (no nullptr sentinels introduced outside the §1.6 modules)
grep -rnE 'return nullptr|[^\=!<>]= nullptr' embdr/cxxstd --include='*.cppm' \
  | grep -vE 'unwind|sigHandler|memoryIterator|memoryAllocator|stringifyFloat|stringFormat'
```

Every hit of a gate is either a bug or a documented §1.6 exception — never a
"close enough".

`clang-tidy` (when available) with `bugprone-*,performance-*,modernize-*,
-concurrency-*,-avoid-non-const-global-variables` is the secondary gate;
compiler warnings are errors: build with `-Wall -Wextra -Wpedantic -Werror`.

### PR checklist

- [ ] Builds in Release **and** Debug, zero new warnings
- [ ] `ctest` green in both profiles
- [ ] Section 10 grep gates print nothing
- [ ] `clang-format` applied
- [ ] No new allocation, exception, RTTI or `nullptr` in `embdr/cxxstd`
- [ ] New module registered in `CMakeLists.txt` (sources + modules)
- [ ] Tests added for every new/changed public function, failure paths included
- [ ] MIT header present in every new file
