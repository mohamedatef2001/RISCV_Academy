# RISC-V_CV

A RISC-V image-processing library (`vec::` kernels) with a scalar reference
implementation (`ref::`), unit tests, and benchmarks. The whole project
cross-compiles to RISC-V and builds **two images per test/benchmark**:

- **gem5** — bare-metal `.bin` images linked against the
  [nexus-am](https://github.com/OpenXiangShan/nexus-am) abstract-machine
  runtime, run on the **XiangShan gem5 fork** (XS-GEM5) for cycle-accurate
  simulation.
- **qemu-user** — static Linux ELFs (`<name>_qemu`) run under
  `qemu-riscv64` user-mode for fast functional validation.

## Prerequisites

- CMake >= 3.22.
- A GNU RISC-V cross toolchain providing `riscv64-linux-gnu-g++` (GCC 13+
  recommended; the build is validated with GCC 15).
- A checkout of **nexus-am** (provides the `am`/`klib` bare-metal runtime).
- A built **XS-GEM5** (`build/RISCV/gem5.opt`) and its `configs/example/kmhv3.py`.
- **QEMU user-mode** — `qemu-riscv64` from Debian/Ubuntu `qemu-user` (see
  [`SETUP.md`](SETUP.md) for the apt line).

If you don't have nexus-am, XS-GEM5, or QEMU yet, [`SETUP.md`](SETUP.md) walks
through the whole thing from an empty machine: host packages, cloning nexus-am
and XS-GEM5, building DRAMsim3 and gem5, then this project.

## Build

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DNEXUS_AM_HOME=$HOME/nexus-am -DGEM5_HOME=$HOME/GEM5
cmake --build build -j8
```

This cross-compiles the library, the unit tests, and the benchmarks. Each
test/benchmark produces a gem5 bare-metal `.bin` (via objcopy) and a static
Linux ELF `<name>_qemu`. The nexus-am `am`/`klib` archives are built
automatically as part of the build, so they always match the configured
architecture and `-march`.

If `AM_HOME` / `XIANG_GEM5` are already set in your environment (as the
reference setup does), you can omit the `-D` flags entirely:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake
cmake --build build -j8
```

## Run 

**gem5** (cycle-accurate):

```bash
cmake --build build --target run_<app_name>_gem5
```

**qemu-user** (fast functional):

```bash
cmake --build build --target run_<app_name>_qemu
```

gem5 pass/fail is detected from the test summary regex (gem5 does not
propagate the guest's exit code). qemu-user tests use the guest exit code
directly (no PASS/FAIL regex).

Single unit-test
```bash
cmake --build build --target run_Add_test_qemu   # fast functional
cmake --build build --target run_Add_test_gem5   # cycle-accurate
```

All unit-tests
```bash
cmake --build build --target run_unit_tests_qemu   # fast functional
cmake --build build --target run_unit_tests_gem5   # cycle-accurate
```

Benchmarck 
```bash
cmake --build build --target run_Add_benchmark_gem5   # cycle-accurate
cmake --build build --target run_Add_benchmark_qemu   # fast functional
```

Benchmarks report cycle and retired-instruction counts on gem5 (milliseconds are
meaningless on a cycle-level simulator). Each gem5 run writes a raw UART log to
`build/<dir>/<target>.log` and gem5 stats to `build/<dir>/<target>-m5out/`.
Each qemu run tees to `build/<dir>/<target>.log` (e.g. `unit_tests_qemu.log`).

## Configuration variables

All install locations and flags are CMake cache variables, so anyone can point
the build at their own toolchain / nexus-am / gem5 / QEMU without editing the
build files. Pass them with `-D<VAR>=value` at configure time.

| Variable | Default | Meaning |
| --- | --- | --- |
| `NEXUS_AM_HOME` | `$AM_HOME`, else `~/nexus-am` | nexus-am checkout |
| `GEM5_HOME` | `$XIANG_GEM5`, else `~/GEM5` | XS-GEM5 checkout |
| `GEM5_BINARY` | `${GEM5_HOME}/build/RISCV/gem5.opt` | gem5 binary |
| `GEM5_CONFIG` | `${GEM5_HOME}/configs/example/kmhv3.py` | gem5 config script |
| `GEM5_EXTRA_ARGS` | `--disable-difftest` | Extra gem5 config arguments |
| `QEMU_BINARY` | `qemu-riscv64` on PATH | qemu-riscv64 user-mode emulator |
| `QEMU_CPU` | `max` | QEMU `-cpu` (default `max` for RVV + bitmanip/crypto) |
| `QEMU_EXTRA_ARGS` | (empty) | Extra qemu-riscv64 arguments |
| `RISCV_TOOLCHAIN_PREFIX` | `riscv64-linux-gnu-` | GNU toolchain triple prefix |
| `AM_ARCH` | `riscv64-xs` | nexus-am target architecture |
| `RISCV_MABI` | `lp64d` | RISC-V `-mabi` |
| `RISCV_MARCH` | `rv64gcv_zba_zbb_zbc_zbs_zbkb_zbkc_zbkx_zknd_zkne_zknh_zkr_zksed_zksh_zkt` | RISC-V `-march` |


## How it works

- **Cross-compile** — `cmake/riscv64_xs_toolchain.cmake` selects the GNU RISC-V
  compilers and shared flags (`-static -mcmodel=medany -fno-rtti
  -fno-exceptions ...`). Freestanding-only flags (`-ffreestanding`, `-fno-builtin`,
  etc.) are applied per-target via `RISCV_BAREMETAL_COMPILE_FLAGS` on bare-metal
  images and libraries only.
- **Two image kinds** — bare-metal images link nexus-am's `am`/`klib` and
  objcopy to `.bin` for gem5. qemu-user images are static Linux ELFs built
  without nexus-am; `riscv_port.hpp` routes I/O to glibc when `RISCV_BAREMETAL`
  is not defined.
- **Bare-metal runtime** — under `RISCV_BAREMETAL`, the library
  (`lib/include/riscv_port.hpp`, `lib/source/riscv_runtime.cpp`) provides
  `operator new`/`delete` over klib's `malloc`/`free`, a `__cxa_pure_virtual`
  trap, cycle counters (`csrr cycle`/`instret` in `lib/include/riscv_timer.hpp`),
  and a static-constructor runner. nexus-am's runtime predates C++: it never
  runs `.init_array` and only enables the FPU, so the runtime enables the vector
  unit (`MSTATUS_VS`) and runs static constructors explicitly. A self-contained
  linker script (`cmake/riscv_init_array.ld`, a copy of nexus-am's
  `loader64.ld`/`section.ld` plus a `KEEP()`ed `.init_array`) keeps those
  constructors from being garbage-collected.
- **Link** — `riscv_add_baremetal_image(<source.cpp>)` creates the executable
  and objcopies the ELF to a raw `.bin`. Callers link `${PROJECT_NAME}` and
  `reference`, then the `riscv_baremetal_runtime` INTERFACE library, which
  supplies `am-${AM_ARCH}.a` and `klib-${AM_ARCH}.a` inside `--start-group`
  (the two archives reference each other) plus bare-metal link options.
- **Unit tests** — real GoogleTest cannot link against klib's bare-metal
  runtime, so `test/framework/include/gtest_lite.hpp` provides a minimal
  self-registering subset (`TEST`, `TEST_P`, `ASSERT_EQ`, `Values`, `Combine`,
  `INSTANTIATE_TEST_SUITE_P`, `RUN_ALL_TESTS`) with gtest-style output that
  ctest can match on.
- **Run on gem5** — `riscv_add_gem5_run(<target>)` takes the bare-metal
  executable target (e.g. `unit_tests_gem5`) and creates a `run_<target>` custom
  target plus a ctest entry (unless `RISCV_GEM5_NO_CTEST` is set on the target).
  The `.bin` path comes from the `RISCV_GEM5_BIN` property set by
  `riscv_add_baremetal_image`. Pass/fail regexes come from target properties
  `RISCV_GEM5_PASS_REGEX` / `RISCV_GEM5_FAIL_REGEX`.
- **Run on qemu-user** — `riscv_add_qemu_run(<target>)` takes the Linux ELF
  target (e.g. `unit_tests_qemu`) and creates `run_<target>` plus a ctest entry
  that runs the ELF under `qemu-riscv64`; ctest uses the guest exit code.

## Image I/O

- **qemu-user** — `Image::Read("in.pgm")` and `Image::Write("out.pgm")` use hosted
  libc to read/write PGM (P2/P5) and PPM (P3/P6). Writes are always binary P5/P6.
- **gem5** — no file I/O on the bare-metal runtime. Images are compiled in as
  headers (`scripts/image_to_header.py`) and loaded with
  `Image::Read(width, height, data)`. `Write` is not supported on gem5.

See [IMAGEIO.md](IMAGEIO.md) for formats and examples.

## BoxFilter Task

The `BoxFilter_Task` branch implements a uniform 3x3 filter for 8-bit
grayscale images. Every non-border result is calculated with integer division:

```text
output[y][x] = floor((top[x-1] + top[x] + top[x+1]
                    + mid[x-1] + mid[x] + mid[x+1]
                    + bot[x-1] + bot[x] + bot[x+1]) / 9)
```

The first and last rows and columns are copied unchanged. Images smaller than
3x3 are also copied unchanged. Both implementations support input and output
referring to the same image.

### BoxFilter files

- Scalar correctness reference:
  [`test/reference/source/image_BoxFilter.cpp`](test/reference/source/image_BoxFilter.cpp)
- RVV implementation:
  [`lib/source/image_BoxFilter.cpp`](lib/source/image_BoxFilter.cpp)
- Unit tests:
  [`test/unit_test/source/BoxFilter_test.cpp`](test/unit_test/source/BoxFilter_test.cpp)
- One-iteration benchmark:
  [`benchmark/source/BoxFilter_benchmark.cpp`](benchmark/source/BoxFilter_benchmark.cpp)

### Scalar approach

The scalar version visits every interior output pixel, loads its complete 3x3
neighborhood, accumulates the nine bytes in a wider integer, divides by nine,
and stores the result. It is the correctness oracle for the RVV output.

### RVV approach

The image bytes are row-major in memory. The middle-row pointer is one image
width after the top-row pointer, and the bottom-row pointer is one width after
the middle row.

For each output row, the RVV loop processes a dynamic number of pixels:

1. Load `vl + 2` bytes from each of the top, middle, and bottom rows.
2. Add the three rows vertically into one 16-bit vector.
3. Slide that vector by one and two lanes and add the three horizontal
   positions into the output accumulator.
4. Divide every 3x3 sum by nine using an exact unsigned multiply-high with
   `7282`, then narrow to 8 bits.
5. Store the completed output vector once.

The vector sequence per chunk is:

```text
3 vector loads
2 widening vertical additions
2 vector slides
2 horizontal additions
1 unsigned multiply-high
1 narrowing operation
1 vector store
```

Partial sums remain in vector registers until the final store, so the normal
out-of-place path does not need a temporary image buffer. A snapshot is
allocated only when the caller explicitly requests in-place filtering.

The implementation uses dynamic vector-length chunking and is not tied to one
VLEN. Two look-ahead lanes are reserved for the horizontal window. With
VLEN=128 and the `e16m8` accumulator, for example, VLMAX is 64 lanes and one
full chunk produces 62 output pixels from 64 loaded pixels. A smaller `vl` is
used for the tail without reading beyond the row.

The largest 3x3 sum is `9 * 255 = 2295`. Over this complete range, the high
half of `sum * 7282`, where `7282 = ceil(2^16 / 9)`, is exactly
`floor(sum / 9)`. This preserves the scalar result without vector division.

### Run the BoxFilter tests

```bash
# Fast functional validation
cmake --build build --target run_BoxFilter_test_qemu

# Cycle-accurate validation
cmake --build build --target run_BoxFilter_test_gem5
```

The tests compare scalar and RVV output across different image sizes, vector
tails, maximum pixel values, borders, and in-place operation.

### Benchmark scalar versus RVV

The benchmark is configured for exactly one iteration. It runs the scalar
implementation once and the RVV implementation once, checks that their output
matches, and reports cycles and retired instructions for both.

```bash
# Fast functional benchmark
cmake --build build --target run_BoxFilter_benchmark_qemu

# Cycle-accurate performance benchmark
cmake --build build --target run_BoxFilter_benchmark_gem5
```

The gem5 run can take substantially longer than QEMU. Its console log and
statistics are written to:

```text
build/benchmark/BoxFilter_benchmark_gem5.log
build/benchmark/BoxFilter_benchmark_gem5-m5out/
```

The reported 60,000-cycle and 20,000-instruction values are non-gating stretch
targets. They do not stop the run or determine correctness.

### Run the BoxFilter visualization

The interactive, step-by-step website is in
[`boxfilter-visualizer/`](boxfilter-visualizer/). Start the server from the
repository root because the page loads the sample image from `images/`:

```bash
python3 -m http.server 8000
```

Open <http://localhost:8000/boxfilter-visualizer/#rvv>.

The page visualizes the memory layout, three row loads, vertical reduction,
slides, horizontal accumulation, divide-by-nine conversion, narrowing, and
final store. If port 8000 reports `Address already in use`, the existing server
is usually still running. Open the link above, or use another port:

```bash
python3 -m http.server 8001
```

Then open <http://localhost:8001/boxfilter-visualizer/#rvv>.
