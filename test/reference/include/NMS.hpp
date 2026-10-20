#pragma once

#include "types.hpp"

#include <cstdint>

namespace ref
{
    // Scalar correctness oracle. Same caller-owned workspace contract and
    // selectable stable sort as vec::NonMaxSuppression. See lib/include/NMS.hpp.
    int NonMaxSuppression(const float* boxes,
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
                          NMSSortAlgorithm sort_algorithm = NMSSortAlgorithm::OddEven);
}
