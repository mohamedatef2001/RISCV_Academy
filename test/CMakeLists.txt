cmake_minimum_required(VERSION 3.22)

# Reference implementation library
add_subdirectory(reference)

# Host-side ONNX Runtime oracle. This is a custom target and is not linked into
# the cross-compiled test binaries.
add_subdirectory(GoldenReference)

# Unit tests
add_subdirectory(unit_test)
