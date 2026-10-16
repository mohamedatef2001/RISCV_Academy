#pragma once

#include "image.hpp"

#include <cstdint>

// Small OpenCV-compatible value types/constants used by image-processing APIs.
// This project currently supports only single-channel 8-bit images, so CV_8U
// and CV_8UC1 intentionally have the same value.
struct Size
{
    constexpr Size(const int width_value = 0, const int height_value = 0)
        : width(width_value), height(height_value)
    {
    }

    int width;
    int height;
};

struct Point
{
    constexpr Point(const int x_value = 0, const int y_value = 0)
        : x(x_value), y(y_value)
    {
    }

    int x;
    int y;
};

inline constexpr int CV_8U = 0;
inline constexpr int CV_8UC1 = CV_8U;

enum BorderType : int
{
    BORDER_CONSTANT = 0,
    BORDER_REPLICATE = 1,
    BORDER_REFLECT_101 = 4,
    BORDER_DEFAULT = BORDER_REFLECT_101,
};

enum class OverFlowPolicy
{
    SATURATE,
    WRAP,
};
