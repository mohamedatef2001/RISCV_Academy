#include "riscv_cv.hpp"
#include "sobel_bmp.hpp"
#include "sobel_input.hpp"

#ifndef RISCV_BAREMETAL
#include <cstdio>
#endif

namespace
{
    constexpr std::uint32_t SobelThresholdSquared = 128 * 128;
    std::uint8_t output[sobel_example::PixelCount];

    bool WriteBmpHosted(const std::uint8_t* pixels)
    {
#ifdef RISCV_BAREMETAL
        (void)pixels;
        return false;
#else
        FILE* file = std::fopen(sobel_example::OutputFilename, "wb");
        if (file == nullptr)
        {
            return false;
        }

        const bool ok =
            std::fwrite(sobel_example::BmpHeader, 1,
                        sizeof(sobel_example::BmpHeader), file)
                == sizeof(sobel_example::BmpHeader)
            && std::fwrite(sobel_example::Palette.bytes, 1,
                           sizeof(sobel_example::Palette.bytes), file)
                == sizeof(sobel_example::Palette.bytes)
            && std::fwrite(pixels, 1, sobel_example::PixelCount, file)
                == sobel_example::PixelCount;

        return std::fclose(file) == 0 && ok;
#endif
    }
}

int main()
{
    // The legacy embedded asset carries two trailing rows that the original
    // 512 x 512 example also ignored. Preserve the asset byte-for-byte and
    // consume only the declared image extent.
    static_assert(sizeof(sobelInput) / sizeof(sobelInput[0])
                  >= sobel_example::PixelCount);

#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    vec::Sobel(sobelInput, output,
               sobel_example::Width, sobel_example::Height,
               SobelThresholdSquared);

#ifdef RISCV_BAREMETAL
    const bool wrote_output =
        sobel_example::WriteBmpWithGem5(output);
#else
    const bool wrote_output = WriteBmpHosted(output);
#endif

    if (!wrote_output)
    {
        printf("Example failed: could not write %s.\n",
               sobel_example::OutputFilename);
        return 1;
    }

    printf("Wrote %s.\n", sobel_example::OutputFilename);
    printf("Example passed.\n");
    return 0;
}
