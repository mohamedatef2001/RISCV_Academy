cmake_minimum_required(VERSION 3.22)
project(T1_RGBtoGRY LANGUAGES CXX)

add_library(RgbToGray STATIC source/RgbToGray.cpp)
target_compile_features(RgbToGray PUBLIC cxx_std_20)
target_include_directories(RgbToGray PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source)

add_executable(RgbToGray_example source/RgbToGray_example.cpp)
target_link_libraries(RgbToGray_example PRIVATE RgbToGray)
target_link_options(RgbToGray_example PRIVATE -static)

find_program(QEMU_RISCV64 NAMES qemu-riscv64 qemu-riscv64-static)
if(QEMU_RISCV64)
    add_custom_target(run_qemu
        COMMAND ${QEMU_RISCV64} -cpu max,vlen=128 $<TARGET_FILE:RgbToGray_example>
        DEPENDS RgbToGray_example
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        USES_TERMINAL)
endif()
