#pragma once

#include <cstddef>
#include <cstdint>

// rgb has 3 bytes (R, G, B) per pixel; gray needs 1 byte per pixel.
void RgbToGray(const std::uint8_t* rgb, std::uint8_t* gray,
               std::size_t pixel_count);
