#pragma once

#include "image.hpp"

#include <cstdint>

namespace vec
{
    // Fixed 3x3 grayscale box filter. The one-pixel border is copied unchanged.
    // input and output may refer to the same Image.
    void BoxFilter(const Image<std::uint8_t>& input,
                   Image<std::uint8_t>& output);
}
