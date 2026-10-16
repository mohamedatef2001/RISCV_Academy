#pragma once

#include "image.hpp"

#include <cstdint>

namespace vec
{
    // OpenCV-shaped boxFilter interface for CV_8UC1 images. ddepth accepts
    // -1 (same depth) or CV_8U/CV_8UC1. Kernel dimensions must be positive and
    // odd. Point(-1, -1) selects the kernel center. Supported border types are
    // BORDER_CONSTANT (zero padding), BORDER_REPLICATE, and the OpenCV default
    // BORDER_REFLECT_101.
    //
    // The caller owns and preallocates both images. In-place operation is not
    // supported because the kernel performs no dynamic allocation.
    void boxFilter(const Image<std::uint8_t>& input,
                   Image<std::uint8_t>& output,
                   int ddepth,
                   Size ksize,
                   Point anchor = Point(-1, -1),
                   bool normalize = true,
                   int border_type = BORDER_DEFAULT);

    // Preserve this repository's historical PascalCase spelling.
    inline void BoxFilter(const Image<std::uint8_t>& input,
                          Image<std::uint8_t>& output,
                          const int ddepth,
                          const Size ksize,
                          const Point anchor = Point(-1, -1),
                          const bool normalize = true,
                          const int border_type = BORDER_DEFAULT)
    {
        boxFilter(input, output, ddepth, ksize, anchor, normalize, border_type);
    }
}
