# Writing host files from bare-metal XiangShan gem5

This directory is the authoritative guide for the file-output method used by
the `SobelLib` branch. The method writes an image, text file, raw buffer, or
other binary data directly from a bare-metal gem5 guest to the host. It does
not require QEMU or a guest operating system.

## Why normal file I/O does not work

The repository's QEMU targets are Linux programs, so `fopen` and `fwrite` are
forwarded to the host. The XiangShan gem5 targets instead load a raw
bare-metal `.bin` using `--raw-cpt`. There is no Linux kernel, filesystem, or
system-call layer in that guest.

The solution uses gem5's existing `m5_write_file` pseudo-operation. The guest
executes the RISC-V instruction `0x9e00007b` (operation `0x4f`), and gem5
copies bytes from simulated memory to a real file below its `-d` output
directory. No gem5 source modification was needed for the tested fork.

```text
bare-metal application
        |
        | a0=data, a1=count, a2=offset, a3=filename
        | instruction 0x9e00007b
        v
gem5 m5_write_file
        |
        | copies simulated memory
        v
host file under gem5's -d directory
```

## Reusable project implementation

The reusable helper is:

```text
lib/include/riscv_gem5_file.hpp
```

Its public operation is:

```cpp
gem5::WriteFile(data, byte_count, file_offset, filename)
```

The return value is the number of bytes written. Always compare it with the
requested count. A call at offset zero creates or truncates the file; later
calls can append structured sections by using their exact cumulative offsets.

The helper executes `fence rw, rw` before the pseudo-operation. This is
required because gem5 reads the simulated memory immediately, while recently
calculated guest stores may otherwise still be pending. The missing fence was
the reason a small number of Sobel pixels originally differed from QEMU.

Keep the buffer and zero-terminated filename alive until the operation
returns. A relative filename is resolved below gem5's `-d` directory.

## Confirm support in another gem5 checkout

Replace the path below with the checkout used by the team:

```sh
export GEM5_ROOT=/absolute/path/to/GEM5
grep -n M5OP_WRITE_FILE "$GEM5_ROOT/include/gem5/asm/generic/m5ops.h"
grep -n 'writefile(ThreadContext' "$GEM5_ROOT/src/sim/pseudo_inst.cc"
grep -n '0x0000007b' "$GEM5_ROOT/util/m5/src/abi/riscv/m5op.S"
```

The first command should find `M5OP_WRITE_FILE = 0x4f`. The inline instruction
in this repository is specific to RISC-V and to a gem5 build that implements
that operation.

## Minimal use in another algorithm

```cpp
#include "riscv_gem5_file.hpp"

static constexpr char OutputName[] = "result.bin";
static std::uint8_t Result[4];

// Calculate Result first.
const bool ok = gem5::WriteFile(Result, sizeof(Result), 0, OutputName)
                == sizeof(Result);
```

For multiple chunks, write the first at offset zero and use non-overlapping
offsets for every later call. The pseudo-operation only copies bytes; the
application remains responsible for its file format, metadata, row padding,
and total size.

## Build and run the working Sobel example

From the repository root, configure once. Replace the paths when the tools are
not in their default locations:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DNEXUS_AM_HOME=/absolute/path/to/nexus-am \
  -DGEM5_HOME=/absolute/path/to/GEM5 \
  -DQEMU_BINARY="$(command -v qemu-riscv64)"
```

Build and run the bare-metal example:

```sh
cmake --build build --target run_SobelWriteFile_example_gem5
```

The relevant outputs are:

```text
build/example/SobelWriteFile_example_gem5.log
build/example/SobelWriteFile_example_gem5-m5out/T2_Sobel_output.bmp
```

The expected BMP is 263222 bytes: a 54-byte header, a 1024-byte grayscale
palette, and 262144 pixels. The repository keeps one previously validated
copy at `images/T2_Sobel_output.bmp`.

Run the Linux/QEMU path and compare both byte-for-byte:

```sh
cmake --build build --target run_SobelWriteFile_example_qemu
cmp build/example/T2_Sobel_output.bmp \
    build/example/SobelWriteFile_example_gem5-m5out/T2_Sobel_output.bmp
cmp images/T2_Sobel_output.bmp \
    build/example/SobelWriteFile_example_gem5-m5out/T2_Sobel_output.bmp
```

## Direct gem5 invocation

The CMake run target is preferred because it uses the configured paths. For a
direct run after building `SobelWriteFile_example_gem5`:

```sh
export PROJECT_ROOT=/absolute/path/to/RISCV_Academy
export GEM5_ROOT=/absolute/path/to/GEM5
cd "$PROJECT_ROOT/build/example"

"$GEM5_ROOT/build/RISCV/gem5.opt" --remote-gdb-port=0 \
  -d "$PROJECT_ROOT/build/example/SobelWriteFile_example_direct-m5out" \
  "$GEM5_ROOT/configs/example/kmhv3.py" \
  --raw-cpt \
  --generic-rv-cpt="$PROJECT_ROOT/build/example/SobelWriteFile_example.bin" \
  --disable-difftest
```

The direct-run output will be:

```text
build/example/SobelWriteFile_example_direct-m5out/T2_Sobel_output.bmp
```

## Troubleshooting

- If no file appears in the shell's current directory, inspect the gem5 `-d`
  directory. That is the output root for relative guest filenames.
- If a file is incomplete, verify every returned byte count and every offset.
- If calculated bytes differ from QEMU, ensure the generic helper's memory
  fence is still present immediately before the pseudo-operation.
- If the instruction is unknown, verify that the selected gem5 fork provides
  `M5OP_WRITE_FILE` for RISC-V.
- A simulator command piped through `tee` may hide the simulator's exit code.
  Check the pass message, output size, and byte comparison instead.
