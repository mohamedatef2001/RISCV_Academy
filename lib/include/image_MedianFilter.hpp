#pragma once

#include "types.hpp"

#include <cstdint>

namespace vec
{
    // Exact median filtering for CV_8UC1-style images. The output has the same
    // dimensions as the input and samples outside the image replicate the
    // nearest edge pixel. In-place operation is not supported. The caller owns
    // workspace; use MedianFilterWorkspaceCapacity() to size it.
    void MedianFilter(const Image<std::uint8_t>& input,
                      Image<std::uint8_t>& output,
                      int kernel_size,
                      MedianFilterWorkspace workspace = {nullptr, 0});
}
