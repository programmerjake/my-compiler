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
#ifndef MEMORY_TO_REGISTER_H_INCLUDED
#define MEMORY_TO_REGISTER_H_INCLUDED

#include "ssa/ssa_nodes.h"
#include "util/variable.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "construct_basic_block_graph.h"

class MemoryToRegister final
{
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_set<std::shared_ptr<SSANode>>> variableToNodeSetMap;
        std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<VariableDescriptor>> nodeToVariableMap;
        std::unordered_set<std::shared_ptr<VariableDescriptor>> variables;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<VariableDescriptor> variable = nullptr;
                if(std::shared_ptr<SSAConstant> constant = std::dynamic_pointer_cast<SSAConstant>(node))
                {
                    std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(constant->value);
                    if(valueVariablePointer != nullptr)
                    {
                        variable = valueVariablePointer->location.variable;
                        if(variable && variable->getKind() != VariableDescriptor::Kind::LocalVariable)
                            variable = nullptr;
                    }
                }
                else if(std::shared_ptr<SSAAllocA> ssaAllocA = std::dynamic_pointer_cast<SSAAllocA>(node))
                {
                    variable = ssaAllocA->getVariableDescriptor();
                }
                if(variable != nullptr)
                {
                    nodeToVariableMap[node] = variable;
                    variableToNodeSetMap[variable].insert(node);
                    variables.insert(variable);
                }
            }
        }
        std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_set<std::shared_ptr<SSABasicBlock>>> variableToUsedBasicBlockSetMap;
        std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_set<std::shared_ptr<SSABasicBlock>>> variableToFirstReferenceIsUseSetMap;
        std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>>> variableToLastStoreMapMap;
        std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_set<std::shared_ptr<SSANode>>> variableToLoadStoreSetMap;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<SSANode> node : block->instructions)
            {
                std::shared_ptr<SSANode> addressNode = nullptr;
                bool readFromAddress = false, writeToAddress = false;
                if(std::shared_ptr<SSALoad> load = std::dynamic_pointer_cast<SSALoad>(node))
                {
                    addressNode = load->address.lock();
                    readFromAddress = true;
                }
                else if(std::shared_ptr<SSAStore> store = std::dynamic_pointer_cast<SSAStore>(node))
                {
                    addressNode = store->address.lock();
                    writeToAddress = true;
                }
                // note: if more load/store node types are added then they also need to be added to the replacement node creation code

                if(addressNode != nullptr)
                {
                    auto iter = nodeToVariableMap.find(addressNode);
                    if(iter != nodeToVariableMap.end())
                    {
                        std::shared_ptr<VariableDescriptor> variable = std::get<1>(*iter);
                        variableToUsedBasicBlockSetMap[variable].insert(block);
                        variableToLoadStoreSetMap[variable].insert(node);
                        std::unordered_set<std::shared_ptr<SSABasicBlock>> &firstReferenceIsUseSet = variableToFirstReferenceIsUseSetMap[variable];
                        std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>> &lastStoreMap = variableToLastStoreMapMap[variable];
                        if(readFromAddress && lastStoreMap.count(block) == 0)
                        {
                            firstReferenceIsUseSet.insert(block);
                        }
                        if(writeToAddress)
                        {
                            lastStoreMap[block] = node;
                        }
                    }
                }
                for(std::shared_ptr<SSANode> inputNode : node->getInputs())
                {
                    if(inputNode == addressNode)
                        continue;
                    auto iter = nodeToVariableMap.find(inputNode);
                    if(iter != nodeToVariableMap.end())
                        variables.erase(std::get<1>(*iter));
                }
            }
        }
        std::vector<std::pair<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSAPhi>>> phiList;
        for(std::shared_ptr<VariableDescriptor> variable : variables)
        {
            std::shared_ptr<TypeNode> variableType = variable->getType();
            if(variableType == nullptr)
            {
                continue;
            }
            const std::unordered_set<std::shared_ptr<SSABasicBlock>> &useSet = variableToFirstReferenceIsUseSetMap[variable];
            const std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>> &lastStoreMap = variableToLastStoreMapMap[variable];
            std::unordered_set<std::shared_ptr<SSABasicBlock>> liveInSet = useSet;
            std::unordered_set<std::shared_ptr<SSABasicBlock>> liveOutSet;
            bool didAnything = true;
            while(didAnything)
            {
                didAnything = false;
                for(std::shared_ptr<SSABasicBlock> block : liveInSet)
                {
                    for(std::weak_ptr<SSABasicBlock> predecessorW : block->sourceBlocks)
                    {
                        std::shared_ptr<SSABasicBlock> predecessor = predecessorW.lock();
                        if(std::get<1>(liveOutSet.insert(predecessor)))
                            didAnything = true;
                    }
                }
                for(std::shared_ptr<SSABasicBlock> block : liveOutSet)
                {
                    if(lastStoreMap.count(block) != 0)
                        continue;
                    if(std::get<1>(liveInSet.insert(block)))
                        didAnything = true;
                }
            }
            if(liveInSet.count(function->startBlock) != 0) // live at function start : may be uninitialized so can't promote to register because it depends on memory value
            {
                continue;
            }
            phiList.clear();
            std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>> blockCurrentNodeMap;
            std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> replacementNodes;
            for(std::shared_ptr<SSABasicBlock> block : function->blocks)
            {
                std::shared_ptr<SSANode> &currentNode = blockCurrentNodeMap[block];
                if(liveInSet.count(block) != 0)
                {
                    std::shared_ptr<SSAPhi> phi = std::make_shared<SSAPhi>(variableType, SpillLocation(variable));
                    block->instructions.push_front(phi);
                    currentNode = phi;
                    phiList.emplace_back(block, phi);
                }
                for(std::shared_ptr<SSANode> node : block->instructions)
                {
                    if(std::shared_ptr<SSALoad> load = std::dynamic_pointer_cast<SSALoad>(node))
                    {
                        if(nodeToVariableMap[load->address.lock()] != variable)
                            continue;
                        assert(currentNode != nullptr);
                        replacementNodes.emplace(node, SSANode::ReplacementNode(currentNode, true));
                    }
                    else if(std::shared_ptr<SSAStore> store = std::dynamic_pointer_cast<SSAStore>(node))
                    {
                        if(nodeToVariableMap[store->address.lock()] != variable)
                            continue;
                        replacementNodes.emplace(node, SSANode::ReplacementNode(nullptr, true));
                        currentNode = store->value.lock();
                        assert(currentNode != nullptr);
                    }
                }
            }
            for(std::pair<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSAPhi>> blockAndPhi : phiList)
            {
                std::shared_ptr<SSABasicBlock> block = std::get<0>(blockAndPhi);
                std::shared_ptr<SSAPhi> phi = std::get<1>(blockAndPhi);
                for(std::weak_ptr<SSABasicBlock> predecessorW : block->sourceBlocks)
                {
                    std::shared_ptr<SSABasicBlock> predecessor = predecessorW.lock();
                    std::shared_ptr<SSANode> node = blockCurrentNodeMap[predecessor];
                    if(node == nullptr)
                        continue;
                    phi->inputs.push_back(SSAPhi::PhiInput{node, predecessor});
                }
            }
            for(std::shared_ptr<SSANode> node : variableToNodeSetMap[variable])
            {
                replacementNodes.emplace(node, SSANode::ReplacementNode(nullptr, true));
            }
            function->replaceNodes(replacementNodes);
        }
        phiList.clear();
    }
};

#endif // MEMORY_TO_REGISTER_H_INCLUDED
