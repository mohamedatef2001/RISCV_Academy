#include "image_BoxFilter.hpp"

#include <cassert>
#include <cstddef>
#include <riscv_vector.h>

#ifndef RISCV_BAREMETAL
#include <cstring>
#endif

namespace
{
vuint16m8_t DivideByNine(vuint16m8_t value, const std::size_t vl)
{
    // Exact floor(value / 9) for the complete 3x3 uint8_t sum range 0..2295.
    // ceil(2^16 / 9) is 7282. For values up to 2295, the high half of the
    // 16x16-bit product is exactly floor(value / 9).
    return __riscv_vmulhu_vx_u16m8(value, 7282, vl);
}

void CopyUnfiltered(const std::uint8_t* input, std::uint8_t* output,
                    const int width, const int height)
{
    memcpy(output, input,
           static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
}

void BoxFilterVector(const std::uint8_t* input, std::uint8_t* output,
                     const int width, const int height)
{
    if (width < 3 || height < 3)
    {
        CopyUnfiltered(input, output, width, height);
        return;
    }

    memcpy(output, input, static_cast<std::size_t>(width));
    memcpy(output + static_cast<std::size_t>(height - 1) * width,
           input + static_cast<std::size_t>(height - 1) * width,
           static_cast<std::size_t>(width));

    // Copy the left and right borders once.
    for (int y = 1; y < height - 1; ++y)
    {
        const std::size_t row_offset = static_cast<std::size_t>(y) * width;
        output[row_offset] = input[row_offset];
        output[row_offset + width - 1] = input[row_offset + width - 1];
    }

    const std::size_t vlmax = __riscv_vsetvlmax_e16m8();
    assert(vlmax > 2);
    const std::size_t max_output_lanes = vlmax - 2;
    const std::size_t interior_width = static_cast<std::size_t>(width - 2);
    for (int y = 1; y < height - 1; ++y)
    {
        const std::uint8_t* top =
            input + static_cast<std::size_t>(y - 1) * width;
        const std::uint8_t* middle = top + width;
        const std::uint8_t* bottom = middle + width;
        std::uint8_t* destination =
            output + static_cast<std::size_t>(y) * width + 1;

        std::size_t x = 0;
        while (x < interior_width)
        {
            const std::size_t remaining = interior_width - x;
            const std::size_t vl =
                remaining < max_output_lanes ? remaining : max_output_lanes;
            const std::size_t load_vl = vl + 2;

            const vuint8m4_t top_pixels =
                __riscv_vle8_v_u8m4(top + x, load_vl);
            const vuint8m4_t middle_pixels =
                __riscv_vle8_v_u8m4(middle + x, load_vl);
            const vuint8m4_t bottom_pixels =
                __riscv_vle8_v_u8m4(bottom + x, load_vl);

            // First reduce the three rows vertically. Then the two slides
            // expose the next two vertical columns for every output lane.
            vuint16m8_t vertical_sum =
                __riscv_vwaddu_vv_u16m8(
                    top_pixels, middle_pixels, load_vl);
            vertical_sum = __riscv_vwaddu_wv_u16m8(
                vertical_sum, bottom_pixels, load_vl);

            vuint16m8_t window_sum =
                __riscv_vslidedown_vx_u16m8(vertical_sum, 2, vl);
            window_sum =
                __riscv_vadd_vv_u16m8(window_sum, vertical_sum, vl);
            vertical_sum =
                __riscv_vslidedown_vx_u16m8(vertical_sum, 1, vl);
            window_sum =
                __riscv_vadd_vv_u16m8(window_sum, vertical_sum, vl);

            const vuint16m8_t quotient =
                DivideByNine(window_sum, vl);
            const vuint8m4_t pixels =
                __riscv_vnsrl_wx_u8m4(quotient, 0, vl);
            __riscv_vse8_v_u8m4(destination + x, pixels, vl);

            x += vl;
        }
    }
}
} // namespace

void vec::BoxFilter(const Image<std::uint8_t>& input,
                    Image<std::uint8_t>& output)
{
    assert(input.Width() == output.Width());
    assert(input.Height() == output.Height());

    const int width = input.Width();
    const int height = input.Height();
    const std::size_t byte_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    const std::uint8_t* source = input.GetPtr(0, 0);
    std::uint8_t* destination = output.GetPtr(0, 0);
    std::uint8_t* snapshot = nullptr;

    if (source == destination)
    {
        snapshot = new std::uint8_t[byte_count];
        memcpy(snapshot, source, byte_count);
        source = snapshot;
    }

    BoxFilterVector(source, destination, width, height);
    delete[] snapshot;
}
