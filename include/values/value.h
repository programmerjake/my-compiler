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
#ifndef VALUE_H_INCLUDED
#define VALUE_H_INCLUDED

#include "context.h"
#include "types/type.h"
#include "types/type_builtin.h"
#include "util/variable.h"
#include <cassert>

class ValueNode;
class ValueBoolean;
class ValueUnknown;
class ValueVariablePointer;
class ValueNullPointer;
class ValueInteger;
class ValueNodeVisitor
{
public:
    virtual ~ValueNodeVisitor() = default;
    virtual void visitValueBoolean(std::shared_ptr<ValueBoolean> node) = 0;
    virtual void visitValueUnknown(std::shared_ptr<ValueUnknown> node) = 0;
    virtual void visitValueVariablePointer(std::shared_ptr<ValueVariablePointer> node) = 0;
    virtual void visitValueNullPointer(std::shared_ptr<ValueNullPointer> node) = 0;
    virtual void visitValueInteger(std::shared_ptr<ValueInteger> node) = 0;
};

class ValueNode : public std::enable_shared_from_this<ValueNode>
{
public:
    virtual ~ValueNode() = default;
    CompilerContext *const context;
    const bool isConstant;
    std::shared_ptr<TypeNode> type;
    ValueNode(CompilerContext *context, std::shared_ptr<TypeNode> type, bool isConstant)
        : context(context), isConstant(isConstant), type(type)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) = 0;
    virtual bool operator ==(const ValueNode &rt) const = 0;
    bool operator !=(const ValueNode &rt) const
    {
        return !operator ==(rt);
    }
    enum class CompareResult
    {
        Less = -1,
        Equal = 0,
        Greater = 1,
        Unknown
    };
    virtual CompareResult compareValue(const ValueNode &rt) const
    {
        return CompareResult::Unknown;
    }
};

class ValueBoolean final : public ValueNode
{
public:
    bool value;
    ValueBoolean(CompilerContext *context, bool value)
        : ValueNode(context, TypeBoolean::make(context), true), value(value)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueBoolean(std::static_pointer_cast<ValueBoolean>(shared_from_this()));
    }
    virtual bool operator ==(const ValueNode &rt) const override
    {
        const ValueBoolean *prt = dynamic_cast<const ValueBoolean *>(&rt);
        if(prt == nullptr)
            return false;
        return prt->value == value;
    }
    virtual CompareResult compareValue(const ValueNode &rt) const override
    {
        const ValueBoolean *prt = dynamic_cast<const ValueBoolean *>(&rt);
        if(prt != nullptr)
        {
            if(value)
            {
                if(prt->value)
                    return CompareResult::Equal;
                return CompareResult::Greater;
            }
            if(prt->value)
                return CompareResult::Less;
            return CompareResult::Equal;
        }
        return CompareResult::Unknown;
    }
};

class ValueUnknown final : public ValueNode
{
public:
    explicit ValueUnknown(CompilerContext *context)
        : ValueNode(context, TypeVoid::make(context), false)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueUnknown(std::static_pointer_cast<ValueUnknown>(shared_from_this()));
    }
    virtual bool operator ==(const ValueNode &rt) const override
    {
        const ValueUnknown *prt = dynamic_cast<const ValueUnknown *>(&rt);
        if(prt == nullptr)
            return false;
        return true;
    }
};

class ValueNullPointer final : public ValueNode
{
public:
    explicit ValueNullPointer(CompilerContext *context)
        : ValueNode(context, TypePointer::make(TypeVoid::make(context)), true)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueNullPointer(std::static_pointer_cast<ValueNullPointer>(shared_from_this()));
    }
    virtual bool operator ==(const ValueNode &rt) const override
    {
        const ValueNullPointer *prt = dynamic_cast<const ValueNullPointer *>(&rt);
        if(prt == nullptr)
            return false;
        return true;
    }
    virtual CompareResult compareValue(const ValueNode &rt) const override;
};

class ValueVariablePointer final : public ValueNode
{
public:
    VariableLocation location;
    explicit ValueVariablePointer(CompilerContext *context, VariableLocation location, std::shared_ptr<TypeNode> variableType)
        : ValueNode(context, TypePointer::make(variableType), false), location(location)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueVariablePointer(std::static_pointer_cast<ValueVariablePointer>(shared_from_this()));
    }
    virtual bool operator ==(const ValueNode &rt) const override
    {
        const ValueVariablePointer *prt = dynamic_cast<const ValueVariablePointer *>(&rt);
        if(prt == nullptr)
            return false;
        return location == prt->location;
    }
    virtual CompareResult compareValue(const ValueNode &rt) const override
    {
        const ValueNullPointer *nullPointer = dynamic_cast<const ValueNullPointer *>(&rt);
        if(nullPointer)
            return CompareResult::Greater;
        const ValueVariablePointer *variablePointer = dynamic_cast<const ValueVariablePointer *>(&rt);
        if(variablePointer != nullptr)
        {
            if(location.variable == variablePointer->location.variable)
            {
                if(location.offset > variablePointer->location.offset)
                    return CompareResult::Greater;
                if(location.offset < variablePointer->location.offset)
                    return CompareResult::Less;
                return CompareResult::Equal;
            }
        }
        return CompareResult::Unknown;
    }
};

class ValueInteger final : public ValueNode
{
private:
    std::uint64_t valueInternal;
public:
    std::int8_t getAsInt8() const
    {
        return static_cast<std::int8_t>(valueInternal);
    }
    std::uint8_t getAsUInt8() const
    {
        return static_cast<std::uint8_t>(valueInternal);
    }
    std::int16_t getAsInt16() const
    {
        return static_cast<std::int16_t>(valueInternal);
    }
    std::uint16_t getAsUInt16() const
    {
        return static_cast<std::uint16_t>(valueInternal);
    }
    std::int32_t getAsInt32() const
    {
        return static_cast<std::int32_t>(valueInternal);
    }
    std::uint32_t getAsUInt32() const
    {
        return static_cast<std::uint32_t>(valueInternal);
    }
    std::int64_t getAsInt64() const
    {
        return static_cast<std::int64_t>(valueInternal);
    }
    std::uint64_t getAsUInt64() const
    {
        return static_cast<std::uint64_t>(valueInternal);
    }
    typedef IntegerWidth Width;
    bool isUnsigned;
    Width width;
    explicit ValueInteger(CompilerContext *context, bool isUnsigned, Width width, std::uint64_t v)
        : ValueNode(context, TypeInteger::make(context, isUnsigned, width), true), valueInternal(v), isUnsigned(isUnsigned), width(width)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueInteger(std::static_pointer_cast<ValueInteger>(shared_from_this()));
    }
    virtual bool operator ==(const ValueNode &rt) const override
    {
        const ValueInteger *prt = dynamic_cast<const ValueInteger *>(&rt);
        if(prt == nullptr)
            return false;
        if(isUnsigned != prt->isUnsigned || width != prt->width)
            return false;
        Width calcWidth = width;
        if(calcWidth == Width::IntNativeSize)
            calcWidth = context->backend->getNativeIntegerWidth();
        switch(calcWidth)
        {
        case Width::Int8:
            if(isUnsigned)
                return getAsUInt8() == prt->getAsUInt8();
            return getAsInt8() == prt->getAsInt8();
        case Width::Int16:
            if(isUnsigned)
                return getAsUInt16() == prt->getAsUInt16();
            return getAsInt16() == prt->getAsInt16();
        case Width::Int32:
            if(isUnsigned)
                return getAsUInt32() == prt->getAsUInt32();
            return getAsInt32() == prt->getAsInt32();
        case Width::Int64:
            if(isUnsigned)
                return getAsUInt64() == prt->getAsUInt64();
            return getAsInt64() == prt->getAsInt64();
        case Width::IntNativeSize:
            break;
        }
        assert(false);
        return valueInternal == prt->valueInternal;
    }
private:
    std::int64_t getCompareValue() const
    {
        Width calcWidth = width;
        if(calcWidth == Width::IntNativeSize)
            calcWidth = context->backend->getNativeIntegerWidth();
        switch(calcWidth)
        {
        case Width::Int8:
            if(isUnsigned)
                return getAsUInt8();
            return getAsInt8();
        case Width::Int16:
            if(isUnsigned)
                return getAsUInt16();
            return getAsInt16();
        case Width::Int32:
            if(isUnsigned)
                return getAsUInt32();
            return getAsInt32();
        case Width::Int64:
            if(isUnsigned)
                return getAsUInt64();
            return getAsInt64();
        case Width::IntNativeSize:
            break;
        }
        assert(false);
        return valueInternal;
    }
public:
    std::int64_t getSignedValue() const
    {
        return getCompareValue();
    }
    std::uint64_t getUnsignedValue() const
    {
        return getCompareValue();
    }
    virtual CompareResult compareValue(const ValueNode &rt) const override
    {
        const ValueInteger *prt = dynamic_cast<const ValueInteger *>(&rt);
        if(prt != nullptr)
        {
            std::int64_t lhsValue = getCompareValue(), rhsValue = prt->getCompareValue();
            std::uint64_t lhsValueU = lhsValue, rhsValueU = rhsValue;
            if(isUnsigned)
            {
                if(!prt->isUnsigned && rhsValue < 0)
                    return CompareResult::Greater;
                if(lhsValueU < rhsValueU)
                    return CompareResult::Less;
                if(lhsValueU > rhsValueU)
                    return CompareResult::Greater;
                return CompareResult::Equal;
            }
            if(prt->isUnsigned)
            {
                if(lhsValue < 0)
                    return CompareResult::Less;
                if(lhsValueU < rhsValueU)
                    return CompareResult::Less;
                if(lhsValueU > rhsValueU)
                    return CompareResult::Greater;
                return CompareResult::Equal;
            }
            if(lhsValue < rhsValue)
                return CompareResult::Less;
            if(lhsValue > rhsValue)
                return CompareResult::Greater;
            return CompareResult::Equal;
        }
        return CompareResult::Unknown;
    }
};

#endif // VALUE_H_INCLUDED
