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
#ifndef SSA_NODE_H_INCLUDED
#define SSA_NODE_H_INCLUDED

#include <memory>
#include <list>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

#include "context.h"
#include "types/type.h"
#include "values/value.h"
#include "util/variable.h"
#include "util/stable_vector.h"

class SSANodeVisitor;
class SSAControlTransfer;
class SSABasicBlock;
class SSAFunction;

class SSANode : public std::enable_shared_from_this<SSANode>
{
    SSANode(const SSANode &) = delete;
    SSANode &operator =(const SSANode &) = delete;
public:
    CompilerContext *const context;
    std::shared_ptr<TypeNode> type;
    SpillLocation spillLocation;
    explicit SSANode(CompilerContext *context, std::shared_ptr<TypeNode> type, SpillLocation spillLocation)
        : context(context), type(type), spillLocation(spillLocation)
    {
        assert(type != nullptr);
        assert(context != nullptr);
        assert(type->context == context);
    }
    virtual ~SSANode() = default;
    virtual void visit(SSANodeVisitor &visitor) = 0;
    virtual std::shared_ptr<ValueNode> evaluateForConstants(const std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> &values) const
    {
        return nullptr;
    }
    virtual std::list<std::shared_ptr<SSANode>> getInputs() const = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
    struct ReplacementNode final
    {
        std::shared_ptr<SSANode> newNode;
        bool isPreexistingNode;
        ReplacementNode(std::shared_ptr<SSANode> newNode, bool isPreexistingNode)
            : newNode(newNode), isPreexistingNode(isPreexistingNode)
        {
        }
    };
    static std::shared_ptr<SSANode> replaceNode(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements, std::shared_ptr<SSANode> node)
    {
        auto iter = replacements.find(node);
        if(iter == replacements.end())
            return node;
        return std::get<1>(*iter).newNode;
    }
    virtual void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, ReplacementNode> &replacements) = 0;
    virtual void removeBlocks(const std::unordered_set<std::shared_ptr<SSABasicBlock>> &removedBlocks)
    {

    }
    virtual void replaceBlock(std::shared_ptr<SSABasicBlock> searchFor, std::shared_ptr<SSABasicBlock> replaceWith)
    {

    }
    virtual void verify(std::shared_ptr<SSABasicBlock> containingBlock, std::shared_ptr<SSAFunction> containingFunction) = 0;
};

class SSABasicBlock : public std::enable_shared_from_this<SSABasicBlock>
{
public:
    CompilerContext *const context;
    explicit SSABasicBlock(CompilerContext *context)
        : context(context)
    {
    }
    std::list<std::weak_ptr<SSABasicBlock>> sourceBlocks;
    std::weak_ptr<SSABasicBlock> immediateDominator;
    std::list<std::weak_ptr<SSABasicBlock>> dominatedBlocks;
    std::list<std::weak_ptr<SSABasicBlock>> destBlocks;
    std::shared_ptr<SSAControlTransfer> controlTransferInstruction;
    stable_vector<std::shared_ptr<SSANode>> instructions; /// all SSAPhi nodes must be first and the only allowed SSAControlTransfer must be last
    void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> &replacements)
    {
        auto iter = replacements.find(std::static_pointer_cast<SSANode>(controlTransferInstruction));
        if(iter != replacements.end())
            controlTransferInstruction = std::dynamic_pointer_cast<SSAControlTransfer>(std::get<1>(*iter).newNode);
        for(auto i = instructions.begin(); i != instructions.end();)
        {
            std::shared_ptr<SSANode> &node = *i;
            auto iter = replacements.find(node);
            if(iter == replacements.end())
            {
                node->replaceNodes(replacements);
                ++i;
                continue;
            }
            SSANode::ReplacementNode replacementNode = std::get<1>(*iter);
            if(replacementNode.isPreexistingNode && replacementNode.newNode != node)
            {
                i = instructions.erase(i);
                continue;
            }
            node = replacementNode.newNode;
            node->replaceNodes(replacements);
        }
    }
    void replaceBlock(std::shared_ptr<SSABasicBlock> searchFor, std::shared_ptr<SSABasicBlock> replaceWith)
    {
        if(immediateDominator.lock() == searchFor)
            immediateDominator = replaceWith;
        auto searchForIterator = sourceBlocks.end();
        auto replaceWithIterator = sourceBlocks.end();
        for(auto i = sourceBlocks.begin(); i != sourceBlocks.end(); ++i)
        {
            if(i->lock() == searchFor)
                searchForIterator = i;
            if(i->lock() == replaceWith)
                replaceWithIterator = i;
        }
        if(searchForIterator != sourceBlocks.end())
        {
            if(replaceWithIterator != sourceBlocks.end())
            {
                sourceBlocks.erase(searchForIterator);
            }
            else
                *searchForIterator = replaceWith;
        }
        searchForIterator = destBlocks.end();
        replaceWithIterator = destBlocks.end();
        for(auto i = destBlocks.begin(); i != destBlocks.end(); ++i)
        {
            if(i->lock() == searchFor)
                searchForIterator = i;
            if(i->lock() == replaceWith)
                replaceWithIterator = i;
        }
        if(searchForIterator != destBlocks.end())
        {
            if(replaceWithIterator != destBlocks.end())
            {
                destBlocks.erase(searchForIterator);
            }
            else
                *searchForIterator = replaceWith;
        }
        searchForIterator = dominatedBlocks.end();
        replaceWithIterator = dominatedBlocks.end();
        for(auto i = dominatedBlocks.begin(); i != dominatedBlocks.end(); ++i)
        {
            if(i->lock() == searchFor)
                searchForIterator = i;
            if(i->lock() == replaceWith)
                replaceWithIterator = i;
        }
        if(searchForIterator != dominatedBlocks.end())
        {
            if(replaceWithIterator != dominatedBlocks.end())
            {
                dominatedBlocks.erase(searchForIterator);
            }
            else
                *searchForIterator = replaceWith;
        }
        for(std::shared_ptr<SSANode> node : instructions)
        {
            node->replaceBlock(searchFor, replaceWith);
        }
    }
    void verify(std::shared_ptr<SSAFunction> containingFunction);
};

#include "ssa/ssa_visitor.h"
#include "ssa/ssa_phi.h" // must be before ssa_control_transfer.h
#include "ssa/ssa_control_transfer.h"

inline void SSABasicBlock::verify(std::shared_ptr<SSAFunction> containingFunction)
{
    bool gotNonPhi = false;
    for(std::shared_ptr<SSANode> node : instructions)
    {
        assert(node);
        node->verify(shared_from_this(), containingFunction);
        assert(node == controlTransferInstruction || dynamic_cast<const SSAControlTransfer *>(node.get()) == nullptr);
        if(dynamic_cast<const SSAPhi *>(node.get()))
        {
            if(gotNonPhi)
                assert(!"phi instructions are not before others");
        }
        else
            gotNonPhi = true;
    }
    assert(destBlocks.size() == (controlTransferInstruction ? controlTransferInstruction->destBlocks.size() : 0));
    for(std::weak_ptr<SSABasicBlock> destBlockW : destBlocks)
    {
        assert(controlTransferInstruction != nullptr);
        std::shared_ptr<SSABasicBlock> destBlock = destBlockW.lock();
        assert(destBlock);
        bool found = false;
        for(std::weak_ptr<SSABasicBlock> destBlock2W : controlTransferInstruction->destBlocks)
        {
            std::shared_ptr<SSABasicBlock> destBlock2 = destBlock2W.lock();
            assert(destBlock2);
            if(destBlock2 == destBlock)
            {
                found = true;
                break;
            }
        }
        if(!found)
            assert(!"dest block not found in control transfer instruction");
    }
}

class SSAFunction : public std::enable_shared_from_this<SSAFunction>
{
public:
    CompilerContext *const context;
    explicit SSAFunction(CompilerContext *context)
        : context(context)
    {
    }
    std::list<std::shared_ptr<SSABasicBlock>> blocks;
    std::shared_ptr<SSABasicBlock> startBlock;
    std::list<std::shared_ptr<SSANode>> parameters;
    std::shared_ptr<SSANode> returnValue;
    void replaceNodes(const std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> &replacements)
    {
        auto iter = replacements.find(returnValue);
        if(iter != replacements.end())
            returnValue = std::get<1>(*iter).newNode;
        for(std::shared_ptr<SSABasicBlock> block : blocks)
        {
            block->replaceNodes(replacements);
        }
        for(std::shared_ptr<SSANode> &node : parameters)
        {
            auto iter = replacements.find(node);
            if(iter != replacements.end())
                node = std::get<1>(*iter).newNode;
        }
    }
    void replaceBlock(std::shared_ptr<SSABasicBlock> searchFor, std::shared_ptr<SSABasicBlock> replaceWith)
    {
        if(startBlock == searchFor)
            startBlock = replaceWith;
        auto searchForIterator = blocks.end();
        auto replaceWithIterator = blocks.end();
        for(auto i = blocks.begin(); i != blocks.end(); ++i)
        {
            if(*i == searchFor)
                searchForIterator = i;
            if(*i == replaceWith)
                replaceWithIterator = i;
        }
        if(searchForIterator != blocks.end())
        {
            if(replaceWithIterator != blocks.end())
            {
                blocks.erase(searchForIterator);
            }
            else
                *searchForIterator = replaceWith;
        }
        for(std::shared_ptr<SSABasicBlock> block : blocks)
        {
            block->replaceBlock(searchFor, replaceWith);
        }
    }
    void mergeBlocks(std::shared_ptr<SSABasicBlock> firstBlock, std::shared_ptr<SSABasicBlock> secondBlock)
    {
        assert(firstBlock->destBlocks.size() == 1);
        assert(secondBlock->sourceBlocks.size() == 1);
        assert(firstBlock->controlTransferInstruction != nullptr);
        assert(firstBlock->instructions.back() == firstBlock->controlTransferInstruction);
        while(!secondBlock->instructions.empty())
        {
            std::shared_ptr<SSAPhi> phi = std::dynamic_pointer_cast<SSAPhi>(secondBlock->instructions.front());
            if(phi == nullptr)
                break;
            assert(phi->inputs.size() == 1);
            assert(phi->inputs.front().block.lock() == firstBlock);
            std::shared_ptr<SSANode> replacementNode = phi->inputs.front().node.lock();
            assert(replacementNode != nullptr);
            std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> replacements;
            replacements.emplace(phi, SSANode::ReplacementNode(replacementNode, true));
            replaceNodes(replacements); // erases phi
        }
        firstBlock->instructions.pop_back();
        firstBlock->instructions.splice(firstBlock->instructions.end(), secondBlock->instructions);
        firstBlock->controlTransferInstruction = secondBlock->controlTransferInstruction;
        replaceBlock(secondBlock, firstBlock);
        firstBlock->destBlocks = secondBlock->destBlocks;
        assert(firstBlock->controlTransferInstruction == nullptr || firstBlock->instructions.back() == firstBlock->controlTransferInstruction);
    }
    std::shared_ptr<SSABasicBlock> splitEdge(std::shared_ptr<SSABasicBlock> firstBlock, std::shared_ptr<SSABasicBlock> secondBlock)
    {
        std::shared_ptr<SSABasicBlock> retval = std::make_shared<SSABasicBlock>(context);
        retval->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, secondBlock);
        retval->instructions.push_back(retval->controlTransferInstruction);
        retval->immediateDominator = firstBlock;
        retval->sourceBlocks.push_back(firstBlock);
        retval->destBlocks.push_back(secondBlock);
        if(secondBlock->immediateDominator.lock() == firstBlock)
        {
            secondBlock->immediateDominator = retval;
            for(std::shared_ptr<SSABasicBlock> block : blocks)
            {
                for(std::shared_ptr<SSABasicBlock> i = block->immediateDominator.lock(); i != nullptr; i = i->immediateDominator.lock())
                {
                    if(i == retval)
                    {
                        retval->dominatedBlocks.push_back(block);
                        break;
                    }
                }
            }
        }
        firstBlock->replaceBlock(secondBlock, retval);
        if(firstBlock != secondBlock)
            secondBlock->replaceBlock(firstBlock, retval);
        firstBlock->dominatedBlocks.push_back(retval);
        retval->dominatedBlocks.push_back(retval);
        blocks.push_back(retval);
        return retval;
    }
    void verify()
    {
        for(std::shared_ptr<SSABasicBlock> block : blocks)
            block->verify(shared_from_this());
    }
};

#endif // SSA_NODE_H_INCLUDED
