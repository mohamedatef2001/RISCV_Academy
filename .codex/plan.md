# RISC-V CV Algorithm Skill Plan

## Goal

Create a repository-scoped Codex skill named `riscv-cv-algorithm` that adds complete
image-algorithm slices to this repository. For each algorithm, the workflow is:

1. Define and implement a clear scalar reference.
2. Implement the equivalent RISC-V Vector (RVV) algorithm.
3. Test the vectorized output against the scalar reference.
4. Benchmark both implementations and report the comparison.

The skill must ignore `example/source/` unless a future request explicitly
brings it into scope.

## Algorithm Organization

For an algorithm named `<Name>`, use the existing `image_Add` layout and naming
style:

| Responsibility | Location |
| --- | --- |
| Vector API | `lib/include/image_<Name>.hpp` |
| RVV implementation | `lib/source/image_<Name>.cpp` |
| Scalar API | `test/reference/include/image_<Name>.hpp` |
| Scalar implementation | `test/reference/source/image_<Name>.cpp` |
| Correctness tests | `test/unit_test/source/<Name>_test.cpp` |
| Comparative benchmark | `benchmark/source/<Name>_benchmark.cpp` |

Also expose the two APIs through:

- `lib/include/riscv_cv.hpp`
- `test/reference/include/reference_cv.hpp`

Preserve the current PascalCase naming convention, such as `image_Add.hpp`,
`Add_test.cpp`, and `Add_benchmark.cpp`.

The existing CMake source globs discover ordinary new `.cpp` files. Do not edit
CMake unless an algorithm genuinely needs a new target, dependency, generated
asset, or other exceptional build behavior.

## Skill Workflow

Before writing code, the skill must inspect the repository and establish the
algorithm contract:

- input and output image types;
- function parameters and namespace API;
- size and shape requirements;
- overflow, rounding, border, and channel behavior where applicable;
- whether input/output aliasing is supported;
- expected behavior for invalid inputs.

The skill should infer these details from the request and existing project
conventions when safe. It should ask for clarification only when a missing
semantic choice would materially change the implementation.

### Scalar reference

- Implement the readable scalar oracle first in `test/reference`.
- Favor obvious correctness over optimization.
- Keep its public signature aligned with the vector API whenever practical.

### RVV implementation

- Place production code in `lib` under the `vec::` namespace.
- Use dynamic `vsetvl` loops and handle tails safely; never assume a fixed
  hardware VLEN.
- Match the scalar implementation exactly for overflow, rounding, borders,
  channels, and other algorithm-specific semantics.
- Preserve the project's bare-metal and QEMU compatibility.

### Correctness tests

- Compare vector output directly with the scalar oracle.
- Use deterministic inputs so failures are reproducible.
- Cover small inputs, exact vector-width cases, tail elements, multiple rows,
  parameter variants, and algorithm-specific boundary cases.
- Add targeted values for overflow, saturation, rounding, or border behavior
  when random data would not guarantee coverage.

### Benchmark

- Use identical preallocated inputs for scalar and vector implementations.
- Keep allocation and initialization outside timed regions.
- Check correctness during the benchmark and return failure on a mismatch.
- Report average cycles, retired instructions, pixels per cycle, and speedup.
- Do not impose a minimum speedup unless the user explicitly requests one.

## Validation Sequence

For an algorithm named `<Name>`, the skill considers the work complete only
after this sequence succeeds:

1. Build the generated targets.
2. Run `run_<Name>_test_qemu` for fast focused correctness.
3. Run `run_unit_tests_qemu` for regression coverage.
4. Run `run_<Name>_benchmark_qemu` for hosted functional validation.
5. Run `run_<Name>_benchmark_gem5` for the final cycle-accurate comparison.

If a required external dependency is unavailable, report the exact blocked
step and preserve all completed validation results. Do not claim full
completion when the gem5 benchmark has not run.

## Skill Package

Create the skill at:

`/home/mohamed/RISCV_Academy/.codex/skills/riscv-cv-algorithm`

The package will contain:

- `SKILL.md` with the repository-specific workflow and constraints;
- `agents/openai.yaml` with:
  - display name: `RISC-V CV Algorithm`;
  - short description: `Add scalar, RVV, tests, and benchmarks`;
  - a default prompt that explicitly invokes `$riscv-cv-algorithm`;
  - implicit invocation enabled.

No scripts, templates, assets, or repository documentation are needed for the
first version. Validate the finished package with the skill creator's
`quick_validate.py`.

## Feedback and Continuous Improvement

The skill should improve from real use without silently changing its own rules:

1. At the end of each algorithm task, summarize relevant feedback, failures,
   missing cases, and repository conventions discovered during the work.
2. Separate reusable lessons from algorithm-specific facts and one-off user
   preferences.
3. Propose a small, concrete skill update only when the lesson would improve
   future algorithm tasks.
4. Ask for confirmation before modifying `SKILL.md` or its metadata.
5. After an approved update, re-run `quick_validate.py` and record what changed
   in the task handoff. Do not create an internal changelog unless requested.

Failed experiments, unverified assumptions, and isolated preferences must not
be promoted into permanent skill instructions. Existing user feedback always
takes priority over inferred lessons.

## Assumptions and Boundaries

- The skill supports arbitrary image algorithms, not only Add-like binary
  pixel operations.
- Scalar implementations remain in the test/reference library rather than the
  production library.
- The existing public library API changes only when adding a requested
  algorithm.
- `example/source/`, unrelated untracked files, and unrelated user changes must
  remain untouched.
- Feedback can suggest skill improvements, but skill files change only with
  explicit user approval.
