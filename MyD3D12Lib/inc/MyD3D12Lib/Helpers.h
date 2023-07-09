#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

#include <stdexcept>

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

template<class T>
constexpr const T& clamp(const T& val, const T& min, const T& max) {
    return val < min ? min : val > max ? max : val;
}

inline float randFloat() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

inline float randFloat(float a, float b) {
    return a + (b - a) * randFloat();
}