#include "image_BoxFilter.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
int Clamp(const int value, const int upper_bound)
{
    if (value < 0)
    {
        return 0;
    }
    if (value >= upper_bound)
    {
        return upper_bound - 1;
    }
    return value;
}

int Reflect101(int value, const int length)
{
    if (length == 1)
    {
        return 0;
    }
    while (value < 0 || value >= length)
    {
        value = value < 0 ? -value : 2 * length - value - 2;
    }
    return value;
}

std::uint8_t FinishThreeByThree(const std::uint32_t sum,
                                const bool normalize)
{
    if (normalize)
    {
        // Exact round-to-nearest division for every possible 3x3 uint8 sum.
        return static_cast<std::uint8_t>(((sum + 4U) * 7282U) >> 16U);
    }
    return static_cast<std::uint8_t>(sum > 255U ? 255U : sum);
}

std::uint32_t SumThree(const std::uint8_t* row, const int x)
{
    return static_cast<std::uint32_t>(row[x - 1]) + row[x] + row[x + 1];
}

void FilterInteriorThreeByThree(const std::uint8_t* input,
                                std::uint8_t* output,
                                const int width,
                                const int height,
                                const bool normalize)
{
    for (int y = 1; y < height - 1; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - 1) * width;
        const std::uint8_t* middle = top + width;
        const std::uint8_t* bottom = middle + width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;

        for (int x = 1; x < width - 1; ++x)
        {
            const std::uint32_t sum =
                SumThree(top, x) + SumThree(middle, x) + SumThree(bottom, x);
            destination[x] = FinishThreeByThree(sum, normalize);
        }
    }
}

std::uint8_t FilterPixel(const std::uint8_t* input,
                         const int width,
                         const int height,
                         const int x,
                         const int y,
                         const Size ksize,
                         const Point anchor,
                         const bool normalize,
                         const int border_type)
{
    std::uint64_t sum = 0;
    for (int ky = 0; ky < ksize.height; ++ky)
    {
        int source_y = y + ky - anchor.y;
        if (source_y < 0 || source_y >= height)
        {
            if (border_type == BORDER_CONSTANT)
            {
                continue;
            }
            source_y = border_type == BORDER_REPLICATE
                           ? Clamp(source_y, height)
                           : Reflect101(source_y, height);
        }

        const std::size_t row = static_cast<std::size_t>(source_y) * width;
        for (int kx = 0; kx < ksize.width; ++kx)
        {
            int source_x = x + kx - anchor.x;
            if (source_x < 0 || source_x >= width)
            {
                if (border_type == BORDER_CONSTANT)
                {
                    continue;
                }
                source_x = border_type == BORDER_REPLICATE
                               ? Clamp(source_x, width)
                               : Reflect101(source_x, width);
            }
            sum += input[row + static_cast<std::size_t>(source_x)];
        }
    }

    if (normalize)
    {
        const std::uint64_t area =
            static_cast<std::uint64_t>(ksize.width) * ksize.height;
        // OpenCV converts the normalized result back to CV_8U with rounding.
        sum = (sum + area / 2U) / area;
    }
    else if (sum > 255U)
    {
        sum = 255U;
    }
    return static_cast<std::uint8_t>(sum);
}

void FilterThreeByThreeBorder(const std::uint8_t* input,
                              std::uint8_t* output,
                              const int width,
                              const int height,
                              const bool normalize,
                              const int border_type)
{
    if (width < 3 || height < 3 ||
        (border_type != BORDER_CONSTANT &&
         border_type != BORDER_REPLICATE))
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (width < 3 || height < 3 || x == 0 || x == width - 1 ||
                    y == 0 || y == height - 1)
                {
                    output[static_cast<std::size_t>(y) * width + x] =
                        FilterPixel(input, width, height, x, y, Size(3, 3),
                                    Point(1, 1), normalize, border_type);
                }
            }
        }
        return;
    }

    const std::uint8_t* first = input;
    const std::uint8_t* second = input + width;
    const std::uint8_t* penultimate =
        input + static_cast<std::size_t>(height - 2) * width;
    const std::uint8_t* last = penultimate + width;
    std::uint8_t* output_last =
        output + static_cast<std::size_t>(height - 1) * width;

    if (border_type == BORDER_CONSTANT)
    {
        output[0] = FinishThreeByThree(
            static_cast<std::uint32_t>(first[0]) + first[1] + second[0] +
                second[1],
            normalize);
        output[width - 1] = FinishThreeByThree(
            static_cast<std::uint32_t>(first[width - 2]) + first[width - 1] +
                second[width - 2] + second[width - 1],
            normalize);
        output_last[0] = FinishThreeByThree(
            static_cast<std::uint32_t>(penultimate[0]) + penultimate[1] +
                last[0] + last[1],
            normalize);
        output_last[width - 1] = FinishThreeByThree(
            static_cast<std::uint32_t>(penultimate[width - 2]) +
                penultimate[width - 1] + last[width - 2] + last[width - 1],
            normalize);

        for (int x = 1; x < width - 1; ++x)
        {
            output[x] =
                FinishThreeByThree(SumThree(first, x) + SumThree(second, x),
                                   normalize);
            output_last[x] = FinishThreeByThree(
                SumThree(penultimate, x) + SumThree(last, x), normalize);
        }
        for (int y = 1; y < height - 1; ++y)
        {
            const std::uint8_t* top =
                input + static_cast<std::size_t>(y - 1) * width;
            const std::uint8_t* middle = top + width;
            const std::uint8_t* bottom = middle + width;
            std::uint8_t* destination =
                output + static_cast<std::size_t>(y) * width;
            destination[0] = FinishThreeByThree(
                static_cast<std::uint32_t>(top[0]) + top[1] + middle[0] +
                    middle[1] + bottom[0] + bottom[1],
                normalize);
            destination[width - 1] = FinishThreeByThree(
                static_cast<std::uint32_t>(top[width - 2]) + top[width - 1] +
                    middle[width - 2] + middle[width - 1] +
                    bottom[width - 2] + bottom[width - 1],
                normalize);
        }
        return;
    }

    output[0] = FinishThreeByThree(
        4U * first[0] + 2U * first[1] + 2U * second[0] + second[1],
        normalize);
    output[width - 1] = FinishThreeByThree(
        4U * first[width - 1] + 2U * first[width - 2] +
            2U * second[width - 1] + second[width - 2],
        normalize);
    output_last[0] = FinishThreeByThree(
        4U * last[0] + 2U * last[1] + 2U * penultimate[0] + penultimate[1],
        normalize);
    output_last[width - 1] = FinishThreeByThree(
        4U * last[width - 1] + 2U * last[width - 2] +
            2U * penultimate[width - 1] + penultimate[width - 2],
        normalize);

    for (int x = 1; x < width - 1; ++x)
    {
        output[x] = FinishThreeByThree(
            2U * SumThree(first, x) + SumThree(second, x), normalize);
        output_last[x] = FinishThreeByThree(
            2U * SumThree(last, x) + SumThree(penultimate, x), normalize);
    }
    for (int y = 1; y < height - 1; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - 1) * width;
        const std::uint8_t* middle = top + width;
        const std::uint8_t* bottom = middle + width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;
        destination[0] = FinishThreeByThree(
            2U * (static_cast<std::uint32_t>(top[0]) + middle[0] + bottom[0]) +
                top[1] + middle[1] + bottom[1],
            normalize);
        destination[width - 1] = FinishThreeByThree(
            2U * (static_cast<std::uint32_t>(top[width - 1]) +
                  middle[width - 1] + bottom[width - 1]) +
                top[width - 2] + middle[width - 2] + bottom[width - 2],
            normalize);
    }
}

bool ResolveAndValidate(const Image<std::uint8_t>& input,
                        Image<std::uint8_t>& output,
                        const int ddepth,
                        const Size ksize,
                        Point& anchor,
                        const int border_type)
{
    if (anchor.x == -1)
    {
        anchor.x = ksize.width / 2;
    }
    if (anchor.y == -1)
    {
        anchor.y = ksize.height / 2;
    }

    const std::uint64_t area =
        ksize.width > 0 && ksize.height > 0
            ? static_cast<std::uint64_t>(ksize.width) * ksize.height
            : 0U;
    const bool valid =
        input.Width() == output.Width() &&
        input.Height() == output.Height() &&
        input.GetPtr(0, 0) != output.GetPtr(0, 0) &&
        (ddepth == -1 || ddepth == CV_8U) &&
        ksize.width > 0 && ksize.height > 0 &&
        (ksize.width & 1) != 0 && (ksize.height & 1) != 0 &&
        anchor.x >= 0 && anchor.x < ksize.width &&
        anchor.y >= 0 && anchor.y < ksize.height &&
        (border_type == BORDER_CONSTANT || border_type == BORDER_REPLICATE ||
         border_type == BORDER_REFLECT_101) &&
        area <= std::numeric_limits<std::uint32_t>::max() / 256U;
    assert(valid);
    return valid;
}
} // namespace

void ref::boxFilter(const Image<std::uint8_t>& input,
                    Image<std::uint8_t>& output,
                    const int ddepth,
                    const Size ksize,
                    Point anchor,
                    const bool normalize,
                    const int border_type)
{
    if (!ResolveAndValidate(input, output, ddepth, ksize, anchor, border_type))
    {
        return;
    }

    const std::uint8_t* source = input.GetPtr(0, 0);
    std::uint8_t* destination = output.GetPtr(0, 0);

    // Keep the common benchmark path fixed-size and compiler-friendly. The
    // public API remains generic; uncommon shapes use the simple oracle below.
    if (ksize.width == 3 && ksize.height == 3 && anchor.x == 1 &&
        anchor.y == 1)
    {
        FilterThreeByThreeBorder(source, destination, input.Width(),
                                 input.Height(), normalize, border_type);
        if (input.Width() >= 3 && input.Height() >= 3)
        {
            FilterInteriorThreeByThree(source, destination, input.Width(),
                                       input.Height(), normalize);
        }
        return;
    }

    for (int y = 0; y < input.Height(); ++y)
    {
        const std::size_t row = static_cast<std::size_t>(y) * input.Width();
        for (int x = 0; x < input.Width(); ++x)
        {
            destination[row + static_cast<std::size_t>(x)] =
                FilterPixel(source, input.Width(), input.Height(), x, y,
                            ksize, anchor, normalize, border_type);
        }
    }
}
