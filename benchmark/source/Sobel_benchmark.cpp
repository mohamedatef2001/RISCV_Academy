#include "benchmark.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    constexpr int width = 128;
    constexpr int height = 100;
    constexpr int loop_count = 3;
    constexpr std::uint32_t threshold_squared = 128 * 128;

    printf("Sobel benchmark: %dx%d, %d iterations\n",
           width, height, loop_count);

    Image<std::uint32_t> input(width, height);
    Image<std::uint8_t> output_reference(width, height);
    Image<std::uint8_t> output_vectorized(width, height);

    RandomInt<std::uint32_t> random(0, 255);
    random.ImageRandomInitialize(input);

    std::uint64_t reference_cycles = 0;
    std::uint64_t reference_instructions = 0;
    std::uint64_t vectorized_cycles = 0;
    std::uint64_t vectorized_instructions = 0;
    bool all_correct = true;

    for (int iteration = 0; iteration < loop_count; ++iteration)
    {
        Timer reference_timer;
        reference_timer.Start();
        ref::Sobel(input, output_reference, threshold_squared);
        reference_timer.Stop();

        Timer vectorized_timer;
        vectorized_timer.Start();
        vec::Sobel(input, output_vectorized, threshold_squared);
        vectorized_timer.Stop();

        reference_cycles += reference_timer.ElapsedCycles();
        reference_instructions += reference_timer.ElapsedInstructions();
        vectorized_cycles += vectorized_timer.ElapsedCycles();
        vectorized_instructions += vectorized_timer.ElapsedInstructions();

        if (!CheckCorrectness(output_reference, output_vectorized))
        {
            all_correct = false;
            break;
        }
    }

    PrintTime("Sobel", width * height,
              vectorized_cycles / loop_count,
              vectorized_instructions / loop_count,
              reference_cycles / loop_count,
              reference_instructions / loop_count);

    if (all_correct)
    {
        printf("Output is correct.\n");
        return 0;
    }

    printf("Output is wrong!\n");
    return 1;
}
