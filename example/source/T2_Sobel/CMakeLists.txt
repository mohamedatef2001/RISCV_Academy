cmake_minimum_required(VERSION 3.22)

riscv_add_linux_executable("${CMAKE_CURRENT_SOURCE_DIR}/T2_Sobel.cpp")
riscv_add_qemu_run(T2_Sobel_qemu)

riscv_add_baremetal_image("${CMAKE_CURRENT_SOURCE_DIR}/T2_Sobel.cpp")
target_link_libraries(T2_Sobel_gem5 PRIVATE
    ${CMAKE_PROJECT_NAME}_gem5 riscv_baremetal_runtime)
set_target_properties(T2_Sobel_gem5 PROPERTIES
    RISCV_GEM5_PASS_REGEX "Wrote T2_Sobel_output.bmp"
    RISCV_GEM5_FAIL_REGEX "Example failed")
riscv_add_gem5_run(T2_Sobel_gem5)
