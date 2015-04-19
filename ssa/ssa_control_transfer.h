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
#ifndef SSA_CONTROL_TRANSFER_H_INCLUDED
#define SSA_CONTROL_TRANSFER_H_INCLUDED

#include "ssa_node.h"
#include "../types/type_builtin.h"
#include "ssa_visitor.h"
#include "../values/values.h"

class SSAControlTransfer : public SSANode
{
public:
    explicit SSAControlTransfer(CompilerContext *context)
        : SSANode(context, TypeVoid::make(context))
    {
    }
    std::list<std::weak_ptr<SSABasicBlock>> destBlocks;
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        return std::make_shared<ValueUnknown>(context);
    }
    virtual std::list<std::weak_ptr<SSABasicBlock>> evaluateControlForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const
    {
        return destBlocks;
    }
};

class SSAUnconditionalJump final : public SSAControlTransfer
{
public:
    SSAUnconditionalJump(CompilerContext *context, std::shared_ptr<SSABasicBlock> destBlock)
        : SSAControlTransfer(context)
    {
        destBlocks.assign(1, destBlock);
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAUnconditionalJump(std::static_pointer_cast<SSAUnconditionalJump>(shared_from_this()));
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<SSANode>> &replacements) override
    {
    }
};

class SSAConditionalJump final : public SSAControlTransfer
{
public:
    std::weak_ptr<SSANode> condition;
    SSAConditionalJump(CompilerContext *context, std::shared_ptr<SSANode> condition, std::shared_ptr<SSABasicBlock> trueDestBlock, std::shared_ptr<SSABasicBlock> falseDestBlock)
        : SSAControlTransfer(context), condition(condition)
    {
        destBlocks.assign(1, trueDestBlock);
        destBlocks.push_back(falseDestBlock);
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAConditionalJump(std::static_pointer_cast<SSAConditionalJump>(shared_from_this()));
    }
    virtual std::list<std::weak_ptr<SSABasicBlock>> evaluateControlForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        auto iter = values.find(condition.lock());
        std::shared_ptr<ValueNode> conditionValueNode = nullptr;
        if(iter != values.end())
            conditionValueNode = std::get<1>(*iter);
        if(dynamic_cast<const ValueUnknown *>(conditionValueNode.get()) != nullptr)
            return std::list<std::weak_ptr<SSABasicBlock>>{};
        std::shared_ptr<ValueBoolean> conditionValueBoolean = std::dynamic_pointer_cast<ValueBoolean>(conditionValueNode);
        if(conditionValueBoolean != nullptr)
        {
            if(conditionValueBoolean->value)
                return std::list<std::weak_ptr<SSABasicBlock>>{destBlocks.front()};
            return std::list<std::weak_ptr<SSABasicBlock>>{destBlocks.back()};
        }
        return destBlocks;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{condition.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<SSANode>> &replacements) override
    {
        condition = replaceNode(replacements, condition.lock());
    }
};

#endif // SSA_CONTROL_TRANSFER_H_INCLUDED
