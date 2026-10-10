#include "image_BoxFilter.hpp"

#include <cassert>
#include <cstddef>

#ifndef RISCV_BAREMETAL
#include <cstring>
#endif

namespace
{
void BoxFilterScalar(const std::uint8_t* input, std::uint8_t* output,
                     const int width, const int height)
{
    const std::size_t byte_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (width < 3 || height < 3)
    {
        memcpy(output, input, byte_count);
        return;
    }

    memcpy(output, input, static_cast<std::size_t>(width));
    memcpy(output + static_cast<std::size_t>(height - 1) * width,
           input + static_cast<std::size_t>(height - 1) * width,
           static_cast<std::size_t>(width));

    for (int y = 1; y < height - 1; ++y)
    {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        output[row] = input[row];
        output[row + width - 1] = input[row + width - 1];

        for (int x = 1; x < width - 1; ++x)
        {
            std::uint16_t sum = 0;
            for (int ky = -1; ky <= 1; ++ky)
            {
                const std::size_t source_row =
                    static_cast<std::size_t>(y + ky) * width;
                for (int kx = -1; kx <= 1; ++kx)
                {
                    sum = static_cast<std::uint16_t>(
                        sum + input[source_row + static_cast<std::size_t>(x + kx)]);
                }
            }
            output[row + x] = static_cast<std::uint8_t>(sum / 9U);
        }
    }
}
} // namespace

void ref::BoxFilter(const Image<std::uint8_t>& input,
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

    BoxFilterScalar(source, destination, width, height);
    delete[] snapshot;
}
