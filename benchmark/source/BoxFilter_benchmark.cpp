#include "benchmark.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <riscv_vector.h>

#ifdef RISCV_QEMU
#include <string>
#else
#include "image0.hpp"
#endif

namespace
{
constexpr std::uint64_t kStretchCycles = 60000;
constexpr std::uint64_t kStretchInstructions = 20000;
constexpr int kIterations = 1;

void LoadImage0(Image<std::uint8_t>& input)
{
#ifdef RISCV_QEMU
    std::string project_dir = __FILE__;
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    const std::string path = project_dir + "/images/image0.pgm";
    input.Read(path.c_str());
#else
    input.Read(image0_width, image0_height, image0_data);
#endif
}

void PrintStretchResult(const std::uint64_t minimum_cycles,
                        const std::uint64_t minimum_instructions)
{
    const bool cycles_met = minimum_cycles <= kStretchCycles;
    const bool instructions_met = minimum_instructions <= kStretchInstructions;
    printf("  stretch target: <= %llu cycles, <= %llu instructions (non-gating)\n",
           (unsigned long long)kStretchCycles,
           (unsigned long long)kStretchInstructions);
    printf("  minimum observed cycles: %llu (%s, delta %+lld)\n",
           (unsigned long long)minimum_cycles, cycles_met ? "MET" : "NOT MET",
           (long long)minimum_cycles - (long long)kStretchCycles);
    printf("  minimum observed instructions: %llu (%s, delta %+lld)\n",
           (unsigned long long)minimum_instructions,
           instructions_met ? "MET" : "NOT MET",
           (long long)minimum_instructions - (long long)kStretchInstructions);
}

} // namespace

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    Image<std::uint8_t> input;
    LoadImage0(input);
    const int width = input.Width();
    const int height = input.Height();
    const int pixel_count = width * height;

    Image<std::uint8_t> output_vectorized(width, height);
    Image<std::uint8_t> output_reference(width, height);

    printf("BoxFilter benchmark: %dx%d, %d iterations\n",
           width, height, kIterations);

    std::uint64_t reference_cycles = 0;
    std::uint64_t reference_instructions = 0;
    std::uint64_t vectorized_cycles = 0;
    std::uint64_t vectorized_instructions = 0;
    std::uint64_t minimum_vector_cycles =
        std::numeric_limits<std::uint64_t>::max();
    std::uint64_t minimum_vector_instructions =
        std::numeric_limits<std::uint64_t>::max();

    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        Timer vector_timer;
        Timer scalar_timer;
        scalar_timer.Start();
        ref::BoxFilter(input, output_reference);
        scalar_timer.Stop();

        vector_timer.Start();
        vec::BoxFilter(input, output_vectorized);
        vector_timer.Stop();

        const std::uint64_t vector_cycles = vector_timer.ElapsedCycles();
        const std::uint64_t vector_instructions =
            vector_timer.ElapsedInstructions();

        const std::uint64_t scalar_cycles = scalar_timer.ElapsedCycles();
        const std::uint64_t scalar_instructions =
            scalar_timer.ElapsedInstructions();
        reference_cycles += scalar_cycles;
        reference_instructions += scalar_instructions;
        vectorized_cycles += vector_cycles;
        vectorized_instructions += vector_instructions;
        if (vector_cycles < minimum_vector_cycles)
        {
            minimum_vector_cycles = vector_cycles;
        }
        if (vector_instructions < minimum_vector_instructions)
        {
            minimum_vector_instructions = vector_instructions;
        }

        printf("  iteration %d scalar: %llu cycles, %llu instructions\n",
               iteration + 1,
               (unsigned long long)scalar_cycles,
               (unsigned long long)scalar_instructions);
        printf("  iteration %d vector: %llu cycles, %llu instructions\n",
               iteration + 1,
               (unsigned long long)vector_cycles,
               (unsigned long long)vector_instructions);

        if (!CheckCorrectness(output_reference, output_vectorized))
        {
            printf("Output is wrong!\n");
            return 1;
        }
    }

    PrintTime("BoxFilter", pixel_count,
              vectorized_cycles / kIterations,
              vectorized_instructions / kIterations,
              reference_cycles / kIterations,
              reference_instructions / kIterations);

    const std::size_t vlmax = __riscv_vsetvlmax_e16m8();
    const std::size_t outputs_per_chunk = vlmax - 2;
    const std::size_t chunks_per_row =
        (static_cast<std::size_t>(width - 2) + outputs_per_chunk - 1) /
        outputs_per_chunk;
    const std::size_t chunk_count =
        chunks_per_row * static_cast<std::size_t>(height - 2);
    const std::size_t memory_instruction_floor = 4 * chunk_count;
    const std::size_t inferred_vlen_bits = vlmax * 16 / 8;

    printf("  vector geometry: VLEN=%llu bits, e16m8 VLMAX=%llu, outputs/chunk=%llu\n",
           (unsigned long long)inferred_vlen_bits,
           (unsigned long long)vlmax,
           (unsigned long long)outputs_per_chunk);
    printf("  vertical-first memory floor (3 loads + 1 store/chunk): %llu instructions\n",
           (unsigned long long)memory_instruction_floor);
    PrintStretchResult(minimum_vector_cycles, minimum_vector_instructions);

    printf("Output is correct.\n");
    return 0;
}
