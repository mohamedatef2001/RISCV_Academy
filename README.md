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

The `BoxFilter_Task` branch implements a uniform box filter for `CV_8UC1`
images with an OpenCV-shaped interface:

```cpp
vec::boxFilter(src, dst, ddepth, Size(width, height),
               Point(anchor_x, anchor_y), normalize, border_type);
```

`ddepth` accepts `-1`, `CV_8U`, or `CV_8UC1`. Kernel dimensions are runtime
parameters and must be positive and odd; tests cover 1x1 through 11x11 as well
as a rectangular 5x11 kernel. `Point(-1, -1)` selects the kernel center.
`BORDER_CONSTANT` uses zero padding, `BORDER_REPLICATE` repeats the nearest
edge pixel, and the OpenCV-compatible default is `BORDER_REFLECT_101`.
Normalized results are rounded to nearest; unnormalized results are saturated
to the 8-bit output range.

The caller preallocates input and output. The scalar and RVV kernels do not use
dynamic allocation, so input/output aliasing (in-place filtering) is not
supported.

### BoxFilter files

- Scalar correctness reference:
  [`test/reference/source/image_BoxFilter.cpp`](test/reference/source/image_BoxFilter.cpp)
- RVV implementation:
  [`lib/source/image_BoxFilter.cpp`](lib/source/image_BoxFilter.cpp)
- Unit tests:
  [`test/unit_test/source/BoxFilter_test.cpp`](test/unit_test/source/BoxFilter_test.cpp)
- One-iteration comparative benchmark:
  [`benchmark/source/BoxFilter_benchmark.cpp`](benchmark/source/BoxFilter_benchmark.cpp)

### Scalar approach

The scalar oracle retains a fixed-work 3x3 centered path so the established
gem5 comparison does not regress merely because the public API became generic.
Other kernel shapes directly evaluate the complete neighborhood for each pixel
with a 64-bit accumulator. Both paths implement the selected border
extrapolation independently from the RVV kernel.

### RVV approach

The RVV implementation retains a specialized 3x3 path using 16-bit widening,
vertical reduction, vector slides, and exact multiply-high division. Other
kernel sizes use 32-bit vector accumulators. When the kernel width fits the
available vector lanes, each source row is loaded once per chunk, reduced
vertically, and reused through `vslidedown`; larger valid kernels fall back to
a direct vector-tap loop. All paths use dynamic `vl` values and handle tails
without assuming a fixed VLEN. Border bands are computed separately so the
interior vector loop never performs out-of-bounds loads.

### How each benchmark kernel works

All benchmark cases apply a normalized box blur: every output pixel is the
rounded average of the pixels in its square neighborhood. A larger kernel
therefore produces a stronger blur, but requires more row reductions and
horizontal additions.

| Kernel | RVV processing |
| --- | --- |
| 3x3 | Uses the dedicated fast path. Three source rows are loaded per vector chunk and widened to 16 bits, then added vertically. Two `vslidedown` operations align the neighboring column sums, and two vector additions produce all overlapping 3x3 sums. Adding 4 followed by multiply-high with 7282 implements exact round-to-nearest division by 9. |
| 5x5 | Uses the generic 32-bit sliding path. Five loaded rows are reduced vertically, then four slides and additions combine five adjacent column sums before division by 25. |
| 7x7 | Uses the same generic path with seven vertically reduced rows and six horizontal slides/additions, followed by division by 49. |
| 9x9 | Uses nine row contributions and eight horizontal slides/additions, followed by division by 81. |
| 11x11 | Uses eleven row contributions and ten horizontal slides/additions, followed by division by 121. |

The 3x3 case is specially optimized because it is the common small blur and
its maximum sum fits safely in 16 bits. Most importantly, border handling is
not performed inside its hot RVV loop. The narrow outer border is calculated
separately using the selected border mode, while the vector loop processes only
interior pixels whose complete 3x3 neighborhood is already in bounds. Removing
per-lane border checks and coordinate correction from the hot loop makes a
large performance difference. The border is not omitted from the output; it is
simply separated from the optimized interior calculation.

### Run the BoxFilter tests

```bash
# Fast functional validation
cmake --build build --target run_BoxFilter_test_qemu

# Full QEMU suite
cmake --build build --target run_unit_tests_qemu
```

The tests compare scalar and RVV output across both border modes, odd kernel
sizes, explicit/default anchors, vector tails, rounding, and saturation.

### Benchmark scalar versus RVV

The benchmark runs every square odd kernel from 3x3 through 11x11 (3, 5, 7,
9, and 11) for one iteration.
Allocation and initialization remain outside timed regions. Each iteration
checks scalar/RVV equality and the report includes average cycles, retired
instructions, cycles per pixel, pixels per cycle, and speedup. The 3x3 gem5
report also prints deltas from the established 931,854-cycle/288,903-instruction
vector baseline.

### Measured gem5 results

The latest local gem5 run used the 640x960 image and one iteration per kernel.
It completed the 3x3, 5x5, and 7x7 cases before the run stopped at the start of
9x9. These are the results preserved in
`build/benchmark/BoxFilter_benchmark_gem5.log`:

| Kernel and border | Scalar cycles | Scalar cycles/pixel | Scalar instructions | RVV cycles | RVV cycles/pixel | RVV instructions | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 3x3 constant | 2,602,478 | 4.235804 | 16,576,463 | 962,671 | 1.566847 | 363,286 | 2.703x |
| 5x5 replicate | 46,742,251 | 76.077881 | 230,504,743 | 5,990,412 | 9.750019 | 4,057,176 | 7.803x |
| 7x7 replicate | 78,751,157 | 128.175720 | 410,044,903 | 9,021,497 | 14.683426 | 8,505,092 | 8.729x |
| 9x9 replicate | Not completed | -- | Not completed | Not completed | -- | Not completed | -- |
| 11x11 replicate | Not reached | -- | Not reached | Not reached | -- | Not reached | -- |

The 3x3 RVV result is 30,817 cycles and 74,383 instructions above the earlier
931,854-cycle and 288,903-instruction baseline. Results depend on the compiler,
simulator configuration, image, and vector length, so this table should be
updated whenever the benchmark is rerun under different conditions.

```bash
# Fast functional benchmark
cmake --build build --target run_BoxFilter_benchmark_qemu

# Run this yourself for cycle-accurate gem5 performance. Disable the fork's
# default 40-million-instruction stop and its 20-million-instruction stats reset
# so every kernel is reported in one uninterrupted run.
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DGEM5_EXTRA_ARGS="--disable-difftest --maxinsts=0 --warmup-insts-no-switch=0"
cmake --build build --target run_BoxFilter_benchmark_gem5
```

The gem5 run can take substantially longer than QEMU. Its console log and
statistics are written to:

```text
build/benchmark/BoxFilter_benchmark_gem5.log
build/benchmark/BoxFilter_benchmark_gem5-m5out/
```

QEMU is used here only for functional validation and confirming that the
benchmark completes. Its user-mode CSR values are not reliable performance
measurements; use gem5 for cycle and retired-instruction comparisons.

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
