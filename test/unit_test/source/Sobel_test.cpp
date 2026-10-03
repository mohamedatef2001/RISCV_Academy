#include "gtest_lite.hpp"

#include <tuple>

#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

class SobelTest : public ::testing::TestWithParam<std::tuple<int, int>>
{
};

TEST_P(SobelTest, VectorizedMatchesReference)
{
    const auto [width, height] = GetParam();

    printf("[ %-7s %16d ]\n", "width:", width);
    printf("[ %-7s %16d ]\n", "height:", height);

    Image<std::uint32_t> input(width, height);
    Image<std::uint8_t> output_reference(width, height);
    Image<std::uint8_t> output_vectorized(width, height);

    RandomInt<std::uint32_t> random(0, 255);
    random.ImageRandomInitialize(input);

    constexpr std::uint32_t threshold_squared = 128 * 128;
    ref::Sobel(input, output_reference, threshold_squared);
    vec::Sobel(input, output_vectorized, threshold_squared);

    ExpectImagesEqual(output_reference, output_vectorized);

    for (int x = 0; x < width; ++x)
    {
        EXPECT_EQ(0, output_vectorized.GetPixel(x, 0));
        EXPECT_EQ(0, output_vectorized.GetPixel(x, height - 1));
    }
    for (int y = 0; y < height; ++y)
    {
        EXPECT_EQ(0, output_vectorized.GetPixel(0, y));
        EXPECT_EQ(0, output_vectorized.GetPixel(width - 1, y));
    }
}

TEST(SobelKnownKernel, DetectsVerticalEdgeAtInclusiveThreshold)
{
    Image<std::uint32_t> input(3, 3);
    Image<std::uint8_t> output_reference(3, 3);
    Image<std::uint8_t> output_vectorized(3, 3);

    for (int y = 0; y < 3; ++y)
    {
        input.SetPixel(0, y, 0);
        input.SetPixel(1, y, 0);
        input.SetPixel(2, y, 255);
    }

    constexpr std::uint32_t exact_magnitude_squared = 1020 * 1020;
    ref::Sobel(input, output_reference, exact_magnitude_squared);
    vec::Sobel(input, output_vectorized, exact_magnitude_squared);

    EXPECT_EQ(255, output_reference.GetPixel(1, 1));
    EXPECT_EQ(255, output_vectorized.GetPixel(1, 1));
    ExpectImagesEqual(output_reference, output_vectorized);
}

// For VLEN=128, e32m4 has 16 lanes. These sizes cover short vectors,
// exact-vector interiors, and one- and multi-vector tail handling.
INSTANTIATE_TEST_SUITE_P(ImageSizesAndVectorTails, SobelTest,
    ::testing::Combine(
        ::testing::Values(3, 7, 17, 18, 19, 34, 35),
        ::testing::Values(3, 8, 13)));
