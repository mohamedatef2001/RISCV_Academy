#pragma once

#include "riscv_gem5_file.hpp"

#include <cstdint>

namespace sobel_example
{
    inline constexpr int Width = 512;
    inline constexpr int Height = 512;
    inline constexpr int PixelCount = Width * Height;
    inline constexpr char OutputFilename[] = "T2_Sobel_output.bmp";

    // 54-byte header for a 512 x 512, 8-bit indexed BMP.
    inline constexpr std::uint8_t BmpHeader[54] = {
        0x42, 0x4d, 0x36, 0x04, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x36, 0x04, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x12, 0x0b,
        0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00
    };

    struct BmpPalette
    {
        std::uint8_t bytes[256 * 4]{};
    };

    constexpr BmpPalette MakeBmpPalette()
    {
        BmpPalette palette{};
        for (int value = 0; value < 256; ++value)
        {
            palette.bytes[value * 4] = static_cast<std::uint8_t>(value);
            palette.bytes[value * 4 + 1] = static_cast<std::uint8_t>(value);
            palette.bytes[value * 4 + 2] = static_cast<std::uint8_t>(value);
        }
        return palette;
    }

    inline constexpr BmpPalette Palette = MakeBmpPalette();

#ifdef RISCV_BAREMETAL
    inline bool WriteBmpWithGem5(const std::uint8_t* pixels)
    {
        constexpr std::uint64_t palette_offset = sizeof(BmpHeader);
        constexpr std::uint64_t pixel_offset =
            sizeof(BmpHeader) + sizeof(Palette.bytes);

        return gem5::WriteFile(BmpHeader, sizeof(BmpHeader), 0, OutputFilename)
                   == sizeof(BmpHeader)
            && gem5::WriteFile(Palette.bytes, sizeof(Palette.bytes),
                               palette_offset, OutputFilename)
                   == sizeof(Palette.bytes)
            && gem5::WriteFile(pixels, PixelCount, pixel_offset, OutputFilename)
                   == PixelCount;
    }
#endif
}
