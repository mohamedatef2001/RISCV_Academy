#include "benchmark.hpp"
#include "image1.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

#include <cstddef>
#include <cstdint>

namespace
{
constexpr int kBenchmarkWidth = 128;
constexpr int kBenchmarkHeight = 96;
constexpr int kBenchmarkIterations = 2;
constexpr unsigned int kNoisePercentage = 5;
constexpr int kKernelSizes[] = {3, 5, 7, 9, 11};

void LoadNoisyImage1Crop(Image<std::uint8_t>& input)
{
    static_assert(image1_channels == 1);
    static_assert(kBenchmarkWidth <= image1_width);
    static_assert(kBenchmarkHeight <= image1_height);
    for (int y = 0; y < kBenchmarkHeight; ++y)
    {
        for (int x = 0; x < kBenchmarkWidth; ++x)
        {
            input.SetPixel(x, y, image1_data[y * image1_width + x]);
        }
    }
    AddSaltAndPepperNoise(input, kNoisePercentage);
}

bool RunCase(const int kernel_size,
             const Image<std::uint8_t>& input,
             Image<std::uint8_t>& scalar_output,
             Image<std::uint8_t>& vector_output,
             MedianFilterWorkspace workspace)
{
    std::uint64_t scalar_cycles = 0;
    std::uint64_t scalar_instructions = 0;
    std::uint64_t vector_cycles = 0;
    std::uint64_t vector_instructions = 0;

    // Untimed warm-up also verifies that the same preallocated workspace can
    // be reused across calls.
    ref::MedianFilter(input, scalar_output, kernel_size);
    vec::MedianFilter(input, vector_output, kernel_size, workspace);
    if (!CheckCorrectness(scalar_output, vector_output))
    {
        return false;
    }

    for (int iteration = 0; iteration < kBenchmarkIterations; ++iteration)
    {
        Timer scalar_timer;
        Timer vector_timer;
        if ((iteration & 1) == 0)
        {
            scalar_timer.Start();
            ref::MedianFilter(input, scalar_output, kernel_size);
            scalar_timer.Stop();
            vector_timer.Start();
            vec::MedianFilter(input, vector_output, kernel_size, workspace);
            vector_timer.Stop();
        }
        else
        {
            vector_timer.Start();
            vec::MedianFilter(input, vector_output, kernel_size, workspace);
            vector_timer.Stop();
            scalar_timer.Start();
            ref::MedianFilter(input, scalar_output, kernel_size);
            scalar_timer.Stop();
        }

        scalar_cycles += scalar_timer.ElapsedCycles();
        scalar_instructions += scalar_timer.ElapsedInstructions();
        vector_cycles += vector_timer.ElapsedCycles();
        vector_instructions += vector_timer.ElapsedInstructions();
        if (!CheckCorrectness(scalar_output, vector_output))
        {
            return false;
        }
    }

    char name[] = "MedianFilter 00x00";
    name[13] = static_cast<char>('0' + kernel_size / 10);
    name[14] = static_cast<char>('0' + kernel_size % 10);
    name[16] = name[13];
    name[17] = name[14];
    PrintTime(name, kBenchmarkWidth * kBenchmarkHeight,
              vector_cycles / kBenchmarkIterations,
              vector_instructions / kBenchmarkIterations,
              scalar_cycles / kBenchmarkIterations,
              scalar_instructions / kBenchmarkIterations);
    return true;
}
} // namespace

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    Image<std::uint8_t> input(kBenchmarkWidth, kBenchmarkHeight);
    Image<std::uint8_t> scalar_output(kBenchmarkWidth, kBenchmarkHeight);
    Image<std::uint8_t> vector_output(kBenchmarkWidth, kBenchmarkHeight);
    LoadNoisyImage1Crop(input);

    const std::size_t workspace_capacity =
        MedianFilterWorkspaceCapacity(kBenchmarkWidth, 11);
    std::uint16_t* workspace_data = new std::uint16_t[workspace_capacity];
    const MedianFilterWorkspace workspace = {
        workspace_data, workspace_capacity};

    bool correct = true;
#ifdef RISCV_BAREMETAL
    // The user requested the cycle-accurate gem5 run for 3x3 only.
    correct = RunCase(3, input, scalar_output, vector_output, workspace);
#else
    for (const int kernel_size : kKernelSizes)
    {
        correct = correct && RunCase(
            kernel_size, input, scalar_output, vector_output, workspace);
    }
#endif

    delete[] workspace_data;
    if (correct)
    {
        printf("Output is correct.\n");
        return 0;
    }
    printf("Output is wrong!\n");
    return 1;
}
