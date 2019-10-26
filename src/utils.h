#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <limits>

//#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <string>

#define FLOATEPSILON 0.0001f

template<typename Val>
struct Epsilon
{
    static Val epsilon() { return std::numeric_limits<Val>::epsilon(); }
};

template<>
struct Epsilon<float>
{
    static float epsilon() { return FLOATEPSILON; }
};

template<typename ValType>
bool IsNear(ValType a, ValType b)
{
    return glm::abs(a - b) < Epsilon<ValType>::epsilon();
}

template<typename Real>
Real RoundEps(Real val)
{
    assert(Epsilon<Real>::epsilon() > 0);
    return glm::floor(val / Epsilon<Real>::epsilon() + 0.5) * Epsilon<Real>::epsilon();
}

template<typename valType>
bool VecEqual(glm::tvec3<valType> const & a, glm::tvec3<valType> const & b)
{
    return IsNear(a.x, b.x) && IsNear(a.y, b.y) && IsNear(a.z, b.z);
}

template<typename valType>
bool VecEqual(glm::tvec2<valType> const & a, glm::tvec2<valType> const & b)
{
    return IsNear(a.x, b.x) && IsNear(a.y, b.y);
}

template<typename vecType>
bool VecEqual(vecType const & a, vecType const & b);

#endif   // UTILS_H_INCLUDED
