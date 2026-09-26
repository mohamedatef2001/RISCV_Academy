#pragma once

#include <cstdint>

using u8 = std::uint8_t;
using u32 = std::uint32_t;

constexpr int IMG_W = 512;
constexpr int IMG_H = 512;
constexpr int IMG_W_SHIFT = 9;  // 512 = 1 << 9
constexpr int IMG_SIZE = 1 << 18; // 512 * 512

inline void sobel_image_mag2_thresh(const u32* img,
                                    u8* outImg,
                                    u32 threshold2)
{
    for (int y = 1; y < IMG_H - 1; ++y)
    {
        const int row0 = (y - 1) << IMG_W_SHIFT;
        const int row1 = y << IMG_W_SHIFT;
        const int row2 = (y + 1) << IMG_W_SHIFT;

        for (int x = 1; x < IMG_W - 1; ++x)
        {
            const int p00 = img[row0 + x - 1];
            const int p01 = img[row0 + x];
            const int p02 = img[row0 + x + 1];

            const int p10 = img[row1 + x - 1];
            const int p12 = img[row1 + x + 1];

            const int p20 = img[row2 + x - 1];
            const int p21 = img[row2 + x];
            const int p22 = img[row2 + x + 1];

            const int gx = -p00 + p02 - (p10 << 1) + (p12 << 1)
                           - p20 + p22;

            const int gy = p00 + (p01 << 1) + p02 - p20
                           - (p21 << 1) - p22;

            const u32 mag2 = static_cast<u32>(gx * gx + gy * gy);

            outImg[row1 + x] = (mag2 >= threshold2) ? 255 : 0;
        }
    }
}
