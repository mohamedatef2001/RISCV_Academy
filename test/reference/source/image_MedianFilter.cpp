#include "image_MedianFilter.hpp"

#include <cassert>
#include <cstdint>

namespace
{
int ClampCoordinate(const int coordinate, const int length)
{
    if (coordinate < 0)
    {
        return 0;
    }
    if (coordinate >= length)
    {
        return length - 1;
    }
    return coordinate;
}

std::uint8_t ScalarMedianAt(const Image<std::uint8_t>& input,
                            const int output_x,
                            const int output_y,
                            const int kernel_size)
{
    constexpr int kMaximumSampleCount =
        kMedianMaximumKernelSize * kMedianMaximumKernelSize;
    std::uint8_t samples[kMaximumSampleCount];
    const int radius = kernel_size / 2;
    int sample_count = 0;

    for (int kernel_y = -radius; kernel_y <= radius; ++kernel_y)
    {
        const int input_y =
            ClampCoordinate(output_y + kernel_y, input.Height());
        for (int kernel_x = -radius; kernel_x <= radius; ++kernel_x)
        {
            const int input_x =
                ClampCoordinate(output_x + kernel_x, input.Width());
            samples[sample_count++] = input.GetPixel(input_x, input_y);
        }
    }

    // Deliberately simple insertion sort: this is the readable ground truth,
    // not a competing optimized implementation.
    for (int current = 1; current < sample_count; ++current)
    {
        const std::uint8_t value = samples[current];
        int insertion = current;
        while (insertion > 0 && samples[insertion - 1] > value)
        {
            samples[insertion] = samples[insertion - 1];
            --insertion;
        }
        samples[insertion] = value;
    }

    return samples[sample_count / 2];
}
} // namespace

void ref::MedianFilter(const Image<std::uint8_t>& input,
                       Image<std::uint8_t>& output,
                       const int kernel_size,
                       const MedianFilterWorkspace workspace)
{
    (void)workspace;
    assert(IsSupportedMedianKernel(kernel_size));
    assert(input.Width() == output.Width());
    assert(input.Height() == output.Height());
    assert(input.GetPtr(0, 0) != output.GetPtr(0, 0));

    for (int y = 0; y < input.Height(); ++y)
    {
        for (int x = 0; x < input.Width(); ++x)
        {
            output.SetPixel(x, y, ScalarMedianAt(input, x, y, kernel_size));
        }
    }
}
