# AGENTS.md

Instructions for AI agents (and new contributors) working on **embedrite**.

Read `CODE_QUALITY.md` first — it is the binding standard. This file is the
workflow; that file is the ruleset. When they conflict, `CODE_QUALITY.md`
wins.

---

## 1. What this project is

- **embedrite**: experimental systems language for deeply embedded targets.
- **`embdc/`**: the compiler driver (C++26, C++20 modules) — front end being
  bootstrapped (`embdr/lex/*`).
- **`embdr/cxxstd/`**: the runtime library that exists today and is the bulk of
  the code.
- **`embdr/tests/`**: Catch2 tests.
- Status: pre-alpha. The language design in `README.md` is a draft (RFC).

Design priority order (never invert it):
**binary size > compile-time evaluation > correctness > readability.**

---

## 2. Hard constraints — non-negotiable

Violating any of these invalidates the change, no matter how clean the rest is.

1. **No exceptions.** No `throw`/`try`/`catch`, no `std::exception` descendants.
   Errors are values: `Maybe<T, E>`, `IoResult`, `*Result`, `*Error`.
2. **No RTTI.** No `dynamic_cast`, no `typeid`. Virtual dispatch is allowed (e.g. `LogWriter`); runtime type queries are
   not.
3. **No heap allocation.** No `new`/`delete` (except placement `::new` into
   owned storage), no `malloc`/`free`, no `std::vector`, `std::string`,
   `std::function`, `std::shared_ptr`, no iostreams in `embdr/**`. Use
   `BackingStorage<T, N>`, `BasicStringView`, caller-provided buffers.
4. **No `nullptr` as a sentinel** in `embdr/cxxstd`. Pointers are always valid
   and initialized; use `Maybe<T*, E>` for absence.
5. **Everything is `noexcept`** (or conditionally `noexcept`) and as
   `constexpr` as possible.
6. **Never remove `-fno-omit-frame-pointer`** from `CMakeLists.txt` — the
   in-process unwinder (`embdr/cxxstd/unwind.cppm`) walks frame pointers.
7. **No dynamic initialization**: no non-trivial function-local `static`s, no
   global constructors (`-fno-use-cxa-atexit` is on).
8. **Tests and `third-party/` are exempt** from rules 3–4 only (`[tests-ok]` in `CODE_QUALITY.md`); they are never
   exempt from rules 1–2.

Check yourself with the grep gates in `CODE_QUALITY.md` §10 — they must print
nothing.

---

## 3. Build and test

```bash
# Release (enforces -fno-exceptions -fno-rtti --gc-sections + LTO)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# Debug (ASan on, EMBDR_DEV defined)
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j"$(nproc)"

# Tests (both profiles)
ctest --test-dir build --output-on-failure
ctest --test-dir build/debug --output-on-failure
```

CMake ≥ 4.0, C++26, modules scanning on. Options:
`-DEMBDR_BUILD_TESTS=OFF`, `-DEMBDR_DISABLE_ASAN=ON`.

**Both profiles must build with zero new warnings before you finish.**

---

## 4. Workflow

1. **Read before writing.** Find the closest existing module (`stringView.cppm`, `memoryMaybe.cppm`, `io.cppm`) and
   imitate its shape:
   license header → global module fragment → `export module` → `import` →
   `namespace embdr::...` → exports.
2. **Never break the subset.** If the feature seems to need an allocation, an
   exception or RTTI, redesign it: fixed-capacity storage, error values,
   `enum class Kind` + `static_cast`, concepts.
3. **Test the failure path.** Every function returning `Maybe`/`IoResult`
   gets a test for the error branch, not just the happy path.
4. **Format last:** `clang-format -i <changed files>` (config: `.clang-format`,
   4-space indent, 120 columns, `_M_`/`_S_` member prefixes).
5. **Verify:** run §4 commands plus the grep gates in `CODE_QUALITY.md` §10.
6. **Commit only when explicitly asked.** Do not amend, force-push, or touch
   git config. Do not commit `build/`, `.idea/`, or `third-party/` changes.

---

## 5. Conventions at a glance

| Thing     | Rule                                                                                       |
|-----------|--------------------------------------------------------------------------------------------|
| File      | `.cppm`, name == module leaf name, MIT header verbatim at top                              |
| Module    | `export module embdr.cxxstd.<name>;` / `embdr.lex.<name>;`                                 |
| Includes  | only in the global module fragment (`module;` before `export module`)                      |
| Namespace | `embdr::cxxstd`, `embdr::lex` — never a new top-level prefix                               |
| Members   | `_M_name`, static helpers `_S_name`                                                        |
| Types     | `PascalCase`; functions/vars `snake_case`; `enum class` enumerators `SCREAMING_SNAKE_CASE` |
| Errors    | `Maybe<T,E>` / `IoResult` / `FooResult` + `FooError`, all `[[nodiscard]]`                  |
| Templates | `static_assert` on every precondition at the top of the template                           |
| Comments  | explain *why*, especially any `reinterpret_cast`, `volatile`, or UB-adjacent code          |
| Style     | follow `.clang-format` exactly; `SortIncludes: Never`                                      |

---

## 6. Definition of done

- [ ] Release **and** Debug builds succeed, no new warnings
- [ ] `ctest` green in both profiles
- [ ] Grep gates in `CODE_QUALITY.md` §10 print nothing
- [ ] `clang-format` applied to changed files
- [ ] New modules/tests registered in `CMakeLists.txt`
- [ ] Failure-path tests added
- [ ] No new allocation, exception, RTTI, or `nullptr` in `embdr/cxxstd`
