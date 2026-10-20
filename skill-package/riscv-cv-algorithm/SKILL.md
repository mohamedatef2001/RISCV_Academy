---
name: riscv-cv-algorithm
description: Add image and computer-vision algorithms to RISCV_Academy as a scalar reference, matching RVV implementation, correctness tests, and comparative benchmark. Use for pixel kernels and tensor/list post-processing such as NMS; exclude example programs unless explicitly requested.
---

# RISC-V CV Algorithm

Build each requested computer-vision operation as one consistent vertical
slice. Treat `image_Add` as a structural example, not as a restriction to
pixel-shaped inputs or one-output kernels.

## Establish the contract

Inspect the current repository before editing. Ignore `example/source/**` unless
the user explicitly brings it into scope, and preserve unrelated or untracked
work.

Determine the operation name, input and output types, parameters, shape rules,
ordering and tie behavior, comparison strictness, overflow or rounding behavior,
supported aliasing, and invalid-input behavior. For image kernels also define
border and channel behavior. For list/tensor post-processing define output
capacity, index layout, batch/class traversal, and whether stages have sequential
dependencies. Infer these from the request and established project conventions
when safe. Ask the user only when an unresolved semantic choice would materially
change the API or results.

Kernel implementations must not allocate or free dynamic memory (`new`,
`delete`, `malloc`, or equivalent). Require the caller to provide all input,
output, and scratch storage needed by the public contract. Expose a deterministic
workspace-capacity helper when an algorithm or selectable strategy changes the
required scratch size. Keep allocation performed by test or benchmark fixtures
outside timed regions. If aliasing
would require an internal snapshot, reject or exclude that aliasing mode
explicitly instead of allocating implicitly.

When broadening an API to support runtime parameters, retain specialized hot
paths for established benchmark cases instead of forcing them through the
generic fallback. Keep the public entry point OpenCV-shaped where requested,
then dispatch internally to fixed-size kernels such as 3x3 and to a generic
odd-kernel implementation. Record the prior gem5 cycle/instruction result and
report the delta; do not treat QEMU CSR timing as cycle-accurate evidence.

Use the current PascalCase file convention. For a new image operation named
`<Name>`, place its files as follows:

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

When the task imports an established scaffold or implements non-image
post-processing, preserve its public filename and symbol convention instead of
forcing the `image_<Name>` prefix. Record the upstream repository and exact
commit for imported fixtures or teaching assets.

## Implement in this order

1. Write the scalar `ref::` implementation as an obvious, readable correctness
   oracle. Do not optimize it at the expense of clarity.
2. Write the production `vec::` implementation with RVV intrinsics. Use dynamic
   `vsetvl` loops, handle tails safely, and never assume a fixed hardware VLEN.
   Match scalar semantics exactly and retain bare-metal and QEMU compatibility.
   Use the [RVV Intrinsics Viewer](https://dzaima.github.io/intrinsics-viewer/#0q1YqVbJSKsosTtYtU9JRSoVzFMsSU1LiyyriMy1yjYAyiUpW0UplSrE6SskglmdeSWp6ahFQwi0nP7EEROfnpCjF1gIA)
   to look up intrinsic names, signatures, types, and corresponding instructions;
   confirm the selected intrinsic is supported by the repository's configured
   compiler and RVV version.
3. Add deterministic parameterized tests that compare vector output directly
   with the scalar oracle. Cover small inputs, exact vector chunks, tails,
   multiple rows, supported parameter variants, and operation-specific boundary
   cases. Add explicit overflow, saturation, rounding, or border values when
   random inputs would not guarantee coverage. For staged selection algorithms,
   isolate threshold boundaries, stable ties, sort variants, degenerate geometry,
   strict comparison boundaries, batches/classes, and vector tails. Validate
   imported golden fixtures independently before using them as the oracle.
4. Add a benchmark using identical preallocated inputs. Keep allocation and
   initialization outside timed regions, time both implementations with the
   repository timer, check correctness, and fail on a mismatch. Report averaged
   cycles, retired instructions, an algorithm-appropriate work unit, and speedup.
   Use pixels for image kernels and candidates/boxes for detection post-processing.
   When comparing selectable strategies, warm every path, alternate timed order,
   use identical data, report multiple representative problem sizes, and verify
   every output against the same oracle. When a relevant historical gem5 result
   exists, print its cycle/instruction deltas as a non-gating regression signal.
   Do not enforce a minimum speedup unless the user asks for one.

Keep the scalar implementation in `test/reference`: it is a validation and
benchmark oracle, not part of the production vector library. If the user asks
for a supported runtime scalar fallback, pause and redesign the library API and
layout explicitly instead of silently moving the reference code into `lib`.

## Validate

Build the new targets, then run these checks in order:

1. `run_<Name>_test_qemu`
2. `run_unit_tests_qemu`
3. `run_<Name>_benchmark_qemu`
4. `run_<Name>_benchmark_gem5`, unless the user asks to run gem5 themselves

Use the actual generated target spelling. QEMU validates behavior and benchmark
completion but its CSR timing is not a cycle-accurate performance result. The
gem5 benchmark is the final cycle-accurate comparison. If an external dependency
or simulator is unavailable or the user reserves the gem5 run for themselves,
report the exact command and the checks that did pass; do not claim complete
performance validation.

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
