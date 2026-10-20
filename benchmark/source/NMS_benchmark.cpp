#include "benchmark.hpp"
#include "image1.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"

namespace
{
    constexpr int kMaxCandidates = 128;
    constexpr int kNumClasses = 3;
    constexpr int kMaxOutput = 40;
    constexpr int kIterations = 6;
    constexpr int kPathCount = 4;
    constexpr int kCandidateCounts[] = {16, 32, 64, 128};
    constexpr float kScoreThreshold = 0.20f;
    constexpr float kIouThreshold = 0.50f;
    constexpr uint64_t kImagePixels =
        static_cast<uint64_t>(image1_width) * image1_height;

    using NMSFunction = decltype(&ref::NonMaxSuppression);

    struct Totals
    {
        uint64_t cycles = 0;
        uint64_t instructions = 0;
    };

    struct BenchmarkPath
    {
        const char* name;
        NMSFunction function;
        NMSSortAlgorithm algorithm;
        int64_t* output;
        NMSWorkspace workspace;
        Totals totals;
        int output_count = 0;
    };

    void GenerateImage1Detections(float* boxes, float* scores,
                                  int candidate_count)
    {
        // Four close proposals form one detection cluster. The clusters cover
        // the image1 rock region and create realistic overlap for IoU NMS.
        for (int i = 0; i < candidate_count; ++i)
        {
            const int cluster = i / 4;
            const int column = cluster % 8;
            const int row = cluster / 8;
            const float shift = static_cast<float>(i & 3) * 2.0f;
            const float x = 25.0f + static_cast<float>(column * 75);
            const float y = 425.0f + static_cast<float>(row * 45);
            boxes[i * 4 + 0] = y + shift;
            boxes[i * 4 + 1] = x + shift;
            boxes[i * 4 + 2] = y + 70.0f + shift;
            boxes[i * 4 + 3] = x + 65.0f + shift;

            for (int cls = 0; cls < kNumClasses; ++cls)
            {
                const int mixed = (i * 37 + cls * 19) % 101;
                scores[cls * candidate_count + i] =
                    static_cast<float>(mixed) / 100.0f;
            }
        }
    }

    int Run(BenchmarkPath& path, const float* boxes, const float* scores,
            int candidate_count)
    {
        path.output_count = path.function(
            boxes, scores, 1, kNumClasses, candidate_count, kMaxOutput,
            kIouThreshold, &kScoreThreshold, CenterPointBox::Corners,
            path.output, path.workspace, path.algorithm);
        return path.output_count;
    }

    void Measure(BenchmarkPath& path, const float* boxes, const float* scores,
                 int candidate_count)
    {
        Timer timer;
        timer.Start();
        Run(path, boxes, scores, candidate_count);
        timer.Stop();
        path.totals.cycles += timer.ElapsedCycles();
        path.totals.instructions += timer.ElapsedInstructions();
    }

    bool SelectedEqual(const BenchmarkPath& expected,
                       const BenchmarkPath& actual)
    {
        if (expected.output_count != actual.output_count)
        {
            return false;
        }
        for (int i = 0; i < expected.output_count * 3; ++i)
        {
            if (expected.output[i] != actual.output[i])
            {
                return false;
            }
        }
        return true;
    }

    void PrintResult(const BenchmarkPath& path, int candidate_count)
    {
        const uint64_t average_cycles = path.totals.cycles / kIterations;
        const uint64_t average_instructions =
            path.totals.instructions / kIterations;
        const uint64_t candidates_per_image =
            static_cast<uint64_t>(candidate_count) * kNumClasses;
        const uint64_t cycles_per_candidate_x100 =
            average_cycles * 100 / candidates_per_image;
        const uint64_t cycles_per_pixel_x1000000 =
            average_cycles * 1000000 / kImagePixels;

        printf("  %-20s %llu cycles, %llu instructions, ", path.name,
               static_cast<unsigned long long>(average_cycles),
               static_cast<unsigned long long>(average_instructions));
        printf("%llu.%02llu cycles/candidate, %llu.%06llu cycles/pixel\n",
               static_cast<unsigned long long>(cycles_per_candidate_x100 / 100),
               static_cast<unsigned long long>(cycles_per_candidate_x100 % 100),
               static_cast<unsigned long long>(cycles_per_pixel_x1000000 / 1000000),
               static_cast<unsigned long long>(cycles_per_pixel_x1000000 % 1000000));
    }

    void PrintRatio(const char* label, uint64_t numerator, uint64_t denominator)
    {
        if (denominator == 0)
        {
            return;
        }
        const uint64_t ratio_x100 = numerator * 100 / denominator;
        printf("  %-20s %llu.%02llux\n", label,
               static_cast<unsigned long long>(ratio_x100 / 100),
               static_cast<unsigned long long>(ratio_x100 % 100));
    }
}

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    static_assert(image1_width == 640 && image1_height == 960);
    static_assert(image1_channels == 1);

    const int capacity = NMSWorkspaceCapacity(
        kMaxCandidates, NMSSortAlgorithm::Bitonic);
    const int output_elements = kNumClasses * kMaxOutput * 3;

    float* boxes = new float[kMaxCandidates * 4];
    float* scores = new float[kNumClasses * kMaxCandidates];
    int64_t* outputs = new int64_t[kPathCount * output_elements];
    int* orders = new int[kPathCount * capacity];
    float* packed_scores = new float[kPathCount * capacity];
    uint8_t* suppressed = new uint8_t[kPathCount * capacity];

    BenchmarkPath paths[kPathCount] = {
        {"scalar odd-even", &ref::NonMaxSuppression,
         NMSSortAlgorithm::OddEven, outputs,
         {orders, packed_scores, suppressed, capacity}},
        {"scalar bitonic", &ref::NonMaxSuppression,
         NMSSortAlgorithm::Bitonic, outputs + output_elements,
         {orders + capacity, packed_scores + capacity,
          suppressed + capacity, capacity}},
        {"RVV odd-even", &vec::NonMaxSuppression,
         NMSSortAlgorithm::OddEven, outputs + 2 * output_elements,
         {orders + 2 * capacity, packed_scores + 2 * capacity,
          suppressed + 2 * capacity, capacity}},
        {"RVV bitonic", &vec::NonMaxSuppression,
         NMSSortAlgorithm::Bitonic, outputs + 3 * output_elements,
         {orders + 3 * capacity, packed_scores + 3 * capacity,
          suppressed + 3 * capacity, capacity}},
    };

    bool correct = true;
    printf("image1 NMS sorting comparison: %d classes, %d iterations, "
           "%llu pixels\n", kNumClasses, kIterations,
           static_cast<unsigned long long>(kImagePixels));
    printf("cycles/candidate is the native NMS metric; cycles/pixel is "
           "normalized by the 640x960 source image.\n");

    for (int scenario = 0; scenario < 4; ++scenario)
    {
        const int candidate_count = kCandidateCounts[scenario];
        GenerateImage1Detections(boxes, scores, candidate_count);

        for (int path = 0; path < kPathCount; ++path)
        {
            paths[path].totals = {};
            Run(paths[path], boxes, scores, candidate_count);
        }
        for (int path = 1; path < kPathCount; ++path)
        {
            correct = correct && SelectedEqual(paths[0], paths[path]);
        }

        // Rotate timed order to distribute cache and branch predictor effects.
        for (int iteration = 0; iteration < kIterations; ++iteration)
        {
            for (int step = 0; step < kPathCount; ++step)
            {
                const int path = (iteration + step) % kPathCount;
                Measure(paths[path], boxes, scores, candidate_count);
            }
            for (int path = 1; path < kPathCount; ++path)
            {
                correct = correct && SelectedEqual(paths[0], paths[path]);
            }
        }

        printf("\n%d candidates/class (%d total candidates)\n",
               candidate_count, candidate_count * kNumClasses);
        for (int path = 0; path < kPathCount; ++path)
        {
            PrintResult(paths[path], candidate_count);
        }
        PrintRatio("RVV odd-even speedup",
                   paths[0].totals.cycles, paths[2].totals.cycles);
        PrintRatio("RVV bitonic speedup",
                   paths[1].totals.cycles, paths[3].totals.cycles);
        PrintRatio("scalar bitonic/odd",
                   paths[0].totals.cycles, paths[1].totals.cycles);
        PrintRatio("RVV bitonic/odd",
                   paths[2].totals.cycles, paths[3].totals.cycles);
    }

    delete[] suppressed;
    delete[] packed_scores;
    delete[] orders;
    delete[] outputs;
    delete[] scores;
    delete[] boxes;

    if (correct)
    {
        printf("Output is correct.\n");
        return 0;
    }
    printf("Output is wrong!\n");
    return 1;
}
