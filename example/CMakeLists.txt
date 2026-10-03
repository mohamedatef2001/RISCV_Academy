cmake_minimum_required(VERSION 3.22)

function(add_riscv_example EXAMPLE_SOURCE)
    get_filename_component(EXAMPLE_SOURCE_ABS
      "${EXAMPLE_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(EXAMPLE_NAME "${EXAMPLE_SOURCE_ABS}" NAME_WE)

    riscv_add_baremetal_image("${EXAMPLE_SOURCE_ABS}")
    target_include_directories(${EXAMPLE_NAME}_gem5 PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/images)
    target_link_libraries(${EXAMPLE_NAME}_gem5 PRIVATE ${CMAKE_PROJECT_NAME}_gem5 reference_gem5 riscv_baremetal_runtime)

    set_target_properties(${EXAMPLE_NAME}_gem5 PROPERTIES
      RISCV_GEM5_PASS_REGEX "Example passed\\."
      RISCV_GEM5_FAIL_REGEX "Example failed")
    riscv_add_gem5_run(${EXAMPLE_NAME}_gem5)

    riscv_add_linux_executable("${EXAMPLE_SOURCE_ABS}")
    target_include_directories(${EXAMPLE_NAME}_qemu PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${EXAMPLE_NAME}_qemu PRIVATE ${CMAKE_PROJECT_NAME}_qemu reference_qemu)

    riscv_add_qemu_run(${EXAMPLE_NAME}_qemu)
endfunction()

file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS source/*.cpp)
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    add_riscv_example("${EXAMPLE_SOURCE}")
endforeach()
