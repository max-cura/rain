#pragma once
//
// file Util.h
// author Maximilien M. Cura
//

namespace rain::lang {
    template<typename T>
    struct RemoveReference {
        typedef T Type;
    };
    template<typename T>
    struct RemoveReference<T&> {
        typedef T Type;
    };
    template<typename T>
    struct RemoveReference<T&&> {
        typedef T Type;
    };
    template<typename T>
    using RemoveReferenceType = typename RemoveReference<T>::Type;

    template<typename T>
    constexpr RemoveReferenceType<T>&& move(T && t) {
        return t;
    }
}
