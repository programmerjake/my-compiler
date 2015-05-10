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

#include "ssa/ssa_node.h"

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
        if(iter == values.end() || std::get<1>(*iter) == nullptr)
            return nullptr;
        std::shared_ptr<ValueNode> lhsValue = std::get<1>(*iter);
        iter = values.find(rhs.lock());
        if(iter == values.end() || std::get<1>(*iter) == nullptr)
            return nullptr;
        std::shared_ptr<ValueNode> rhsValue = std::get<1>(*iter);
        ValueNode::CompareResult compareResult = lhsValue->compareValue(*rhsValue);
        if(compareResult == ValueNode::CompareResult::Unknown)
            return nullptr;
        int v = 0;
        if(compareResult == ValueNode::CompareResult::Less)
            v = -1;
        if(compareResult == ValueNode::CompareResult::Greater)
            v = 1;
        bool result = false;
        switch(compareOperator)
        {
        case CompareOperator::E:
            result = (v == 0);
            break;
        case CompareOperator::G:
            result = (v > 0);
            break;
        case CompareOperator::GE:
            result = (v >= 0);
            break;
        case CompareOperator::L:
            result = (v < 0);
            break;
        case CompareOperator::LE:
            result = (v <= 0);
            break;
        default: // NE
            result = (v != 0);
            break;
        }
        return std::make_shared<ValueBoolean>(context, result);
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
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(type && type->toNonConstant()->toNonVolatile() == TypeBoolean::make(context));
        assert(lhs.lock());
        assert(rhs.lock());
    }
};

#endif // SSA_COMPARE_H_INCLUDED
