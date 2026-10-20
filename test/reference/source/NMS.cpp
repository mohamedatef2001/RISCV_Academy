#include "NMS.hpp"

#include "riscv_port.hpp"

namespace
{
    bool BetterCandidate(float left_score, int left_index,
                         float right_score, int right_index)
    {
        return left_score > right_score ||
               (left_score == right_score && left_index < right_index);
    }

    void SwapCandidate(float* scores, int* order, int left, int right)
    {
        const float score = scores[left];
        scores[left] = scores[right];
        scores[right] = score;

        const int index = order[left];
        order[left] = order[right];
        order[right] = index;
    }

    int ApplyScoreSuppression(const float* class_scores, int spatial_dim,
                              const float* score_threshold, int* order,
                              float* packed_scores)
    {
        int count = 0;
        for (int box = 0; box < spatial_dim; ++box)
        {
            const float score = class_scores[box];
            if (score_threshold != nullptr && score < *score_threshold)
            {
                continue;
            }
            order[count] = box;
            packed_scores[count] = score;
            ++count;
        }
        return count;
    }

    void OddEvenSort(int* order, float* scores, int count)
    {
        for (int phase = 0; phase < count; ++phase)
        {
            const int start = phase & 1;
            for (int left = start; left + 1 < count; left += 2)
            {
                const int right = left + 1;
                if (BetterCandidate(scores[right], order[right],
                                    scores[left], order[left]))
                {
                    SwapCandidate(scores, order, left, right);
                }
            }
        }
    }

    void BitonicSort(int* order, float* scores, int count, int capacity)
    {
        for (int i = count; i < capacity; ++i)
        {
            scores[i] = -__builtin_inff();
            order[i] = 0x7fffffff;
        }

        for (int size = 2; size <= capacity; size <<= 1)
        {
            for (int stride = size >> 1; stride > 0; stride >>= 1)
            {
                for (int block = 0; block < capacity; block += 2 * stride)
                {
                    const bool descending = (block & size) == 0;
                    for (int lane = 0; lane < stride; ++lane)
                    {
                        const int left = block + lane;
                        const int right = left + stride;
                        const bool left_better = BetterCandidate(
                            scores[left], order[left], scores[right], order[right]);
                        if ((descending && !left_better) ||
                            (!descending && left_better))
                        {
                            SwapCandidate(scores, order, left, right);
                        }
                    }
                }
            }
        }
    }

    float IntersectionOverUnion(const float* a, const float* b)
    {
        const float overlap_y = __builtin_fmaxf(
            0.0f, __builtin_fminf(a[2], b[2]) - __builtin_fmaxf(a[0], b[0]));
        const float overlap_x = __builtin_fmaxf(
            0.0f, __builtin_fminf(a[3], b[3]) - __builtin_fmaxf(a[1], b[1]));
        const float area_a = (a[2] - a[0]) * (a[3] - a[1]);
        const float area_b = (b[2] - b[0]) * (b[3] - b[1]);
        if (area_a <= 0.0f || area_b <= 0.0f)
        {
            return 0.0f;
        }
        const float intersection = overlap_y * overlap_x;
        const float union_area = area_a + area_b - intersection;
        return union_area > 0.0f ? intersection / union_area : 0.0f;
    }
}

int ref::NonMaxSuppression(const float* boxes,
                           const float* scores,
                           int num_batches,
                           int num_classes,
                           int spatial_dim,
                           int64_t max_output_boxes_per_class,
                           float iou_threshold,
                           const float* score_threshold,
                           CenterPointBox center_point_box,
                           int64_t* selected_indices,
                           NMSWorkspace workspace,
                           NMSSortAlgorithm sort_algorithm)
{
    if (num_batches <= 0 || num_classes <= 0 || spatial_dim <= 0 ||
        max_output_boxes_per_class <= 0)
    {
        return 0;
    }

    assert(center_point_box == CenterPointBox::Corners);
    assert(boxes != nullptr && scores != nullptr && selected_indices != nullptr);
    assert(workspace.order != nullptr && workspace.scores != nullptr &&
           workspace.suppressed != nullptr);
    assert(workspace.capacity >= NMSWorkspaceCapacity(spatial_dim, sort_algorithm));

    const int64_t class_limit = max_output_boxes_per_class < spatial_dim
        ? max_output_boxes_per_class : spatial_dim;
    int selected_count = 0;

    for (int batch = 0; batch < num_batches; ++batch)
    {
        const float* batch_boxes = boxes + static_cast<int64_t>(batch) * spatial_dim * 4;
        for (int cls = 0; cls < num_classes; ++cls)
        {
            const float* class_scores = scores +
                (static_cast<int64_t>(batch) * num_classes + cls) * spatial_dim;
            const int candidate_count = ApplyScoreSuppression(
                class_scores, spatial_dim, score_threshold,
                workspace.order, workspace.scores);

            if (sort_algorithm == NMSSortAlgorithm::Bitonic)
            {
                BitonicSort(workspace.order, workspace.scores, candidate_count,
                            NMSWorkspaceCapacity(spatial_dim, sort_algorithm));
            }
            else
            {
                OddEvenSort(workspace.order, workspace.scores, candidate_count);
            }

            for (int i = 0; i < candidate_count; ++i)
            {
                workspace.suppressed[i] = 0;
            }

            int64_t selected_for_class = 0;
            for (int i = 0; i < candidate_count && selected_for_class < class_limit; ++i)
            {
                if (workspace.suppressed[i] != 0)
                {
                    continue;
                }

                const int kept_index = workspace.order[i];
                int64_t* row = selected_indices + static_cast<int64_t>(selected_count) * 3;
                row[0] = batch;
                row[1] = cls;
                row[2] = kept_index;
                ++selected_count;
                ++selected_for_class;

                const float* kept = batch_boxes + kept_index * 4;
                for (int j = i + 1; j < candidate_count; ++j)
                {
                    if (workspace.suppressed[j] == 0 &&
                        IntersectionOverUnion(kept,
                            batch_boxes + workspace.order[j] * 4) > iou_threshold)
                    {
                        workspace.suppressed[j] = 1;
                    }
                }
            }
        }
    }
    return selected_count;
}
