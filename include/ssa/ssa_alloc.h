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
#ifndef SSA_ALLOC_H_INCLUDED
#define SSA_ALLOC_H_INCLUDED

#include "ssa/ssa_node.h"
#include "types/type.h"
#include "util/variable.h"

class SSAAllocA final : public SSANode
{
private:
    std::shared_ptr<VariableDescriptor> variableDescriptor;
public:
    std::shared_ptr<TypeNode> variableType;
    explicit SSAAllocA(std::shared_ptr<TypeNode> variableTypeIn)
        : SSANode(variableTypeIn->context, TypePointer::make(variableTypeIn)->toConstant(), nullptr), variableType(variableTypeIn)
    {
        variableDescriptor = std::make_shared<VariableDescriptor>(VariableDescriptor::Kind::LocalVariable, variableTypeIn);
    }
    std::shared_ptr<VariableDescriptor> getVariableDescriptor() const
    {
        variableDescriptor->ssaAllocA = std::static_pointer_cast<SSAAllocA>(std::const_pointer_cast<SSANode>(shared_from_this()));
        return variableDescriptor;
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAAllocA(std::static_pointer_cast<SSAAllocA>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        return std::make_shared<ValueVariablePointer>(context, VariableLocation(getVariableDescriptor()), type->dereference());
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) override
    {
        assert(type);
        assert(variableType);
    }
};

#endif // SSA_ALLOC_H_INCLUDED
