"""Prove the golden cases match ONNX Runtime for NonMaxSuppression-11."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import numpy as np
import pytest

from nms_cases import NMS_CASES
from nms_onnx import run_nms


@pytest.mark.parametrize("case", NMS_CASES, ids=lambda case: case["name"])
def test_official_onnx_example(case):
    actual = run_nms(case)
    expected = np.array(case["expected_indices"], dtype=np.int64)

    assert actual.shape == expected.shape
    assert np.array_equal(actual, expected)
