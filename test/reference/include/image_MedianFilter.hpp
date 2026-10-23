#pragma once

#include "types.hpp"

#include <cstdint>

namespace ref
{
    // Readable scalar correctness oracle matching vec::MedianFilter exactly.
    // The workspace argument keeps both APIs identical; the scalar path does
    // not use it.
    void MedianFilter(const Image<std::uint8_t>& input,
                      Image<std::uint8_t>& output,
                      int kernel_size,
                      MedianFilterWorkspace workspace = {nullptr, 0});
}
