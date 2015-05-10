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

#include "ssa/ssa_nodes.h"
#include "rtl/rtl_nodes.h"
#include "types/types.h"
#include "values/values.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "construct_basic_block_graph.h"
#include "construct_liveness_info.h"
#include "dump.h"
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
            if(done)
            {
                for(std::shared_ptr<SSABasicBlock> basicBlock : currentUsedBlocks)
                {
                    for(std::shared_ptr<SSANode> node : basicBlock->instructions)
                    {
                        std::shared_ptr<ValueNode> &value = values[node];
                        if(isValueUndefined(value))
                        {
                            value = varying;
                            done = false;
                        }
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
                    std::shared_ptr<SSABasicBlock> target = *targetSet.begin();
                    std::shared_ptr<SSANode> replacementNode = std::make_shared<SSAUnconditionalJump>(function->context, target);
                    assert(replacementNode != nullptr);
                    nodeReplacementMap.emplace(node, SSANode::ReplacementNode(replacementNode, false));
                    values[replacementNode] = nullptr;
                    blocks[replacementNode] = block;
                    for(std::weak_ptr<SSABasicBlock> oldTargetW : controlTransferNode->destBlocks)
                    {
                        std::shared_ptr<SSABasicBlock> oldTarget = oldTargetW.lock();
                        if(oldTarget == target)
                            continue;
                        for(std::shared_ptr<SSANode> node2 : oldTarget->instructions)
                        {
                            if(std::shared_ptr<SSAPhi> phi = std::dynamic_pointer_cast<SSAPhi>(node2))
                            {
                                for(auto i = phi->inputs.begin(); i != phi->inputs.end();)
                                {
                                    const SSAPhi::PhiInput &input = *i;
                                    if(input.block.lock() == block)
                                        i = phi->inputs.erase(i);
                                    else
                                        ++i;
                                }
                            }
                            else // all phis are at the beginning
                                break;
                        }
                    }
                }
                if(dynamic_cast<const SSAConstant *>(node.get()) != nullptr)
                    continue;
                if(node->hasSideEffects())
                    continue;
                if(isValueUndefined(value) || isValueVarying(value))
                    continue;
                std::shared_ptr<SSANode> replacementNode = std::make_shared<SSAConstant>(value, nullptr);
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
    void visitRTLFunction(std::shared_ptr<RTLFunction> function)
    {
        const std::shared_ptr<ValueNode> undefined = std::make_shared<ValueUnknown>(function->context);
        const std::shared_ptr<ValueNode> varying = nullptr;
        std::unordered_map<std::shared_ptr<RTLBasicBlock>, std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>>> blockRegisterStartValueMapMap;
        std::unordered_set<std::shared_ptr<RTLRegister>> registers;
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<RTLNode> node : block->instructions)
            {
                for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v : node->getInputRegisters())
                {
                    registers.insert(std::get<0>(v));
                }
                for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v : node->getOutputRegisters())
                {
                    registers.insert(std::get<0>(v));
                }
            }
        }
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> &registerStartValueMap = blockRegisterStartValueMapMap[block];
            for(std::shared_ptr<RTLRegister> r : registers)
            {
                registerStartValueMap[r] = undefined;
            }
        }
        std::unordered_set<std::shared_ptr<RTLBasicBlock>> usedBlocksSet;
        usedBlocksSet.insert(function->startBlock);
        bool done = false;
        while(!done)
        {
            done = true;
            std::vector<std::shared_ptr<RTLBasicBlock>> blockVisitList(usedBlocksSet.begin(), usedBlocksSet.end());
            for(std::shared_ptr<RTLBasicBlock> block : blockVisitList)
            {
                std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> registerValueMap = blockRegisterStartValueMapMap[block];
                std::list<std::shared_ptr<RTLBasicBlock>> targetBlocks;
                for(std::shared_ptr<RTLNode> node : block->instructions)
                {
                    std::shared_ptr<RTLControlTransfer> controlTransfer = std::dynamic_pointer_cast<RTLControlTransfer>(node);
                    if(controlTransfer != nullptr)
                        targetBlocks = controlTransfer->evaluateControlForConstants(registerValueMap);
                    for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> registerValue : node->evaluateForConstants(registerValueMap))
                    {
                        std::shared_ptr<RTLRegister> r = std::get<0>(registerValue);
                        std::shared_ptr<ValueNode> &value = registerValueMap[r];
                        std::shared_ptr<ValueNode> newValue = std::get<1>(registerValue);
                        value = newValue;
                    }
                }
                for(std::shared_ptr<RTLBasicBlock> targetBlock : targetBlocks)
                {
                    if(std::get<1>(usedBlocksSet.insert(targetBlock)))
                        done = false;
                    std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> &targetBlockRegisterValueMap = blockRegisterStartValueMapMap[targetBlock];
                    for(std::shared_ptr<RTLRegister> r : registers)
                    {
                        std::shared_ptr<ValueNode> &value = targetBlockRegisterValueMap[r];
                        std::shared_ptr<ValueNode> newValue = registerValueMap[r];
                        if(isValueUndefined(newValue) || isValueVarying(value))
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
                }
            }
            if(done)
            {
                for(std::shared_ptr<RTLBasicBlock> block : blockVisitList)
                {
                    std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> &registerStartValueMap = blockRegisterStartValueMapMap[block];
                    for(std::shared_ptr<RTLRegister> r : registers)
                    {
                        std::shared_ptr<ValueNode> &value = registerStartValueMap[r];
                        if(isValueUndefined(value))
                        {
                            done = false;
                            value = varying;
                        }
                    }
                }
            }
        }
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            std::unordered_map<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> registerValueMap = blockRegisterStartValueMapMap[block];
            std::list<std::shared_ptr<RTLBasicBlock>> targetBlocks;
            bool canRewriteControlTransfer = true;
            for(auto i = block->instructions.begin(); i != block->instructions.end(); )
            {
                std::shared_ptr<RTLNode> node = *i;
                std::shared_ptr<RTLControlTransfer> controlTransfer = std::dynamic_pointer_cast<RTLControlTransfer>(node);
                bool canRewrite = true;
                if(node->hasSideEffects())
                {
                    if(controlTransfer != nullptr)
                        canRewriteControlTransfer = false;
                    canRewrite = false;
                }
                if(controlTransfer != nullptr)
                {
                    targetBlocks = controlTransfer->evaluateControlForConstants(registerValueMap);
                    canRewrite = false;
                }
                if(dynamic_cast<const RTLLoadConstant *>(node.get()) != nullptr)
                {
                    canRewrite = false;
                }
                std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>>> registerValuesList = node->evaluateForConstants(registerValueMap);
                for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> registerValue : registerValuesList)
                {
                    if(controlTransfer != nullptr)
                        canRewriteControlTransfer = false; // control transfer instruction writes to registers
                    std::shared_ptr<RTLRegister> r = std::get<0>(registerValue);
                    std::shared_ptr<ValueNode> &value = registerValueMap[r];
                    std::shared_ptr<ValueNode> newValue = std::get<1>(registerValue);
                    value = newValue;
                    if(isValueUndefined(value) || isValueVarying(value))
                    {
                        canRewrite = false;
                    }
                }
                if(!canRewrite)
                {
                    ++i;
                    continue;
                }
                i = block->instructions.erase(i);
                for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<ValueNode>> registerValue : registerValuesList)
                {
                    if(controlTransfer != nullptr)
                        canRewriteControlTransfer = false; // control transfer instruction writes to registers
                    std::shared_ptr<RTLRegister> r = std::get<0>(registerValue);
                    std::shared_ptr<ValueNode> value = registerValueMap[r];
                    block->instructions.insert(i, std::make_shared<RTLLoadConstant>(r, value));
                }
            }
            if(dynamic_cast<const RTLUnconditionalJump *>(block->controlTransferInstruction.get()) != nullptr)
                continue;
            if(canRewriteControlTransfer)
            {
                if(targetBlocks.size() == 1)
                {
                    assert(block->controlTransferInstruction == block->instructions.back());
                    block->controlTransferInstruction = std::make_shared<RTLUnconditionalJump>(targetBlocks.front());
                    block->instructions.back() = block->controlTransferInstruction;
                }
            }
        }
        ConstructBasicBlockGraphVisitor().visitRTLFunction(function);
        std::unordered_set<std::shared_ptr<RTLBasicBlock>> reachableBlocks;
        reachableBlocks.insert(function->startBlock);
        std::vector<std::shared_ptr<RTLBasicBlock>> reachableBlocksWorkList;
        reachableBlocksWorkList.push_back(function->startBlock);
        while(!reachableBlocksWorkList.empty())
        {
            std::shared_ptr<RTLBasicBlock> block = reachableBlocksWorkList.back();
            reachableBlocksWorkList.pop_back();
            for(std::weak_ptr<RTLBasicBlock> targetBlockW : block->destBlocks)
            {
                std::shared_ptr<RTLBasicBlock> targetBlock = targetBlockW.lock();
                if(std::get<1>(reachableBlocks.insert(targetBlock)))
                {
                    reachableBlocksWorkList.push_back(targetBlock);
                }
            }
        }
        std::vector<std::shared_ptr<RTLBasicBlock>> unreachableBlocks;
        unreachableBlocks.reserve(function->blocks.size() - reachableBlocks.size());
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            if(reachableBlocks.count(block) != 0)
                continue;
            unreachableBlocks.push_back(block);
        }
        for(auto i = function->blocks.begin(); i != function->blocks.end(); )
        {
            std::shared_ptr<RTLBasicBlock> block = *i;
            if(reachableBlocks.count(block) != 0)
            {
                for(std::shared_ptr<RTLNode> node : block->instructions)
                {
                    for(std::shared_ptr<RTLBasicBlock> b : unreachableBlocks)
                    {
                        node->handleRemoveBasicBlock(b);
                    }
                }
                ++i;
            }
            else
                i = function->blocks.erase(i);
        }
        std::unordered_set<std::shared_ptr<RTLNode>> usedNodesSet;
        std::unordered_map<std::shared_ptr<RTLBasicBlock>, std::unordered_set<std::shared_ptr<RTLRegister>>> blockUsedRegistersAtEndSetMap;
        done = false;
        while(!done)
        {
            done = true;
            for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
            {
                if(block->controlTransferInstruction != nullptr)
                    if(std::get<1>(usedNodesSet.insert(block->controlTransferInstruction)))
                        done = false;
                std::unordered_set<std::shared_ptr<RTLRegister>> usedRegistersSet = blockUsedRegistersAtEndSetMap[block];
                for(auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
                {
                    std::shared_ptr<RTLNode> node = *i;
                    if(node->hasSideEffects())
                        if(std::get<1>(usedNodesSet.insert(node)))
                            done = false;
                    for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v : node->getOutputRegisters())
                    {
                        std::shared_ptr<RTLRegister> r = std::get<0>(v);
                        if(usedRegistersSet.erase(r) > 0)
                            if(std::get<1>(usedNodesSet.insert(node)))
                                done = false;
                    }
                    if(usedNodesSet.count(node) > 0)
                    {
                        for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v : node->getInputRegisters())
                        {
                            std::shared_ptr<RTLRegister> r = std::get<0>(v);
                            usedRegistersSet.insert(r);
                        }
                    }
                }
                for(std::weak_ptr<RTLBasicBlock> predecessorBlockW : block->sourceBlocks)
                {
                    std::shared_ptr<RTLBasicBlock> predecessorBlock = predecessorBlockW.lock();
                    std::unordered_set<std::shared_ptr<RTLRegister>> &predecessorBlockUsedRegistersSet = blockUsedRegistersAtEndSetMap[predecessorBlock];
                    for(std::shared_ptr<RTLRegister> r : usedRegistersSet)
                    {
                        if(std::get<1>(predecessorBlockUsedRegistersSet.insert(r)))
                            done = false;
                    }
                }
            }
        }
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            for(auto i = block->instructions.begin(); i != block->instructions.end(); )
            {
                const std::shared_ptr<RTLNode> &node = *i;
                if(usedNodesSet.count(node) > 0)
                    ++i;
                else
                    i = block->instructions.erase(i);
            }
        }
        ConstructLivenessInfo().visitRTLFunction(function);
    }
};

#endif // CONST_DEAD_CODE_H_INCLUDED
