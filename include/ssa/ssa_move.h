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
#ifndef SSA_MOVE_H_INCLUDED
#define SSA_MOVE_H_INCLUDED

#include "ssa/ssa_node.h"
#include "values/value.h"

class SSAMove final : public SSANode
{
public:
    std::weak_ptr<SSANode> source;
    explicit SSAMove(std::shared_ptr<SSANode> source, SpillLocation spillLocation)
        : SSANode(source->context, source->type, spillLocation), source(source)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAMove(std::static_pointer_cast<SSAMove>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        auto iter = values.find(source.lock());
        if(iter != values.end())
            return std::get<1>(*iter);
        return nullptr;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{source.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        source = replaceNode(replacements, source.lock());
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(source.lock());
    }
};

class SSALoad final : public SSANode
{
public:
    std::weak_ptr<SSANode> address;
    explicit SSALoad(std::shared_ptr<SSANode> address, SpillLocation spillLocation)
        : SSANode(address->context, address->type->dereference(), spillLocation), address(address)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSALoad(std::static_pointer_cast<SSALoad>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        return nullptr;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{address.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        address = replaceNode(replacements, address.lock());
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(address.lock());
    }
};

class SSAStore final : public SSANode
{
public:
    std::weak_ptr<SSANode> address;
    std::weak_ptr<SSANode> value;
    SSAStore(std::shared_ptr<SSANode> address, std::shared_ptr<SSANode> value)
        : SSANode(address->context, TypeVoid::make(address->context), nullptr), address(address), value(value)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAStore(std::static_pointer_cast<SSAStore>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        return nullptr;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{address.lock(), value.lock()};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        address = replaceNode(replacements, address.lock());
        value = replaceNode(replacements, value.lock());
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(address.lock());
        assert(value.lock());
    }
};

#endif // SSA_MOVE_H_INCLUDED
