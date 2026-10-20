"""ONNX NonMaxSuppression-11 examples used by host pytest and C++ fixtures.

Only cases the educational kernel supports: corner boxes [y1, x1, y2, x2]
with y1 <= y2 and x1 <= x2. The official ONNX examples `flipped_coordinates`
(unordered corners) and `center_point_box_format` are omitted.
"""

BOXES_COMMON = [
    [
        [0.0, 0.0, 1.0, 1.0],
        [0.0, 0.1, 1.0, 1.1],
        [0.0, -0.1, 1.0, 0.9],
        [0.0, 10.0, 1.0, 11.0],
        [0.0, 10.1, 1.0, 11.1],
        [0.0, 100.0, 1.0, 101.0],
    ]
]

SCORES_COMMON = [[[0.9, 0.75, 0.6, 0.95, 0.5, 0.3]]]

NMS_CASES = [
    {
        "name": "suppress_by_IOU",
        "boxes": BOXES_COMMON,
        "scores": SCORES_COMMON,
        "max_output_boxes_per_class": 3,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 3], [0, 0, 0], [0, 0, 5]],
    },
    {
        "name": "suppress_by_IOU_and_scores",
        "boxes": BOXES_COMMON,
        "scores": SCORES_COMMON,
        "max_output_boxes_per_class": 3,
        "iou_threshold": 0.5,
        "score_threshold": 0.4,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 3], [0, 0, 0]],
    },
    {
        "name": "limit_output_size",
        "boxes": BOXES_COMMON,
        "scores": SCORES_COMMON,
        "max_output_boxes_per_class": 2,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 3], [0, 0, 0]],
    },
    {
        "name": "single_box",
        "boxes": [[[0.0, 0.0, 1.0, 1.0]]],
        "scores": [[[0.9]]],
        "max_output_boxes_per_class": 3,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 0]],
    },
    {
        "name": "identical_boxes",
        "boxes": [[[0.0, 0.0, 1.0, 1.0]] * 10],
        "scores": [[[0.9] * 10]],
        "max_output_boxes_per_class": 3,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 0]],
    },
    {
        "name": "two_classes",
        "boxes": BOXES_COMMON,
        "scores": [[SCORES_COMMON[0][0], SCORES_COMMON[0][0]]],
        "max_output_boxes_per_class": 2,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 3], [0, 0, 0], [0, 1, 3], [0, 1, 0]],
    },
    {
        "name": "two_batches",
        "boxes": [BOXES_COMMON[0], BOXES_COMMON[0]],
        "scores": [SCORES_COMMON[0], SCORES_COMMON[0]],
        "max_output_boxes_per_class": 2,
        "iou_threshold": 0.5,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 3], [0, 0, 0], [1, 0, 3], [1, 0, 0]],
    },
    {
        "name": "iou_threshold_boundary",
        "boxes": [
            [
                [0.0, 0.0, 1.0, 1.0],
                [0.5, 0.5, 1.5, 1.5],
            ]
        ],
        "scores": [[[0.9, 0.8]]],
        "max_output_boxes_per_class": 3,
        "iou_threshold": 0.25 / 1.75,
        "score_threshold": 0.0,
        "center_point_box": 0,
        "expected_indices": [[0, 0, 0], [0, 0, 1]],
    },
]
