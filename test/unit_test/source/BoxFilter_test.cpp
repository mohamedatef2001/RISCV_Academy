#include "gtest_lite.hpp"

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

namespace
{
void FillPattern(Image<std::uint8_t>& image)
{
    for (int y = 0; y < image.Height(); ++y)
    {
        for (int x = 0; x < image.Width(); ++x)
        {
            image.SetPixel(
                x, y,
                static_cast<std::uint8_t>((37 * x + 53 * y + 11 * x * y) & 0xff));
        }
    }
}

void FillNinePixelSum(Image<std::uint8_t>& image, unsigned int sum)
{
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            const unsigned int value = sum > 255U ? 255U : sum;
            image.SetPixel(x, y, static_cast<std::uint8_t>(value));
            sum -= value;
        }
    }
}
} // namespace

class BoxFilterTest : public ::testing::TestWithParam<std::tuple<int, int>>
{
};

TEST_P(BoxFilterTest, VectorizedMatchesReference)
{
    const auto [width, height] = GetParam();
    printf("[ %-7s %16d ]\n", "width:", width);
    printf("[ %-7s %16d ]\n", "height:", height);

    Image<std::uint8_t> input(width, height);
    Image<std::uint8_t> expected(width, height);
    Image<std::uint8_t> actual(width, height);
    FillPattern(input);

    ref::BoxFilter(input, expected);
    vec::BoxFilter(input, actual);
    ExpectImagesEqual(expected, actual);
}

TEST(BoxFilterTest, UsesFloorDivisionAndCopiesBorder)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> expected(3, 3);
    Image<std::uint8_t> actual(3, 3);

    const std::uint8_t values[9] = {1, 2, 3, 4, 14, 6, 7, 8, 9};
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            input.SetPixel(x, y, values[3 * y + x]);
        }
    }

    ref::BoxFilter(input, expected);
    vec::BoxFilter(input, actual);

    // Sum is 54: floor(54 / 9) = 6. Every non-center pixel is a border.
    EXPECT_EQ(6, expected.GetPixel(1, 1));
    EXPECT_EQ(6, actual.GetPixel(1, 1));
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            if (x != 1 || y != 1)
            {
                EXPECT_EQ(input.GetPixel(x, y), actual.GetPixel(x, y));
            }
        }
    }
}

TEST(BoxFilterTest, MaximumWindowDoesNotOverflow)
{
    Image<std::uint8_t> input(5, 5);
    Image<std::uint8_t> expected(5, 5);
    Image<std::uint8_t> actual(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            input.SetPixel(x, y, 255);
        }
    }

    ref::BoxFilter(input, expected);
    vec::BoxFilter(input, actual);
    ExpectImagesEqual(expected, actual);
    EXPECT_EQ(255, actual.GetPixel(2, 2));
}

TEST(BoxFilterTest, SupportsInPlaceOperation)
{
    Image<std::uint8_t> input(67, 9);
    Image<std::uint8_t> expected(67, 9);
    Image<std::uint8_t> vector_in_place(67, 9);
    Image<std::uint8_t> scalar_in_place(67, 9);
    FillPattern(input);

    for (int y = 0; y < input.Height(); ++y)
    {
        for (int x = 0; x < input.Width(); ++x)
        {
            const std::uint8_t value = input.GetPixel(x, y);
            vector_in_place.SetPixel(x, y, value);
            scalar_in_place.SetPixel(x, y, value);
        }
    }

    ref::BoxFilter(input, expected);
    vec::BoxFilter(vector_in_place, vector_in_place);
    ref::BoxFilter(scalar_in_place, scalar_in_place);
    ExpectImagesEqual(expected, vector_in_place);
    ExpectImagesEqual(expected, scalar_in_place);
}

#ifndef RISCV_BAREMETAL
TEST(BoxFilterTest, MagicMultiplyMatchesDivisionForEveryPossibleSum)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> actual(3, 3);

    for (unsigned int sum = 0; sum <= 9U * 255U; ++sum)
    {
        FillNinePixelSum(input, sum);
        vec::BoxFilter(input, actual);
        ASSERT_EQ(sum / 9U, actual.GetPixel(1, 1));
    }
}
#endif

// On VLEN=128, e16m8 has 64 lanes and each chunk reserves two look-ahead
// lanes, so 62 interior outputs fit in a full chunk. These sizes cover exact
// chunks, tails, multiple chunks, and the width/height small-image paths.
INSTANTIATE_TEST_SUITE_P(ImageSizes, BoxFilterTest,
    ::testing::Combine(
        ::testing::Values(1, 2, 3, 4, 63, 64, 65, 66, 127, 128, 129),
        ::testing::Values(1, 2, 3, 9))
    );
