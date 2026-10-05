#include "gtest_lite.hpp"

#include <tuple>

#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

class RgbToGrayTest : public ::testing::TestWithParam<std::tuple<int, int>>
{
};

TEST_P(RgbToGrayTest, VectorizedMatchesReference)
{
    const auto [width, height] = GetParam();

    printf("[ %-7s %16d ]\n", "width:", width);
    printf("[ %-7s %16d ]\n", "height:", height);

    Image<std::uint8_t, ImageType::RGB> input(width, height);
    Image<std::uint8_t> output_reference(width, height);
    Image<std::uint8_t> output_vectorized(width, height);

    RandomInt<std::uint8_t> random(0, 255);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int channel = 0; channel < 3; ++channel)
            {
                input.SetPixel(x, y, random.get(), channel);
            }
        }
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    ref::RgbToGray(input.GetPtr(0, 0), output_reference.GetPtr(0, 0), pixel_count);
    vec::RgbToGray(input.GetPtr(0, 0), output_vectorized.GetPtr(0, 0), pixel_count);

    ExpectImagesEqual(output_reference, output_vectorized);
}

TEST(RgbToGrayTest, UsesRequestedWeightsAndNearestRounding)
{
    const std::uint8_t rgb[] = {
        0, 0, 0,
        255, 255, 255,
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        128, 128, 128,
        0, 1, 0,
    };
    const std::uint8_t expected[] = {0, 255, 76, 150, 29, 128, 1};
    std::uint8_t scalar[7] = {};
    std::uint8_t vectorized[7] = {};

    ref::RgbToGray(rgb, scalar, 7);
    vec::RgbToGray(rgb, vectorized, 7);

    for (std::size_t pixel = 0; pixel < 7; ++pixel)
    {
        EXPECT_EQ(expected[pixel], scalar[pixel]);
        EXPECT_EQ(expected[pixel], vectorized[pixel]);
    }
}

TEST(RgbToGrayTest, ZeroPixelsLeavesOutputUntouched)
{
    const std::uint8_t rgb[] = {255, 255, 255};
    std::uint8_t scalar = 42;
    std::uint8_t vectorized = 42;
    ref::RgbToGray(rgb, &scalar, 0);
    vec::RgbToGray(rgb, &vectorized, 0);
    EXPECT_EQ(42, scalar);
    EXPECT_EQ(42, vectorized);
}

// Exhaustive coverage is inexpensive on QEMU, but prohibitively slow on gem5.
// Use the original decimal formula as an independent oracle for both Q24 paths.
#ifndef RISCV_BAREMETAL
TEST(RgbToGrayTest, AllRgbValuesMatchDecimalFormula)
{
    std::uint8_t rgb[3 * 256];
    std::uint8_t scalar[256];
    std::uint8_t vectorized[256];
    for (unsigned int red = 0; red < 256; ++red)
    {
        for (unsigned int green = 0; green < 256; ++green)
        {
            for (unsigned int blue = 0; blue < 256; ++blue)
            {
                rgb[3 * blue] = red;
                rgb[3 * blue + 1] = green;
                rgb[3 * blue + 2] = blue;
            }
            ref::RgbToGray(rgb, scalar, 256);
            vec::RgbToGray(rgb, vectorized, 256);
            for (unsigned int blue = 0; blue < 256; ++blue)
            {
                const unsigned int expected =
                    (299 * red + 587 * green + 114 * blue + 500) / 1000;
                ASSERT_EQ(expected, scalar[blue]);
                ASSERT_EQ(expected, vectorized[blue]);
            }
        }
    }
}
#endif

// Exercise full blocks and tails at several VLENs. Keep the Cartesian product
// small enough for the shared gtest-lite registry's 128 parameterized names.
INSTANTIATE_TEST_SUITE_P(ImageSizes, RgbToGrayTest,
    ::testing::Combine(
        ::testing::Values(
            1, 3, 4, 5, 7, 15, 16, 17,
            31, 32, 33, 47, 63, 64, 65,
            127, 128, 129, 255, 256, 257),
        ::testing::Values(
            1,           // single row
            3,           // odd height
            10))         // even height
    );
