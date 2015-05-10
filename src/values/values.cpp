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
#include "values/values.h"

ValueNode::CompareResult ValueNullPointer::compareValue(const ValueNode &rt) const
{
    const ValueNullPointer *nullPointer = dynamic_cast<const ValueNullPointer *>(&rt);
    if(nullPointer)
        return CompareResult::Equal;
    const ValueVariablePointer *variablePointer = dynamic_cast<const ValueVariablePointer *>(&rt);
    if(variablePointer != nullptr)
        return CompareResult::Less;
    return CompareResult::Unknown;
}

std::shared_ptr<ValueNode> ValueBoolean::typeCast(std::shared_ptr<TypeNode> destType)
{
    if(std::shared_ptr<TypeBoolean> typeBoolean = std::dynamic_pointer_cast<TypeBoolean>(destType->toNonConstant()->toNonVolatile()))
    {
        return shared_from_this();
    }
    if(std::shared_ptr<TypeInteger> typeInteger = std::dynamic_pointer_cast<TypeInteger>(destType->toNonConstant()->toNonVolatile()))
    {
        return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, (value ? 1 : 0));
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueNullPointer::typeCast(std::shared_ptr<TypeNode> destType)
{
    if(std::shared_ptr<TypeBoolean> typeBoolean = std::dynamic_pointer_cast<TypeBoolean>(destType->toNonConstant()->toNonVolatile()))
    {
        return std::make_shared<ValueBoolean>(context, false);
    }
    if(std::shared_ptr<TypePointer> typePointer = std::dynamic_pointer_cast<TypePointer>(destType->toNonConstant()->toNonVolatile()))
    {
        return shared_from_this();
    }
    if(std::shared_ptr<TypeInteger> typeInteger = std::dynamic_pointer_cast<TypeInteger>(destType->toNonConstant()->toNonVolatile()))
    {
        return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, 0);
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueVariablePointer::typeCast(std::shared_ptr<TypeNode> destType)
{
    if(std::shared_ptr<TypeBoolean> typeBoolean = std::dynamic_pointer_cast<TypeBoolean>(destType->toNonConstant()->toNonVolatile()))
    {
        return std::make_shared<ValueBoolean>(context, true);
    }
    if(std::shared_ptr<TypePointer> typePointer = std::dynamic_pointer_cast<TypePointer>(destType->toNonConstant()->toNonVolatile()))
    {
        return shared_from_this();
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueVariablePointer::add(std::shared_ptr<ValueNode> r)
{
    VariableLocation retval = location;
    if(std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(r))
    {
        if(valueInteger->isUnsigned)
            retval.offset += valueInteger->getUnsignedValue();
        else
            retval.offset += valueInteger->getSignedValue();
        return std::make_shared<ValueVariablePointer>(context, retval, type->dereference());
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueVariablePointer::subtract(std::shared_ptr<ValueNode> r)
{
    VariableLocation retval = location;
    if(std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(r))
    {
        std::uint64_t typeSize = type->dereference()->getTypeProperties().size;
        if(typeSize == 0)
            return nullptr;
        if(valueInteger->isUnsigned)
            retval.offset -= valueInteger->getUnsignedValue() * typeSize;
        else
            retval.offset -= valueInteger->getSignedValue() * typeSize;
        return std::make_shared<ValueVariablePointer>(context, retval, type->dereference());
    }
    if(std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(r))
    {
        VariableLocation rlocation = valueVariablePointer->location;
        std::uint64_t typeSize = valueVariablePointer->type->dereference()->getTypeProperties().size;
        if(typeSize == 0)
            return nullptr;
        if(location.variable != rlocation.variable)
            return nullptr;
        return std::make_shared<ValueInteger>(context, false, IntegerWidth::IntNativeSize, static_cast<std::int64_t>(location.offset - rlocation.offset) / static_cast<std::int64_t>(typeSize));
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueInteger::typeCast(std::shared_ptr<TypeNode> destType)
{
    if(std::shared_ptr<TypeBoolean> typeBoolean = std::dynamic_pointer_cast<TypeBoolean>(destType->toNonConstant()->toNonVolatile()))
    {
        return std::make_shared<ValueBoolean>(context, getUnsignedValue() != 0);
    }
    if(std::shared_ptr<TypeInteger> typeInteger = std::dynamic_pointer_cast<TypeInteger>(destType->toNonConstant()->toNonVolatile()))
    {
        if(isUnsigned)
            return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, getUnsignedValue());
        return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, getSignedValue());
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueInteger::add(std::shared_ptr<ValueNode> r)
{
    if(std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(r))
    {
        if(isUnsigned)
            return std::make_shared<ValueInteger>(context, true, width, getUnsignedValue() + valueInteger->getUnsignedValue());
        return std::make_shared<ValueInteger>(context, false, width, getSignedValue() + valueInteger->getSignedValue());
    }
    if(std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(r))
    {
        VariableLocation retval = valueVariablePointer->location;
        std::uint64_t typeSize = valueVariablePointer->type->dereference()->getTypeProperties().size;
        if(typeSize == 0)
            return nullptr;
        if(isUnsigned)
            retval.offset += getUnsignedValue() * typeSize;
        else
            retval.offset += getSignedValue() * typeSize;
        return std::make_shared<ValueVariablePointer>(context, retval, valueVariablePointer->type->dereference());
    }
    return nullptr;
}

std::shared_ptr<ValueNode> ValueInteger::subtract(std::shared_ptr<ValueNode> r)
{
    if(std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(r))
    {
        if(isUnsigned)
            return std::make_shared<ValueInteger>(context, true, width, getUnsignedValue() - valueInteger->getUnsignedValue());
        return std::make_shared<ValueInteger>(context, false, width, getSignedValue() - valueInteger->getSignedValue());
    }
    return nullptr;
}
