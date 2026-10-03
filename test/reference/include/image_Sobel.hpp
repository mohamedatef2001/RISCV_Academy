#pragma once

#include "types.hpp"

#include <cstdint>

namespace ref
{
    // Scalar correctness oracle for vec::Sobel. The input contract and border
    // behavior intentionally match the vector API.
    void Sobel(const Image<std::uint32_t>& input,
               Image<std::uint8_t>& output,
               std::uint32_t threshold_squared);

    void Sobel(const std::uint32_t* input,
               std::uint8_t* output,
               int width,
               int height,
               std::uint32_t threshold_squared);
}
