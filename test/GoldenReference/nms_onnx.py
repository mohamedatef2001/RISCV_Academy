"""Run one ONNX NonMaxSuppression node with ONNX Runtime."""

import numpy as np
import onnx
import onnxruntime as ort


def run_nms(case):
    boxes = np.array(case["boxes"], dtype=np.float32)
    scores = np.array(case["scores"], dtype=np.float32)
    max_output_boxes_per_class = np.array(
        [case["max_output_boxes_per_class"]], dtype=np.int64
    )
    iou_threshold = np.array([case["iou_threshold"]], dtype=np.float32)

    input_names = ["boxes", "scores", "max_output_boxes_per_class", "iou_threshold"]
    input_values = [boxes, scores, max_output_boxes_per_class, iou_threshold]
    input_types = [
        onnx.TensorProto.FLOAT,
        onnx.TensorProto.FLOAT,
        onnx.TensorProto.INT64,
        onnx.TensorProto.FLOAT,
    ]

    if case.get("score_threshold") is not None:
        input_names.append("score_threshold")
        input_values.append(np.array([case["score_threshold"]], dtype=np.float32))
        input_types.append(onnx.TensorProto.FLOAT)

    node = onnx.helper.make_node(
        "NonMaxSuppression",
        inputs=input_names,
        outputs=["selected_indices"],
        center_point_box=int(case.get("center_point_box", 0)),
    )

    inputs_info = [
        onnx.helper.make_tensor_value_info(name, dtype, list(value.shape))
        for name, dtype, value in zip(input_names, input_types, input_values)
    ]
    output_info = onnx.helper.make_tensor_value_info(
        "selected_indices", onnx.TensorProto.INT64, [None, 3]
    )

    graph = onnx.helper.make_graph([node], "nms_golden", inputs_info, [output_info])
    model = onnx.helper.make_model(
        graph, opset_imports=[onnx.helper.make_opsetid("", 11)]
    )
    model.ir_version = 8
    onnx.checker.check_model(model)

    session = ort.InferenceSession(
        model.SerializeToString(), providers=["CPUExecutionProvider"]
    )
    return session.run(
        ["selected_indices"],
        {name: value for name, value in zip(input_names, input_values)},
    )[0]
