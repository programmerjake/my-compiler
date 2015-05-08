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
#ifndef CONVERT_SSA_TO_RTL_H_INCLUDED
#define CONVERT_SSA_TO_RTL_H_INCLUDED

#include "ssa/ssa_visitor.h"
#include "types/type.h"
#include "values/value.h"
#include "rtl/rtl_nodes.h"
#include "optimization/control_flow_simplification/control_flow_simplification.h"
#include "optimization/phi_removal/phi_removal.h"
#include "dump.h"
#include "construct_liveness_info.h"
#include "construct_basic_block_graph.h"

#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <deque>
#include <list>
#include <sstream>
#include <iostream>

class ConvertSSAToRTL final : public SSANodeVisitor
{
private:
    std::unordered_map<std::shared_ptr<SSAFunction>, std::shared_ptr<RTLFunction>> functionMap;
    std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<RTLBasicBlock>> basicBlockMap;
    std::shared_ptr<RTLBasicBlock> getOrMakeRTLBasicBlock(std::shared_ptr<SSABasicBlock> node)
    {
        std::shared_ptr<RTLBasicBlock> &retval = basicBlockMap[node];
        if(retval == nullptr)
            retval = std::make_shared<RTLBasicBlock>(node->context);
        return retval;
    }
    std::shared_ptr<RTLFunction> getOrMakeRTLFunction(std::shared_ptr<SSAFunction> node)
    {
        std::shared_ptr<RTLFunction> &retval = functionMap[node];
        if(retval == nullptr)
            retval = std::make_shared<RTLFunction>(node->context);
        return retval;
    }
    std::shared_ptr<RTLBasicBlock> currentlyGeneratingBasicBlock;
    std::shared_ptr<RTLFunction> currentlyGeneratingFunction;
    std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<RTLRegister>> registerMap;
    std::unordered_map<std::shared_ptr<RTLRegister>, std::unordered_set<std::shared_ptr<SSANode>>> reverseRegisterMap;
    std::size_t nextVirtualRegisterName = 0;
    std::string makeVirtualRegisterName()
    {
        std::ostringstream ss;
        ss << "virtual#" << ++nextVirtualRegisterName;
        return ss.str();
    }
public:
    virtual void visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node) override
    {
        currentlyGeneratingBasicBlock->controlTransferInstruction = std::make_shared<RTLUnconditionalJump>(getOrMakeRTLBasicBlock(node->destBlocks.front().lock()));
        currentlyGeneratingBasicBlock->instructions.push_back(currentlyGeneratingBasicBlock->controlTransferInstruction);
    }
    virtual void visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node) override
    {
        currentlyGeneratingBasicBlock->controlTransferInstruction = std::make_shared<RTLConditionalJump>(registerMap[node->condition.lock()], getOrMakeRTLBasicBlock(node->destBlocks.front().lock()), getOrMakeRTLBasicBlock(node->destBlocks.back().lock()));
        currentlyGeneratingBasicBlock->instructions.push_back(currentlyGeneratingBasicBlock->controlTransferInstruction);
    }
    virtual void visitSSAPhi(std::shared_ptr<SSAPhi> node) override
    {
        std::shared_ptr<RTLRegister> sourceRegister;
#ifndef NDEBUG
        bool isFirst = true;
        for(SSAPhi::PhiInput i : node->inputs)
        {
            if(isFirst)
            {
                isFirst = false;
                sourceRegister = registerMap[i.node.lock()];
            }
            else
            {
                assert(sourceRegister == registerMap[i.node.lock()]);
            }
        }
        assert(!isFirst);
#else
        sourceRegister = registerMap[node->inputs.front().node.lock()];
#endif
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLMove>(registerMap[node], sourceRegister, node->type);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSAConstant(std::shared_ptr<SSAConstant> node) override
    {
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLLoadConstant>(registerMap[node], node->value);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSAMove(std::shared_ptr<SSAMove> node) override
    {
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLMove>(registerMap[node], registerMap[node->source.lock()], node->type);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSALoad(std::shared_ptr<SSALoad> node) override
    {
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLLoad>(registerMap[node], registerMap[node->address.lock()], node->address.lock()->type);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSAStore(std::shared_ptr<SSAStore> node) override
    {
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLStore>(registerMap[node->address.lock()], registerMap[node->value.lock()], node->address.lock()->type);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSACompare(std::shared_ptr<SSACompare> node) override
    {
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLCompare>(registerMap[node], registerMap[node->lhs.lock()], registerMap[node->rhs.lock()], node->compareOperator, node->lhs.lock()->type);
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
    virtual void visitSSAAllocA(std::shared_ptr<SSAAllocA> node) override
    {
        std::shared_ptr<VariableDescriptor> variableDescriptor = node->getVariableDescriptor();
        variableDescriptor->allocate(currentlyGeneratingFunction->localVariablesSize);
        std::shared_ptr<RTLNode> newNode = std::make_shared<RTLLoadConstant>(registerMap[node], std::make_shared<ValueVariablePointer>(node->context, VariableLocation(variableDescriptor), node->type->dereference()));
        currentlyGeneratingBasicBlock->instructions.push_back(newNode);
    }
private:
    void visitSSANode(std::shared_ptr<SSANode> node)
    {
        node->visit(*this);
    }
    void visitSSABasicBlock(std::shared_ptr<SSABasicBlock> block)
    {
        currentlyGeneratingBasicBlock = getOrMakeRTLBasicBlock(block);
        currentlyGeneratingFunction->blocks.push_back(currentlyGeneratingBasicBlock);
        for(std::weak_ptr<SSABasicBlock> sourceBlockW : block->sourceBlocks)
        {
            std::shared_ptr<SSABasicBlock> sourceBlock = sourceBlockW.lock();
            currentlyGeneratingBasicBlock->sourceBlocks.push_back(getOrMakeRTLBasicBlock(sourceBlock));
        }
        for(std::weak_ptr<SSABasicBlock> destBlockW : block->destBlocks)
        {
            std::shared_ptr<SSABasicBlock> destBlock = destBlockW.lock();
            currentlyGeneratingBasicBlock->destBlocks.push_back(getOrMakeRTLBasicBlock(destBlock));
        }
        for(std::shared_ptr<SSANode> node : block->instructions)
        {
            visitSSANode(node);
        }
    }
    void clearAllButFunctionMap()
    {
        currentlyGeneratingBasicBlock = nullptr;
        currentlyGeneratingFunction = nullptr;
        registerMap.clear();
        reverseRegisterMap.clear();
    }
public:
    std::shared_ptr<RTLFunction> visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        clearAllButFunctionMap();
        PhiRemoval().visitSSAFunction(function);
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        currentlyGeneratingFunction = getOrMakeRTLFunction(function);
        std::deque<std::pair<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSABasicBlock>>> fixPhiEdgesWorklist;
        for(std::shared_ptr<SSABasicBlock> target : function->blocks)
        {
            getOrMakeRTLBasicBlock(target);
            if(target->instructions.empty())
                continue;
            if(dynamic_cast<const SSAPhi *>(target->instructions.front().get()) == nullptr) // all phi functions must be at front
                continue;
            for(std::weak_ptr<SSABasicBlock> sourceW : target->sourceBlocks)
            {
                std::shared_ptr<SSABasicBlock> source = sourceW.lock();
                fixPhiEdgesWorklist.emplace_back(source, target);
            }
        }
        for(std::pair<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSABasicBlock>> edge : fixPhiEdgesWorklist)
        {
            std::shared_ptr<SSABasicBlock> source = std::get<0>(edge);
            std::shared_ptr<SSABasicBlock> target = std::get<1>(edge);
            if(source->destBlocks.size() > 1 || target->sourceBlocks.size() > 1)
            {
                function->splitEdge(source, target);
            }
        }
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        fixPhiEdgesWorklist.clear();
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<SSAPhi> phi = std::dynamic_pointer_cast<SSAPhi>(node);
                if(phi == nullptr)
                    break;
                for(SSAPhi::PhiInput &i : phi->inputs)
                {
                    std::shared_ptr<SSABasicBlock> fixupBlock = i.block.lock();
                    assert(fixupBlock != nullptr);
                    assert(fixupBlock->destBlocks.size() == 1);
                    assert(fixupBlock->destBlocks.front().lock() == block);
                    auto insertPosition = fixupBlock->instructions.end();
                    --insertPosition; // skip control transfer instruction
                    std::shared_ptr<SSANode> newNode = std::make_shared<SSAMove>(i.node.lock(), phi->spillLocation);
                    i.node = newNode;
                    fixupBlock->instructions.insert(insertPosition, newNode);
                }
            }
        }
        ControlFlowSimplification().visitSSAFunction(function);
        std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>>> nodeSetMap;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>> &returnNodeSet = nodeSetMap[node];
                if(returnNodeSet == nullptr)
                {
                    returnNodeSet = std::make_shared<std::unordered_set<std::shared_ptr<SSANode>>>();
                    returnNodeSet->insert(node);
                }
                std::shared_ptr<SSAPhi> phi = std::dynamic_pointer_cast<SSAPhi>(node);
                if(phi != nullptr)
                {
                    std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>> nodeSet = nullptr;
                    for(SSAPhi::PhiInput i : phi->inputs)
                    {
                        std::shared_ptr<SSANode> inputNode = i.node.lock();
                        std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>> &inputNodeSet = nodeSetMap[inputNode];
                        if(inputNodeSet == nullptr)
                        {
                            inputNodeSet = std::make_shared<std::unordered_set<std::shared_ptr<SSANode>>>();
                            inputNodeSet->insert(inputNode);
                        }
                        if(nodeSet == nullptr)
                            nodeSet = inputNodeSet;
                        if(inputNodeSet == nodeSet)
                            continue;
                        std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>> oldInputNodeSet = inputNodeSet;
                        inputNodeSet = nodeSet;
                        for(std::shared_ptr<SSANode> otherNode : *oldInputNodeSet)
                        {
                            nodeSetMap[otherNode] = inputNodeSet;
                            returnNodeSet->insert(otherNode);
                        }
                    }
                }
            }
        }
        std::unordered_map<std::shared_ptr<std::unordered_set<std::shared_ptr<SSANode>>>, std::shared_ptr<RTLRegister>> nodeSetToRegisterMap;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<RTLRegister> &r = nodeSetToRegisterMap[nodeSetMap[node]];
                if(r == nullptr)
                {
                    r = std::make_shared<RTLRegister>(function->context, makeVirtualRegisterName(), node->spillLocation);
                }
                registerMap[node] = r;
                reverseRegisterMap[r].insert(node);
            }
        }
        nodeSetMap.clear();
        nodeSetToRegisterMap.clear();
        currentlyGeneratingFunction->startBlock = getOrMakeRTLBasicBlock(function->startBlock);
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            visitSSABasicBlock(block);
        }
        ConstructLivenessInfo().visitRTLFunction(currentlyGeneratingFunction);
        clearAllButFunctionMap();
        return functionMap[function];
    }
};

#endif // CONVERT_SSA_TO_RTL_H_INCLUDED
