# Setup: XS-GEM5 + nexus-am + RISCV-CV

From an empty machine to a benchmark running on the simulator. Validated on
Ubuntu 26.04 (GCC 15.2, binutils 2.46, CMake 4.2.3).

The three repos live side by side. This document assumes `~/workspace`:

```
~/workspace/
├── GEM5/       # XiangShan gem5 fork (the simulator)
├── nexus-am/   # bare-metal runtime (am + klib)
└── RISCV-CV/   # this project
```

## 1. Host packages

```bash
sudo apt install build-essential scons python3-dev pkg-config lld cmake \
                 zlib1g-dev libzstd-dev libprotobuf-dev protobuf-compiler \
                 libgoogle-perftools-dev \
                 g++-riscv64-linux-gnu binutils-riscv64-linux-gnu \
                 qemu-user
```

`qemu-user` provides `qemu-riscv64` for fast functional runs. If the dynamic
binary is missing on your distro, install `qemu-user-static` instead. Check
with `qemu-riscv64 --version`.

## 2. Clone

```bash
mkdir -p ~/workspace && cd ~/workspace
git clone https://github.com/OpenXiangShan/GEM5.git
git clone https://github.com/OpenXiangShan/nexus-am.git
git clone https://github.com/AhmedFathySV/RISCV-CV.git
```

## 3. Build DRAMsim3 inside gem5

```bash
cd ~/workspace/GEM5/ext/dramsim3
git clone https://github.com/umd-memsys/DRAMsim3.git DRAMsim3
cd DRAMsim3 && make libdramsim3.so -j$(nproc)
```

Not optional: gem5 enables DRAMsim3 as soon as the directory exists, so an
unbuilt clone fails the gem5 link with `cannot find -ldramsim3`. Use the bundled
`Makefile` rather than cmake — it writes the `.so` where gem5's `RPATH` expects
it. One-time step, about 11 seconds.

## 4. Build gem5

```bash
cd ~/workspace/GEM5
CCFLAGS_EXTRA="-Wno-error=array-bounds" scons build/RISCV/gem5.opt --gold-linker -j$(nproc)
```

Produces `build/RISCV/gem5.opt` (~1 GB). Check it with `./build/RISCV/gem5.opt -h`
(there is no `--version`).

- `CCFLAGS_EXTRA` works around a GCC 15 false-positive `-Warray-bounds` that
  `-Werror` turns fatal. It must be an environment variable, not a SCons
  argument, and it is needed on **every** build.
- If the gold linker probe fails (gold was dropped in binutils 2.45+), swap
  `--gold-linker` for `--linker=lld`.
- The closing warnings about backtrace, libpng, and HDF5 are optional features
  being compiled out. They don't affect CPU simulation.

## 5. Build nexus-am

```bash
cd ~/workspace/nexus-am
export AM_HOME=$PWD
make ARCH=riscv64-xs LINUX_GNU_TOOLCHAIN=1
```

## 6. Build RISCV-CV

```bash
cd ~/workspace/RISCV-CV
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DNEXUS_AM_HOME=$HOME/workspace/nexus-am \
  -DGEM5_HOME=$HOME/workspace/GEM5
cmake --build build -j$1
```

Configure fails fast if nexus-am or `gem5.opt` is missing, which is why steps
3–5 come first. See `README.md` for all cache variables.

## 7. Run

```bash
cmake --build build --target run_Add_benchmark_qemu   # fast functional
cmake --build build --target run_Add_benchmark_gem5   # cycle-accurate gem5
ctest --test-dir build --output-on-failure
```

Logs land in `build/<dir>/<name>.log`, gem5 stats in `build/<dir>/<name>-m5out/`.
Expected benchmark result (128x100, 3 iterations, KMHV3 model):

```
CLAMP:  reference 196516 cycles / vectorized 8048 cycles -> 24.41x
WRAP:   reference 206603 cycles / vectorized 6512 cycles -> 31.72x
Output is correct.
```

gem5 prints assorted `warn:` lines during the run (uartlite, MicroTAGE, issue
queue) — those are normal.
