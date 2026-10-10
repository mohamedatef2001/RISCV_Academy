#pragma once

#include "image.hpp"

#include <cstdint>

namespace ref
{
    // Scalar correctness oracle for the fixed 3x3 grayscale box filter.
    void BoxFilter(const Image<std::uint8_t>& input,
                   Image<std::uint8_t>& output);
}
