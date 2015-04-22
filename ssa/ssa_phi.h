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
#include "ssa_node.h"
#ifndef SSA_PHI_H_INCLUDED
#define SSA_PHI_H_INCLUDED

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
        std::shared_ptr<TypeNode> type = node->type;
        assert(type);
#ifndef NDEBUG
        for(const PhiInput &i : inputs)
        {
            std::shared_ptr<SSANode> in = i.node.lock();
            assert(in->type == type);
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
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const override
    {
        std::shared_ptr<ValueNode> retval = nullptr;
        bool isFirst = true;
        for(PhiInput i : inputs)
        {
            std::shared_ptr<SSANode> node = i.node.lock();
            assert(node != nullptr);
            auto iter = values.find(node);
            std::shared_ptr<ValueNode> value = nullptr;
            if(iter != values.end())
                value = std::get<1>(*iter);
            if(isFirst)
            {
                retval = value;
                isFirst = false;
            }
            else
            {
                if(dynamic_cast<const ValueUnknown *>(retval.get()) != nullptr)
                {
                    retval = value;
                }
                else if(dynamic_cast<const ValueUnknown *>(value.get()) != nullptr)
                {
                    //retval = retval;
                }
                else if(retval == nullptr)
                {
                    //retval = nullptr;
                }
                else if(value == nullptr)
                {
                    retval = nullptr;
                }
                else if(*retval != *value)
                {
                    retval = nullptr;
                }
                else
                {
                    //retval = retval;
                }
            }
        }
        return retval;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const override
    {
        std::list<std::shared_ptr<SSANode>> retval;
        for(PhiInput i : inputs)
        {
            assert(i.node.lock() != nullptr);
            retval.push_back(i.node.lock());
        }
        return std::move(retval);
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) override
    {
        for(PhiInput &i : inputs)
        {
            assert(i.node.lock() != nullptr);
            i.node = replaceNode(replacements, i.node.lock());
            assert(i.node.lock() != nullptr);
        }
    }
    virtual void removeBlocks(const std::unordered_set<std::shared_ptr<SSABasicBlock>> &removedBlocks) override
    {
        for(auto i = inputs.begin(); i != inputs.end();)
        {
            if(removedBlocks.count(i->block.lock()) != 0)
                i = inputs.erase(i);
            else
                ++i;
        }
    }
    virtual void replaceBlock(std::shared_ptr<SSABasicBlock> searchFor, std::shared_ptr<SSABasicBlock> replaceWith) override
    {
        for(PhiInput &i : inputs)
        {
            if(i.block.lock() == searchFor)
                i.block = replaceWith;
        }
    }
};

#endif // SSA_PHI_H_INCLUDED
