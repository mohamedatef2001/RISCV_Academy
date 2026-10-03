#pragma once

#include "types.hpp"

#include <cstdint>

namespace vec
{
    // The input stores one 8-bit grayscale intensity (0..255) in each u32.
    // Output pixels are 255 when squared Sobel magnitude >= threshold_squared.
    // The one-pixel border is always set to zero.
    void Sobel(const Image<std::uint32_t>& input,
               Image<std::uint8_t>& output,
               std::uint32_t threshold_squared);

    // Raw-buffer overload for embedded assets and file-format examples.
    void Sobel(const std::uint32_t* input,
               std::uint8_t* output,
               int width,
               int height,
               std::uint32_t threshold_squared);
}
