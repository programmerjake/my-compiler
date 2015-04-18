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
#ifndef SSA_PHI_H_INCLUDED
#define SSA_PHI_H_INCLUDED

#include "ssa_node.h"
#include <cassert>

class SSAPhi final : public SSANode
{
public:
    struct PhiInput final
    {
        std::weak_ptr<SSANode> node;
        std::weak_ptr<SSABasicBlock> block;
    };
    std::list<PhiInput> inputs;
private:
    static std::shared_ptr<TypeNode> calcType(const std::list<PhiInput> &inputs)
    {
        assert(!inputs.empty());
        std::shared_ptr<SSANode> node = inputs.front().node.lock();
        assert(node);
        std::shared_ptr<TypeNode> type = node->type.lock();
        assert(type);
#ifndef NDEBUG
        for(const PhiInput &i : inputs)
        {
            std::shared_ptr<SSANode> in = i.node.lock();
            assert(in->type.lock() == type);
        }
#endif
        return type;
    }
    static CompilerContext *calcContext(const std::list<PhiInput> &inputs)
    {
        assert(!inputs.empty());
        std::shared_ptr<SSANode> node = inputs.front().node.lock();
        assert(node);
        return node->context;
    }
public:
    explicit SSAPhi(std::list<PhiInput> inputs)
        : SSANode(calcContext(inputs), calcType(inputs)), inputs(inputs)
    {
    }
    explicit SSAPhi(std::shared_ptr<TypeNode> type)
        : SSANode(type->context, type)
    {
    }
    virtual void visit(SSANodeVisitor &visitor) override
    {
        visitor.visitSSAPhi(std::static_pointer_cast<SSAPhi>(shared_from_this()));
    }
};

#endif // SSA_PHI_H_INCLUDED
