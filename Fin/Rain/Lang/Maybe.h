#pragma once
//
// file Maybe.h
// author Maximilien M. Cura
//

#include <Fin/Rain/Lang/Util.h>
#include <type_traits>

extern void _Exit(int);

namespace rain::lang {
    template<typename F, typename R>
    constexpr inline bool FunctionReturns = std::is_same_v<std::invoke_result_t<F>, R>;

    template<class T>
    struct Maybe {
        union {
            T inner;
        };
        bool has_value;

        inline constexpr Maybe(T const& t)
            : inner{t},
            has_value {true}
        {}
        inline constexpr Maybe(T&& t)
            : inner{move(t)}
            , has_value{true}
        {}
        inline constexpr Maybe()
            : has_value{false}
        {}

        [[nodiscard]] inline constexpr T unwrap() const {
            if(has_value)
                return inner;
            _Exit(1);
        }
        [[nodiscard]] inline explicit constexpr operator T () const
        {
            return unwrap();
        }

        [[nodiscard]] inline constexpr T unwrap_or(T backup) const {
            if (has_value)
                return inner;
            return backup;
        }
        template<typename F>
            requires lang::FunctionReturns<F,T>
        inline constexpr T unwrap_or(F fn) const {
            if(has_value)
                return inner;
            return fn();
        }

        [[nodiscard]] inline constexpr operator bool () const {
            return has_value;
        }
        [[nodiscard]] inline constexpr bool operator! () const {
            return !has_value;
        }
    };
}
