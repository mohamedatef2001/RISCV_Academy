#include "image_BoxFilter.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <riscv_vector.h>

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

std::uint8_t FilterBorderPixel(const std::uint8_t* input,
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
        sum = (sum + area / 2U) / area;
    }
    else if (sum > 255U)
    {
        sum = 255U;
    }
    return static_cast<std::uint8_t>(sum);
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

void FillBorder(const std::uint8_t* input,
                std::uint8_t* output,
                const int width,
                const int height,
                const Size ksize,
                const Point anchor,
                const bool normalize,
                const int border_type,
                const int interior_x_begin,
                const int interior_x_end,
                const int interior_y_begin,
                const int interior_y_end)
{
    for (int y = 0; y < height; ++y)
    {
        const bool full_row = y < interior_y_begin || y >= interior_y_end;
        const int left_end = full_row ? width : interior_x_begin;
        const int right_begin = full_row ? width : interior_x_end;
        const std::size_t row = static_cast<std::size_t>(y) * width;

        for (int x = 0; x < left_end; ++x)
        {
            output[row + static_cast<std::size_t>(x)] =
                FilterBorderPixel(input, width, height, x, y, ksize, anchor,
                                  normalize, border_type);
        }
        for (int x = right_begin; x < width; ++x)
        {
            output[row + static_cast<std::size_t>(x)] =
                FilterBorderPixel(input, width, height, x, y, ksize, anchor,
                                  normalize, border_type);
        }
    }
}

std::uint8_t FinishBorder3x3(const std::uint32_t sum,
                             const bool normalize)
{
    if (normalize)
    {
        return static_cast<std::uint8_t>(((sum + 4U) * 7282U) >> 16U);
    }
    return static_cast<std::uint8_t>(sum > 255U ? 255U : sum);
}

std::uint32_t SumThreeScalar(const std::uint8_t* row, const int x)
{
    return static_cast<std::uint32_t>(row[x - 1]) + row[x] + row[x + 1];
}

void FillBorder3x3(const std::uint8_t* input,
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
        FillBorder(input, output, width, height, Size(3, 3), Point(1, 1),
                   normalize, border_type, 1, width - 1, 1, height - 1);
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
        output[0] = FinishBorder3x3(
            static_cast<std::uint32_t>(first[0]) + first[1] + second[0] +
                second[1],
            normalize);
        output[width - 1] = FinishBorder3x3(
            static_cast<std::uint32_t>(first[width - 2]) + first[width - 1] +
                second[width - 2] + second[width - 1],
            normalize);
        output_last[0] = FinishBorder3x3(
            static_cast<std::uint32_t>(penultimate[0]) + penultimate[1] +
                last[0] + last[1],
            normalize);
        output_last[width - 1] = FinishBorder3x3(
            static_cast<std::uint32_t>(penultimate[width - 2]) +
                penultimate[width - 1] + last[width - 2] + last[width - 1],
            normalize);

        for (int x = 1; x < width - 1; ++x)
        {
            output[x] = FinishBorder3x3(
                SumThreeScalar(first, x) + SumThreeScalar(second, x),
                normalize);
            output_last[x] = FinishBorder3x3(
                SumThreeScalar(penultimate, x) + SumThreeScalar(last, x),
                normalize);
        }
        for (int y = 1; y < height - 1; ++y)
        {
            const std::uint8_t* top =
                input + static_cast<std::size_t>(y - 1) * width;
            const std::uint8_t* middle = top + width;
            const std::uint8_t* bottom = middle + width;
            std::uint8_t* destination =
                output + static_cast<std::size_t>(y) * width;
            destination[0] = FinishBorder3x3(
                static_cast<std::uint32_t>(top[0]) + top[1] + middle[0] +
                    middle[1] + bottom[0] + bottom[1],
                normalize);
            destination[width - 1] = FinishBorder3x3(
                static_cast<std::uint32_t>(top[width - 2]) + top[width - 1] +
                    middle[width - 2] + middle[width - 1] +
                    bottom[width - 2] + bottom[width - 1],
                normalize);
        }
        return;
    }

    output[0] = FinishBorder3x3(
        4U * first[0] + 2U * first[1] + 2U * second[0] + second[1],
        normalize);
    output[width - 1] = FinishBorder3x3(
        4U * first[width - 1] + 2U * first[width - 2] +
            2U * second[width - 1] + second[width - 2],
        normalize);
    output_last[0] = FinishBorder3x3(
        4U * last[0] + 2U * last[1] + 2U * penultimate[0] + penultimate[1],
        normalize);
    output_last[width - 1] = FinishBorder3x3(
        4U * last[width - 1] + 2U * last[width - 2] +
            2U * penultimate[width - 1] + penultimate[width - 2],
        normalize);

    for (int x = 1; x < width - 1; ++x)
    {
        output[x] = FinishBorder3x3(
            2U * SumThreeScalar(first, x) + SumThreeScalar(second, x),
            normalize);
        output_last[x] = FinishBorder3x3(
            2U * SumThreeScalar(last, x) + SumThreeScalar(penultimate, x),
            normalize);
    }
    for (int y = 1; y < height - 1; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - 1) * width;
        const std::uint8_t* middle = top + width;
        const std::uint8_t* bottom = middle + width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width;
        destination[0] = FinishBorder3x3(
            2U * (static_cast<std::uint32_t>(top[0]) + middle[0] + bottom[0]) +
                top[1] + middle[1] + bottom[1],
            normalize);
        destination[width - 1] = FinishBorder3x3(
            2U * (static_cast<std::uint32_t>(top[width - 1]) +
                  middle[width - 1] + bottom[width - 1]) +
                top[width - 2] + middle[width - 2] + bottom[width - 2],
            normalize);
    }
}

vuint8m4_t Finish3x3(vuint16m8_t sum,
                     const bool normalize,
                     const std::size_t vl)
{
    if (normalize)
    {
        // Rounded division by nine: floor((sum + 4) / 9). Multiplication by
        // ceil(2^16 / 9) is exact throughout the possible 3x3 range.
        sum = __riscv_vadd_vx_u16m8(sum, 4, vl);
        sum = __riscv_vmulhu_vx_u16m8(sum, 7282, vl);
    }
    else
    {
        sum = __riscv_vminu_vx_u16m8(sum, 255, vl);
    }
    return __riscv_vnsrl_wx_u8m4(sum, 0, vl);
}

void FilterInterior3x3(const std::uint8_t* input,
                       std::uint8_t* output,
                       const int width,
                       const int x_begin,
                       const int x_end,
                       const int y_begin,
                       const int y_end,
                       const Point anchor,
                       const bool normalize)
{
    const std::size_t vlmax = __riscv_vsetvlmax_e16m8();
    const std::size_t max_output_lanes = vlmax - 2U;
    const std::size_t output_count =
        static_cast<std::size_t>(x_end - x_begin);

    for (int y = y_begin; y < y_end; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - anchor.y) * width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width + x_begin;

        std::size_t x = 0;
        while (x < output_count)
        {
            const std::size_t remaining = output_count - x;
            const std::size_t vl = remaining < max_output_lanes
                                       ? remaining
                                       : max_output_lanes;
            const std::size_t load_vl = vl + 2U;
            const vuint8m4_t row0 = __riscv_vle8_v_u8m4(top + x, load_vl);
            const vuint8m4_t row1 =
                __riscv_vle8_v_u8m4(top + width + x, load_vl);
            const vuint8m4_t row2 =
                __riscv_vle8_v_u8m4(top + 2 * width + x, load_vl);

            vuint16m8_t vertical =
                __riscv_vwaddu_vv_u16m8(row0, row1, load_vl);
            vertical = __riscv_vwaddu_wv_u16m8(vertical, row2, load_vl);
            vuint16m8_t sum =
                __riscv_vslidedown_vx_u16m8(vertical, 2, vl);
            sum = __riscv_vadd_vv_u16m8(sum, vertical, vl);
            vertical = __riscv_vslidedown_vx_u16m8(vertical, 1, vl);
            sum = __riscv_vadd_vv_u16m8(sum, vertical, vl);

            __riscv_vse8_v_u8m4(destination + x,
                                Finish3x3(sum, normalize, vl), vl);
            x += vl;
        }
    }
}

vuint8m2_t FinishGeneric(vuint32m8_t sum,
                         const std::uint32_t area,
                         const bool normalize,
                         const std::size_t vl)
{
    if (normalize)
    {
        sum = __riscv_vadd_vx_u32m8(sum, area / 2U, vl);
        sum = __riscv_vdivu_vx_u32m8(sum, area, vl);
    }
    else
    {
        sum = __riscv_vminu_vx_u32m8(sum, 255, vl);
    }
    const vuint16m4_t narrowed16 = __riscv_vnsrl_wx_u16m4(sum, 0, vl);
    return __riscv_vnsrl_wx_u8m2(narrowed16, 0, vl);
}

void FilterInteriorSliding(const std::uint8_t* input,
                           std::uint8_t* output,
                           const int width,
                           const Size ksize,
                           const Point anchor,
                           const bool normalize,
                           const int x_begin,
                           const int x_end,
                           const int y_begin,
                           const int y_end,
                           const std::size_t vlmax)
{
    const std::size_t lookahead = static_cast<std::size_t>(ksize.width - 1);
    const std::size_t max_output_lanes = vlmax - lookahead;
    const std::size_t output_count =
        static_cast<std::size_t>(x_end - x_begin);
    const std::uint32_t area = static_cast<std::uint32_t>(ksize.width) *
                               static_cast<std::uint32_t>(ksize.height);

    for (int y = y_begin; y < y_end; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - anchor.y) * width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width + x_begin;
        std::size_t x = 0;
        while (x < output_count)
        {
            const std::size_t remaining = output_count - x;
            const std::size_t vl = remaining < max_output_lanes
                                       ? remaining
                                       : max_output_lanes;
            const std::size_t load_vl = vl + lookahead;
            vuint32m8_t vertical =
                __riscv_vmv_v_x_u32m8(0, load_vl);
            for (int ky = 0; ky < ksize.height; ++ky)
            {
                const std::uint8_t* row =
                    top + static_cast<std::size_t>(ky) * width + x;
                const vuint8m2_t pixels =
                    __riscv_vle8_v_u8m2(row, load_vl);
                vertical = __riscv_vadd_vv_u32m8(
                    vertical, __riscv_vzext_vf4_u32m8(pixels, load_vl),
                    load_vl);
            }

            vuint32m8_t sum = vertical;
            for (int kx = 1; kx < ksize.width; ++kx)
            {
                sum = __riscv_vadd_vv_u32m8(
                    sum,
                    __riscv_vslidedown_vx_u32m8(vertical,
                                                static_cast<std::size_t>(kx),
                                                vl),
                    vl);
            }
            __riscv_vse8_v_u8m2(destination + x,
                                FinishGeneric(sum, area, normalize, vl), vl);
            x += vl;
        }
    }
}

void FilterInteriorDirect(const std::uint8_t* input,
                          std::uint8_t* output,
                          const int width,
                          const Size ksize,
                          const Point anchor,
                          const bool normalize,
                          const int x_begin,
                          const int x_end,
                          const int y_begin,
                          const int y_end)
{
    const std::size_t output_count =
        static_cast<std::size_t>(x_end - x_begin);
    const std::uint32_t area = static_cast<std::uint32_t>(ksize.width) *
                               static_cast<std::uint32_t>(ksize.height);
    for (int y = y_begin; y < y_end; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - anchor.y) * width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width + x_begin;
        std::size_t x = 0;
        while (x < output_count)
        {
            const std::size_t vl =
                __riscv_vsetvl_e32m8(output_count - x);
            vuint32m8_t sum = __riscv_vmv_v_x_u32m8(0, vl);
            for (int ky = 0; ky < ksize.height; ++ky)
            {
                const std::uint8_t* row =
                    top + static_cast<std::size_t>(ky) * width + x;
                for (int kx = 0; kx < ksize.width; ++kx)
                {
                    const vuint8m2_t pixels =
                        __riscv_vle8_v_u8m2(row + kx, vl);
                    sum = __riscv_vadd_vv_u32m8(
                        sum, __riscv_vzext_vf4_u32m8(pixels, vl), vl);
                }
            }
            __riscv_vse8_v_u8m2(destination + x,
                                FinishGeneric(sum, area, normalize, vl), vl);
            x += vl;
        }
    }
}
} // namespace

void vec::boxFilter(const Image<std::uint8_t>& input,
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

    const int width = input.Width();
    const int height = input.Height();
    const int interior_x_begin = anchor.x;
    const int interior_x_end = width - (ksize.width - anchor.x - 1);
    const int interior_y_begin = anchor.y;
    const int interior_y_end = height - (ksize.height - anchor.y - 1);
    const std::uint8_t* source = input.GetPtr(0, 0);
    std::uint8_t* destination = output.GetPtr(0, 0);

    if (interior_x_begin >= interior_x_end ||
        interior_y_begin >= interior_y_end)
    {
        FillBorder(source, destination, width, height, ksize, anchor,
                   normalize, border_type, width, width, height, height);
        return;
    }

    if (ksize.width == 3 && ksize.height == 3 && anchor.x == 1 &&
        anchor.y == 1)
    {
        FillBorder3x3(source, destination, width, height, normalize,
                      border_type);
        FilterInterior3x3(source, destination, width, interior_x_begin,
                          interior_x_end, interior_y_begin, interior_y_end,
                          anchor, normalize);
        return;
    }

    FillBorder(source, destination, width, height, ksize, anchor, normalize,
               border_type, interior_x_begin, interior_x_end,
               interior_y_begin, interior_y_end);

    const std::size_t vlmax = __riscv_vsetvlmax_e32m8();
    if (static_cast<std::size_t>(ksize.width) <= vlmax)
    {
        FilterInteriorSliding(source, destination, width, ksize, anchor,
                              normalize, interior_x_begin, interior_x_end,
                              interior_y_begin, interior_y_end, vlmax);
    }
    else
    {
        FilterInteriorDirect(source, destination, width, ksize, anchor,
                             normalize, interior_x_begin, interior_x_end,
                             interior_y_begin, interior_y_end);
    }
}
