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
#ifndef CONSTRUCT_BASIC_BLOCK_GRAPH_H_INCLUDED
#define CONSTRUCT_BASIC_BLOCK_GRAPH_H_INCLUDED

#include "ssa/ssa_visitor.h"
#include "types/type.h"
#include "values/value.h"
#include "ssa/ssa_nodes.h"
#include "rtl/rtl_nodes.h"
#include <unordered_map>
#include <unordered_set>
#include <iostream>

class ConstructBasicBlockGraphVisitor final
{
private:
    enum class Stage
    {
        Clearing,
        FillingSourceAndDest,
        ConstructingPredecessorGraph,
        ConstructingDominatorGraph,
        FillDominatedValues,
        FillImmediateDominator,
    };
    Stage stage = Stage::Clearing;
    bool done = false;
    std::unordered_map<std::shared_ptr<SSABasicBlock>, std::unordered_set<std::shared_ptr<SSABasicBlock>>> predecessorSetMap;
    std::unordered_map<std::shared_ptr<SSABasicBlock>, std::unordered_set<std::shared_ptr<SSABasicBlock>>> dominatingSetMap;
    std::unordered_map<std::shared_ptr<SSABasicBlock>, std::size_t> basicBlockNameMap;
    std::size_t nextName = 1;
    void resetNames()
    {
        basicBlockNameMap.clear();
        nextName = 1;
    }
    std::size_t getBasicBlockName(std::shared_ptr<SSABasicBlock> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = basicBlockNameMap.find(node);
        if(iter == basicBlockNameMap.end())
        {
            return basicBlockNameMap[node] = nextName++;
        }
        return std::get<1>(*iter);
    }
    std::size_t getBasicBlockName(std::weak_ptr<SSABasicBlock> node)
    {
        return getBasicBlockName(node.lock());
    }
    void dumpBlockSet(const std::unordered_set<std::shared_ptr<SSABasicBlock>> &set)
    {
        std::cout << "{";
        const char *seperator = "";
        for(std::shared_ptr<SSABasicBlock> node : set)
        {
            std::cout << seperator;
            seperator = ",";
            std::cout << getBasicBlockName(node);
        }
        std::cout << "}";
    }
    void dumpBlockList(const std::list<std::shared_ptr<SSABasicBlock>> &l)
    {
        std::cout << "[";
        const char *seperator = "";
        for(std::shared_ptr<SSABasicBlock> node : l)
        {
            std::cout << seperator;
            seperator = ",";
            std::cout << getBasicBlockName(node);
        }
        std::cout << "]";
    }
    void dumpBlockList(const std::list<std::weak_ptr<SSABasicBlock>> &l)
    {
        std::cout << "[";
        const char *seperator = "";
        for(std::weak_ptr<SSABasicBlock> nodeW : l)
        {
            std::cout << seperator;
            seperator = ",";
            std::cout << getBasicBlockName(nodeW);
        }
        std::cout << "]";
    }
    void visitSSABasicBlock(std::shared_ptr<SSABasicBlock> node)
    {
        switch(stage)
        {
        case Stage::Clearing:
        {
            node->sourceBlocks.clear();
            node->destBlocks.clear();
            node->dominatedBlocks.clear();
            node->immediateDominator.reset();
            return;
        }
        case Stage::FillingSourceAndDest:
        {
            if(node->controlTransferInstruction == nullptr)
                return;
            for(std::weak_ptr<SSABasicBlock> destBlockW : node->controlTransferInstruction->destBlocks)
            {
                std::shared_ptr<SSABasicBlock> destBlock = destBlockW.lock();
                node->destBlocks.push_back(destBlock);
                destBlock->sourceBlocks.push_back(node);
            }
            return;
        }
        case Stage::ConstructingPredecessorGraph:
        {
            std::unordered_set<std::shared_ptr<SSABasicBlock>> &predecessorSet = predecessorSetMap[node];
            for(std::weak_ptr<SSABasicBlock> sourceBlockW : node->sourceBlocks)
            {
                std::shared_ptr<SSABasicBlock> sourceBlock = sourceBlockW.lock();
                if(sourceBlock == node)
                    continue;
                const std::unordered_set<std::shared_ptr<SSABasicBlock>> &sourcePredecessorSet = predecessorSetMap[sourceBlock];
                if(std::get<1>(predecessorSet.insert(sourceBlock)))
                    done = false;
                for(std::shared_ptr<SSABasicBlock> sourcePredecessor : sourcePredecessorSet)
                {
                    if(sourcePredecessor == node)
                        continue;
                    if(std::get<1>(predecessorSet.insert(sourcePredecessor)))
                        done = false;
                }
            }
            return;
        }
        case Stage::ConstructingDominatorGraph:
        {
            std::unordered_set<std::shared_ptr<SSABasicBlock>> newDominatorsSet;
            bool isFirst = true;
            for(std::shared_ptr<SSABasicBlock> sourceBlock : predecessorSetMap[node])
            {
                const std::unordered_set<std::shared_ptr<SSABasicBlock>> &sourceDominators = dominatingSetMap[sourceBlock];
                if(isFirst)
                {
                    newDominatorsSet = sourceDominators;
                }
                else
                {
                    for(auto i = newDominatorsSet.begin(); i != newDominatorsSet.end();)
                    {
                        if(sourceDominators.count(*i) == 0)
                            i = newDominatorsSet.erase(i);
                        else
                            ++i;
                    }
                }
            }
            newDominatorsSet.insert(node);
            std::unordered_set<std::shared_ptr<SSABasicBlock>> &currentDominatorsSet = dominatingSetMap[node];
            if(currentDominatorsSet != newDominatorsSet)
            {
                done = false;
            }
            currentDominatorsSet = std::move(newDominatorsSet);
            return;
        }
        case Stage::FillDominatedValues:
        {
            const std::unordered_set<std::shared_ptr<SSABasicBlock>> &currentDominatorsSet = dominatingSetMap[node];
            for(std::shared_ptr<SSABasicBlock> dominator : currentDominatorsSet)
            {
                dominator->dominatedBlocks.push_back(node);
            }
            return;
        }
        case Stage::FillImmediateDominator:
        {
            const std::unordered_set<std::shared_ptr<SSABasicBlock>> &currentDominatorsSet = dominatingSetMap[node];
            for(std::shared_ptr<SSABasicBlock> dominator : currentDominatorsSet)
            {
                if(dominator == node)
                    continue;
                bool isImmediateDominator = true;
                const std::unordered_set<std::shared_ptr<SSABasicBlock>> &dominatorDominators = dominatingSetMap[dominator];
                for(std::shared_ptr<SSABasicBlock> i : currentDominatorsSet)
                {
                    if(i == node)
                        continue;
                    if(dominatorDominators.count(i) == 0)
                    {
                        isImmediateDominator = false;
                        break;
                    }
                }
                if(isImmediateDominator)
                {
                    node->immediateDominator = dominator;
                    break;
                }
            }
            return;
        }
        }
    }
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> node)
    {
        resetNames();
        stage = Stage::Clearing;
        for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
        {
            visitSSABasicBlock(basicBlock);
        }
        stage = Stage::FillingSourceAndDest;
        for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
        {
            visitSSABasicBlock(basicBlock);
        }
        stage = Stage::ConstructingPredecessorGraph;
        predecessorSetMap.clear();
        do
        {
            done = true;
            for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
            {
                visitSSABasicBlock(basicBlock);
            }
        }
        while(!done);
        stage = Stage::ConstructingDominatorGraph;
        dominatingSetMap.clear();
        do
        {
            done = true;
            for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
            {
                visitSSABasicBlock(basicBlock);
            }
        }
        while(!done);
        stage = Stage::FillDominatedValues;
        for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
        {
            visitSSABasicBlock(basicBlock);
        }
        stage = Stage::FillImmediateDominator;
        for(std::shared_ptr<SSABasicBlock> basicBlock : node->blocks)
        {
            visitSSABasicBlock(basicBlock);
        }
        dominatingSetMap.clear();
        predecessorSetMap.clear();
    }
    void visitRTLFunction(std::shared_ptr<RTLFunction> function)
    {
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            block->sourceBlocks.clear();
            block->destBlocks.clear();
        }
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            if(block->controlTransferInstruction == nullptr)
                continue;
            for(std::weak_ptr<RTLBasicBlock> targetBlockW : block->controlTransferInstruction->getTargets())
            {
                std::shared_ptr<RTLBasicBlock> targetBlock = targetBlockW.lock();
                block->destBlocks.push_back(targetBlock);
                targetBlock->sourceBlocks.push_back(block);
            }
        }

    }
};

#endif // CONSTRUCT_BASIC_BLOCK_GRAPH_H_INCLUDED
