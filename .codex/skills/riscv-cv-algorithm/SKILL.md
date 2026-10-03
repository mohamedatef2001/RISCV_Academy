---
name: riscv-cv-algorithm
description: Add image-processing algorithms to the RISCV_Academy repository as a scalar reference, matching RVV implementation, correctness tests, and comparative benchmark. Use for new algorithms or algorithm-specific fixes in this repository; exclude example programs unless explicitly requested.
---

# RISC-V CV Algorithm

Build each requested image operation as one consistent vertical slice. Treat
`image_Add` as the structural example, not as a restriction to binary pixel
operations.

## Establish the contract

Inspect the current repository before editing. Ignore `example/source/**` unless
the user explicitly brings it into scope, and preserve unrelated or untracked
work.

Determine the operation name, input and output types, parameters, shape rules,
overflow or rounding behavior, border and channel behavior, supported aliasing,
and invalid-input behavior. Infer these from the request and established project
conventions when safe. Ask the user only when an unresolved semantic choice
would materially change the API or results.

Use the current PascalCase file convention. For an operation named `<Name>`,
place its files as follows:

| Role | Path |
| --- | --- |
| Vector API | `lib/include/image_<Name>.hpp` |
| RVV implementation | `lib/source/image_<Name>.cpp` |
| Scalar API | `test/reference/include/image_<Name>.hpp` |
| Scalar implementation | `test/reference/source/image_<Name>.cpp` |
| Correctness test | `test/unit_test/source/<Name>_test.cpp` |
| Benchmark | `benchmark/source/<Name>_benchmark.cpp` |

Declare matching functions in `vec::` and `ref::` with identical signatures
whenever practical. Add their headers to `lib/include/riscv_cv.hpp` and
`test/reference/include/reference_cv.hpp`, respectively. The existing CMake
globs discover ordinary new `.cpp` files; change CMake only for a genuine new
dependency, generated asset, or exceptional target requirement.

## Implement in this order

1. Write the scalar `ref::` implementation as an obvious, readable correctness
   oracle. Do not optimize it at the expense of clarity.
2. Write the production `vec::` implementation with RVV intrinsics. Use dynamic
   `vsetvl` loops, handle tails safely, and never assume a fixed hardware VLEN.
   Match scalar semantics exactly and retain bare-metal and QEMU compatibility.
3. Add deterministic parameterized tests that compare vector output directly
   with the scalar oracle. Cover small inputs, exact vector chunks, tails,
   multiple rows, supported parameter variants, and operation-specific boundary
   cases. Add explicit overflow, saturation, rounding, or border values when
   random inputs would not guarantee coverage.
4. Add a benchmark using identical preallocated inputs. Keep allocation and
   initialization outside timed regions, time both implementations with the
   repository timer, check correctness, and fail on a mismatch. Report averaged
   cycles, retired instructions, pixels per cycle, and speedup. Do not enforce a
   minimum speedup unless the user asks for one.

Keep the scalar implementation in `test/reference`: it is a validation and
benchmark oracle, not part of the production vector library. If the user asks
for a supported runtime scalar fallback, pause and redesign the library API and
layout explicitly instead of silently moving the reference code into `lib`.

## Validate

Build the new targets, then run these checks in order:

1. `run_<Name>_test_qemu`
2. `run_unit_tests_qemu`
3. `run_<Name>_benchmark_qemu`
4. `run_<Name>_benchmark_gem5`

Use the actual generated target spelling. The gem5 benchmark is the final
cycle-accurate comparison. If an external dependency or simulator is
unavailable, report the exact blocked command and the checks that did pass; do
not claim complete validation.

## Learn from feedback

At the end of each algorithm task, summarize user feedback, failures, missing
coverage, and newly discovered repository conventions. Separate reusable
workflow lessons from facts specific to one algorithm and from one-off user
preferences.

When a reusable lesson would improve later work, propose a small concrete
change to this skill and request confirmation before editing the skill. After
an approved change, validate the skill again with the skill creator's
`quick_validate.py` and mention the change in the handoff. Never promote an
unverified assumption, failed experiment, or isolated preference into a
permanent rule, and never rewrite the skill silently.
