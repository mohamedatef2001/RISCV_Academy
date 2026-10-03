#include "image_Sobel.hpp"

#include <riscv_vector.h>

void vec::Sobel(const std::uint32_t* input,
                std::uint8_t* output,
                const int width,
                const int height,
                const std::uint32_t threshold_squared)
{
    assert(input != nullptr && output != nullptr);
    assert(width > 0 && height > 0);

    for (int index = 0; index < width * height; ++index)
    {
        output[index] = 0;
    }

    for (int y = 1; y < height - 1; ++y)
    {
        const std::uint32_t* row0 = &input[(y - 1) * width];
        const std::uint32_t* row1 = &input[y * width];
        const std::uint32_t* row2 = &input[(y + 1) * width];
        std::uint8_t* output_row = &output[y * width];

        int x = 1;
        while (x < width - 1)
        {
            const std::size_t vl =
                __riscv_vsetvl_e32m4(static_cast<std::size_t>(width - 1 - x));

            const vint32m4_t p00 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row0[x - 1]), vl);
            const vint32m4_t p01 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row0[x]), vl);
            const vint32m4_t p02 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row0[x + 1]), vl);
            const vint32m4_t p10 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row1[x - 1]), vl);
            const vint32m4_t p12 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row1[x + 1]), vl);
            const vint32m4_t p20 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row2[x - 1]), vl);
            const vint32m4_t p21 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row2[x]), vl);
            const vint32m4_t p22 = __riscv_vle32_v_i32m4(
                reinterpret_cast<const std::int32_t*>(&row2[x + 1]), vl);

            vint32m4_t right = __riscv_vadd_vv_i32m4(p02, p22, vl);
            right = __riscv_vadd_vv_i32m4(
                right, __riscv_vsll_vx_i32m4(p12, 1, vl), vl);
            vint32m4_t left = __riscv_vadd_vv_i32m4(p00, p20, vl);
            left = __riscv_vadd_vv_i32m4(
                left, __riscv_vsll_vx_i32m4(p10, 1, vl), vl);
            const vint32m4_t gx = __riscv_vsub_vv_i32m4(right, left, vl);

            vint32m4_t top = __riscv_vadd_vv_i32m4(p00, p02, vl);
            top = __riscv_vadd_vv_i32m4(
                top, __riscv_vsll_vx_i32m4(p01, 1, vl), vl);
            vint32m4_t bottom = __riscv_vadd_vv_i32m4(p20, p22, vl);
            bottom = __riscv_vadd_vv_i32m4(
                bottom, __riscv_vsll_vx_i32m4(p21, 1, vl), vl);
            const vint32m4_t gy = __riscv_vsub_vv_i32m4(top, bottom, vl);

            const vint32m4_t gx_squared =
                __riscv_vmul_vv_i32m4(gx, gx, vl);
            const vint32m4_t gy_squared =
                __riscv_vmul_vv_i32m4(gy, gy, vl);
            const vuint32m4_t magnitude_squared =
                __riscv_vreinterpret_v_i32m4_u32m4(
                    __riscv_vadd_vv_i32m4(gx_squared, gy_squared, vl));

            const vbool8_t edge_mask = __riscv_vmsgeu_vx_u32m4_b8(
                magnitude_squared, threshold_squared, vl);
            const vuint8m1_t zeros = __riscv_vmv_v_x_u8m1(0, vl);
            const vuint8m1_t result = __riscv_vmerge_vxm_u8m1(
                zeros, 255, edge_mask, vl);
            __riscv_vse8_v_u8m1(&output_row[x], result, vl);

            x += static_cast<int>(vl);
        }
    }
}

void vec::Sobel(const Image<std::uint32_t>& input,
                Image<std::uint8_t>& output,
                const std::uint32_t threshold_squared)
{
    assert(input.Width() == output.Width() && input.Height() == output.Height());
    Sobel(input.GetPtr(0, 0), output.GetPtr(0, 0),
          input.Width(), input.Height(), threshold_squared);
}
