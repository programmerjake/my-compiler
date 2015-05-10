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
#ifndef SSA_ARITH_LOGIC_H_INCLUDED
#define SSA_ARITH_LOGIC_H_INCLUDED

#include "ssa/ssa_node.h"

class SSATypeCast final : public SSANode
{
public:
    std::weak_ptr<SSANode> arg;
    SSATypeCast(std::shared_ptr<SSANode> arg, std::shared_ptr<TypeNode> type, SpillLocation spillLocation)
        : SSANode(arg->context, type, spillLocation), arg(arg)
    {
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override final
    {
        return std::list<std::shared_ptr<SSANode>>{arg.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        arg = replaceNode(replacements, arg.lock());
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(arg.lock());
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        std::shared_ptr<SSANode> argLocked = this->arg.lock();
        auto iter = values.find(argLocked);
        if(iter == values.end())
            return nullptr;
        std::shared_ptr<ValueNode> value = std::get<1>(*iter);
        if(value->type == type)
            return value;
        std::shared_ptr<TypeBoolean> typeBoolean = std::dynamic_pointer_cast<TypeBoolean>(type->toNonConstant()->toNonVolatile());
        std::shared_ptr<TypePointer> typePointer = std::dynamic_pointer_cast<TypePointer>(type->toNonConstant()->toNonVolatile());
        std::shared_ptr<TypeInteger> typeInteger = std::dynamic_pointer_cast<TypeInteger>(type->toNonConstant()->toNonVolatile());
        std::shared_ptr<ValueBoolean> valueBoolean = std::dynamic_pointer_cast<ValueBoolean>(value);
        std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(value);
        std::shared_ptr<ValueNullPointer> valueNullPointer = std::dynamic_pointer_cast<ValueNullPointer>(value);
        std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(value);
        if(typeBoolean)
        {
            if(valueBoolean)
                return valueBoolean;
            if(valueNullPointer)
                return std::make_shared<ValueBoolean>(context, false);
            if(valueVariablePointer)
                return std::make_shared<ValueBoolean>(context, true);
            if(valueInteger)
                return std::make_shared<ValueBoolean>(context, valueInteger->getUnsignedValue() != 0);
            return nullptr;
        }
        if(typeInteger)
        {
            if(valueBoolean)
                return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, (valueBoolean->value ? 1 : 0));
            if(valueNullPointer)
                return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, 0);
            if(valueInteger)
            {
                if(valueInteger->isUnsigned)
                    return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, valueInteger->getUnsignedValue());
                return std::make_shared<ValueInteger>(context, typeInteger->isUnsigned, typeInteger->width, valueInteger->getSignedValue());
            }
            return nullptr;
        }
        if(typePointer)
        {
            if(valueInteger)
                if(valueInteger->getUnsignedValue() == 0)
                    return std::make_shared<ValueNullPointer>(context);
            if(valueNullPointer)
                return valueNullPointer;
            if(valueVariablePointer)
                return std::make_shared<ValueVariablePointer>(context, valueVariablePointer->location, type->dereference());
            return nullptr;
        }
        return nullptr;
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSATypeCast(std::static_pointer_cast<SSATypeCast>(shared_from_this()));
    }
};

class SSAArithLogicUnary : public SSANode
{
public:
    std::weak_ptr<SSANode> arg;
    SSAArithLogicUnary(std::shared_ptr<SSANode> arg, SpillLocation spillLocation)
        : SSANode(arg->context, arg->type, spillLocation), arg(arg)
    {
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override final
    {
        return std::list<std::shared_ptr<SSANode>>{arg.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        arg = replaceNode(replacements, arg.lock());
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(arg.lock());
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        auto iter = values.find(arg.lock());
        if(iter == values.end())
            return nullptr;
        return evaluateForConstantsHelper(std::get<1>(*iter));
    }
protected:
    virtual std::shared_ptr<ValueNode> evaluateForConstantsHelper(std::shared_ptr<ValueNode> value) const = 0;
};

class SSAArithLogicBinary : public SSANode
{
public:
    std::weak_ptr<SSANode> lhs;
    std::weak_ptr<SSANode> rhs;
    SSAArithLogicBinary(std::shared_ptr<SSANode> lhs, std::shared_ptr<SSANode> rhs, SpillLocation spillLocation)
        : SSANode(lhs->context, lhs->type, spillLocation), lhs(lhs), rhs(rhs)
    {
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override final
    {
        return std::list<std::shared_ptr<SSANode>>{lhs.lock(), rhs.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        lhs = replaceNode(replacements, lhs.lock());
        rhs = replaceNode(replacements, rhs.lock());
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(lhs.lock());
        assert(rhs.lock());
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        std::shared_ptr<ValueNode> lhsValue = nullptr;
        auto iter = values.find(lhs.lock());
        if(iter != values.end())
            lhsValue = std::get<1>(*iter);
        std::shared_ptr<ValueNode> rhsValue = nullptr;
        iter = values.find(rhs.lock());
        if(iter != values.end())
            rhsValue = std::get<1>(*iter);
        return evaluateForConstantsHelper(lhsValue, rhsValue);
    }
protected:
    virtual std::shared_ptr<ValueNode> evaluateForConstantsHelper(std::shared_ptr<ValueNode> lhsValue, std::shared_ptr<ValueNode> rhsValue) const = 0;
};

class SSAAdd final : public SSAArithLogicBinary
{
public:
    using SSAArithLogicBinary::SSAArithLogicBinary;
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAAdd(std::static_pointer_cast<SSAAdd>(shared_from_this()));
    }
protected:
    virtual std::shared_ptr<ValueNode> evaluateForConstantsHelper(std::shared_ptr<ValueNode> lhsValue, std::shared_ptr<ValueNode> rhsValue) const override
    {
        std::shared_ptr<ValueInteger> lhsInteger = std::dynamic_pointer_cast<ValueInteger>(lhsValue);
        std::shared_ptr<ValueInteger> rhsInteger = std::dynamic_pointer_cast<ValueInteger>(rhsValue);
        std::shared_ptr<ValueVariablePointer> lhsVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(lhsValue);
        std::shared_ptr<ValueVariablePointer> rhsVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(rhsValue);
        if(lhsInteger && rhsInteger)
        {
            if(lhsInteger->isUnsigned)
                return std::make_shared<ValueInteger>(context, true, lhsInteger->width, lhsInteger->getUnsignedValue() + rhsInteger->getUnsignedValue());
            return std::make_shared<ValueInteger>(context, false, lhsInteger->width, lhsInteger->getSignedValue() + rhsInteger->getSignedValue());
        }
        if(lhsInteger && rhsVariablePointer)
        {
            VariableLocation location = rhsVariablePointer->location;
            if(lhsInteger->isUnsigned)
                location.offset += lhsInteger->getUnsignedValue();
            else
                location.offset += lhsInteger->getSignedValue();
            return std::make_shared<ValueVariablePointer>(context, location, rhsVariablePointer->variableType);
        }
        if(rhsInteger && lhsVariablePointer)
        {
            VariableLocation location = lhsVariablePointer->location;
            if(rhsInteger->isUnsigned)
                location.offset += rhsInteger->getUnsignedValue();
            else
                location.offset += rhsInteger->getSignedValue();
            return std::make_shared<ValueVariablePointer>(context, location, lhsVariablePointer->variableType);
        }
    }
};

#endif // SSA_ARITH_LOGIC_H_INCLUDED
