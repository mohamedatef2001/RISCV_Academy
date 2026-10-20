#pragma once

// gtest-lite assertion helpers for image comparisons. Shared non-test
// utilities (FillDeterministic, CheckCorrectness, RandomInt) live in the
// library's riscv_utils.hpp.

#include "gtest_lite.hpp"
#include "riscv_utils.hpp"

#include <cstdint>

template <typename T>
void ExpectImagesEqual(const Image<T>& expected, const Image<T>& actual)
{
    EXPECT_TRUE(CheckCorrectness(expected, actual));
}

inline bool CheckSelectedIndicesEqual(const int64_t* expected, const int64_t* actual,
                                      int expected_count, int actual_count)
{
    if (expected_count != actual_count)
    {
        printf("Selected index count mismatch: expected = %d, actual = %d\n",
               expected_count, actual_count);
        return false;
    }
    for (int i = 0; i < expected_count * 3; ++i)
    {
        if (expected[i] != actual[i])
        {
            printf("Selected index mismatch at element %d: expected = %lld, actual = %lld\n",
                   i, static_cast<long long>(expected[i]),
                   static_cast<long long>(actual[i]));
            return false;
        }
    }
    return true;
}

inline void ExpectSelectedIndicesEqual(const int64_t* expected, const int64_t* actual,
                                       int expected_count, int actual_count)
{
    EXPECT_TRUE(CheckSelectedIndicesEqual(expected, actual,
                                          expected_count, actual_count));
}
