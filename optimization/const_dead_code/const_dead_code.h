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
#ifndef CONST_DEAD_CODE_H_INCLUDED
#define CONST_DEAD_CODE_H_INCLUDED

#include "../../ssa/ssa_nodes.h"
#include "../../types/types.h"
#include "../../values/values.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../../construct_basic_block_graph.h"
#include "../../dump.h"
#include <iostream>
#include <cassert>

/** uses Sparse Conditional Constant Propagation
 */
class ConstantPropagationAndDeadCodeElimination final
{
private:
    bool isValueUndefined(std::shared_ptr<ValueNode> node)
    {
        if(dynamic_cast<const ValueUnknown *>(node.get()) != nullptr)
            return true;
        return false;
    }
    bool isValueVarying(std::shared_ptr<ValueNode> node)
    {
        return node == nullptr;
    }
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<ValueNode>> values;
        std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<SSABasicBlock>> blocks;
        std::unordered_map<std::shared_ptr<SSAControlTransfer>, std::unordered_set<std::shared_ptr<SSABasicBlock>>> targetSets;
        const std::shared_ptr<ValueNode> undefined = std::make_shared<ValueUnknown>(function->context);
        const std::shared_ptr<ValueNode> varying = nullptr;
        std::size_t blockCount = 0;
        for(std::shared_ptr<SSABasicBlock> basicBlock : function->blocks)
        {
            blockCount++;
            for(std::shared_ptr<SSANode> node : basicBlock->instructions)
            {
                values[node] = undefined;
                blocks[node] = basicBlock;
            }
        }
        std::unordered_set<std::shared_ptr<SSABasicBlock>> usedBlocks;
        usedBlocks.insert(function->startBlock);
        std::vector<std::shared_ptr<SSABasicBlock>> currentUsedBlocks;
        currentUsedBlocks.reserve(blockCount);
        for(bool done = false; !done;)
        {
            done = true;
            currentUsedBlocks.assign(usedBlocks.begin(), usedBlocks.end());
            for(std::shared_ptr<SSABasicBlock> basicBlock : currentUsedBlocks)
            {
                for(std::shared_ptr<SSANode> node : basicBlock->instructions)
                {
                    std::shared_ptr<ValueNode> &value = values[node];
                    if(isValueVarying(value))
                        continue;
                    std::shared_ptr<ValueNode> newValue = node->evaluateForConstants(values);
                    if(isValueUndefined(value) && isValueUndefined(newValue))
                        continue;
                    if(isValueVarying(newValue))
                    {
                        value = varying;
                        done = false;
                    }
                    else if(isValueUndefined(value))
                    {
                        value = newValue;
                        done = false;
                    }
                    else if(*value != *newValue)
                    {
                        value = varying;
                        done = false;
                    }
                }
                if(basicBlock->controlTransferInstruction != nullptr)
                {
                    std::unordered_set<std::shared_ptr<SSABasicBlock>> &currentTargetSet = targetSets[basicBlock->controlTransferInstruction];
                    for(std::weak_ptr<SSABasicBlock> iW : basicBlock->controlTransferInstruction->evaluateControlForConstants(values))
                    {
                        std::shared_ptr<SSABasicBlock> i = iW.lock();
                        currentTargetSet.insert(i);
                        if(std::get<1>(usedBlocks.insert(i)))
                            done = false;
                    }
                }
            }
        }
        std::list<std::shared_ptr<SSABasicBlock>> usedBlocksWorkList(usedBlocks.begin(), usedBlocks.end());
        while(!usedBlocksWorkList.empty())
        {
            std::shared_ptr<SSABasicBlock> basicBlock = usedBlocksWorkList.front();
            usedBlocksWorkList.pop_front();
            for(std::shared_ptr<SSANode> node : basicBlock->instructions)
            {
                for(std::shared_ptr<SSANode> inputNode : node->getInputs())
                {
                    std::shared_ptr<SSABasicBlock> inputBlock = blocks[inputNode];
                    if(std::get<1>(usedBlocks.insert(inputBlock)))
                        usedBlocksWorkList.push_back(inputBlock);
                    if(usedBlocks.size() >= blockCount)
                        break;
                }
                if(usedBlocks.size() >= blockCount)
                    break;
            }
            if(usedBlocks.size() >= blockCount)
                break;
        }
        usedBlocksWorkList.clear();
        std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> nodeReplacementMap;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<ValueNode> value = values[node];
                if(node->hasSideEffects())
                    continue;
                std::shared_ptr<SSAControlTransfer> controlTransferNode = std::dynamic_pointer_cast<SSAControlTransfer>(node);
                if(controlTransferNode != nullptr)
                {
                    if(dynamic_cast<const SSAUnconditionalJump *>(controlTransferNode.get()) != nullptr)
                        continue;
                    const std::unordered_set<std::shared_ptr<SSABasicBlock>> &targetSet = targetSets[controlTransferNode];
                    if(targetSet.size() != 1)
                        continue;
                    std::shared_ptr<SSANode> replacementNode = std::make_shared<SSAUnconditionalJump>(function->context, *targetSet.begin());
                    assert(replacementNode != nullptr);
                    nodeReplacementMap.emplace(node, SSANode::ReplacementNode(replacementNode, false));
                    values[replacementNode] = nullptr;
                    blocks[replacementNode] = block;
                }
                if(dynamic_cast<const SSAConstant *>(node.get()) != nullptr)
                    continue;
                if(node->hasSideEffects())
                    continue;
                if(isValueUndefined(value) || isValueVarying(value))
                    continue;
                std::shared_ptr<SSANode> replacementNode = std::make_shared<SSAConstant>(value);
                assert(replacementNode != nullptr);
                nodeReplacementMap.emplace(node, SSANode::ReplacementNode(replacementNode, false));
                values[replacementNode] = value;
                blocks[replacementNode] = block;
            }
        }
        function->replaceNodes(nodeReplacementMap);
        std::unordered_set<std::shared_ptr<SSANode>> usedNodes;
        if(function->returnValue != nullptr)
            usedNodes.insert(function->returnValue);
        for(std::shared_ptr<SSANode> node : function->parameters)
            usedNodes.insert(node);
        for(std::shared_ptr<SSABasicBlock> basicBlock : usedBlocks)
        {
            for(std::shared_ptr<SSANode> node : basicBlock->instructions)
            {
                if(node->hasSideEffects())
                    usedNodes.insert(node);
            }
            if(basicBlock->controlTransferInstruction != nullptr)
                usedNodes.insert(basicBlock->controlTransferInstruction);
        }
        std::list<std::shared_ptr<SSANode>> usedNodesWorkList(usedNodes.begin(), usedNodes.end());
        while(!usedNodesWorkList.empty())
        {
            std::shared_ptr<SSANode> node = usedNodesWorkList.front();
            usedNodesWorkList.pop_front();
            for(std::shared_ptr<SSANode> inputNode : node->getInputs())
            {
                if(std::get<1>(usedNodes.insert(inputNode)))
                    usedNodesWorkList.push_back(inputNode);
            }
        }
        usedBlocks.insert(function->endBlock); // need end block even for functions that don't return
        std::unordered_set<std::shared_ptr<SSABasicBlock>> removedBlocks;
        for(auto i = function->blocks.begin(); i != function->blocks.end();)
        {
            if(usedBlocks.count(*i) != 0)
                ++i;
            else
            {
                removedBlocks.insert(*i);
                i = function->blocks.erase(i);
            }
        }
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(auto i = block->instructions.begin(); i != block->instructions.end();)
            {
                if(usedNodes.count(*i) != 0)
                {
                    (*i)->removeBlocks(removedBlocks);
                    ++i;
                }
                else
                    i = block->instructions.erase(i);
            }
        }
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
    }
};

#endif // CONST_DEAD_CODE_H_INCLUDED
