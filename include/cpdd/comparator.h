#pragma once
#include "bits/stdc++.h"

namespace cpdd {
//*----------- double precision comparision ----------------
//* God made the integers, all else is the work of man.
//* -- Leopold Kronecker
template<typename T>
class Num_Comparator {
   public:
    static inline bool
    Gt( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> ) {
            return a > b;
        } else if ( std::is_floating_point_v<T> ) {
            return a - b > eps;
        } else {
            return a > b;
        }
    }

    static inline bool
    Lt( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return a < b;
        else if ( std::is_floating_point_v<T> )
            return a - b < -eps;
        else
            return a < b;
    }

    static inline bool
    Eq( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return a == b;
        else if ( std::is_floating_point_v<T> )
            return std::abs( a - b ) < eps;
        else
            return a == b;
    }

    static inline bool
    Geq( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return a >= b;
        else if ( std::is_floating_point_v<T> )
            return ~Lt( a, b );
        else
            return a >= b;
    }

    static inline bool
    Leq( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return a <= b;
        else if ( std::is_floating_point_v<T> )
            return ~Gt( a, b );
        else
            return a <= b;
    }

    static inline T
    min( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return std::min( a, b );
        else if ( std::is_floating_point_v<T> )
            return Lt( a, b ) ? a : b;
        else
            return std::min( a, b );
    }

    static inline T
    max( const T a, const T b ) {
        if constexpr ( std::is_integral_v<T> )
            return std::max( a, b );
        else if ( std::is_floating_point_v<T> )
            return Gt( a, b ) ? a : b;
        else
            return std::max( a, b );
    }

   private:
    static constexpr T eps = std::numeric_limits<T>::epsilon();
};

}  // namespace cpdd