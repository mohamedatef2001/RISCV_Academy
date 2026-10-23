#include "gtest_lite.hpp"

#include <cstddef>
#include <cstdint>

#include "reference_cv.hpp"
#include "riscv_cv.hpp"
#include "test_utils.hpp"

namespace
{
struct MedianCase
{
    int width;
    int height;
    int kernel_size;
};

struct WorkspaceOwner
{
    WorkspaceOwner(const int width, const int kernel_size)
        : capacity(MedianFilterWorkspaceCapacity(width, kernel_size)),
          data(new std::uint16_t[capacity > 0 ? capacity : 1])
    {
    }

    ~WorkspaceOwner()
    {
        delete[] data;
    }

    MedianFilterWorkspace Get() const
    {
        return {capacity > 0 ? data : nullptr, capacity};
    }

    std::size_t capacity;
    std::uint16_t* data;
};

void FillPattern(Image<std::uint8_t>& image)
{
    for (int y = 0; y < image.Height(); ++y)
    {
        for (int x = 0; x < image.Width(); ++x)
        {
            const int mixed = 37 * x + 53 * y + 11 * x * y + 17;
            image.SetPixel(x, y, static_cast<std::uint8_t>(mixed & 0xff));
        }
    }
}
} // namespace

class MedianFilterTest : public ::testing::TestWithParam<MedianCase>
{
};

TEST_P(MedianFilterTest, VectorizedMatchesScalarReference)
{
    const MedianCase parameters = GetParam();
    printf("[ median case: %dx%d, kernel=%dx%d ]\n",
           parameters.width, parameters.height,
           parameters.kernel_size, parameters.kernel_size);

    Image<std::uint8_t> input(parameters.width, parameters.height);
    Image<std::uint8_t> expected(parameters.width, parameters.height);
    Image<std::uint8_t> actual(parameters.width, parameters.height);
    FillPattern(input);
    WorkspaceOwner workspace(parameters.width, parameters.kernel_size);

    ref::MedianFilter(input, expected, parameters.kernel_size);
    vec::MedianFilter(input, actual, parameters.kernel_size, workspace.Get());
    ExpectImagesEqual(expected, actual);
}

TEST(MedianFilterTest, ReplicatedBorderHasKnownThreeByThreeValues)
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

    vec::MedianFilter(input, output, 3);
    EXPECT_EQ(2, static_cast<int>(output.GetPixel(0, 0)));
    EXPECT_EQ(5, static_cast<int>(output.GetPixel(1, 1)));
    EXPECT_EQ(8, static_cast<int>(output.GetPixel(2, 2)));
}

TEST(MedianFilterTest, SaltAndPepperNoiseIsDeterministic)
{
    constexpr unsigned int kNoisePercentage = 5;
    Image<std::uint8_t> first(31, 17);
    Image<std::uint8_t> second(31, 17);
    FillPattern(first);
    FillPattern(second);
    AddSaltAndPepperNoise(first, kNoisePercentage);
    AddSaltAndPepperNoise(second, kNoisePercentage);
    ExpectImagesEqual(first, second);
}

TEST(MedianFilterTest, ThreeByThreeNetworkMatchesEveryBinaryPatch)
{
    constexpr int kPatchSize = 3;
    constexpr int kPatchPixels = kPatchSize * kPatchSize;
    constexpr int kBinaryPatchCount = 1 << kPatchPixels;
    Image<std::uint8_t> input(kPatchSize, kPatchSize);
    Image<std::uint8_t> expected(kPatchSize, kPatchSize);
    Image<std::uint8_t> actual(kPatchSize, kPatchSize);

    for (int pattern = 0; pattern < kBinaryPatchCount; ++pattern)
    {
        for (int index = 0; index < kPatchPixels; ++index)
        {
            input.SetPixel(index % kPatchSize, index / kPatchSize,
                (pattern & (1 << index)) != 0 ? 255U : 0U);
        }
        ref::MedianFilter(input, expected, kPatchSize);
        vec::MedianFilter(input, actual, kPatchSize);
        ASSERT_EQ(expected.GetPixel(1, 1), actual.GetPixel(1, 1));
    }
}

TEST(MedianFilterTest, AllKernelsMatchOnDuplicateHeavyImpulseNoise)
{
    constexpr int kWidth = 67;
    constexpr int kHeight = 21;
    constexpr unsigned int kNoisePercentage = 20;
    constexpr int kKernelSizes[] = {3, 5, 7, 9, 11};
    Image<std::uint8_t> input(kWidth, kHeight);
    Image<std::uint8_t> expected(kWidth, kHeight);
    Image<std::uint8_t> actual(kWidth, kHeight);

    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            input.SetPixel(x, y, 128U);
        }
    }
    AddSaltAndPepperNoise(input, kNoisePercentage);

    const std::size_t capacity = MedianFilterWorkspaceCapacity(kWidth, 11);
    std::uint16_t* data = new std::uint16_t[capacity];
    const MedianFilterWorkspace workspace = {data, capacity};
    for (const int kernel_size : kKernelSizes)
    {
        ref::MedianFilter(input, expected, kernel_size);
        vec::MedianFilter(input, actual, kernel_size, workspace);
        ExpectImagesEqual(expected, actual);
    }
    delete[] data;
}

INSTANTIATE_TEST_SUITE_P(AllSupportedKernels, MedianFilterTest,
    ::testing::Values(
        MedianCase{1, 1, 3},
        MedianCase{1, 1, 5},
        MedianCase{1, 1, 7},
        MedianCase{1, 1, 9},
        MedianCase{1, 1, 11},
        MedianCase{7, 5, 3},
        MedianCase{15, 9, 5},
        MedianCase{17, 11, 7},
        MedianCase{31, 13, 9},
        MedianCase{33, 15, 11},
        MedianCase{47, 19, 3},
        MedianCase{49, 17, 5},
        MedianCase{51, 15, 7},
        MedianCase{53, 13, 9},
        MedianCase{55, 11, 11})
    );
