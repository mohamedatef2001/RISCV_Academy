#include "reference_cv.hpp"
#include "riscv_cv.hpp"

#ifdef RISCV_QEMU
#include "image1.hpp"
#endif

namespace
{
    constexpr int kCandidateCount = 6;
    constexpr float kScoreThreshold = 0.50f;
    constexpr float kIouThreshold = 0.50f;

    // Detector proposals for the large rock in image1. NMS consumes detector
    // boxes and scores; it does not infer them from the image pixels.
    // Box layout is [y1, x1, y2, x2].
    constexpr float kRockBoxes[kCandidateCount * 4] = {
        432.0f,  40.0f, 625.0f, 639.0f, // 0: best rock proposal
        425.0f,  32.0f, 632.0f, 639.0f, // 1: overlapping duplicate
        445.0f,  55.0f, 615.0f, 625.0f, // 2: overlapping duplicate
        415.0f,  25.0f, 640.0f, 639.0f, // 3: overlapping duplicate
        455.0f,  70.0f, 600.0f, 610.0f, // 4: overlapping duplicate
        345.0f, 200.0f, 370.0f, 280.0f, // 5: low-confidence horizon proposal
    };

    constexpr float kRockScores[kCandidateCount] = {
        0.98f, 0.93f, 0.87f, 0.79f, 0.71f, 0.30f,
    };

    bool SameSelection(const int64_t* expected, int expected_count,
                       const int64_t* actual, int actual_count)
    {
        if (expected_count != actual_count)
        {
            return false;
        }
        for (int i = 0; i < expected_count * 3; ++i)
        {
            if (expected[i] != actual[i])
            {
                return false;
            }
        }
        return true;
    }

#ifdef RISCV_QEMU
    void CopyImage1(Image<uint8_t>& destination)
    {
        static_assert(image1_width == 640 && image1_height == 960);
        static_assert(image1_channels == 1);
        for (int y = 0; y < image1_height; ++y)
        {
            for (int x = 0; x < image1_width; ++x)
            {
                destination.SetPixel(
                    x, y, image1_data[y * image1_width + x]);
            }
        }
    }

    void DrawBox(Image<uint8_t>& image, const float* box,
                 uint8_t value, int thickness)
    {
        int y1 = static_cast<int>(box[0]);
        int x1 = static_cast<int>(box[1]);
        int y2 = static_cast<int>(box[2]);
        int x2 = static_cast<int>(box[3]);
        if (y1 < 0) y1 = 0;
        if (x1 < 0) x1 = 0;
        if (y2 >= image.Height()) y2 = image.Height() - 1;
        if (x2 >= image.Width()) x2 = image.Width() - 1;

        for (int t = 0; t < thickness; ++t)
        {
            const int top = y1 + t;
            const int bottom = y2 - t;
            const int left = x1 + t;
            const int right = x2 - t;
            if (top > bottom || left > right)
            {
                break;
            }
            for (int x = left; x <= right; ++x)
            {
                image.SetPixel(x, top, value);
                image.SetPixel(x, bottom, value);
            }
            for (int y = top; y <= bottom; ++y)
            {
                image.SetPixel(left, y, value);
                image.SetPixel(right, y, value);
            }
        }
    }
#endif
}

int main()
{
#ifdef RISCV_BAREMETAL
    riscv_enable_vector();
#endif

    constexpr int capacity = 8;
    int scalar_odd_order[capacity];
    int scalar_bitonic_order[capacity];
    int vector_odd_order[capacity];
    int vector_bitonic_order[capacity];
    float scalar_odd_scores[capacity];
    float scalar_bitonic_scores[capacity];
    float vector_odd_scores[capacity];
    float vector_bitonic_scores[capacity];
    uint8_t scalar_odd_suppressed[capacity];
    uint8_t scalar_bitonic_suppressed[capacity];
    uint8_t vector_odd_suppressed[capacity];
    uint8_t vector_bitonic_suppressed[capacity];
    int64_t scalar_odd_output[kCandidateCount * 3];
    int64_t scalar_bitonic_output[kCandidateCount * 3];
    int64_t vector_odd_output[kCandidateCount * 3];
    int64_t vector_bitonic_output[kCandidateCount * 3];

    const NMSWorkspace scalar_odd_workspace = {
        scalar_odd_order, scalar_odd_scores, scalar_odd_suppressed, capacity};
    const NMSWorkspace scalar_bitonic_workspace = {
        scalar_bitonic_order, scalar_bitonic_scores,
        scalar_bitonic_suppressed, capacity};
    const NMSWorkspace vector_odd_workspace = {
        vector_odd_order, vector_odd_scores, vector_odd_suppressed, capacity};
    const NMSWorkspace vector_bitonic_workspace = {
        vector_bitonic_order, vector_bitonic_scores,
        vector_bitonic_suppressed, capacity};

    const int scalar_odd_count = ref::NonMaxSuppression(
        kRockBoxes, kRockScores, 1, 1, kCandidateCount, kCandidateCount,
        kIouThreshold, &kScoreThreshold, CenterPointBox::Corners,
        scalar_odd_output, scalar_odd_workspace, NMSSortAlgorithm::OddEven);
    const int scalar_bitonic_count = ref::NonMaxSuppression(
        kRockBoxes, kRockScores, 1, 1, kCandidateCount, kCandidateCount,
        kIouThreshold, &kScoreThreshold, CenterPointBox::Corners,
        scalar_bitonic_output, scalar_bitonic_workspace,
        NMSSortAlgorithm::Bitonic);
    const int vector_odd_count = vec::NonMaxSuppression(
        kRockBoxes, kRockScores, 1, 1, kCandidateCount, kCandidateCount,
        kIouThreshold, &kScoreThreshold, CenterPointBox::Corners,
        vector_odd_output, vector_odd_workspace, NMSSortAlgorithm::OddEven);
    const int vector_bitonic_count = vec::NonMaxSuppression(
        kRockBoxes, kRockScores, 1, 1, kCandidateCount, kCandidateCount,
        kIouThreshold, &kScoreThreshold, CenterPointBox::Corners,
        vector_bitonic_output, vector_bitonic_workspace,
        NMSSortAlgorithm::Bitonic);

    const bool correct =
        SameSelection(scalar_odd_output, scalar_odd_count,
                      scalar_bitonic_output, scalar_bitonic_count) &&
        SameSelection(scalar_odd_output, scalar_odd_count,
                      vector_odd_output, vector_odd_count) &&
        SameSelection(scalar_odd_output, scalar_odd_count,
                      vector_bitonic_output, vector_bitonic_count) &&
        scalar_odd_count == 1 && scalar_odd_output[2] == 0;

    printf("image1 NMS example: large rock in the sea\n");
    printf("  input detector proposals: %d\n", kCandidateCount);
    printf("  score threshold: %.2f, IoU threshold: %.2f\n",
           static_cast<double>(kScoreThreshold),
           static_cast<double>(kIouThreshold));
    for (int i = 0; i < kCandidateCount; ++i)
    {
        printf("  proposal %d: score %.2f, box [%.0f, %.0f, %.0f, %.0f]\n",
               i, static_cast<double>(kRockScores[i]),
               static_cast<double>(kRockBoxes[i * 4 + 0]),
               static_cast<double>(kRockBoxes[i * 4 + 1]),
               static_cast<double>(kRockBoxes[i * 4 + 2]),
               static_cast<double>(kRockBoxes[i * 4 + 3]));
    }
    printf("  selected rows: %d\n", vector_odd_count);
    for (int i = 0; i < vector_odd_count; ++i)
    {
        const int64_t* row = vector_odd_output + i * 3;
        const int box_index = static_cast<int>(row[2]);
        printf("  selected [%lld, %lld, %lld], score %.2f\n",
               static_cast<long long>(row[0]),
               static_cast<long long>(row[1]),
               static_cast<long long>(row[2]),
               static_cast<double>(kRockScores[box_index]));
    }
    printf("  all four scalar/RVV sorting paths agree\n");

#ifdef RISCV_QEMU
    Image<uint8_t> candidates(image1_width, image1_height);
    Image<uint8_t> selected(image1_width, image1_height);
    CopyImage1(candidates);
    CopyImage1(selected);
    for (int i = 0; i < kCandidateCount; ++i)
    {
        if (kRockScores[i] >= kScoreThreshold)
        {
            DrawBox(candidates, kRockBoxes + i * 4, 255, 1);
        }
    }
    for (int i = 0; i < vector_odd_count; ++i)
    {
        const int box_index = static_cast<int>(vector_odd_output[i * 3 + 2]);
        DrawBox(selected, kRockBoxes + box_index * 4, 255, 3);
    }
    candidates.Write("image1_nms_candidates.pgm");
    selected.Write("image1_nms_selected.pgm");
    printf("  wrote image1_nms_candidates.pgm and image1_nms_selected.pgm\n");
#endif

    if (correct)
    {
        printf("Example passed.\n");
        return 0;
    }
    printf("Example failed.\n");
    return 1;
}
