#include "NMS.hpp"

#include "riscv_port.hpp"
#include "riscv_wrappers.hpp"

#include <cstddef>

namespace
{
    int ApplyScoreSuppression(const float* class_scores, int spatial_dim,
                              const float* score_threshold, int* order,
                              float* packed_scores)
    {
        int candidate_count = 0;
        int offset = 0;
        while (offset < spatial_dim)
        {
            const size_t vl = rvv::SetVlF32M4(
                static_cast<size_t>(spatial_dim - offset));
            const rvv::F32M4 score = rvv::LoadF32M4(class_scores + offset, vl);
            rvv::U32M4 indices = rvv::IotaU32M4(vl);
            indices = rvv::AddU32M4(indices, static_cast<uint32_t>(offset), vl);

            if (score_threshold == nullptr)
            {
                rvv::StoreF32M4(packed_scores + candidate_count, score, vl);
                rvv::StoreI32M4(order + candidate_count,
                    rvv::BitcastU32ToI32M4(indices), vl);
                candidate_count += static_cast<int>(vl);
            }
            else
            {
                const rvv::MaskF32M4 keep =
                    rvv::MaskF32GreaterEqualScalarM4(score, *score_threshold, vl);
                const size_t kept = rvv::CountTrueF32M4(keep, vl);
                rvv::StoreF32M4(packed_scores + candidate_count,
                    rvv::CompressF32M4(score, keep, vl), kept);
                rvv::StoreI32M4(order + candidate_count,
                    rvv::BitcastU32ToI32M4(
                        rvv::CompressU32M4(indices, keep, vl)), kept);
                candidate_count += static_cast<int>(kept);
            }
            offset += static_cast<int>(vl);
        }
        return candidate_count;
    }

    rvv::MaskF32M4 SwapMask(rvv::F32M4 left_score,
                            rvv::F32M4 right_score,
                            rvv::I32M4 left_order,
                            rvv::I32M4 right_order,
                            bool descending,
                            size_t vl)
    {
        const rvv::MaskF32M4 equal =
            rvv::MaskF32EqualM4(left_score, right_score, vl);
        if (descending)
        {
            const rvv::MaskF32M4 score_swap =
                rvv::MaskF32LessThanM4(left_score, right_score, vl);
            const rvv::MaskF32M4 tie_swap = rvv::AndMaskM4(
                equal, rvv::MaskI32GreaterThanM4(left_order, right_order, vl), vl);
            return rvv::OrMaskM4(score_swap, tie_swap, vl);
        }

        const rvv::MaskF32M4 score_swap =
            rvv::MaskF32GreaterThanM4(left_score, right_score, vl);
        const rvv::MaskF32M4 tie_swap = rvv::AndMaskM4(
            equal, rvv::MaskI32LessThanM4(left_order, right_order, vl), vl);
        return rvv::OrMaskM4(score_swap, tie_swap, vl);
    }

    void CompareSwapStrided(int* order, float* scores, int start, int pair_count)
    {
        constexpr ptrdiff_t stride = 2 * static_cast<ptrdiff_t>(sizeof(float));
        int pair_offset = 0;
        while (pair_offset < pair_count)
        {
            const size_t vl = rvv::SetVlF32M4(
                static_cast<size_t>(pair_count - pair_offset));
            float* left_score_ptr = scores + start + 2 * pair_offset;
            int* left_order_ptr = order + start + 2 * pair_offset;
            const rvv::F32M4 left_score =
                rvv::LoadStridedF32M4(left_score_ptr, stride, vl);
            const rvv::F32M4 right_score =
                rvv::LoadStridedF32M4(left_score_ptr + 1, stride, vl);
            const rvv::I32M4 left_order =
                rvv::LoadStridedI32M4(left_order_ptr, stride, vl);
            const rvv::I32M4 right_order =
                rvv::LoadStridedI32M4(left_order_ptr + 1, stride, vl);
            const rvv::MaskF32M4 swap = SwapMask(
                left_score, right_score, left_order, right_order, true, vl);

            rvv::StoreStridedF32M4(left_score_ptr, stride,
                rvv::MergeF32M4(left_score, right_score, swap, vl), vl);
            rvv::StoreStridedF32M4(left_score_ptr + 1, stride,
                rvv::MergeF32M4(right_score, left_score, swap, vl), vl);
            rvv::StoreStridedI32M4(left_order_ptr, stride,
                rvv::MergeI32M4(left_order, right_order, swap, vl), vl);
            rvv::StoreStridedI32M4(left_order_ptr + 1, stride,
                rvv::MergeI32M4(right_order, left_order, swap, vl), vl);
            pair_offset += static_cast<int>(vl);
        }
    }

    void OddEvenSort(int* order, float* scores, int count)
    {
        for (int phase = 0; phase < count; ++phase)
        {
            const int start = phase & 1;
            CompareSwapStrided(order, scores, start, (count - start) / 2);
        }
    }

    void FillBitonicPadding(int* order, float* scores, int count, int capacity)
    {
        int offset = count;
        while (offset < capacity)
        {
            const size_t vl = rvv::SetVlF32M4(
                static_cast<size_t>(capacity - offset));
            rvv::StoreF32M4(scores + offset,
                rvv::SplatF32M4(-__builtin_inff(), vl), vl);
            rvv::StoreI32M4(order + offset,
                rvv::SplatI32M4(0x7fffffff, vl), vl);
            offset += static_cast<int>(vl);
        }
    }

    void CompareSwapBitonicStage(int* order, float* scores, int capacity,
                                 int size, int stride)
    {
        const int pair_count = capacity / 2;
        int pair_offset = 0;
        while (pair_offset < pair_count)
        {
            const size_t vl = rvv::SetVlF32M4(
                static_cast<size_t>(pair_count - pair_offset));

            // Pair number p maps to:
            //   left  = 2 * floor(p / stride) * stride + p % stride
            //   right = left + stride
            // Since stride is a power of two, masks replace division/modulo.
            rvv::U32M4 pair = rvv::AddU32M4(
                rvv::IotaU32M4(vl), static_cast<uint32_t>(pair_offset), vl);
            const rvv::U32M4 lane = rvv::BitAndU32M4(
                pair, static_cast<uint32_t>(stride - 1), vl);
            rvv::U32M4 block = rvv::BitAndU32M4(
                pair, ~static_cast<uint32_t>(stride - 1), vl);
            block = rvv::ShiftLeftU32M4(block, 1, vl);
            const rvv::U32M4 left_index = rvv::AddU32M4(block, lane, vl);
            const rvv::U32M4 right_index = rvv::AddU32M4(
                left_index, static_cast<uint32_t>(stride), vl);
            const rvv::U32M4 left_bytes =
                rvv::ShiftLeftU32M4(left_index, 2, vl);
            const rvv::U32M4 right_bytes =
                rvv::ShiftLeftU32M4(right_index, 2, vl);

            const rvv::F32M4 left_score =
                rvv::GatherF32M4(scores, left_bytes, vl);
            const rvv::F32M4 right_score =
                rvv::GatherF32M4(scores, right_bytes, vl);
            const rvv::I32M4 left_order =
                rvv::GatherI32M4(order, left_bytes, vl);
            const rvv::I32M4 right_order =
                rvv::GatherI32M4(order, right_bytes, vl);

            const rvv::U32M4 direction = rvv::BitAndU32M4(
                left_index, static_cast<uint32_t>(size), vl);
            const rvv::MaskF32M4 descending =
                rvv::MaskU32EqualScalarM4(direction, 0, vl);
            const rvv::MaskF32M4 ascending =
                rvv::MaskU32NotEqualScalarM4(direction, 0, vl);
            const rvv::MaskF32M4 descending_swap = SwapMask(
                left_score, right_score, left_order, right_order, true, vl);
            const rvv::MaskF32M4 ascending_swap = SwapMask(
                left_score, right_score, left_order, right_order, false, vl);
            const rvv::MaskF32M4 swap = rvv::OrMaskM4(
                rvv::AndMaskM4(descending, descending_swap, vl),
                rvv::AndMaskM4(ascending, ascending_swap, vl), vl);

            rvv::ScatterF32M4(scores, left_bytes,
                rvv::MergeF32M4(left_score, right_score, swap, vl), vl);
            rvv::ScatterF32M4(scores, right_bytes,
                rvv::MergeF32M4(right_score, left_score, swap, vl), vl);
            rvv::ScatterI32M4(order, left_bytes,
                rvv::MergeI32M4(left_order, right_order, swap, vl), vl);
            rvv::ScatterI32M4(order, right_bytes,
                rvv::MergeI32M4(right_order, left_order, swap, vl), vl);
            pair_offset += static_cast<int>(vl);
        }
    }

    void BitonicSort(int* order, float* scores, int count, int capacity)
    {
        FillBitonicPadding(order, scores, count, capacity);
        for (int size = 2; size <= capacity; size <<= 1)
        {
            for (int stride = size >> 1; stride > 0; stride >>= 1)
            {
                CompareSwapBitonicStage(
                    order, scores, capacity, size, stride);
            }
        }
    }

    void SuppressOverlaps(const float* boxes, int* order, uint8_t* suppressed,
                          int begin, int count, const float* kept,
                          float iou_threshold)
    {
        const float kept_height = kept[2] - kept[0];
        const float kept_width = kept[3] - kept[1];
        const float kept_area = kept_height * kept_width;
        if (kept_area <= 0.0f)
        {
            return;
        }

        int offset = begin;
        while (offset < count)
        {
            const size_t vl = rvv::SetVlF32M2(
                static_cast<size_t>(count - offset));
            const rvv::I32M2 indices = rvv::LoadI32M2(order + offset, vl);
            rvv::U32M2 byte_offsets = rvv::ShiftLeftU32M2(
                rvv::BitcastI32ToU32M2(indices), 4, vl);

            const rvv::F32M2 ymin = rvv::GatherF32M2(boxes, byte_offsets, vl);
            const rvv::F32M2 xmin = rvv::GatherF32M2(
                boxes, rvv::AddU32M2(byte_offsets, 4, vl), vl);
            const rvv::F32M2 ymax = rvv::GatherF32M2(
                boxes, rvv::AddU32M2(byte_offsets, 8, vl), vl);
            const rvv::F32M2 xmax = rvv::GatherF32M2(
                boxes, rvv::AddU32M2(byte_offsets, 12, vl), vl);

            const rvv::F32M2 overlap_y = rvv::MaxF32ScalarM2(
                rvv::SubF32M2(rvv::MinF32ScalarM2(ymax, kept[2], vl),
                              rvv::MaxF32ScalarM2(ymin, kept[0], vl), vl),
                0.0f, vl);
            const rvv::F32M2 overlap_x = rvv::MaxF32ScalarM2(
                rvv::SubF32M2(rvv::MinF32ScalarM2(xmax, kept[3], vl),
                              rvv::MaxF32ScalarM2(xmin, kept[1], vl), vl),
                0.0f, vl);
            const rvv::F32M2 intersection =
                rvv::MulF32M2(overlap_y, overlap_x, vl);
            const rvv::F32M2 area = rvv::MulF32M2(
                rvv::SubF32M2(ymax, ymin, vl),
                rvv::SubF32M2(xmax, xmin, vl), vl);
            const rvv::F32M2 union_area = rvv::SubF32M2(
                rvv::AddF32ScalarM2(area, kept_area, vl), intersection, vl);
            const rvv::F32M2 iou =
                rvv::DivF32M2(intersection, union_area, vl);

            rvv::MaskF32M2 kill = rvv::MaskF32GreaterThanScalarM2(
                iou, iou_threshold, vl);
            kill = rvv::AndMaskM2(kill,
                rvv::MaskF32GreaterThanScalarM2(area, 0.0f, vl), vl);
            kill = rvv::AndMaskM2(kill,
                rvv::MaskF32GreaterThanScalarM2(union_area, 0.0f, vl), vl);
            // Re-storing 1 to an already suppressed lane is harmless. Avoiding
            // a load and mask comparison keeps the greedy scan vector-only.
            rvv::StoreU8Mf2Masked(suppressed + offset,
                rvv::SplatU8Mf2(1, vl), kill, vl);
            offset += static_cast<int>(vl);
        }
    }
}

int vec::NonMaxSuppression(const float* boxes,
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

            rvv::FillU8(workspace.suppressed, 0,
                        static_cast<size_t>(candidate_count));
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

                SuppressOverlaps(batch_boxes, workspace.order, workspace.suppressed,
                                 i + 1, candidate_count,
                                 batch_boxes + kept_index * 4, iou_threshold);
            }
        }
    }
    return selected_count;
}
