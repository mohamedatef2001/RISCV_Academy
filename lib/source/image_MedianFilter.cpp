#include "image_MedianFilter.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <riscv_vector.h>

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

std::uint8_t ScalarMedianAt(const std::uint8_t* input,
                            const int width,
                            const int height,
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
        const int input_y = ClampCoordinate(output_y + kernel_y, height);
        const std::uint8_t* row =
            input + static_cast<std::size_t>(input_y) * width;
        for (int kernel_x = -radius; kernel_x <= radius; ++kernel_x)
        {
            const int input_x = ClampCoordinate(output_x + kernel_x, width);
            samples[sample_count++] = row[input_x];
        }
    }

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

void FillScalarBorder(const std::uint8_t* input,
                      std::uint8_t* output,
                      const int width,
                      const int height,
                      const int y,
                      const int radius,
                      const int kernel_size)
{
    const int left_count = radius < width ? radius : width;
    for (int x = 0; x < left_count; ++x)
    {
        output[static_cast<std::size_t>(y) * width + x] =
            ScalarMedianAt(input, width, height, x, y, kernel_size);
    }

    const int right_begin = width - radius > left_count
        ? width - radius
        : left_count;
    for (int x = right_begin; x < width; ++x)
    {
        output[static_cast<std::size_t>(y) * width + x] =
            ScalarMedianAt(input, width, height, x, y, kernel_size);
    }
}

void CompareSwap(vuint8m1_t& lower, vuint8m1_t& upper,
                 const std::size_t vl)
{
    const vuint8m1_t original_lower = lower;
    lower = __riscv_vminu_vv_u8m1(lower, upper, vl);
    upper = __riscv_vmaxu_vv_u8m1(original_lower, upper, vl);
}

void Median3x3(const std::uint8_t* input,
               std::uint8_t* output,
               const int width,
               const int height)
{
    constexpr int kKernelSize = 3;
    constexpr int kRadius = kKernelSize / 2;
    if (width <= 2 * kRadius)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                output[static_cast<std::size_t>(y) * width + x] =
                    ScalarMedianAt(input, width, height, x, y, kKernelSize);
            }
        }
        return;
    }

    for (int y = 0; y < height; ++y)
    {
        FillScalarBorder(input, output, width, height, y, kRadius, kKernelSize);
        const int top_y = ClampCoordinate(y - 1, height);
        const int bottom_y = ClampCoordinate(y + 1, height);
        const std::uint8_t* top =
            input + static_cast<std::size_t>(top_y) * width;
        const std::uint8_t* middle =
            input + static_cast<std::size_t>(y) * width;
        const std::uint8_t* bottom =
            input + static_cast<std::size_t>(bottom_y) * width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;

        int x = kRadius;
        const int interior_end = width - kRadius;
        while (x < interior_end)
        {
            const std::size_t vl = __riscv_vsetvl_e8m1(
                static_cast<std::size_t>(interior_end - x));
            vuint8m1_t p0 = __riscv_vle8_v_u8m1(top + x - 1, vl);
            vuint8m1_t p1 = __riscv_vle8_v_u8m1(top + x, vl);
            vuint8m1_t p2 = __riscv_vle8_v_u8m1(top + x + 1, vl);
            vuint8m1_t p3 = __riscv_vle8_v_u8m1(middle + x - 1, vl);
            vuint8m1_t p4 = __riscv_vle8_v_u8m1(middle + x, vl);
            vuint8m1_t p5 = __riscv_vle8_v_u8m1(middle + x + 1, vl);
            vuint8m1_t p6 = __riscv_vle8_v_u8m1(bottom + x - 1, vl);
            vuint8m1_t p7 = __riscv_vle8_v_u8m1(bottom + x, vl);
            vuint8m1_t p8 = __riscv_vle8_v_u8m1(bottom + x + 1, vl);

            // Median-selection form of Dobbelaere's 9-input, 25-comparator
            // network. Backward pruning leaves the 20 comparisons that can
            // influence sorted wire 4.
            CompareSwap(p0, p3, vl); CompareSwap(p1, p7, vl);
            CompareSwap(p2, p5, vl); CompareSwap(p4, p8, vl);
            CompareSwap(p0, p7, vl); CompareSwap(p2, p4, vl);
            CompareSwap(p3, p8, vl); CompareSwap(p5, p6, vl);
            CompareSwap(p0, p2, vl); CompareSwap(p1, p3, vl);
            CompareSwap(p4, p5, vl); CompareSwap(p7, p8, vl);
            CompareSwap(p1, p4, vl); CompareSwap(p3, p6, vl);
            CompareSwap(p5, p7, vl); CompareSwap(p2, p4, vl);
            CompareSwap(p3, p5, vl); CompareSwap(p2, p3, vl);
            CompareSwap(p4, p5, vl); CompareSwap(p3, p4, vl);

            __riscv_vse8_v_u8m1(destination + x, p4, vl);
            x += static_cast<int>(vl);
        }
    }
}

template <int KernelSize>
void MedianByRadixSelection(const std::uint8_t* input,
                            std::uint8_t* output,
                            const int width,
                            const int height)
{
    constexpr int kRadius = KernelSize / 2;
    constexpr int kMedianRank = KernelSize * KernelSize / 2;
    constexpr std::uint8_t kMostSignificantBit = 0x80U;

    if (width <= 2 * kRadius)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                output[static_cast<std::size_t>(y) * width + x] =
                    ScalarMedianAt(input, width, height, x, y, KernelSize);
            }
        }
        return;
    }

    for (int y = 0; y < height; ++y)
    {
        FillScalarBorder(input, output, width, height, y, kRadius, KernelSize);
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;
        int x = kRadius;
        const int interior_end = width - kRadius;

        while (x < interior_end)
        {
            const std::size_t vl = __riscv_vsetvl_e8m1(
                static_cast<std::size_t>(interior_end - x));
            vuint8m1_t median = __riscv_vmv_v_x_u8m1(0, vl);

            for (std::uint8_t bit = kMostSignificantBit; bit != 0; bit >>= 1)
            {
                const vuint8m1_t trial =
                    __riscv_vor_vx_u8m1(median, bit, vl);
                vuint8m1_t below_count = __riscv_vmv_v_x_u8m1(0, vl);

                for (int kernel_y = -kRadius; kernel_y <= kRadius; ++kernel_y)
                {
                    const int input_y =
                        ClampCoordinate(y + kernel_y, height);
                    const std::uint8_t* row = input +
                        static_cast<std::size_t>(input_y) * width;
                    for (int kernel_x = -kRadius; kernel_x <= kRadius;
                         ++kernel_x)
                    {
                        const vuint8m1_t sample = __riscv_vle8_v_u8m1(
                            row + x + kernel_x, vl);
                        const vbool8_t below =
                            __riscv_vmsltu_vv_u8m1_b8(sample, trial, vl);
                        const vuint8m1_t increment =
                            __riscv_vmerge_vxm_u8m1(
                                __riscv_vmv_v_x_u8m1(0, vl), 1, below, vl);
                        below_count = __riscv_vadd_vv_u8m1(
                            below_count, increment, vl);
                    }
                }

                const vbool8_t keep_bit = __riscv_vmsleu_vx_u8m1_b8(
                    below_count, kMedianRank, vl);
                median = __riscv_vmerge_vvm_u8m1(
                    median, trial, keep_bit, vl);
            }

            __riscv_vse8_v_u8m1(destination + x, median, vl);
            x += static_cast<int>(vl);
        }
    }
}

void ClearHistogram(std::uint16_t* histogram, const std::size_t count)
{
    std::size_t offset = 0;
    while (offset < count)
    {
        const std::size_t vl = __riscv_vsetvl_e16m8(count - offset);
        __riscv_vse16_v_u16m8(
            histogram + offset, __riscv_vmv_v_x_u16m8(0, vl), vl);
        offset += vl;
    }
}

void AddHistogram(std::uint16_t* destination,
                  const std::uint16_t* source)
{
    int offset = 0;
    while (offset < kMedianHistogramStride)
    {
        const std::size_t vl = __riscv_vsetvl_e16m8(
            static_cast<std::size_t>(kMedianHistogramStride - offset));
        const vuint16m8_t dst =
            __riscv_vle16_v_u16m8(destination + offset, vl);
        const vuint16m8_t src =
            __riscv_vle16_v_u16m8(source + offset, vl);
        __riscv_vse16_v_u16m8(
            destination + offset, __riscv_vadd_vv_u16m8(dst, src, vl), vl);
        offset += static_cast<int>(vl);
    }
}

void SlideHistogram(std::uint16_t* window,
                    const std::uint16_t* leaving,
                    const std::uint16_t* entering)
{
    int offset = 0;
    while (offset < kMedianHistogramStride)
    {
        const std::size_t vl = __riscv_vsetvl_e16m8(
            static_cast<std::size_t>(kMedianHistogramStride - offset));
        vuint16m8_t values = __riscv_vle16_v_u16m8(window + offset, vl);
        values = __riscv_vsub_vv_u16m8(
            values, __riscv_vle16_v_u16m8(leaving + offset, vl), vl);
        values = __riscv_vadd_vv_u16m8(
            values, __riscv_vle16_v_u16m8(entering + offset, vl), vl);
        __riscv_vse16_v_u16m8(window + offset, values, vl);
        offset += static_cast<int>(vl);
    }
}

std::uint8_t MedianFromHistogram(const std::uint16_t* histogram,
                                 const int median_rank)
{
    const std::uint16_t* coarse = histogram + kMedianFineBinCount;
    int cumulative = 0;
    int coarse_index = 0;
    for (; coarse_index < kMedianCoarseBinCount; ++coarse_index)
    {
        if (cumulative + coarse[coarse_index] > median_rank)
        {
            break;
        }
        cumulative += coarse[coarse_index];
    }

    const int fine_begin = coarse_index * kMedianFineBinsPerCoarseBin;
    const int fine_end = fine_begin + kMedianFineBinsPerCoarseBin;
    for (int value = fine_begin; value < fine_end; ++value)
    {
        cumulative += histogram[value];
        if (cumulative > median_rank)
        {
            return static_cast<std::uint8_t>(value);
        }
    }
    return 255U;
}

template <int KernelSize>
void MedianBySlidingHistogram(const std::uint8_t* input,
                              std::uint8_t* output,
                              const int width,
                              const int height,
                              MedianFilterWorkspace workspace)
{
    constexpr int kRadius = KernelSize / 2;
    constexpr int kMedianRank = KernelSize * KernelSize / 2;
    const std::size_t required = MedianFilterWorkspaceCapacity(width, KernelSize);
    assert(workspace.histogram != nullptr);
    assert(workspace.capacity >= required);

    std::uint16_t* column_histograms = workspace.histogram;
    std::uint16_t* window_histogram = column_histograms +
        static_cast<std::size_t>(width) * kMedianHistogramStride;
    ClearHistogram(workspace.histogram, required);

    // Initialize the vertically replicated window centered on output row zero.
    for (int x = 0; x < width; ++x)
    {
        std::uint16_t* column = column_histograms +
            static_cast<std::size_t>(x) * kMedianHistogramStride;
        for (int kernel_y = -kRadius; kernel_y <= kRadius; ++kernel_y)
        {
            const int input_y = ClampCoordinate(kernel_y, height);
            const std::uint8_t value =
                input[static_cast<std::size_t>(input_y) * width + x];
            ++column[value];
            ++column[kMedianFineBinCount +
                     value / kMedianFineBinsPerCoarseBin];
        }
    }

    for (int y = 0; y < height; ++y)
    {
        if (y > 0)
        {
            const int leaving_y = ClampCoordinate(y - kRadius - 1, height);
            const int entering_y = ClampCoordinate(y + kRadius, height);
            const std::uint8_t* leaving_row = input +
                static_cast<std::size_t>(leaving_y) * width;
            const std::uint8_t* entering_row = input +
                static_cast<std::size_t>(entering_y) * width;
            for (int x = 0; x < width; ++x)
            {
                std::uint16_t* column = column_histograms +
                    static_cast<std::size_t>(x) * kMedianHistogramStride;
                const std::uint8_t leaving = leaving_row[x];
                const std::uint8_t entering = entering_row[x];
                --column[leaving];
                --column[kMedianFineBinCount +
                         leaving / kMedianFineBinsPerCoarseBin];
                ++column[entering];
                ++column[kMedianFineBinCount +
                         entering / kMedianFineBinsPerCoarseBin];
            }
        }

        ClearHistogram(window_histogram, kMedianHistogramStride);
        for (int kernel_x = -kRadius; kernel_x <= kRadius; ++kernel_x)
        {
            const int input_x = ClampCoordinate(kernel_x, width);
            AddHistogram(window_histogram, column_histograms +
                static_cast<std::size_t>(input_x) * kMedianHistogramStride);
        }

        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x)
        {
            destination[x] = MedianFromHistogram(window_histogram, kMedianRank);
            if (x + 1 < width)
            {
                const int leaving_x = ClampCoordinate(x - kRadius, width);
                const int entering_x = ClampCoordinate(x + kRadius + 1, width);
                SlideHistogram(
                    window_histogram,
                    column_histograms + static_cast<std::size_t>(leaving_x) *
                        kMedianHistogramStride,
                    column_histograms + static_cast<std::size_t>(entering_x) *
                        kMedianHistogramStride);
            }
        }
    }
}
} // namespace

void vec::MedianFilter(const Image<std::uint8_t>& input,
                       Image<std::uint8_t>& output,
                       const int kernel_size,
                       const MedianFilterWorkspace workspace)
{
    assert(IsSupportedMedianKernel(kernel_size));
    assert(input.Width() == output.Width());
    assert(input.Height() == output.Height());
    assert(input.GetPtr(0, 0) != output.GetPtr(0, 0));

    const std::uint8_t* source = input.GetPtr(0, 0);
    std::uint8_t* destination = output.GetPtr(0, 0);
    const int width = input.Width();
    const int height = input.Height();

    switch (kernel_size)
    {
    case 3:
        Median3x3(source, destination, width, height);
        break;
    case 5:
        MedianByRadixSelection<5>(source, destination, width, height);
        break;
    case 7:
        MedianByRadixSelection<7>(source, destination, width, height);
        break;
    case 9:
        MedianBySlidingHistogram<9>(
            source, destination, width, height, workspace);
        break;
    case 11:
        MedianBySlidingHistogram<11>(
            source, destination, width, height, workspace);
        break;
    default:
        assert(false);
    }
}
