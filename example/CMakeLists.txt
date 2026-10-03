cmake_minimum_required(VERSION 3.22)

file(GLOB_RECURSE EXAMPLE_SOURCES source/*.cpp)

foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)

    # RgbToGray_example uses hosted PPM/PGM file I/O and is qemu-only.
    if(EXAMPLE_NAME STREQUAL "RgbToGray_example")
        riscv_add_linux_executable(${EXAMPLE_SOURCE})
        target_link_libraries(${EXAMPLE_NAME}_qemu PRIVATE ${CMAKE_PROJECT_NAME}_qemu reference_qemu)
        riscv_add_qemu_run(${EXAMPLE_NAME}_qemu)
        continue()
    endif()

    riscv_add_baremetal_image(${EXAMPLE_SOURCE})
    target_include_directories(${EXAMPLE_NAME}_gem5 PRIVATE ${CMAKE_SOURCE_DIR}/images)
    target_link_libraries(${EXAMPLE_NAME}_gem5 PRIVATE ${CMAKE_PROJECT_NAME}_gem5 reference_gem5 riscv_baremetal_runtime)

    set_target_properties(${EXAMPLE_NAME}_gem5 PROPERTIES
      RISCV_GEM5_PASS_REGEX "Example passed\\."
      RISCV_GEM5_FAIL_REGEX "Example failed")
    riscv_add_gem5_run(${EXAMPLE_NAME}_gem5)

    riscv_add_linux_executable(${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME}_qemu PRIVATE ${CMAKE_PROJECT_NAME}_qemu reference_qemu)

    riscv_add_qemu_run(${EXAMPLE_NAME}_qemu)
endforeach()
