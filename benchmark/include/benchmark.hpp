#pragma once

#include "riscv_port.hpp"  // printf, srand, rand (klib on bare metal)
#include "riscv_cv.hpp"
#include "riscv_timer.hpp"
#include <cstdint>

// Print cycle/instruction counts, per-pixel rates, and speedup for a kernel pair.
inline void PrintTime(const char* name, const int num_pixels,
                      const uint64_t vectorized_cycles, const uint64_t vectorized_instrs,
                      const uint64_t scalar_cycles, const uint64_t scalar_instrs)
{
    printf("%s: num_pixels: %d\n", name, num_pixels);

    const float scalar_pixels_per_cycle =
        static_cast<float>(num_pixels) / static_cast<float>(scalar_cycles);
    const float scalar_cycles_per_pixel =
        static_cast<float>(scalar_cycles) / static_cast<float>(num_pixels);
    const float vectorized_pixels_per_cycle =
        static_cast<float>(num_pixels) / static_cast<float>(vectorized_cycles);
    const float vectorized_cycles_per_pixel =
        static_cast<float>(vectorized_cycles) / static_cast<float>(num_pixels);

    printf("  scalar     : %llu cycles, %llu instructions, "
           "%f cycles/pixel, %f pixels/cycle\n",
           (unsigned long long)scalar_cycles,
           (unsigned long long)scalar_instrs,
           scalar_cycles_per_pixel, scalar_pixels_per_cycle);

    printf("  vectorized : %llu cycles, %llu instructions, "
           "%f cycles/pixel, %f pixels/cycle\n",
           (unsigned long long)vectorized_cycles,
           (unsigned long long)vectorized_instrs,
           vectorized_cycles_per_pixel, vectorized_pixels_per_cycle);

    if (vectorized_cycles > 0)
    {
        // Print speedup as x100 to avoid floating-point printf.
        const float speedup = (float)scalar_cycles / (float)vectorized_cycles;
        printf("  speedup    : %f x\n", speedup);
    }
}
