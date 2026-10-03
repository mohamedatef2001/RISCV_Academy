# Sobel worked example

The Sobel example demonstrates both responsibilities separately:

| Responsibility | Current file |
|---|---|
| Generic gem5 host-file operation | `lib/include/riscv_gem5_file.hpp` |
| RVV Sobel implementation | `lib/source/image_Sobel.cpp` |
| Scalar correctness reference | `test/reference/source/image_Sobel.cpp` |
| BMP header, palette, and offsets | `example/include/sobel_bmp.hpp` |
| Embedded 512 x 512 input | `example/include/sobel_input.hpp` |
| Runnable QEMU/gem5 example | `example/source/SobelWriteFile_example.cpp` |
| Correctness coverage | `test/unit_test/source/Sobel_test.cpp` |
| Scalar-versus-vector benchmark | `benchmark/source/Sobel_benchmark.cpp` |

The generated 8-bit grayscale BMP is assembled in three calls:

| Offset | Bytes | Content |
|---:|---:|---|
| 0 | 54 | BMP header; creates or truncates the file |
| 54 | 1024 | 256-entry grayscale palette |
| 1078 | 262144 | 512 x 512 Sobel output pixels |

The final size is 263222 bytes and the filename is
`T2_Sobel_output.bmp`. The image dimensions, BMP header, pixel count, and
write offsets must all be changed together if the example dimensions change.

The active example calls `vec::Sobel` once and exports the resulting buffer.
Timing is intentionally kept in `Sobel_benchmark.cpp`, so output-file work is
not mixed into the algorithm measurement.

Useful focused commands are:

```sh
cmake --build build --target run_Sobel_test_qemu
cmake --build build --target run_Sobel_benchmark_qemu
cmake --build build --target run_SobelWriteFile_example_qemu
cmake --build build --target run_SobelWriteFile_example_gem5
```

For cycle and instruction results on the XiangShan model, run:

```sh
cmake --build build --target run_Sobel_benchmark_gem5
```

The scalar and vector results, including speedup, are printed to the terminal
and saved in `build/benchmark/Sobel_benchmark_gem5.log`.

## Verified baseline

The reorganized branch was validated on 2026-10-03 with its configured
XiangShan gem5 model. For the 128 x 100 benchmark averaged over three runs:

| Implementation | Cycles | Instructions | Pixels/cycle |
|---|---:|---:|---:|
| Scalar reference | 69,945 | 425,872 | 0.183001 |
| Hand-written RVV | 113,861 | 51,810 | 0.112418 |

The outputs matched. The measured speedup was `0.614302x`: RVV retired far
fewer instructions but was slower in cycles on this simulator configuration.
This result is intentionally reported as measured rather than treated as a
minimum-performance requirement.

Both QEMU and gem5 regenerated a 263222-byte BMP matching the retained golden
file byte-for-byte. Its SHA-256 is:

```text
68962ac1e6aabf079dd4e6fa5812b1dfcf16ddb22c08dbcd8b3744f4623a4f3d
```
