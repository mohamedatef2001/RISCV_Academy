#include "image1.hpp"
#include "riscv_cv.hpp"

#include <cstddef>
#include <cstdint>

namespace
{
constexpr unsigned int kNoisePercentage = 5;
constexpr int kKernelSizes[] = {3, 5, 7, 9, 11};
constexpr char kTemporaryInputPath[] =
    "/tmp/median_filter_input_noisy.pgm";

void LoadNoisyImage1(Image<std::uint8_t>& image)
{
    static_assert(image1_channels == 1);
    for (int y = 0; y < image1_height; ++y)
    {
        for (int x = 0; x < image1_width; ++x)
        {
            image.SetPixel(x, y, image1_data[y * image1_width + x]);
        }
    }
    AddSaltAndPepperNoise(image, kNoisePercentage);
}

} // namespace

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    Image<std::uint8_t> noisy(image1_width, image1_height);
    Image<std::uint8_t> filtered(image1_width, image1_height);
    LoadNoisyImage1(noisy);

    const std::size_t workspace_capacity =
        MedianFilterWorkspaceCapacity(image1_width, 11);
    std::uint16_t* workspace_data = new std::uint16_t[workspace_capacity];
    const MedianFilterWorkspace workspace = {
        workspace_data, workspace_capacity};

#ifdef RISCV_QEMU
    noisy.Write(kTemporaryInputPath);
#endif

    for (const int kernel_size : kKernelSizes)
    {
        vec::MedianFilter(noisy, filtered, kernel_size, workspace);
#ifdef RISCV_QEMU
        char temporary_path[] = "/tmp/median_filter_output_00x00.pgm";
        temporary_path[26] = static_cast<char>('0' + kernel_size / 10);
        temporary_path[27] = static_cast<char>('0' + kernel_size % 10);
        temporary_path[29] = temporary_path[26];
        temporary_path[30] = temporary_path[27];
        filtered.Write(temporary_path);
#endif
    }

    delete[] workspace_data;
    printf("Example passed.\n");
    return 0;
}
