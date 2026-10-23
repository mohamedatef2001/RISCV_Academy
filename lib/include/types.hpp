#pragma once

#include "image.hpp"

#include <cstddef>
#include <cstdint>

enum class OverFlowPolicy
{
    SATURATE,
    WRAP,
};

inline constexpr int kMedianFineBinCount = 256;
inline constexpr int kMedianCoarseBinCount = 16;
inline constexpr int kMedianFineBinsPerCoarseBin =
    kMedianFineBinCount / kMedianCoarseBinCount;
inline constexpr int kMedianHistogramStride =
    kMedianFineBinCount + kMedianCoarseBinCount;
inline constexpr int kMedianMaximumKernelSize = 11;

struct MedianFilterWorkspace
{
    std::uint16_t* histogram;
    std::size_t capacity;
};

inline bool IsSupportedMedianKernel(const int kernel_size)
{
    return kernel_size == 3 || kernel_size == 5 || kernel_size == 7 ||
           kernel_size == 9 || kernel_size == 11;
}

// Capacity is measured in uint16_t elements. Only the sliding-histogram paths
// (9x9 and 11x11) need workspace: one fine+coarse histogram per image column,
// plus one histogram for the current horizontal window.
inline std::size_t MedianFilterWorkspaceCapacity(const int image_width,
                                                 const int kernel_size)
{
    if (image_width <= 0 || (kernel_size != 9 && kernel_size != 11))
    {
        return 0;
    }
    return static_cast<std::size_t>(image_width + 1) *
           kMedianHistogramStride;
}
