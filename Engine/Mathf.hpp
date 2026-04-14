#pragma once

template<class T>
constexpr const T& clamp(T const& v, T const& lo, T const& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}