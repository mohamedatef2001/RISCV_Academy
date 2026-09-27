# CMake generated Testfile for 
# Source directory: /home/mohamed/RISCV_Academy/example/source/T2_Sobel
# Build directory: /home/mohamed/RISCV_Academy/build/example/T2_Sobel
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[T2_Sobel_qemu]=] "/usr/bin/qemu-riscv64" "-cpu" "max,vlen=128" "/home/mohamed/RISCV_Academy/build/example/T2_Sobel/T2_Sobel_qemu")
set_tests_properties([=[T2_Sobel_qemu]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/mohamed/RISCV_Academy/cmake/riscv_add_baremetal.cmake;225;add_test;/home/mohamed/RISCV_Academy/example/source/T2_Sobel/CMakeLists.txt;4;riscv_add_qemu_run;/home/mohamed/RISCV_Academy/example/source/T2_Sobel/CMakeLists.txt;0;")
add_test([=[T2_Sobel_gem5]=] "/home/mohamed/GEM5/build/RISCV/gem5.opt" "--remote-gdb-port=0" "-d" "/home/mohamed/RISCV_Academy/build/example/T2_Sobel/T2_Sobel_gem5-m5out" "/home/mohamed/GEM5/configs/example/kmhv3.py" "--raw-cpt" "--generic-rv-cpt=/home/mohamed/RISCV_Academy/build/example/T2_Sobel/T2_Sobel.bin" "--disable-difftest")
set_tests_properties([=[T2_Sobel_gem5]=] PROPERTIES  FAIL_REGULAR_EXPRESSION "Example failed" PASS_REGULAR_EXPRESSION "Wrote T2_Sobel_output.bmp" _BACKTRACE_TRIPLES "/home/mohamed/RISCV_Academy/cmake/riscv_add_baremetal.cmake;164;add_test;/home/mohamed/RISCV_Academy/example/source/T2_Sobel/CMakeLists.txt;12;riscv_add_gem5_run;/home/mohamed/RISCV_Academy/example/source/T2_Sobel/CMakeLists.txt;0;")
