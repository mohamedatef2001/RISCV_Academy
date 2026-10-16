#pragma once

#include "image.hpp"

#include <cstdint>

namespace ref
{
    // Scalar correctness oracle matching vec::boxFilter's OpenCV-shaped API.
    void boxFilter(const Image<std::uint8_t>& input,
                   Image<std::uint8_t>& output,
                   int ddepth,
                   Size ksize,
                   Point anchor = Point(-1, -1),
                   bool normalize = true,
                   int border_type = BORDER_DEFAULT);

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
