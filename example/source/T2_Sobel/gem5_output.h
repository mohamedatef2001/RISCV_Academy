#pragma once

#include "Sobel.h"
#include "header.h"

#include <cstdint>

struct BmpPalette
{
    u8 bytes[256 * 4]{};
};

constexpr BmpPalette make_bmp_palette()
{
    BmpPalette palette{};
    for (int value = 0; value < 256; ++value)
    {
        palette.bytes[value * 4] = static_cast<u8>(value);
        palette.bytes[value * 4 + 1] = static_cast<u8>(value);
        palette.bytes[value * 4 + 2] = static_cast<u8>(value);
    }
    return palette;
}

// These bytes are loaded with the bare-metal image, so gem5 can copy them
// without waiting for guest stores to reach physical memory.
inline constexpr BmpPalette bmpPalette = make_bmp_palette();
inline constexpr char gem5BmpName[] = "T2_Sobel_output.bmp";

// gem5 RISC-V m5_write_file pseudo instruction (function 0x4f).
// gem5 writes into its -d output directory, without a guest filesystem.
inline std::uint64_t gem5_write_file(const void* data, std::uint64_t size,
                                    std::uint64_t offset, const char* name)
{
    register std::uintptr_t a0 asm("a0") = reinterpret_cast<std::uintptr_t>(data);
    register std::uintptr_t a1 asm("a1") = size;
    register std::uintptr_t a2 asm("a2") = offset;
    register std::uintptr_t a3 asm("a3") = reinterpret_cast<std::uintptr_t>(name);
    asm volatile(".4byte 0x9e00007b" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3) : "memory");
    return a0;
}

inline bool write_bmp_prefix_gem5()
{
    constexpr std::uint64_t palette_size = sizeof(bmpPalette.bytes);
    return gem5_write_file(bmpHeader, sizeof(bmpHeader), 0, gem5BmpName) == sizeof(bmpHeader)
        && gem5_write_file(bmpPalette.bytes, palette_size, sizeof(bmpHeader), gem5BmpName)
           == palette_size;
}

inline bool write_bmp_pixels_gem5(const u8* pixels)
{
    // m5_write_file reads guest memory while the CPU may still have pending
    // Sobel stores. Drain those stores before the simulator copies the pixels.
    asm volatile("fence rw, rw" ::: "memory");
    constexpr std::uint64_t pixel_offset = sizeof(bmpHeader) + sizeof(bmpPalette.bytes);
    return gem5_write_file(pixels, IMG_SIZE, pixel_offset, gem5BmpName) == IMG_SIZE;
}
