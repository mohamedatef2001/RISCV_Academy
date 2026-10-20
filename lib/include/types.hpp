#pragma once

#include "image.hpp"

#include <cstdint>

enum class OverFlowPolicy
{
    SATURATE,
    WRAP,
};

enum class CenterPointBox
{
    Corners = 0,
    Center = 1,
};

enum class NMSSortAlgorithm
{
    OddEven,
    Bitonic,
};

// Caller-owned temporary storage for NonMaxSuppression. Odd-even sorting
// needs spatial_dim elements. Bitonic sorting needs the next power of two so
// it can pad the sorting network with sentinel candidates.
struct NMSWorkspace
{
    int* order;
    float* scores;
    uint8_t* suppressed;
    int capacity;
};

inline int NMSWorkspaceCapacity(int spatial_dim, NMSSortAlgorithm algorithm)
{
    if (spatial_dim <= 0 || algorithm == NMSSortAlgorithm::OddEven)
    {
        return spatial_dim > 0 ? spatial_dim : 0;
    }

    int capacity = 1;
    while (capacity < spatial_dim)
    {
        capacity <<= 1;
    }
    return capacity;
}
