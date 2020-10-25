#pragma once
//
// file Integers.h
// author Maximilien M. Cura
//

namespace rain::math {
    typedef unsigned char u8_t;
    typedef unsigned short u16_t;
    typedef unsigned int u32_t;
    typedef unsigned long u64_t;
    typedef unsigned __int128 u128_t;
    typedef unsigned long usize_t;
    typedef unsigned long uptr_t;
    typedef signed char i8_t;
    typedef signed short i16_t;
    typedef signed int i32_t;
    typedef signed long i64_t;
    typedef signed __int128 i128_t;
    typedef signed long isize_t;
    typedef signed long iptr_t;
}

namespace rain {
    using math::u8_t;
    using math::u16_t;
    using math::u32_t;
    using math::u64_t;
    using math::u128_t;
    using math::usize_t;
    using math::uptr_t;
    using math::i8_t;
    using math::i16_t;
    using math::i32_t;
    using math::i64_t;
    using math::i128_t;
    using math::isize_t;
    using math::iptr_t;
}