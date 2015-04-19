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
#ifndef SSA_CONST_H_INCLUDED
#define SSA_CONST_H_INCLUDED

#include "ssa_node.h"
#include "../values/value.h"

class SSAConstant final : public SSANode
{
public:
    std::shared_ptr<ValueNode> value;
    SSAConstant(std::shared_ptr<ValueNode> value)
        : SSANode(value->context, value->type.lock()), value(value)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAConstant(std::static_pointer_cast<SSAConstant>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        return value;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        return std::list<std::shared_ptr<SSANode>>{};
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<SSANode>> &replacements) override
    {
    }
};

#endif // SSA_CONST_H_INCLUDED
