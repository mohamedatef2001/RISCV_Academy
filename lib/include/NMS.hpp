#pragma once

#include "types.hpp"

#include <cstdint>

namespace vec
{
    // Vectorized ONNX-style NonMaxSuppression (RVV). boxes is [num_batches, spatial_dim, 4]
    // float32 in corner format [y1, x1, y2, x2] with y1 <= y2 and x1 <= x2.
    // scores is [num_batches, num_classes, spatial_dim] float32, and
    // selected_indices is a caller-owned row-major [*, 3] int64 buffer with
    // capacity num_batches * num_classes * max_output_boxes_per_class rows.
    // center_point_box must be CenterPointBox::Corners (0); other values assert.
    // For each batch and class: drop scores below score_threshold (if set),
    // sort remaining boxes by score, then greedily keep boxes and suppress
    // later ones whose IoU exceeds iou_threshold, up to
    // max_output_boxes_per_class. Returns the number of selected rows.
    // workspace is caller-owned and must have at least
    // NMSWorkspaceCapacity(spatial_dim, sort_algorithm) elements in every
    // array. The kernel performs no dynamic allocation.
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
