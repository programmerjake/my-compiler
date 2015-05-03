/* Copyright (c) 2015 Jacob R. Lifshay
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
#ifndef SPILL_LOCATION_H_INCLUDED
#define SPILL_LOCATION_H_INCLUDED

#include <cstddef>
#include <cstdint>

struct VariableLocation final
{
    enum class LocationKind
    {
        None,
        LocalVariable,
        FunctionArgument,
        GlobalVariable,
    };
    LocationKind locationKind = LocationKind::None;
    std::uint64_t start = 0;
    constexpr VariableLocation()
    {
    }
    constexpr VariableLocation(std::nullptr_t)
    {
    }
    constexpr VariableLocation(LocationKind locationKind, std::uint64_t start)
        : locationKind(locationKind), start(start)
    {
    }
    VariableLocation &operator =(std::nullptr_t)
    {
        return *this = VariableLocation(nullptr);
    }
    constexpr bool operator ==(const VariableLocation &rt) const
    {
        return ((locationKind == LocationKind::None) ? rt.locationKind == LocationKind::None : (locationKind == rt.locationKind && start == rt.start));
    }
    constexpr bool operator !=(const VariableLocation &rt) const
    {
        return !operator ==(rt);
    }
    constexpr bool empty() const
    {
        return locationKind == LocationKind::None;
    }
    constexpr bool good() const
    {
        return !empty();
    }
    explicit constexpr operator bool() const
    {
        return good();
    }
    constexpr bool operator !() const
    {
        return empty();
    }
};

typedef VariableLocation SpillLocation;

#endif // SPILL_LOCATION_H_INCLUDED
