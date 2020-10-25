#pragma once
//
// file Util.h
// author Maximilien M. Cura
//

namespace rain::math {
    template <typename T>
    constexpr inline bool is_power_of_2 (T x) {
        return (x & (x - 1)) == 0;
    }

    template<typename T>
    constexpr inline T max(T a, T b)
    {
        return a > b ? a : b;
    }
    template<typename T>
    constexpr inline T min(T a, T b)
    {
        return a < b ? a : b;
    }
    template<typename T>
    constexpr inline T round_up(T a, T b)
    {
        T intermediate = (a + (b - 1));
        return intermediate - (intermediate % b);
    }
}