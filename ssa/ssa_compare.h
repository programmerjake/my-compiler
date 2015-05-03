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
#ifndef SSA_COMPARE_H_INCLUDED
#define SSA_COMPARE_H_INCLUDED

#include "ssa_node.h"

class SSACompare final : public SSANode
{
public:
    enum class CompareOperator
    {
        E,
        NE,
        L,
        LE,
        G,
        GE
    };
    std::weak_ptr<SSANode> lhs;
    std::weak_ptr<SSANode> rhs;
    CompareOperator compareOperator;
    SSACompare(std::shared_ptr<SSANode> lhs, CompareOperator compareOperator, std::shared_ptr<SSANode> rhs, SpillLocation spillLocation)
        : SSANode(lhs->context, TypeBoolean::make(lhs->context), spillLocation), lhs(lhs), rhs(rhs), compareOperator(compareOperator)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSACompare(std::static_pointer_cast<SSACompare>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        auto iter = values.find(lhs.lock());
        if(iter == values.end())
            return nullptr;
        std::shared_ptr<ValueNode> lhsValue = std::get<1>(*iter);
        iter = values.find(rhs.lock());
        if(iter == values.end())
            return nullptr;
        std::shared_ptr<ValueNode> rhsValue = std::get<1>(*iter);
        std::shared_ptr<ValueBoolean> lhsValueBoolean = std::dynamic_pointer_cast<ValueBoolean>(lhsValue);
        std::shared_ptr<ValueBoolean> rhsValueBoolean = std::dynamic_pointer_cast<ValueBoolean>(rhsValue);
        if(lhsValueBoolean && rhsValueBoolean)
        {
            switch(compareOperator)
            {
            case CompareOperator::E:
                return std::make_shared<ValueBoolean>(context, lhsValueBoolean->value == rhsValueBoolean->value);
            case CompareOperator::G:
                return std::make_shared<ValueBoolean>(context, lhsValueBoolean->value && !rhsValueBoolean->value);
            case CompareOperator::GE:
                return std::make_shared<ValueBoolean>(context, lhsValueBoolean->value || !rhsValueBoolean->value);
            case CompareOperator::L:
                return std::make_shared<ValueBoolean>(context, !lhsValueBoolean->value && rhsValueBoolean->value);
            case CompareOperator::LE:
                return std::make_shared<ValueBoolean>(context, !lhsValueBoolean->value || rhsValueBoolean->value);
            default: // NE
                return std::make_shared<ValueBoolean>(context, lhsValueBoolean->value != rhsValueBoolean->value);
            }
        }
        std::shared_ptr<ValueNullPointer> lhsValueNullPointer = std::dynamic_pointer_cast<ValueNullPointer>(lhsValue);
        std::shared_ptr<ValueNullPointer> rhsValueNullPointer = std::dynamic_pointer_cast<ValueNullPointer>(rhsValue);
        if(lhsValueNullPointer && rhsValueNullPointer)
        {
            switch(compareOperator)
            {
            case CompareOperator::E:
                return std::make_shared<ValueBoolean>(context, true);
            case CompareOperator::G:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::GE:
                return std::make_shared<ValueBoolean>(context, true);
            case CompareOperator::L:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::LE:
                return std::make_shared<ValueBoolean>(context, true);
            default: // NE
                return std::make_shared<ValueBoolean>(context, false);
            }
        }
        std::shared_ptr<ValueLocalVariablePointer> lhsValueLocalVariablePointer = std::dynamic_pointer_cast<ValueLocalVariablePointer>(lhsValue);
        std::shared_ptr<ValueLocalVariablePointer> rhsValueLocalVariablePointer = std::dynamic_pointer_cast<ValueLocalVariablePointer>(rhsValue);
        if(lhsValueLocalVariablePointer && rhsValueNullPointer)
        {
            switch(compareOperator)
            {
            case CompareOperator::E:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::G:
                return std::make_shared<ValueBoolean>(context, true);
            case CompareOperator::GE:
                return std::make_shared<ValueBoolean>(context, true);
            case CompareOperator::L:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::LE:
                return std::make_shared<ValueBoolean>(context, false);
            default: // NE
                return std::make_shared<ValueBoolean>(context, false);
            }
        }
        if(lhsValueNullPointer && rhsValueLocalVariablePointer)
        {
            switch(compareOperator)
            {
            case CompareOperator::E:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::G:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::GE:
                return std::make_shared<ValueBoolean>(context, false);
            case CompareOperator::L:
                return std::make_shared<ValueBoolean>(context, true);
            case CompareOperator::LE:
                return std::make_shared<ValueBoolean>(context, true);
            default: // NE
                return std::make_shared<ValueBoolean>(context, false);
            }
        }
        if(lhsValueLocalVariablePointer && rhsValueLocalVariablePointer)
        {
            switch(compareOperator)
            {
            case CompareOperator::E:
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start == rhsValueLocalVariablePointer->start);
            case CompareOperator::G:
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start > rhsValueLocalVariablePointer->start);
            case CompareOperator::GE:
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start >= rhsValueLocalVariablePointer->start);
            case CompareOperator::L:
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start < rhsValueLocalVariablePointer->start);
            case CompareOperator::LE:
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start <= rhsValueLocalVariablePointer->start);
            default: // NE
                return std::make_shared<ValueBoolean>(context, lhsValueLocalVariablePointer->start != rhsValueLocalVariablePointer->start);
            }
        }
        return nullptr;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{lhs.lock(), rhs.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        lhs = replaceNode(replacements, lhs.lock());
        rhs = replaceNode(replacements, rhs.lock());
    }
};

#endif // SSA_COMPARE_H_INCLUDED
