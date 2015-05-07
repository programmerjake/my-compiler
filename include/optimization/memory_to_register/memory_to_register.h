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
#include "construct_basic_block_graph.h"

class MemoryToRegister final : public SSANodeVisitor
{
private:
    typedef typename random_access_list<std::shared_ptr<SSANode>>::iterator InstructionIterator;
    std::unordered_multimap<std::shared_ptr<VariableDescriptor>, InstructionIterator> addressRegisterVariableToInstructionIteratorMap;
    std::unordered_map<std::shared_ptr<SSANode>, std::shared_ptr<VariableDescriptor>> addressRegisterInstructionToVariableMap;
    std::unordered_map<std::shared_ptr<SSANode>, std::pair<std::shared_ptr<SSABasicBlock>, InstructionIterator>> fencePoints;
    std::unordered_map<std::shared_ptr<VariableDescriptor>, std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>>> variableNodesMap;
    std::unordered_set<std::shared_ptr<VariableDescriptor>> variables;
    InstructionIterator currentInstructionIterator;
    std::shared_ptr<SSABasicBlock> currentBasicBlock;
    enum class Stage
    {
        FindReferringInstructions,
        FindFencePoints,
        RewriteLoadAndStore,
    };
    Stage stage = Stage::FindReferringInstructions;
    bool didAnything = false;
    void addAddressInstruction(InstructionIterator iter, std::shared_ptr<SSANode> node, std::shared_ptr<VariableDescriptor> variable)
    {
        variables.insert(variable);
        if(std::get<1>(addressRegisterInstructionToVariableMap.emplace(node, variable)))
        {
            addressRegisterVariableToInstructionIteratorMap.emplace(variable, iter);
            didAnything = true;
        }
    }
public:
    virtual void visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        case Stage::RewriteLoadAndStore:
        {
            break;
        }
        }
    }
    virtual void visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        case Stage::RewriteLoadAndStore:
        {
            break;
        }
        }
    }
    virtual void visitSSAPhi(std::shared_ptr<SSAPhi> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            std::shared_ptr<VariableDescriptor> variable;
            bool isFirst = true;
            for(const SSAPhi::PhiInput &input : node->inputs)
            {
                std::shared_ptr<SSANode> inputNode = input.node.lock();
                std::shared_ptr<VariableDescriptor> currentVariable = nullptr;
                auto iter = addressRegisterInstructionToVariableMap.find(inputNode);
                if(iter != addressRegisterInstructionToVariableMap.end())
                    currentVariable = std::get<1>(*iter);
                if(isFirst)
                {
                    variable = currentVariable;
                    isFirst = false;
                }
                else if(variable != currentVariable)
                {
                    variable = nullptr;
                    break;
                }
            }
            if(variable != nullptr)
            {
                addAddressInstruction(currentInstructionIterator, node, variable);
            }
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        case Stage::RewriteLoadAndStore:
        {
            break;
        }
        }
    }
    virtual void visitSSAConstant(std::shared_ptr<SSAConstant> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            if(std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(node->value))
            {
                std::shared_ptr<VariableDescriptor> variable = valueVariablePointer->location.variable;
                addAddressInstruction(currentInstructionIterator, node, variable);
            }
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        case Stage::RewriteLoadAndStore:
        {
            break;
        }
        }
    }
    virtual void visitSSAMove(std::shared_ptr<SSAMove> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            auto iter = addressRegisterInstructionToVariableMap.find(node->source.lock());
            if(iter != addressRegisterInstructionToVariableMap.end())
                addAddressInstruction(currentInstructionIterator, node, std::get<1>(*iter));
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        case Stage::RewriteLoadAndStore:
        {
            break;
        }
        }
    }
    virtual void visitSSALoad(std::shared_ptr<SSALoad> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            break;
        }
        case Stage::FindFencePoints:
        {
            if(addressRegisterInstructionToVariableMap.count(node->address.lock()) == 0)
                fencePoints.emplace(node, std::pair<std::shared_ptr<SSABasicBlock>, InstructionIterator>(currentBasicBlock, currentInstructionIterator));
            break;
        }
        case Stage::RewriteLoadAndStore:
        {

            break;
        }
        }
    }
    virtual void visitSSAStore(std::shared_ptr<SSAStore> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            break;
        }
        case Stage::FindFencePoints:
        {
            if(addressRegisterInstructionToVariableMap.count(node->address.lock()) == 0)
                fencePoints.emplace(node, std::pair<std::shared_ptr<SSABasicBlock>, InstructionIterator>(currentBasicBlock, currentInstructionIterator));
            break;
        }
        }
    }
    virtual void visitSSACompare(std::shared_ptr<SSACompare> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        }
    }
    virtual void visitSSAAllocA(std::shared_ptr<SSAAllocA> node) override
    {
        switch(stage)
        {
        case Stage::FindReferringInstructions:
        {
            std::shared_ptr<VariableDescriptor> variable = node->getVariableDescriptor();
            addAddressInstruction(currentInstructionIterator, node, variable);
            break;
        }
        case Stage::FindFencePoints:
        {
            break;
        }
        }
    }
private:
    void visitSSABasicBlock(std::shared_ptr<SSABasicBlock> block)
    {
        currentBasicBlock = block;
        for(InstructionIterator i = block->instructions.begin(); i != block->instructions.end(); ++i)
        {
            currentInstructionIterator = i;
            (*i)->visit(*this);
        }
    }
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        stage = Stage::FindReferringInstructions;
        addressRegisterVariableToInstructionIteratorMap.clear();
        addressRegisterInstructionToVariableMap.clear();
        fencePoints.clear();
        variableNodesMap.clear();
        variables.clear();
        do
        {
            didAnything = false;
            for(std::shared_ptr<SSABasicBlock> block : function->blocks)
                visitSSABasicBlock(block);
        }
        while(didAnything);
        stage = Stage::FindFencePoints;
        for(std::shared_ptr<SSABasicBlock> block : function->blocks)
            visitSSABasicBlock(block);

        if(fencePoints.empty())
        {
            for(std::shared_ptr<VariableDescriptor> variable : variables)
            {
                std::shared_ptr<TypeNode> type = variable->getType();
                if(type == nullptr)
                    continue;
                type = type->dereference();
                if(type == nullptr)
                    continue;
                std::list<std::shared_ptr<SSAPhi>> phiInstructions;
                std::unordered_map<std::shared_ptr<SSABasicBlock>, std::shared_ptr<SSANode>> &nodesMap = variableNodesMap[variable];
                for(std::shared_ptr<SSABasicBlock> block : function->blocks)
                {
                    if(!block->sourceBlocks.empty())
                    {
                        std::shared_ptr<SSAPhi> phi = std::make_shared<SSAPhi>(type, nullptr);
                        phiInstructions.push_front(phi);
                        block->instructions.push_front(phi);
                        nodesMap[block] = phi;
                    }
                }
                #error add find first load and last store
                #error add rewrite load and stores
                #error add phi fixup
            }
        }
        addressRegisterVariableToInstructionIteratorMap.clear();
        addressRegisterInstructionToVariableMap.clear();
        fencePoints.clear();
        variableNodesMap.clear();
        variables.clear();
        currentInstructionIterator = InstructionIterator();
        currentBasicBlock = nullptr;
    }
};

#endif // MEMORY_TO_REGISTER_H_INCLUDED
