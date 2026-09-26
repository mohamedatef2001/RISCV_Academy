# T2 Sobel

`T2_Sobel.cpp` runs the algorithm in `Sobel.h` on the embedded `image.h`
pixels. `header.h` contains the BMP header. QEMU writes the BMP through
normal file I/O; bare-metal gem5 uses the `m5_write_file` operation in
`gem5_output.h`.

Configure once from the repository root:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake
```

Run QEMU:

```sh
cmake --build build --target run_T2_Sobel_qemu
```

Output: `build/example/T2_Sobel/T2_Sobel_output.bmp`.

Run the existing bare-metal gem5 configuration:

```sh
cmake --build build --target run_T2_Sobel_gem5
```

gem5 writes the image to
`build/example/T2_Sobel/T2_Sobel_gem5-m5out/T2_Sobel_output.bmp`.
The filename is relative to gem5's output directory; no guest filesystem is
needed. The gem5 run computes Sobel once so writing the image does not affect
the algorithm timing.
