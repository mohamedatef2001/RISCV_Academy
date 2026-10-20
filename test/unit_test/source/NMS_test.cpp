#include "gtest_lite.hpp"

#include <cstdint>

#include "nms_onnx_cases.hpp"
#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

namespace
{
    struct WorkspaceOwner
    {
        explicit WorkspaceOwner(int spatial_dim)
            : capacity(NMSWorkspaceCapacity(spatial_dim, NMSSortAlgorithm::Bitonic)),
              order(new int[capacity > 0 ? capacity : 1]),
              scores(new float[capacity > 0 ? capacity : 1]),
              suppressed(new uint8_t[capacity > 0 ? capacity : 1])
        {
        }

        ~WorkspaceOwner()
        {
            delete[] suppressed;
            delete[] scores;
            delete[] order;
        }

        NMSWorkspace Get() const
        {
            return {order, scores, suppressed, capacity};
        }

        int capacity;
        int* order;
        float* scores;
        uint8_t* suppressed;
    };

    int OutputRows(const NMSOnnxCase& test_case)
    {
        const int64_t rows = static_cast<int64_t>(test_case.num_batches) *
            test_case.num_classes * test_case.max_output_boxes_per_class;
        return rows > 0 ? static_cast<int>(rows) : 1;
    }

    int RunReference(const NMSOnnxCase& test_case, int64_t* output,
                     NMSWorkspace workspace, NMSSortAlgorithm algorithm)
    {
        return ref::NonMaxSuppression(
            test_case.boxes, test_case.scores, test_case.num_batches,
            test_case.num_classes, test_case.spatial_dim,
            test_case.max_output_boxes_per_class, test_case.iou_threshold,
            test_case.score_threshold, test_case.center_point_box, output,
            workspace, algorithm);
    }

    int RunVector(const NMSOnnxCase& test_case, int64_t* output,
                  NMSWorkspace workspace, NMSSortAlgorithm algorithm)
    {
        return vec::NonMaxSuppression(
            test_case.boxes, test_case.scores, test_case.num_batches,
            test_case.num_classes, test_case.spatial_dim,
            test_case.max_output_boxes_per_class, test_case.iou_threshold,
            test_case.score_threshold, test_case.center_point_box, output,
            workspace, algorithm);
    }
}

TEST(NMSTest, ReferenceMatchesOnnxGoldenWithBothSorts)
{
    const NMSSortAlgorithm algorithms[] = {
        NMSSortAlgorithm::OddEven, NMSSortAlgorithm::Bitonic};
    for (int algorithm = 0; algorithm < 2; ++algorithm)
    {
        for (int i = 0; i < kNMSOnnxCaseCount; ++i)
        {
            const NMSOnnxCase& test_case = kNMSOnnxCases[i];
            WorkspaceOwner workspace(test_case.spatial_dim);
            int64_t* actual = new int64_t[OutputRows(test_case) * 3];
            const int actual_count = RunReference(
                test_case, actual, workspace.Get(), algorithms[algorithm]);
            ExpectSelectedIndicesEqual(test_case.expected_indices, actual,
                                       test_case.expected_count, actual_count);
            delete[] actual;
        }
    }
}

TEST(NMSTest, VectorizedMatchesOnnxGoldenWithBothSorts)
{
    const NMSSortAlgorithm algorithms[] = {
        NMSSortAlgorithm::OddEven, NMSSortAlgorithm::Bitonic};
    for (int algorithm = 0; algorithm < 2; ++algorithm)
    {
        for (int i = 0; i < kNMSOnnxCaseCount; ++i)
        {
            const NMSOnnxCase& test_case = kNMSOnnxCases[i];
            WorkspaceOwner workspace(test_case.spatial_dim);
            int64_t* actual = new int64_t[OutputRows(test_case) * 3];
            const int actual_count = RunVector(
                test_case, actual, workspace.Get(), algorithms[algorithm]);
            ExpectSelectedIndicesEqual(test_case.expected_indices, actual,
                                       test_case.expected_count, actual_count);
            delete[] actual;
        }
    }
}

TEST(NMSTest, VectorizedMatchesReferenceForTailsAndStableTies)
{
    const int box_count = 80;
    const int capacity = NMSWorkspaceCapacity(
        box_count, NMSSortAlgorithm::Bitonic);
    float* boxes = new float[static_cast<size_t>(box_count) * 4];
    float* scores = new float[box_count];
    int64_t* expected = new int64_t[static_cast<size_t>(box_count) * 3];
    int64_t* actual = new int64_t[static_cast<size_t>(box_count) * 3];
    int* reference_order = new int[capacity];
    int* vector_order = new int[capacity];
    float* reference_scores = new float[capacity];
    float* vector_scores = new float[capacity];
    uint8_t* reference_suppressed = new uint8_t[capacity];
    uint8_t* vector_suppressed = new uint8_t[capacity];
    const NMSWorkspace reference_workspace = {
        reference_order, reference_scores, reference_suppressed, capacity};
    const NMSWorkspace vector_workspace = {
        vector_order, vector_scores, vector_suppressed, capacity};
    const float score_threshold = 0.0f;

    for (int i = 0; i < box_count; ++i)
    {
        const float origin = static_cast<float>(i * 10);
        boxes[i * 4 + 0] = origin;
        boxes[i * 4 + 1] = 0.0f;
        boxes[i * 4 + 2] = origin + 1.0f;
        boxes[i * 4 + 3] = 1.0f;
        scores[i] = 1.0f - 0.001f * static_cast<float>(i);
    }

    const NMSSortAlgorithm algorithms[] = {
        NMSSortAlgorithm::OddEven, NMSSortAlgorithm::Bitonic};
    for (int algorithm = 0; algorithm < 2; ++algorithm)
    {
        const int expected_count = ref::NonMaxSuppression(
            boxes, scores, 1, 1, box_count, box_count, 0.5f,
            &score_threshold, CenterPointBox::Corners, expected,
            reference_workspace, algorithms[algorithm]);
        const int actual_count = vec::NonMaxSuppression(
            boxes, scores, 1, 1, box_count, box_count, 0.5f,
            &score_threshold, CenterPointBox::Corners, actual,
            vector_workspace, algorithms[algorithm]);
        ExpectSelectedIndicesEqual(expected, actual, expected_count, actual_count);
    }

    for (int i = 0; i < box_count; ++i)
    {
        boxes[i * 4 + 0] = 0.0f;
        boxes[i * 4 + 1] = 0.0f;
        boxes[i * 4 + 2] = 1.0f;
        boxes[i * 4 + 3] = 1.0f;
        scores[i] = 0.9f;
    }
    for (int algorithm = 0; algorithm < 2; ++algorithm)
    {
        const int expected_count = ref::NonMaxSuppression(
            boxes, scores, 1, 1, box_count, 5, 0.5f, &score_threshold,
            CenterPointBox::Corners, expected, reference_workspace,
            algorithms[algorithm]);
        const int actual_count = vec::NonMaxSuppression(
            boxes, scores, 1, 1, box_count, 5, 0.5f, &score_threshold,
            CenterPointBox::Corners, actual, vector_workspace,
            algorithms[algorithm]);
        ExpectSelectedIndicesEqual(expected, actual, expected_count, actual_count);
        EXPECT_EQ(0, static_cast<int>(actual[2]));
    }

    delete[] vector_suppressed;
    delete[] reference_suppressed;
    delete[] vector_scores;
    delete[] reference_scores;
    delete[] vector_order;
    delete[] reference_order;
    delete[] actual;
    delete[] expected;
    delete[] scores;
    delete[] boxes;
}

TEST(NMSTest, EmptyInputReturnsZeroWithoutWorkspace)
{
    const NMSWorkspace no_workspace = {nullptr, nullptr, nullptr, 0};
    EXPECT_EQ(0, ref::NonMaxSuppression(
        nullptr, nullptr, 0, 1, 1, 1, 0.5f, nullptr,
        CenterPointBox::Corners, nullptr, no_workspace));
    EXPECT_EQ(0, vec::NonMaxSuppression(
        nullptr, nullptr, 1, 0, 1, 1, 0.5f, nullptr,
        CenterPointBox::Corners, nullptr, no_workspace));
}

TEST(NMSTest, ScoreSuppressionHonorsInclusiveBoundaryAndNullThreshold)
{
    const float boxes[] = {
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 3.0f, 1.0f, 4.0f,
        0.0f, 6.0f, 1.0f, 7.0f,
        0.0f, 9.0f, 1.0f, 10.0f,
    };
    const float scores[] = {0.49f, 0.50f, 0.90f, 0.10f};
    const float threshold = 0.50f;
    const int64_t expected_threshold[] = {0, 0, 2, 0, 0, 1};
    const int64_t expected_null[] = {
        0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0, 3};
    int64_t actual[12];
    WorkspaceOwner workspace(4);

    int count = vec::NonMaxSuppression(
        boxes, scores, 1, 1, 4, 4, 0.5f, &threshold,
        CenterPointBox::Corners, actual, workspace.Get(),
        NMSSortAlgorithm::Bitonic);
    ExpectSelectedIndicesEqual(expected_threshold, actual, 2, count);

    count = vec::NonMaxSuppression(
        boxes, scores, 1, 1, 4, 4, 0.5f, nullptr,
        CenterPointBox::Corners, actual, workspace.Get(),
        NMSSortAlgorithm::OddEven);
    ExpectSelectedIndicesEqual(expected_null, actual, 4, count);
}

TEST(NMSTest, StableSortPreservesOriginalOrderForEqualScores)
{
    constexpr int count = 9;
    float boxes[count * 4];
    float scores[count];
    int64_t expected[count * 3];
    int64_t actual[count * 3];
    for (int i = 0; i < count; ++i)
    {
        boxes[i * 4 + 0] = 0.0f;
        boxes[i * 4 + 1] = static_cast<float>(i * 3);
        boxes[i * 4 + 2] = 1.0f;
        boxes[i * 4 + 3] = static_cast<float>(i * 3 + 1);
        scores[i] = 0.75f;
        expected[i * 3 + 0] = 0;
        expected[i * 3 + 1] = 0;
        expected[i * 3 + 2] = i;
    }

    WorkspaceOwner workspace(count);
    const NMSSortAlgorithm algorithms[] = {
        NMSSortAlgorithm::OddEven, NMSSortAlgorithm::Bitonic};
    for (int algorithm = 0; algorithm < 2; ++algorithm)
    {
        const int actual_count = vec::NonMaxSuppression(
            boxes, scores, 1, 1, count, count, 0.5f, nullptr,
            CenterPointBox::Corners, actual, workspace.Get(),
            algorithms[algorithm]);
        ExpectSelectedIndicesEqual(expected, actual, count, actual_count);
    }
}

TEST(NMSTest, IoUSuppressionIsStrictAndDegenerateBoxesDoNotSuppress)
{
    const float boxes[] = {
        0.0f, 0.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 1.5f, 1.5f,
        2.0f, 2.0f, 2.0f, 3.0f,
    };
    const float scores[] = {0.9f, 0.8f, 0.7f};
    const float boundary = 0.25f / 1.75f;
    const int64_t expected[] = {
        0, 0, 0, 0, 0, 1, 0, 0, 2};
    int64_t actual[9];
    WorkspaceOwner workspace(3);

    const int actual_count = vec::NonMaxSuppression(
        boxes, scores, 1, 1, 3, 3, boundary, nullptr,
        CenterPointBox::Corners, actual, workspace.Get(),
        NMSSortAlgorithm::Bitonic);
    ExpectSelectedIndicesEqual(expected, actual, 3, actual_count);
}
