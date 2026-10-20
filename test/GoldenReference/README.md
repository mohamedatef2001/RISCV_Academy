# NonMaxSuppression golden reference

Host-side ONNX golden cases for `ref::NonMaxSuppression` and
`vec::NonMaxSuppression`. These cases are the independent source of truth for
both selectable sorting paths.

## Setup

```bash
python3 -m venv .venv-nms
.venv-nms/bin/pip install -r test/GoldenReference/requirements.txt
```

## Prove the cases against ONNX Runtime

```bash
.venv-nms/bin/python -m pytest test/GoldenReference/test_nms_golden.py -q
```

or configure the RISC-V build with that host Python and use its custom target:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64_xs_toolchain.cmake \
  -DRISCV_GOLDEN_PYTHON="$PWD/.venv-nms/bin/python"
cmake --build build --target run_nms_golden
```

## C++ fixtures used by the RISC-V tests

```bash
.venv-nms/bin/python test/GoldenReference/generate_cpp_fixtures.py
```

This rewrites `test/unit_test/include/nms_onnx_cases.hpp` from `nms_cases.py`
(corners-only ONNX examples with `y1 <= y2` and `x1 <= x2`). Run it again only
after you change `nms_cases.py`.
