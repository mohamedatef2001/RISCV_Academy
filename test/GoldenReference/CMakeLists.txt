cmake_minimum_required(VERSION 3.22)

# Host-side ONNX golden reference. This target is not cross-compiled; it runs
# the official NonMaxSuppression examples against ONNX Runtime on the host.
set(RISCV_GOLDEN_PYTHON "" CACHE FILEPATH
    "Host Python executable with pytest, ONNX, and ONNX Runtime installed")
if(NOT RISCV_GOLDEN_PYTHON)
  find_program(RISCV_GOLDEN_PYTHON_DEFAULT NAMES python3 python)
  if(RISCV_GOLDEN_PYTHON_DEFAULT)
    set(RISCV_GOLDEN_PYTHON "${RISCV_GOLDEN_PYTHON_DEFAULT}" CACHE FILEPATH
        "Host Python executable with pytest, ONNX, and ONNX Runtime installed" FORCE)
  endif()
endif()

if(RISCV_GOLDEN_PYTHON)
  add_custom_target(run_nms_golden
    COMMAND ${RISCV_GOLDEN_PYTHON} -m pytest test_nms_golden.py -q
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running NonMaxSuppression ONNX golden reference tests"
    USES_TERMINAL)
else()
  message(STATUS "python3/python not found; run_nms_golden target is disabled")
endif()
