#include "image_Sobel.hpp"

void ref::Sobel(const std::uint32_t* input,
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

        for (int x = 1; x < width - 1; ++x)
        {
            const std::int32_t p00 = row0[x - 1];
            const std::int32_t p01 = row0[x];
            const std::int32_t p02 = row0[x + 1];
            const std::int32_t p10 = row1[x - 1];
            const std::int32_t p12 = row1[x + 1];
            const std::int32_t p20 = row2[x - 1];
            const std::int32_t p21 = row2[x];
            const std::int32_t p22 = row2[x + 1];

            const std::int32_t gx = p02 + (p12 << 1) + p22
                                  - p00 - (p10 << 1) - p20;
            const std::int32_t gy = p00 + (p01 << 1) + p02
                                  - p20 - (p21 << 1) - p22;
            const std::uint32_t magnitude_squared =
                static_cast<std::uint32_t>(gx * gx + gy * gy);

            output_row[x] = magnitude_squared >= threshold_squared ? 255 : 0;
        }
    }
}

void ref::Sobel(const Image<std::uint32_t>& input,
                Image<std::uint8_t>& output,
                const std::uint32_t threshold_squared)
{
    assert(input.Width() == output.Width() && input.Height() == output.Height());
    Sobel(input.GetPtr(0, 0), output.GetPtr(0, 0),
          input.Width(), input.Height(), threshold_squared);
}
