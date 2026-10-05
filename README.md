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

## RGB-to-gray RVV intrinsics

The RVV implementation is in
[`lib/source/image_RgbToGray.cpp`](lib/source/image_RgbToGray.cpp). Include
`<riscv_vector.h>` and compile with the vector extension enabled, as this
project does through `-march=rv64gcv...`.

The kernel represents the floating-point weights `0.299`, `0.587`, and `0.114`
as Q24 integers. It evaluates the following expression with integer vector
instructions, so neither floating-point operations nor integer division are
needed:

```text
gray = (5016388 * red + 9848226 * green + 1912603 * blue + 2^23) >> 24
```

Every data-processing intrinsic below takes `vl` as its final argument. Only
the first `vl` lanes are active, which lets the same loop handle a full vector
and the final partial vector safely.

| Intrinsic | RVV operation | How it is used |
| --- | --- | --- |
| `__riscv_vsetvlmax_e32m8()` | `vsetvli`, SEW=32, LMUL=8 | Returns the maximum number of 32-bit lanes available for one block. `e32` selects 32-bit elements and `m8` groups eight vector registers. |
| `__riscv_vlse8_v_u8m2(base, stride, vl)` | `vlse8.v` | Loads `vl` unsigned bytes separated by `stride` bytes. With `stride = 3`, calls at `rgb`, `rgb + 1`, and `rgb + 2` load the red, green, and blue channels from packed RGB pixels. |
| `__riscv_vzext_vf4_u32m8(src, vl)` | `vzext.vf4` | Zero-extends unsigned 8-bit lanes to unsigned 32-bit lanes. The `vf4` suffix means the destination elements are four times wider. |
| `__riscv_vmul_vx_u32m8(vector, scalar, vl)` | `vmul.vx` | Multiplies every 32-bit vector lane by one scalar Q24 color weight. `vx` means vector-scalar operands. |
| `__riscv_vadd_vv_u32m8(lhs, rhs, vl)` | `vadd.vv` | Adds two vectors lane by lane; used to combine the three weighted channels. `vv` means vector-vector operands. |
| `__riscv_vadd_vx_u32m8(vector, scalar, vl)` | `vadd.vx` | Adds the Q24 value `2^23` to every lane so the later shift rounds to the nearest integer. |
| `__riscv_vnsrl_wx_u16m4(wide, shift, vl)` | `vnsrl.wx` | Logically shifts each 32-bit lane right by 24 and narrows it to 16 bits. `wx` means a wide vector source and scalar shift amount. |
| `__riscv_vnsrl_wx_u8m2(wide, shift, vl)` | `vnsrl.wx` | Narrows the 16-bit result to 8 bits. This kernel uses a zero shift because the Q24 shift was already applied. |
| `__riscv_vse8_v_u8m2(base, vector, vl)` | `vse8.v` | Stores the first `vl` grayscale bytes contiguously to the output image. |

A typical strip-mined RVV loop follows this pattern:

```cpp
const std::size_t block_pixels = __riscv_vsetvlmax_e32m8();
for (std::size_t pixel = 0; pixel < pixel_count;)
{
    const std::size_t remaining = pixel_count - pixel;
    const std::size_t vl = remaining < block_pixels ? remaining : block_pixels;

    const std::uint8_t* input = rgb + 3 * pixel;
    const vuint8m2_t red = __riscv_vlse8_v_u8m2(input, 3, vl);
    const vuint32m8_t red_q24 = __riscv_vmul_vx_u32m8(
        __riscv_vzext_vf4_u32m8(red, vl), 5016388, vl);

    // Process green and blue, add the three vectors and rounding bias,
    // narrow to uint8_t, then store vl output pixels.
    pixel += vl;
}
```

Use the
[RVV Intrinsics Viewer](https://dzaima.github.io/intrinsics-viewer/#0q1YqVbJSKsosTtYtU9JRSoVzFMsSU1LiyyriMy1yjYAyiUpW0UplSrE6SskglmdeSWp6ahFQwi0nP7EEROfnpCjF1gIA)
to look up intrinsic signatures and their corresponding assembly instructions.
When using GCC 13, also consult its
[RVV intrinsic documentation](https://gcc.gnu.org/onlinedocs/gcc-13.1.0/gcc/RISC-V-Vector-Intrinsics.html):
that compiler implements intrinsic specification version 0.11 and does not
provide every newer tuple or explicit rounding-mode intrinsic form.

## Image I/O

- **qemu-user** — `Image::Read("in.pgm")` and `Image::Write("out.pgm")` use hosted
  libc to read/write PGM (P2/P5) and PPM (P3/P6). Writes are always binary P5/P6.
- **gem5** — no file I/O on the bare-metal runtime. Images are compiled in as
  headers (`scripts/image_to_header.py`) and loaded with
  `Image::Read(width, height, data)`. `Write` is not supported on gem5.

See [IMAGEIO.md](IMAGEIO.md) for formats and examples.
