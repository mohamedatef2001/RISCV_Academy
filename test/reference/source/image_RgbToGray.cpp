#include "image_RgbToGray.hpp"

#include <cassert>

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
        const int red = rgb[offset];
        const int green = rgb[offset + 1];
        const int blue = rgb[offset + 2];

        // 299/1000, 587/1000, and 114/1000 are the requested weights.
        // Adding 500 rounds to the nearest whole grayscale value.
        gray[pixel] = static_cast<std::uint8_t>(
            (299 * red + 587 * green + 114 * blue + 500) / 1000);
    }
}
