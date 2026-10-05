#include "image_RgbToGray.hpp"

#include <cassert>
#include <riscv_vector.h>

namespace
{
// Q24 encodings of 0.299, 0.587, and 0.114, rounded upward. For 8-bit
// channels the error is nonnegative and less than 0.001, preserving the
// original nearest-rounded decimal formula for every RGB combination.
constexpr std::uint32_t kRedQ24 = 5016388;
constexpr std::uint32_t kGreenQ24 = 9848226;
constexpr std::uint32_t kBlueQ24 = 1912603;
constexpr std::uint32_t kRoundQ24 = 1U << 23;
constexpr unsigned int kFractionBits = 24;
constexpr std::size_t kChannels = 3;
} // namespace

void vec::RgbToGray(const std::uint8_t* rgb, std::uint8_t* gray,
                   const std::size_t pixel_count)
{
    assert(rgb != nullptr);
    assert(gray != nullptr);

    const std::size_t block_pixels = __riscv_vsetvlmax_e32m8();
    for (std::size_t pixel = 0; pixel < pixel_count;)
    {
        // e32,m8 and e8,m2 have the same lane count. Query VLMAX once;
        // the intrinsics configure vl for each full block or final tail.
        const std::size_t remaining = pixel_count - pixel;
        const std::size_t vl = remaining < block_pixels ? remaining : block_pixels;

        // Read the active RGB pixels without reading beyond the final pixel.
        const std::uint8_t* input = rgb + kChannels * pixel;
        const vuint8m2_t red = __riscv_vlse8_v_u8m2(input, kChannels, vl);
        const vuint8m2_t green = __riscv_vlse8_v_u8m2(input + 1, kChannels, vl);
        const vuint8m2_t blue = __riscv_vlse8_v_u8m2(input + 2, kChannels, vl);

        // Independent products avoid a serial multiply-accumulate chain.
        const vuint32m8_t red_weighted = __riscv_vmul_vx_u32m8(
            __riscv_vzext_vf4_u32m8(red, vl), kRedQ24, vl);
        const vuint32m8_t green_weighted = __riscv_vmul_vx_u32m8(
            __riscv_vzext_vf4_u32m8(green, vl), kGreenQ24, vl);
        const vuint32m8_t blue_weighted = __riscv_vmul_vx_u32m8(
            __riscv_vzext_vf4_u32m8(blue, vl), kBlueQ24, vl);

        // Add 0.5 for nearest rounding. Even white fits in uint32_t:
        // 255 * (kRedQ24 + kGreenQ24 + kBlueQ24) + kRoundQ24 = 4,286,578,943.
        vuint32m8_t weighted =
            __riscv_vadd_vv_u32m8(red_weighted, green_weighted, vl);
        weighted = __riscv_vadd_vv_u32m8(weighted, blue_weighted, vl);
        weighted = __riscv_vadd_vx_u32m8(weighted, kRoundQ24, vl);

        // Shift away the fractional bits as part of the narrowing instruction.
        const vuint16m4_t gray_16 =
            __riscv_vnsrl_wx_u16m4(weighted, kFractionBits, vl);
        const vuint8m2_t gray_8 = __riscv_vnsrl_wx_u8m2(gray_16, 0, vl);
        __riscv_vse8_v_u8m2(gray + pixel, gray_8, vl);
        pixel += vl;
    }
}
