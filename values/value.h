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

class ValueNode;
class ValueBoolean;
class ValueUnknown;
class ValueVariablePointer;
class ValueNullPointer;
class ValueNodeVisitor
{
public:
    virtual ~ValueNodeVisitor() = default;
    virtual void visitValueBoolean(std::shared_ptr<ValueBoolean> node) = 0;
    virtual void visitValueUnknown(std::shared_ptr<ValueUnknown> node) = 0;
    virtual void visitValueVariablePointer(std::shared_ptr<ValueVariablePointer> node) = 0;
    virtual void visitValueNullPointer(std::shared_ptr<ValueNullPointer> node) = 0;
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
};

#endif // VALUE_H_INCLUDED
