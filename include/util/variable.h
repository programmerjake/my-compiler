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
#ifndef VARIABLE_H_INCLUDED
#define VARIABLE_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <memory>
#include "context.h"
#include "types/type.h"
#include "backend/backend.h"

class SSAAllocA;

class VariableDescriptor final
{
public:
    enum class Kind
    {
        None,
        LocalVariable,
        FunctionArgument,
        GlobalVariable,
    };
    static constexpr std::uint64_t NoStart = ~static_cast<std::uint64_t>(0);
private:
    Kind kind;
    std::shared_ptr<TypeNode> type;
    std::uint64_t start;
    TypeProperties typeProperties;
public:
    std::weak_ptr<SSAAllocA> ssaAllocA;
    Kind getKind() const
    {
        return kind;
    }
    std::uint64_t getStart() const
    {
        if(empty())
            return NoStart;
        return start;
    }
    std::shared_ptr<TypeNode> getType() const
    {
        if(empty())
            return nullptr;
        return type;
    }
    void setStart(std::uint64_t v)
    {
        assert(!empty() || v == NoStart);
        start = v;
        assert(start == NoStart || start % getAlignment() == 0);
    }
    VariableDescriptor()
        : kind(Kind::None), type(nullptr), start(NoStart), typeProperties()
    {
    }
    VariableDescriptor(Kind kind, std::shared_ptr<TypeNode> type, std::uint64_t start = NoStart)
        : kind(kind), type(type), start(start)
    {
        assert(kind != Kind::None && type != nullptr);
        assert(start == NoStart || start % getAlignment() == 0);
        typeProperties = type->getTypeProperties();
    }
    VariableDescriptor(Kind kind, std::uint64_t start, TypeProperties typeProperties)
        : kind(kind), type(nullptr), start(start), typeProperties(typeProperties)
    {
        assert(kind != Kind::None);
        assert(start == NoStart || start % getAlignment() == 0);
    }
    bool empty() const
    {
        return kind == Kind::None;
    }
    bool needAllocation() const
    {
        return !empty() && getStart() == NoStart;
    }
    std::uint64_t getSize() const
    {
        return getTypeProperties().size;
    }
    std::uint64_t getAlignment() const
    {
        return getTypeProperties().alignment;
    }
    TypeProperties getTypeProperties() const
    {
        if(empty())
            return TypeProperties();
        return typeProperties;
    }
    void allocate(std::uint64_t &usedSpace)
    {
        if(empty())
            setStart(NoStart);
        else
            setStart(getTypeProperties().allocateVariable(usedSpace));
    }
};

struct VariableLocation final
{
    std::shared_ptr<VariableDescriptor> variable;
    std::uint64_t offset = 0;
    constexpr VariableLocation()
        : variable(), offset(0)
    {
    }
    constexpr VariableLocation(std::nullptr_t)
        : VariableLocation()
    {
    }
    VariableLocation(std::shared_ptr<VariableDescriptor> variable, std::uint64_t offset = 0)
        : variable(variable), offset(offset)
    {
    }
    VariableLocation &operator =(std::nullptr_t)
    {
        return *this = VariableLocation(nullptr);
    }
    bool operator ==(const VariableLocation &rt) const
    {
        return variable == rt.variable && (empty() || offset == rt.offset);
    }
    bool operator !=(const VariableLocation &rt) const
    {
        return !operator ==(rt);
    }
    bool empty() const
    {
        return variable == nullptr;
    }
    bool good() const
    {
        return !empty();
    }
    explicit operator bool() const
    {
        return good();
    }
    bool operator !() const
    {
        return empty();
    }
    std::uint64_t getStart() const
    {
        return variable->getStart() + offset;
    }
};

struct SpillLocation final
{
    typedef VariableDescriptor::Kind Kind;
    Kind kind;
    std::uint64_t start;
    std::shared_ptr<VariableDescriptor> variable;
    SpillLocation()
        : kind(Kind::None), start(0), variable(nullptr)
    {
    }
    SpillLocation(std::nullptr_t)
        : SpillLocation()
    {
    }
    SpillLocation(std::shared_ptr<VariableDescriptor> variable)
        : kind(variable->getKind()), start(variable->getStart()), variable(variable)
    {
    }
    SpillLocation(Kind kind, std::uint64_t start, TypeProperties typeProperties)
        : kind(kind), start(start), variable(std::make_shared<VariableDescriptor>(kind, start, typeProperties))
    {
    }
    bool empty() const
    {
        return kind == Kind::None;
    }
    bool operator ==(const SpillLocation &rt) const
    {
        if(empty())
            return rt.empty();
        return kind == rt.kind && start == rt.start;
    }
    bool operator !=(const SpillLocation &rt) const
    {
        return !operator ==(rt);
    }
};

#endif // VARIABLE_H_INCLUDED
