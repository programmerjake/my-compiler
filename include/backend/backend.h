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
#ifndef BACKEND_H_INCLUDED
#define BACKEND_H_INCLUDED

#include <memory>
#include <ostream>
#include <list>
#include <stdexcept>
#include <cstdint>
#include "types/integer_width.h"

class RTLFunction;

struct TypeProperties final
{
    std::uint64_t alignment;
    std::uint64_t size;
    std::uint64_t allocateVariable(std::uint64_t &memoryAllocated) const
    {
        memoryAllocated += ((alignment - memoryAllocated % alignment) % alignment);
        std::uint64_t retval = memoryAllocated;
        memoryAllocated += size;
        return retval;
    }
    bool good() const
    {
        return size > 0;
    }
    TypeProperties()
        : alignment(1), size(0)
    {
    }
    TypeProperties(std::uint64_t size)
        : alignment(size), size(size)
    {
    }
    TypeProperties(std::uint64_t alignment, std::uint64_t size)
        : alignment(alignment), size(size)
    {
    }
};

class TypeNode;

class Backend : public std::enable_shared_from_this<Backend>
{
    Backend(const Backend &) = delete;
    Backend &operator =(const Backend &) = delete;
public:
    Backend() = default;
    virtual ~Backend() = default;
    virtual void outputAsAssembly(std::ostream &os, std::list<std::shared_ptr<RTLFunction>> functions) const = 0;
    virtual TypeProperties getTypeProperties(std::shared_ptr<TypeNode> type) const = 0;
    virtual IntegerWidth getNativeIntegerWidth() const = 0;
};

class NotImplementedException : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

#endif // BACKEND_H_INCLUDED
