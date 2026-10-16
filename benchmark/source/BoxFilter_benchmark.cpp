#include "benchmark.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

#include <cstdint>

#ifdef RISCV_QEMU
#include <string>
#else
#include "image0.hpp"
#endif

namespace
{
constexpr int kIterations = 1;
constexpr std::uint64_t kBaseline3x3VectorCycles = 931854;
constexpr std::uint64_t kBaseline3x3VectorInstructions = 288903;

struct BenchmarkCase
{
    const char* name;
    Size ksize;
    int border_type;
};

constexpr BenchmarkCase kCases[] = {
    {"BoxFilter 3x3 constant", Size(3, 3), BORDER_CONSTANT},
    {"BoxFilter 5x5 replicate", Size(5, 5), BORDER_REPLICATE},
    {"BoxFilter 7x7 replicate", Size(7, 7), BORDER_REPLICATE},
    {"BoxFilter 9x9 replicate", Size(9, 9), BORDER_REPLICATE},
    {"BoxFilter 11x11 replicate", Size(11, 11), BORDER_REPLICATE},
};

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

bool RunCase(const BenchmarkCase& benchmark_case,
             const Image<std::uint8_t>& input,
             Image<std::uint8_t>& output_reference,
             Image<std::uint8_t>& output_vectorized)
{
    std::uint64_t reference_cycles = 0;
    std::uint64_t reference_instructions = 0;
    std::uint64_t vectorized_cycles = 0;
    std::uint64_t vectorized_instructions = 0;

    printf("\n%s: image=%dx%d, iterations=%d\n", benchmark_case.name,
           input.Width(), input.Height(), kIterations);
    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
        Timer scalar_timer;
        scalar_timer.Start();
        ref::boxFilter(input, output_reference, -1, benchmark_case.ksize,
                       Point(-1, -1), true, benchmark_case.border_type);
        scalar_timer.Stop();

        Timer vector_timer;
        vector_timer.Start();
        vec::boxFilter(input, output_vectorized, -1, benchmark_case.ksize,
                       Point(-1, -1), true, benchmark_case.border_type);
        vector_timer.Stop();

        reference_cycles += scalar_timer.ElapsedCycles();
        reference_instructions += scalar_timer.ElapsedInstructions();
        vectorized_cycles += vector_timer.ElapsedCycles();
        vectorized_instructions += vector_timer.ElapsedInstructions();

        if (!CheckCorrectness(output_reference, output_vectorized))
        {
            printf("Output is wrong!\n");
            return false;
        }
    }

    PrintTime(benchmark_case.name, input.Width() * input.Height(),
              vectorized_cycles / kIterations,
              vectorized_instructions / kIterations,
              reference_cycles / kIterations,
              reference_instructions / kIterations);
#ifdef RISCV_BAREMETAL
    if (benchmark_case.ksize.width == 3 && benchmark_case.ksize.height == 3 &&
        benchmark_case.border_type == BORDER_CONSTANT)
    {
        const std::uint64_t average_cycles = vectorized_cycles / kIterations;
        const std::uint64_t average_instructions =
            vectorized_instructions / kIterations;
        printf("  prior gem5 vector baseline: %llu cycles, %llu instructions\n",
               (unsigned long long)kBaseline3x3VectorCycles,
               (unsigned long long)kBaseline3x3VectorInstructions);
        printf("  vector baseline delta     : %+lld cycles, %+lld instructions\n",
               (long long)average_cycles -
                   (long long)kBaseline3x3VectorCycles,
               (long long)average_instructions -
                   (long long)kBaseline3x3VectorInstructions);
    }
#endif
    return true;
}
} // namespace

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    Image<std::uint8_t> input;
    LoadImage0(input);
    Image<std::uint8_t> output_vectorized(input.Width(), input.Height());
    Image<std::uint8_t> output_reference(input.Width(), input.Height());

    for (const BenchmarkCase& benchmark_case : kCases)
    {
        if (!RunCase(benchmark_case, input, output_reference,
                     output_vectorized))
        {
            return 1;
        }
    }

    printf("Output is correct.\n");
    return 0;
}
