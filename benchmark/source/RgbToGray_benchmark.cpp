#include "benchmark.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

int main()
{
#ifdef RISCV_BAREMETAL
    // Enable the vector unit before any auto-vectorized loop runs.
    riscv_enable_vector();
#endif

    const int width = 128;
    const int height = 100;
    const int loop_count = 3;
    const int pixel_count = width * height;

    printf("RgbToGray benchmark: %dx%d, %d iterations\n",
           width, height, loop_count);

    Image<std::uint8_t, ImageType::RGB> input(width, height);
    Image<std::uint8_t> output_reference(width, height);
    Image<std::uint8_t> output_vectorized(width, height);

    RandomInt<std::uint8_t> random(0, 255);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int channel = 0; channel < 3; ++channel)
            {
                input.SetPixel(x, y, random.get(), channel);
            }
        }
    }

    std::uint64_t reference_cycles = 0;
    std::uint64_t vectorized_cycles = 0;
    std::uint64_t reference_instrs = 0;
    std::uint64_t vectorized_instrs = 0;
    bool all_correct = true;

    for (int iteration = 0; iteration < loop_count; ++iteration)
    {
        Timer timer_scalar;
        Timer timer_vectorized;

        timer_scalar.Start();
        ref::RgbToGray(input.GetPtr(0, 0), output_reference.GetPtr(0, 0),
                       static_cast<std::size_t>(pixel_count));
        timer_scalar.Stop();

        timer_vectorized.Start();
        vec::RgbToGray(input.GetPtr(0, 0), output_vectorized.GetPtr(0, 0),
                       static_cast<std::size_t>(pixel_count));
        timer_vectorized.Stop();

        reference_cycles += timer_scalar.ElapsedCycles();
        reference_instrs += timer_scalar.ElapsedInstructions();
        vectorized_cycles += timer_vectorized.ElapsedCycles();
        vectorized_instrs += timer_vectorized.ElapsedInstructions();

        if (!CheckCorrectness(output_reference, output_vectorized))
        {
            all_correct = false;
            break;
        }
    }

    PrintTime("RgbToGray", pixel_count,
              vectorized_cycles / loop_count, vectorized_instrs / loop_count,
              reference_cycles / loop_count, reference_instrs / loop_count);

    if (all_correct)
    {
        printf("Output is correct.\n");
        return 0;
    }

    printf("Output is wrong!\n");
    return 1;
}
