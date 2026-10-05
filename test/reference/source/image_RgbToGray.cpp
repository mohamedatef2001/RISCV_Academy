#include "image_RgbToGray.hpp"

#include <cassert>

namespace
{
// Q24 encodings of 0.299, 0.587, and 0.114. Rounding each coefficient upward
// keeps this division-free form bit-exact with the original / 1000 expression
// for every combination of 8-bit RGB channels.
constexpr std::uint32_t kRedQ24 = 5016388;
constexpr std::uint32_t kGreenQ24 = 9848226;
constexpr std::uint32_t kBlueQ24 = 1912603;
constexpr std::uint32_t kRoundQ24 = 1U << 23;
constexpr unsigned int kFractionBits = 24;
}

/*

RGB input:   [R0 G0 B0] --- [R1 G1 B1] --- [R2 G2 B2] --- ...
                  |             |             |
                  v             v             v
Gray output:    [Y0] -------- [Y1] -------- [Y2] -------- ...


*/

void ref::RgbToGray(const std::uint8_t* rgb, std::uint8_t* gray,
                    std::size_t pixel_count)
{
    assert(rgb != nullptr);
    assert(gray != nullptr);

    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
    {
        const std::size_t offset = 3 * pixel;
        const std::uint32_t red = rgb[offset];
        const std::uint32_t green = rgb[offset + 1];
        const std::uint32_t blue = rgb[offset + 2];

        const std::uint32_t weighted =
            kRedQ24 * red + kGreenQ24 * green + kBlueQ24 * blue + kRoundQ24;
        gray[pixel] = static_cast<std::uint8_t>(weighted >> kFractionBits);
    }
}
