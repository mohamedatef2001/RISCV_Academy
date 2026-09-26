//cmake --build build --target run_T2_Sobel_gem5
// cmake --build build --target run_T2_Sobel_qemu
#include "Sobel.h"
#include "header.h"
#include "image.h"

#ifdef RISCV_BAREMETAL
#include "gem5_output.h"
#include "riscv_port.hpp"
#else
#include "timer.h"
#include <cstdio>
#endif

u8 outImg[IMG_SIZE];

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
    if (!write_bmp_prefix_gem5())
    {
        printf("Example failed: gem5 could not write the BMP header.\n");
        return 1;
    }
#endif

    // Initialize output image to black
    for (int i = 0; i < IMG_SIZE; ++i)
        outImg[i] = 0;

#ifdef RISCV_BAREMETAL
    sobel_image_mag2_thresh(imageDataSW, outImg, 16384);
    if (!write_bmp_pixels_gem5(outImg))
    {
        printf("Example failed: gem5 could not write the BMP pixels.\n");
        return 1;
    }
    printf("Wrote T2_Sobel_output.bmp in gem5 output directory.\n");
#else
    run_sobel_timed(imageDataSW, outImg, 16384); // threshold2 = 128^2

    FILE* file = std::fopen("T2_Sobel_output.bmp", "wb");
    if (file == nullptr)
    {
        std::printf("Cannot create output image.\n");
        return 1;
    }

    // Write BMP header
    std::fwrite(bmpHeader, 1, sizeof(bmpHeader), file);

    for (int value = 0; value < 256; ++value)
    {
        const u8 palette[4] = {
            static_cast<u8>(value),
            static_cast<u8>(value),
            static_cast<u8>(value),
            0
        };
        std::fwrite(palette, 1, sizeof(palette), file);
    }

    // Write image data
    std::fwrite(outImg, 1, IMG_SIZE, file);
    std::fclose(file);

    std::printf("Wrote T2_Sobel_output.bmp\n");
#endif
    return 0;
}
