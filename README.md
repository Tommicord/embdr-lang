# embedrite

**embedrite** is an experimental systems programming language for deeply embedded
targets, with the **embdc** compiler written in C++26 (C++20 modules).

Design goals, in order of priority:

1. **Binary size at or below C** (and far below C++) — whole-program
   compile-time evaluation, no exceptions/RTTI/unwinding, dead-section
   elimination, aggressive inlining control.
2. **Compile-time first** — expressions whose values are known at compile time
   are *implicitly* `cexpr`; the compiler folds them, including pointers to
   static objects (resolved to fixed absolute addresses).
3. **Wide arithmetic types** — `int128/256/512`, `float16/128/256/512` that map
   to SIMD registers when the target has them, with a software fallback built
   on the widest available integer type.
4. **No null, no UB by default** — pointers are always valid and initialized
   (`static`/`bss`/`data` or stack); errors are values, not exceptions.
5. **Monadic error handling** — `result<T, E>` with `?` propagation; no
   exceptions, no unwinding tables (a direct size win on embedded).
6. **Modules as scopes** — functions and types live in modules; no global
   namespace soup.
7. **Fast builds** — per-module compilation with a staged, parallel pipeline:
   ROS checker → vectorizer → LTO, never two of them on the same file at once.

> Status: **pre-alpha**. The runtime library (`embdr/cxxstd`) exists; the
> compiler front-end is being bootstrapped. The language design below is a
> draft (RFC) and will evolve.

---

## 1. Language idea (draft)

### 1.1 Types

| Category       | Types                                                                                                                                      |
|----------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| Integers       | `int8..int64`, `uint8..uint64`, `int128`, `int256`, `int512`                                                                               |
| Floats         | `float16`, `float32`, `float64`, `float128`, `float256`, `float512`                                                                        |
| SIMD vectors   | lane types backed by NEON / Helium (MVE) / RISC-V V / SSE / AVX-512 when present                                                           |
| Error handling | `result<T, E>` (monad, `?` operator) — no exceptions                                                                                       |
| Optionality    | no `null` anywhere; absence is an explicit sum type                                                                                        |
| Pointers       | never null, always point to initialized memory; `volatile` blocks are the only way to opt out (and the only place UB is possible)            |

Wide types lower to hardware when the ISA provides it (e.g. AVX-512 ZMM,
SVE, RVV registers) and otherwise to multi-limb software arithmetic on the
target's largest integer registers. Lane-parallel operations (SIMD) and
scalar big-integer semantics (carry chains) are tracked separately in the
type system.

### 1.2 Implicit compile-time evaluation

Any expression whose operands are compile-time constants is evaluated at
compile time *by default* — no `cexpr` annotation required:

```text
int32 area = 3 * 4 * 5;          // folded, 0 runtime cost (no annotation)
int32 mask = (1 << 16) - 1;      // folded to an immediate
int32 p    = &static_buf[3];     // folded to a fixed absolute address
```

A `cexpr` annotation only *forces* evaluation (and is a compile error if the
expression is not computable at compile time). Side-effecting operations are
never implicitly folded — purity is inferred per expression.

### 1.3 Error handling

```text
function ::read_sensor(bus: i2c::bus) -> result<sample, i2c::error> {
    let raw = bus.read_reg(REG_TEMP)?;   // propagates on error
    ret Ok(decode(raw));
}
```

No `throw`, no unwind tables in the binary, no `std::unwind` runtime.

### 1.4 Modules

```text
module drivers::bme280 {
    pub function ::init(bus: i2c::bus) -> result<device, i2c::error> { ... }
}
```

Modules are the unit of parallel compilation *and* the unit of the staged
optimization pipeline (§2.3).

### 1.5 No UB by default

- Pointers are always dereferenceable and point to initialized objects.
- The compiler proves this via the **ROS (Reference-Ownership Semantics)
  checker** — a borrow-checker-style ownership/lifetime analysis.
- `volatile { }` blocks and `volatile` objects are the only regions where UB
  can occur; they are explicit, auditable and greppable.

---

## 2. Optimization pipeline

### 2.1 Optimizations (planned)

| #  | Optimization                                    | Idea                                                                          | Status                   |
|----|-------------------------------------------------|-------------------------------------------------------------------------------|--------------------------|
| 1  | Implicit `cexpr` folding                        | constant folding as a language guarantee, not a heuristic                     | planned                  |
| 2  | Compile-time pointers                           | known addresses → fixed absolute/PC-relative operands                         | planned                  |
| 3  | Nested-loop branchless conversion               | deeply nested `for` → branchless arithmetic → bitmasks kept in SIMD registers | planned                  |
| 4  | AST-level branchless rewrite                    | non-branchless → branchless (IR-level melding, see *MERIT* below)             | planned                  |
| 5  | 8× unroll (simple loops) / SIMD (complex loops) | unroll-and-jam vs. vectorization chosen per loop                              | planned                  |
| 6  | Linked-structure prefetching                    | compiler inserts cache-line prefetches for linked lists/heaps                 | planned                  |
| 7  | ROS checker                                     | ownership/lifetime verification, no GC, UB only inside `volatile`             | planned                  |
| 8  | Code vectorizer                                 | target-aware vectorization of wide types (`int256/512`, `float*`)             | planned                  |
| 9  | LTO                                             | per-module, staged after ROS + vectorizer                                     | planned                  |
| 10 | Inlining control                                | large functions are **not** implicitly inlined (fast compiles, small code)    | planned                  |
| 11 | Non-destructive move for simple stores          | compiler detects trivially copyable stores and elides destructive moves       | planned (SROA/SSA based) |

### 2.2 Pipeline diagram

```text
                 per module, in parallel across files
   ┌──────────┐   ┌──────────────┐   ┌────────────┐   ┌─────────┐
   │  source  │──▶│ lexer/parser │──▶│  AST + cexpr│──▶│  ROS    │
   └──────────┘   └──────────────┘   │  folding    │   │ checker │
                                     └────────────┘   └────┬────┘
                                                           │ (file released)
                    ┌──────────────────────────────────────┘
                    ▼
             ┌──────────────┐    ┌──────────────┐    ┌─────────────┐
             │ code         │───▶│ linker (LTO, │───▶│  image      │
             │ vectorizer   │    │ gc-sections) │    │  (.elf/.bin)│
             └──────────────┘    └──────────────┘    └─────────────┘
```

Ordering rule: **ROS, vectorizer and LTO never run on the same file at the
same time.** When ROS finishes a module, the vectorizer picks it up; LTO runs
last across the linked unit. This keeps all three phases parallel without
invalidating each other's results.

### 2.3 Optimizations (papers *not* implemented in LLVM/GCC)

These are the references behind the table in §2.1 — optimizations that stock
LLVM/GCC do not ship:

**Expression level**
- *Equality Saturation: A New Approach to Optimization* — Tate et al., POPL 2009;
  engine: **egg**, Willsey et al., POPL 2021; MLIR `eqsat` dialect, arXiv 2025.
  Non-destructive, global-cost rewriting — the model for §2.1 #3/#4.
- *MERIT: Eliminate Branches by Melding IR Instructions* — ECOOP 2026.
  Branch removal at target-independent IR level → branchless code without
  speculation.
- *Stochastic Superoptimization (STOKE)* — Schkufza et al., ASPLOS 2013
  (+ PLDI 2014 for tunable-precision FP); *Souper* — CCO 2017 (~7,900
  optimizations LLVM missed; 4.4% smaller clang binary). Basis for beating C
  on size.

**Floating-point / complex math**
- *Herbie* — Panchekha et al., PLDI 2015: automatic accuracy-improving FP
  rewrites (up to 60 bits recovered).
- *RLibm* — Lim & Nagarakatte, PLDI 2021 / POPL 2022 / PLDI 2025:
  correctly-rounded elementary functions generated as LP problems (partly in
  LLVM libc, but as a library, not an optimizer).
- *FPTaylor* (TOPLAS 2018), *Gappa* (IEEE TC 2011), *PRECiSA* (SAFECOMP 2017):
  verified round-off error bounds — supports the "no UB" story.

**Loops & memory**
- *Pluto* — Bondhugula et al., PLDI 2008 + polyhedral survey (ACM TACO 2024).
  LLVM Polly / GCC Graphite exist but are off-by-default and weak; full
  polyhedral scheduling for nested math loops is effectively absent from
  production compilers.
- *Dependence-Based Prefetching for Linked Data Structures* — Roth & Sohi,
  ASPLOS 1999; *Linkey*, arXiv 2025. LLVM has a `prefetch` intrinsic but no
  pass that auto-prefetches pointer chasing (§2.1 #6).

**Wide integers on SIMD**
- *A RISC-V Vector Extension for Multi-word Arithmetic* — Eum, Zhang,
  Franchetti, SC Workshops 2025 (+ HPEC 2024). Carry-chain multi-word
  arithmetic on RVV/AVX-512 — exactly the `int256/512` lowering strategy.

**Search-based / learned optimization**
- *Halide* (PLDI 2013, CACM 2018), *Ansor* (OSDI 2020), *TVM* (OSDI 2018):
  schedule search + learned cost models instead of hand-tuned heuristics.

**Automatic differentiation (complex math)**
- *Enzyme* — Moses & Churavy, NeurIPS 2020; GPU kernels SC 2021: reverse-mode
  AD as an LLVM plugin, works on CPU + CUDA/ROCm.

**GPU+CPU compilers worth studying:** Halide, Tiramisu (CGO 2019), Futhark
(PLDI 2017), TVM/Ansor, SPIRAL/FFTX (*Proc. IEEE 2018*), Enzyme, DaCe.

### Build options

| Option                       | Effect                                                              |
|------------------------------|---------------------------------------------------------------------|
| `-DCMAKE_BUILD_TYPE=Release` | `-fno-exceptions -fno-rtti -ffunction-sections --gc-sections` + LTO |
| `-DEMBDR_EMBEDDED=ON`        | embedded link profile (`--gc-sections`)                             |
| `-DEMBDR_DISABLE_ASAN=ON`    | skip AddressSanitizer in Debug                                      |

---

## 3. Roadmap

- [ ] Phase 0: test harness
- [ ] Phase 1: lexer
- [ ] Phase 2: parser + AST
- [ ] Phase 3: sema / type checker
- [ ] Phase 4: implicit-cexpr folding
- [ ] Phase 5: ROS checker
- [ ] Phase 6: LLVM IR lowering
- [ ] Phase 7: codegen + first cross-target binary size benchmark vs. C
- [ ] Wide types (`int128` first, soft-limb fallback in `embdr/cxxstd`)
- [ ] Branchless/AST optimization pass (MERIT-style)
- [ ] Linked-structure prefetch pass
- [ ] Staged parallel pipeline (ROS → vectorizer → LTO)

---

## 4. License

MIT (see [LICENSE](LICENSE)).
