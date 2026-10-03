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

// With VLEN=128 and LMUL=2, the current simulator handles 32 uint8_t pixels
// per iteration. The implementation still uses dynamic vsetvl for other VLENs.
INSTANTIATE_TEST_SUITE_P(ImageSizes, RgbToGrayTest,
    ::testing::Combine(
        ::testing::Values(
            1, 7,        // less than one vector
            32,          // exact one vector
            33, 47, 63,  // one vector plus a tail
            64,          // multiple exact vectors
            65),         // multiple vectors plus a tail
        ::testing::Values(
            1,           // single row
            3,           // odd height
            10))         // even height
    );
