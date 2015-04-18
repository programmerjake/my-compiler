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

#include "../context.h"
#include "../types/type.h"
#include "../types/type_builtin.h"

class ValueNode;
class ValueBoolean;
class ValueNodeVisitor
{
public:
    virtual ~ValueNodeVisitor() = default;
    virtual void visitValueBoolean(std::shared_ptr<ValueBoolean> node) = 0;
};

class ValueNode : public std::enable_shared_from_this<ValueNode>
{
public:
    virtual ~ValueNode() = default;
    CompilerContext *const context;
    std::weak_ptr<TypeNode> type;
    ValueNode(CompilerContext *context, std::shared_ptr<TypeNode> type)
        : context(context), type(type)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) = 0;
};

class ValueBoolean final : public ValueNode
{
public:
    bool value;
    ValueBoolean(CompilerContext *context, bool value)
        : ValueNode(context, TypeBoolean::make(context)), value(value)
    {
    }
    virtual void visit(ValueNodeVisitor &visitor) override
    {
        visitor.visitValueBoolean(std::static_pointer_cast<ValueBoolean>(shared_from_this()));
    }
};

#endif // VALUE_H_INCLUDED
