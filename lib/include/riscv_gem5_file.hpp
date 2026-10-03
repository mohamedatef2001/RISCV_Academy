#pragma once

#include <cstdint>

// Generic bare-metal bridge to gem5's host-side m5_write_file operation.
// This is deliberately independent of Sobel and of any particular file
// format so another algorithm can reuse it unchanged.
namespace gem5
{
#ifdef RISCV_BAREMETAL
    inline std::uint64_t WriteFile(const void* data,
                                   const std::uint64_t byte_count,
                                   const std::uint64_t file_offset,
                                   const char* filename)
    {
        register std::uintptr_t a0 asm("a0") =
            reinterpret_cast<std::uintptr_t>(data);
        register std::uintptr_t a1 asm("a1") = byte_count;
        register std::uintptr_t a2 asm("a2") = file_offset;
        register std::uintptr_t a3 asm("a3") =
            reinterpret_cast<std::uintptr_t>(filename);

        // gem5 reads simulated memory synchronously. Drain guest stores before
        // asking the simulator to copy a newly calculated buffer.
        asm volatile("fence rw, rw" ::: "memory");
        asm volatile(".4byte 0x9e00007b"
                     : "+r"(a0)
                     : "r"(a1), "r"(a2), "r"(a3)
                     : "memory");
        return a0;
    }
#endif
}
