# BoxFilter Task

This document describes the `BoxFilter_Task` branch, its scalar and RISC-V
Vector (RVV) implementations, the interactive visualization, and the commands
used to test and benchmark both versions.

## What the filter does

The filter applies a uniform 3x3 kernel to an 8-bit grayscale image. For each
non-border output pixel, it adds the nine neighboring bytes and uses integer
division by nine:

```text
output[y][x] = floor((top[x-1] + top[x] + top[x+1]
                    + mid[x-1] + mid[x] + mid[x+1]
                    + bot[x-1] + bot[x] + bot[x+1]) / 9)
```

The first and last rows and columns are copied unchanged. Images smaller than
3x3 are also copied unchanged. Both implementations support input and output
referring to the same image.

## Implementations

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
and stores the result. It is the oracle used to verify the RVV output.

### RVV approach

The image bytes are row-major in memory, so the middle-row pointer is one image
width after the top-row pointer, and the bottom-row pointer is one width after
the middle row.

For each output row, the RVV loop processes a dynamic number of output pixels:

1. Load `vl + 2` bytes from each of the top, middle, and bottom rows.
2. Add the three rows vertically into one 16-bit vector.
3. Slide that vector by one and two lanes and add the three horizontal
   positions into the output accumulator.
4. Divide every 3x3 sum by nine using an exact unsigned multiply-high with
   `7282`, then narrow to 8 bits.
5. Store the completed output vector once.

The hot loop therefore uses this vector sequence per chunk:

```text
3 vector loads
2 widening vertical additions
2 vector slides
2 horizontal additions
1 unsigned multiply-high
1 narrowing operation
1 vector store
```

No temporary image buffer is used in the normal out-of-place path: partial
sums remain in vector registers until the final store. A snapshot is allocated
only when the caller explicitly requests in-place filtering.

The implementation uses `vsetvl`-compatible dynamic chunking and is not tied to
one vector length. Two look-ahead lanes are reserved for the horizontal window.
For example, with VLEN=128 and the `e16m8` accumulator, VLMAX is 64 lanes, so a
full chunk produces 62 output pixels from 64 loaded pixels. Tail chunks use a
smaller `vl`, so they do not read beyond the row.

The largest possible 3x3 sum is `9 * 255 = 2295`. Over this complete range,
the high half of `sum * 7282`, where `7282 = ceil(2^16 / 9)`, is exactly
`floor(sum / 9)`. This preserves the scalar result without a vector division.

## Build the branch

Switch to the branch and configure the project if a build directory does not
already exist:

```bash
git switch BoxFilter_Task

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DNEXUS_AM_HOME=/path/to/nexus-am \
  -DGEM5_HOME=/path/to/GEM5

cmake --build build -j2
```

See [`SETUP.md`](SETUP.md) for the complete toolchain, nexus-am, QEMU, and gem5
setup.

## Run correctness tests

Fast functional test with QEMU:

```bash
cmake --build build --target run_BoxFilter_test_qemu
```

Cycle-accurate test with gem5:

```bash
cmake --build build --target run_BoxFilter_test_gem5
```

The tests compare scalar and RVV output across different image sizes, tails,
maximum pixel values, borders, and in-place operation.

## Benchmark scalar versus RVV

The benchmark is configured for exactly one iteration. Each invocation runs
the scalar implementation once, the RVV implementation once, checks that their
outputs match, and reports cycles and retired instructions for both.

QEMU functional benchmark:

```bash
cmake --build build --target run_BoxFilter_benchmark_qemu
```

gem5 performance benchmark:

```bash
cmake --build build --target run_BoxFilter_benchmark_gem5
```

The gem5 run is cycle-accurate and can take substantially longer than QEMU.
Its console log is written to:

```text
build/benchmark/BoxFilter_benchmark_gem5.log
```

gem5 statistics are written below:

```text
build/benchmark/BoxFilter_benchmark_gem5-m5out/
```

The reported 60,000-cycle and 20,000-instruction values are non-gating stretch
targets; they do not stop the run or determine correctness.

## Run the interactive website

The visualization is in [`boxfilter-visualizer/`](boxfilter-visualizer/). Start
the server from the repository root because the page loads the sample image
from the repository's `images/` directory:

```bash
python3 -m http.server 8000
```

Then open:

<http://localhost:8000/boxfilter-visualizer/#rvv>

The page presents the memory layout and moves step by step with the scalar and
RVV code, including the three row loads, vertical reduction, slides,
accumulation, divide-by-nine conversion, narrowing, and final store.

If port 8000 reports `Address already in use`, the existing website server is
usually still running, so open the link above. Alternatively, start another
server and use its matching port:

```bash
python3 -m http.server 8001
```

<http://localhost:8001/boxfilter-visualizer/#rvv>
