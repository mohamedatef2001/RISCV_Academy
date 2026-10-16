#include "reference_cv.hpp"
#include "riscv_cv.hpp"

#include <cstdint>

#ifdef RISCV_QEMU
#include <string>
#else
#include "image0.hpp"
#endif

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    Image<std::uint8_t> input;
#ifdef RISCV_QEMU
    std::string project_dir = __FILE__;
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    project_dir.resize(project_dir.find_last_of('/'));
    const std::string input_path = project_dir + "/images/image0.pgm";
    const std::string output_path = project_dir + "/images/BoxFilter_output.pgm";
    input.Read(input_path.c_str());
#else
    input.Read(image0_width, image0_height, image0_data);
#endif

    Image<std::uint8_t> expected(input.Width(), input.Height());
    Image<std::uint8_t> output(input.Width(), input.Height());
    ref::boxFilter(input, expected, -1, Size(3, 3), Point(-1, -1), true,
                   BORDER_REPLICATE);

    Timer timer;
    timer.Start();
    vec::boxFilter(input, output, -1, Size(3, 3), Point(-1, -1), true,
                   BORDER_REPLICATE);
    timer.Stop();

    if (!CheckCorrectness(expected, output))
    {
        printf("Example failed.\n");
        return 1;
    }

    printf("BoxFilter: %llu cycles, %llu instructions\n",
           (unsigned long long)timer.ElapsedCycles(),
           (unsigned long long)timer.ElapsedInstructions());

#ifdef RISCV_QEMU
    output.Write(output_path.c_str());
    printf("Wrote %s (%dx%d)\n", output_path.c_str(),
           output.Width(), output.Height());
#endif

    printf("Example passed.\n");
    return 0;
}
