#include "image_RgbToGray.hpp"

#include <cassert>
#include <riscv_vector.h>

void vec::RgbToGray(const std::uint8_t* rgb, std::uint8_t* gray,
                    const std::size_t pixel_count)
{
    assert(rgb != nullptr);
    assert(gray != nullptr);

    std::size_t vl = 0;
    for (std::size_t pixel = 0; pixel < pixel_count; pixel += vl)
    {
        vl = __riscv_vsetvl_e8m2(pixel_count - pixel);

        const std::uint8_t* input = rgb + 3 * pixel;
        const vuint8m2_t red = __riscv_vlse8_v_u8m2(input, 3, vl);
        const vuint8m2_t green = __riscv_vlse8_v_u8m2(input + 1, 3, vl);
        const vuint8m2_t blue = __riscv_vlse8_v_u8m2(input + 2, 3, vl);

        const vuint16m4_t red_wide = __riscv_vzext_vf2_u16m4(red, vl);
        const vuint16m4_t green_wide = __riscv_vzext_vf2_u16m4(green, vl);
        const vuint16m4_t blue_wide = __riscv_vzext_vf2_u16m4(blue, vl);

        vuint32m8_t weighted = __riscv_vwmulu_vx_u32m8(red_wide, 299, vl);
        weighted = __riscv_vwmaccu_vx_u32m8(weighted, 587, green_wide, vl);
        weighted = __riscv_vwmaccu_vx_u32m8(weighted, 114, blue_wide, vl);
        weighted = __riscv_vadd_vx_u32m8(weighted, 500, vl);
        weighted = __riscv_vdivu_vx_u32m8(weighted, 1000, vl);

        const vuint16m4_t gray_16 = __riscv_vnsrl_wx_u16m4(weighted, 0, vl);
        const vuint8m2_t gray_8 = __riscv_vnsrl_wx_u8m2(gray_16, 0, vl);
        __riscv_vse8_v_u8m2(gray + pixel, gray_8, vl);
    }
}
