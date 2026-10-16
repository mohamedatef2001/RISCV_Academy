#include "gtest_lite.hpp"

#include <cstdint>

#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

namespace
{
struct BoxCase
{
    int width;
    int height;
    int kernel_width;
    int kernel_height;
    int border_type;
    bool normalize;
};

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

void FillSum(Image<std::uint8_t>& image, unsigned int sum)
{
    for (int y = 0; y < image.Height(); ++y)
    {
        for (int x = 0; x < image.Width(); ++x)
        {
            const unsigned int value = sum > 255U ? 255U : sum;
            image.SetPixel(x, y, static_cast<std::uint8_t>(value));
            sum -= value;
        }
    }
}
} // namespace

class BoxFilterTest : public ::testing::TestWithParam<BoxCase>
{
};

TEST_P(BoxFilterTest, VectorizedMatchesScalarOracle)
{
    const BoxCase parameters = GetParam();
    printf("[ %-14s %dx%d, kernel=%dx%d, border=%d, normalize=%d ]\n",
           "box case:", parameters.width, parameters.height,
           parameters.kernel_width, parameters.kernel_height,
           parameters.border_type, parameters.normalize ? 1 : 0);

    Image<std::uint8_t> input(parameters.width, parameters.height);
    Image<std::uint8_t> expected(parameters.width, parameters.height);
    Image<std::uint8_t> actual(parameters.width, parameters.height);
    FillPattern(input);

    const Size ksize(parameters.kernel_width, parameters.kernel_height);
    ref::boxFilter(input, expected, -1, ksize, Point(-1, -1),
                   parameters.normalize, parameters.border_type);
    vec::boxFilter(input, actual, -1, ksize, Point(-1, -1),
                   parameters.normalize, parameters.border_type);
    ExpectImagesEqual(expected, actual);
}

TEST(BoxFilterTest, ConstantAndReplicateBordersHaveExpectedValues)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> constant_output(3, 3);
    Image<std::uint8_t> replicate_output(3, 3);
    std::uint8_t value = 1;
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            input.SetPixel(x, y, value++);
        }
    }

    vec::boxFilter(input, constant_output, CV_8UC1, Size(3, 3),
                   Point(-1, -1), true, BORDER_CONSTANT);
    vec::boxFilter(input, replicate_output, CV_8U, Size(3, 3),
                   Point(-1, -1), true, BORDER_REPLICATE);

    const std::uint8_t expected_constant[9] = {1, 2, 2, 3, 5, 4, 3, 4, 3};
    const std::uint8_t expected_replicate[9] = {2, 3, 4, 4, 5, 6, 6, 7, 8};
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            const int index = 3 * y + x;
            EXPECT_EQ(expected_constant[index],
                      constant_output.GetPixel(x, y));
            EXPECT_EQ(expected_replicate[index],
                      replicate_output.GetPixel(x, y));
        }
    }
}

TEST(BoxFilterTest, DefaultBorderMatchesOpenCvReflect101)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> output(3, 3);
    std::uint8_t value = 1;
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            input.SetPixel(x, y, value++);
        }
    }

    vec::boxFilter(input, output, -1, Size(3, 3));
    EXPECT_EQ(4, output.GetPixel(0, 0)); // round(33 / 9)
}

TEST(BoxFilterTest, NormalizedResultsRoundToNearest)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> output(3, 3);
    FillSum(input, 50U);

    vec::boxFilter(input, output, -1, Size(3, 3), Point(-1, -1), true,
                   BORDER_CONSTANT);
    EXPECT_EQ(6, output.GetPixel(1, 1));
}

#ifndef RISCV_BAREMETAL
TEST(BoxFilterTest, FastThreeByThreeDivisionIsExactForEverySum)
{
    Image<std::uint8_t> input(3, 3);
    Image<std::uint8_t> expected(3, 3);
    Image<std::uint8_t> actual(3, 3);
    for (unsigned int sum = 0; sum <= 9U * 255U; ++sum)
    {
        FillSum(input, sum);
        ref::boxFilter(input, expected, -1, Size(3, 3), Point(-1, -1), true,
                       BORDER_CONSTANT);
        vec::boxFilter(input, actual, -1, Size(3, 3), Point(-1, -1), true,
                       BORDER_CONSTANT);
        ASSERT_EQ((sum + 4U) / 9U, expected.GetPixel(1, 1));
        ASSERT_EQ(expected.GetPixel(1, 1), actual.GetPixel(1, 1));
    }
}
#endif

TEST(BoxFilterTest, UnnormalizedOutputSaturatesToEightBits)
{
    Image<std::uint8_t> input(5, 5);
    Image<std::uint8_t> output(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            input.SetPixel(x, y, 255);
        }
    }

    vec::boxFilter(input, output, -1, Size(5, 5), Point(-1, -1), false,
                   BORDER_REPLICATE);
    EXPECT_EQ(255, output.GetPixel(0, 0));
    EXPECT_EQ(255, output.GetPixel(2, 2));
    EXPECT_EQ(255, output.GetPixel(4, 4));
}

TEST(BoxFilterTest, ExplicitAnchorMatchesScalarOracle)
{
    Image<std::uint8_t> input(37, 19);
    Image<std::uint8_t> expected(37, 19);
    Image<std::uint8_t> actual(37, 19);
    FillPattern(input);

    ref::boxFilter(input, expected, -1, Size(5, 7), Point(0, 6), true,
                   BORDER_REPLICATE);
    vec::boxFilter(input, actual, -1, Size(5, 7), Point(0, 6), true,
                   BORDER_REPLICATE);
    ExpectImagesEqual(expected, actual);
}

TEST(BoxFilterTest, NonCenteredThreeByThreeAnchorUsesGenericSemantics)
{
    Image<std::uint8_t> input(19, 11);
    Image<std::uint8_t> expected(19, 11);
    Image<std::uint8_t> actual(19, 11);
    FillPattern(input);

    ref::boxFilter(input, expected, -1, Size(3, 3), Point(0, 2), true,
                   BORDER_CONSTANT);
    vec::boxFilter(input, actual, -1, Size(3, 3), Point(0, 2), true,
                   BORDER_CONSTANT);
    ExpectImagesEqual(expected, actual);
}

// VLEN=128 gives 32 u32m8 lanes. These cases exercise tiny images, scalar-only
// border handling, exact/tail vector chunks, generic odd kernels through 11x11,
// the wide-kernel fallback, rectangular kernels, both border modes, and
// normalized/unnormalized output.
INSTANTIATE_TEST_SUITE_P(BoxConfigurations, BoxFilterTest,
    ::testing::Values(
        BoxCase{1, 1, 1, 1, BORDER_CONSTANT, true},
        BoxCase{1, 7, 3, 5, BORDER_DEFAULT, true},
        BoxCase{2, 4, 3, 3, BORDER_CONSTANT, true},
        BoxCase{7, 5, 5, 5, BORDER_REPLICATE, true},
        BoxCase{31, 9, 3, 3, BORDER_CONSTANT, true},
        BoxCase{64, 9, 3, 3, BORDER_REPLICATE, true},
        BoxCase{63, 9, 3, 3, BORDER_CONSTANT, false},
        BoxCase{65, 13, 5, 5, BORDER_CONSTANT, true},
        BoxCase{67, 15, 7, 7, BORDER_REPLICATE, true},
        BoxCase{71, 17, 9, 9, BORDER_CONSTANT, true},
        BoxCase{73, 19, 11, 11, BORDER_REPLICATE, true},
        BoxCase{79, 21, 5, 11, BORDER_CONSTANT, false},
        BoxCase{97, 9, 33, 3, BORDER_REPLICATE, true})
    );
