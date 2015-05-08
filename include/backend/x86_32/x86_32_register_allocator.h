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
#ifndef X86_32_REGISTER_ALLOCATOR_H_INCLUDED
#define X86_32_REGISTER_ALLOCATOR_H_INCLUDED

#include "backend/x86_32/x86_32_asm_nodes.h"
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <vector>
#include "util/stable_vector.h"
#include <iostream>

class X86_32RegisterAllocator final
{
private:
    struct LiveRangeData final
    {
        std::unordered_set<std::shared_ptr<LiveRangeData>> intersectingLiveRanges;
        std::shared_ptr<ValueNode> constantValue = nullptr;
        bool isConstant = true;
        const std::shared_ptr<X86_32AsmRegister> originalRegister;
        std::shared_ptr<X86_32AsmRegister> allocatedRegister;
        std::unordered_set<std::shared_ptr<LiveRangeData>> combinableLiveRanges;
        typedef typename X86_32AsmBasicBlock::InstructionList::iterator InstructionIterator;
        std::vector<std::pair<std::shared_ptr<X86_32AsmBasicBlock>, InstructionIterator>> spillLoadPoints;
        std::vector<std::pair<std::shared_ptr<X86_32AsmBasicBlock>, InstructionIterator>> spillStorePoints;
        struct LiveRange final
        {
            std::shared_ptr<X86_32AsmBasicBlock> block;
            InstructionIterator start, end; // defining instruction is start[-1], last using instruction is end[0]
            LiveRange(std::shared_ptr<X86_32AsmBasicBlock> block, InstructionIterator start, InstructionIterator end)
                : block(block), start(start), end(end)
            {
            }
        };
        std::vector<LiveRange> liveRanges;
        explicit LiveRangeData(std::shared_ptr<X86_32AsmRegister> originalRegister)
            : originalRegister(originalRegister)
        {
        }
    };
    std::shared_ptr<LiveRangeData> getOrMakeLiveRange(std::unordered_map<std::shared_ptr<X86_32AsmRegister>, std::shared_ptr<LiveRangeData>> &registerToLiveRangeMap, std::shared_ptr<X86_32AsmRegister> r, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        std::shared_ptr<LiveRangeData> &retval = registerToLiveRangeMap[r];
        if(retval == nullptr)
        {
            retval = std::make_shared<LiveRangeData>(r);
            liveRanges.insert(retval);
        }
        return retval;
    }
    void addAllLiveRangeIntersections(const std::unordered_set<std::shared_ptr<X86_32AsmRegister>> &currentlyLiveRegisters, std::unordered_map<std::shared_ptr<X86_32AsmRegister>, std::shared_ptr<LiveRangeData>> &registerToLiveRangeMap, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        for(std::shared_ptr<X86_32AsmRegister> r1 : currentlyLiveRegisters)
        {
            std::shared_ptr<LiveRangeData> liveRange1 = getOrMakeLiveRange(registerToLiveRangeMap, r1, liveRanges);
            for(std::shared_ptr<X86_32AsmRegister> r2 : currentlyLiveRegisters)
            {
                std::shared_ptr<LiveRangeData> liveRange2 = getOrMakeLiveRange(registerToLiveRangeMap, r2, liveRanges);
                liveRange1->intersectingLiveRanges.insert(liveRange2);
                liveRange1->combinableLiveRanges.erase(liveRange2);
            }
        }
    }
    void calculateLiveRanges(std::shared_ptr<X86_32AsmFunction> function, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        std::unordered_map<std::shared_ptr<X86_32AsmRegister>, std::shared_ptr<LiveRangeData>> registerToLiveRangeMap;
        std::vector<std::shared_ptr<X86_32AsmRegister>> currentMoveRegisters;
        for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
        {
            std::unordered_set<std::shared_ptr<X86_32AsmRegister>> currentlyLiveRegisters = block->liveRegistersAtEnd;
            addAllLiveRangeIntersections(currentlyLiveRegisters, registerToLiveRangeMap, liveRanges);
            std::unordered_map<std::shared_ptr<X86_32AsmRegister>, LiveRangeData::InstructionIterator> liveRangeEnds;
            for(std::shared_ptr<X86_32AsmRegister> r : currentlyLiveRegisters)
            {
                liveRangeEnds[r] = block->instructions.end();
            }
            for(auto i = block->instructions.end(); i != block->instructions.begin();)
            {
                std::shared_ptr<X86_32AsmNode> node = *--i;
                bool isMove = dynamic_cast<const X86_32AsmNodeMove *>(node.get()) != nullptr;
                std::shared_ptr<ValueNode> constantValue = nullptr;
                if(const X86_32AsmNodeLoadConstant *loadNode = dynamic_cast<const X86_32AsmNodeLoadConstant *>(node.get()))
                    constantValue = loadNode->value;
                currentMoveRegisters.clear();
                for(std::shared_ptr<X86_32AsmRegister> r : node->outputSet())
                {
                    std::shared_ptr<LiveRangeData> liveRange = getOrMakeLiveRange(registerToLiveRangeMap, r, liveRanges);
                    if(constantValue != nullptr && liveRange->isConstant && (liveRange->constantValue == nullptr || *liveRange->constantValue == *constantValue))
                        liveRange->constantValue = constantValue;
                    else
                        liveRange->isConstant = false;
                    currentlyLiveRegisters.erase(r);
                    auto iter = liveRangeEnds.find(r);
                    if(iter != liveRangeEnds.end())
                    {
                        liveRange->liveRanges.emplace_back(block, i + 1, std::get<1>(*iter));
                        liveRangeEnds.erase(iter);
                    }
                    if(isMove)
                        currentMoveRegisters.push_back(r);
                    liveRange->spillStorePoints.emplace_back(block, i);
                }
                for(std::shared_ptr<X86_32AsmRegister> r : node->inputSet())
                {
                    std::shared_ptr<LiveRangeData> liveRange = getOrMakeLiveRange(registerToLiveRangeMap, r, liveRanges);
                    currentlyLiveRegisters.insert(r);
                    if(isMove)
                        currentMoveRegisters.push_back(r);
                    if(liveRangeEnds.count(r) == 0)
                        liveRangeEnds[r] = i;
                    liveRange->spillLoadPoints.emplace_back(block, i);
                }
                if(isMove)
                {
                    for(std::shared_ptr<X86_32AsmRegister> r1 : currentMoveRegisters)
                    {
                        std::shared_ptr<LiveRangeData> liveRange1 = getOrMakeLiveRange(registerToLiveRangeMap, r1, liveRanges);
                        for(std::shared_ptr<X86_32AsmRegister> r2 : currentMoveRegisters)
                        {
                            if(r1 == r2)
                                continue;
                            std::shared_ptr<LiveRangeData> liveRange2 = getOrMakeLiveRange(registerToLiveRangeMap, r2, liveRanges);
                            if(liveRange1->intersectingLiveRanges.count(liveRange2) == 0)
                                liveRange1->combinableLiveRanges.insert(liveRange2);
                        }
                    }
                }
                addAllLiveRangeIntersections(currentlyLiveRegisters, registerToLiveRangeMap, liveRanges);
            }
            for(auto p : liveRangeEnds)
            {
                std::shared_ptr<LiveRangeData> liveRange = getOrMakeLiveRange(registerToLiveRangeMap, std::get<0>(p), liveRanges);
                liveRange->liveRanges.emplace_back(block, block->instructions.begin(), std::get<1>(p));
            }
        }
    }
    std::size_t getPhysicalRegisterCount(X86_32AsmRegister::PhysicalRegisterKindMask v,
                                         std::unordered_map<X86_32AsmRegister::PhysicalRegisterKindMask, std::size_t> &physicalRegisterCountsMap,
                                         const std::vector<std::shared_ptr<X86_32AsmRegister>> &physicalRegisters)
    {
        auto iter = physicalRegisterCountsMap.find(v);
        if(iter != physicalRegisterCountsMap.end())
            return std::get<1>(*iter);
        std::size_t &retval = physicalRegisterCountsMap[v];
        retval = 0;
        for(std::shared_ptr<X86_32AsmRegister> r : physicalRegisters)
        {
            if(r->isSpecialPurpose)
                continue;
            if(r->physicalRegisterKindMask & v)
                retval++;
        }
        return retval;
    }
public:
    void visitX86_32AsmFunction(std::shared_ptr<X86_32AsmFunction> function)
    {
        const std::vector<std::shared_ptr<X86_32AsmRegister>> &physicalRegisters = X86_32AsmRegister::getPhysicalRegisters(function->context);
        std::unordered_map<X86_32AsmRegister::PhysicalRegisterKindMask, std::size_t> physicalRegisterCountsMap;
        std::unordered_set<std::shared_ptr<LiveRangeData>> liveRanges;
        for(std::size_t tryCount = 0;; tryCount++)
        {
            liveRanges.clear();
            calculateLiveRanges(function, liveRanges);
            if(tryCount >= liveRanges.size())
                throw std::runtime_error("can't allocate registers");
            std::vector<std::shared_ptr<LiveRangeData>> liveRangeStack;
            std::unordered_set<std::shared_ptr<LiveRangeData>> liveRangesLeft = liveRanges;
            while(!liveRangesLeft.empty())
            {
                bool processedAny = false;
                std::shared_ptr<LiveRangeData> minIntersectingLiveRangeCountLiveRange = nullptr;
                std::size_t minIntersectingLiveRangeCount = 0;
                for(auto i = liveRangesLeft.begin(); i != liveRangesLeft.end();)
                {
                    std::shared_ptr<LiveRangeData> liveRange = *i;
                    std::size_t matchingRegisterCount = getPhysicalRegisterCount(liveRange->originalRegister->physicalRegisterKindMask, physicalRegisterCountsMap, physicalRegisters);
                    std::size_t intersectingLiveRangeCount = 0;
                    for(std::shared_ptr<LiveRangeData> intersectingLiveRange : liveRange->intersectingLiveRanges)
                    {
                        if(liveRangesLeft.count(intersectingLiveRange) == 0)
                            continue;
                        intersectingLiveRangeCount++;
                    }
                    if(intersectingLiveRangeCount < matchingRegisterCount) // will have a register to allocate
                    {
                        liveRangeStack.push_back(liveRange);
                        i = liveRangesLeft.erase(i);
                        processedAny = true;
                    }
                    else
                    {
                        if(minIntersectingLiveRangeCountLiveRange == nullptr || minIntersectingLiveRangeCount > intersectingLiveRangeCount)
                        {
                            minIntersectingLiveRangeCountLiveRange = liveRange;
                            minIntersectingLiveRangeCount = intersectingLiveRangeCount;
                        }
                        ++i;
                    }
                }
                if(!processedAny)
                {
                    if(minIntersectingLiveRangeCountLiveRange == nullptr)
                        break;
                    liveRangeStack.push_back(minIntersectingLiveRangeCountLiveRange);
                    liveRangesLeft.erase(minIntersectingLiveRangeCountLiveRange);
                }
            }
            std::unordered_set<std::shared_ptr<LiveRangeData>> spilledLiveRanges;
            while(!liveRangeStack.empty())
            {
                std::shared_ptr<LiveRangeData> liveRange = liveRangeStack.back();
                liveRangeStack.pop_back();
                if(liveRange->originalRegister->registerType != X86_32AsmRegister::RegisterType::Virtual)
                {
                    liveRange->allocatedRegister = liveRange->originalRegister;
                    continue;
                }
                std::unordered_set<std::shared_ptr<X86_32AsmRegister>> intersectingRegisters, preferredRegisters;
                for(std::shared_ptr<LiveRangeData> intersectingLiveRange : liveRange->intersectingLiveRanges)
                {
                    intersectingRegisters.insert(intersectingLiveRange->originalRegister);
                    if(intersectingLiveRange->originalRegister && intersectingLiveRange->originalRegister->registerType == X86_32AsmRegister::RegisterType::Physical)
                    {
                        for(std::shared_ptr<X86_32AsmRegister> r : intersectingLiveRange->originalRegister->getPhysicalRegisterInterferenceSet())
                        {
                            intersectingRegisters.insert(r);
                        }
                    }
                    intersectingRegisters.insert(intersectingLiveRange->allocatedRegister);
                    if(intersectingLiveRange->allocatedRegister && intersectingLiveRange->allocatedRegister->registerType == X86_32AsmRegister::RegisterType::Physical)
                    {
                        for(std::shared_ptr<X86_32AsmRegister> r : intersectingLiveRange->allocatedRegister->getPhysicalRegisterInterferenceSet())
                        {
                            intersectingRegisters.insert(r);
                        }
                    }
                }
                for(std::shared_ptr<LiveRangeData> preferredLiveRange : liveRange->combinableLiveRanges)
                {
                    preferredRegisters.insert(preferredLiveRange->originalRegister);
                    preferredRegisters.insert(preferredLiveRange->allocatedRegister);
                }
                std::shared_ptr<X86_32AsmRegister> pickedRegister = nullptr;
                for(std::shared_ptr<X86_32AsmRegister> r : physicalRegisters)
                {
                    if(r->isSpecialPurpose && preferredRegisters.count(r) == 0)
                    {
                        continue;
                    }
                    if(!(r->physicalRegisterKindMask & liveRange->originalRegister->physicalRegisterKindMask))
                    {
                        continue;
                    }
                    if(intersectingRegisters.count(r) != 0)
                    {
                        continue;
                    }
                    if(pickedRegister == nullptr)
                        pickedRegister = r;
                    if(preferredRegisters.count(r) != 0)
                    {
                        pickedRegister = r;
                        break;
                    }
                }
                if(pickedRegister == nullptr)
                {
                    spilledLiveRanges.insert(liveRange);
                }
                else
                {
                    liveRange->allocatedRegister = pickedRegister;
                }
            }
            if(spilledLiveRanges.empty())
                break;
            for(std::shared_ptr<LiveRangeData> liveRange : spilledLiveRanges)
            {
                SpillLocation spillLocation = nullptr;
                if(!liveRange->isConstant || liveRange->constantValue == nullptr) // if not a constant live range allocate local
                    spillLocation = liveRange->originalRegister->physicalRegisterKindMask.createSpillLocation(function->localVariablesSize);
                for(auto p : liveRange->spillLoadPoints)
                {
                    std::shared_ptr<X86_32AsmBasicBlock> block = std::get<0>(p);
                    auto pos = std::get<1>(p);
                    bool good = false;
                    for(LiveRangeData::LiveRange r : liveRange->liveRanges)
                    {
                        if(r.block != block)
                            continue;
                        if(r.end == r.start) // from one instruction to it's immediate successor
                            continue;
                        if(pos <= r.end && pos + 1 >= r.start)
                        {
                            good = true;
                            break;
                        }
                    }
                    if(good)
                    {
                        if(liveRange->isConstant && liveRange->constantValue)
                        {
                            std::shared_ptr<X86_32AsmNode> node = std::make_shared<X86_32AsmNodeLoadConstant>(liveRange->originalRegister, liveRange->constantValue);
                            block->instructions.insert(pos, node);
                        }
                        else
                        {
                            if(spillLocation.kind != SpillLocation::Kind::LocalVariable)
                                throw std::runtime_error("register spill location kind not implemented");
                            std::shared_ptr<X86_32AsmNode> node = std::make_shared<X86_32AsmNodeLoadLocal>(liveRange->originalRegister, VariableLocation(spillLocation.variable));
                            block->instructions.insert(pos, node);
                        }
                    }
                }
                for(auto p : liveRange->spillStorePoints)
                {
                    std::shared_ptr<X86_32AsmBasicBlock> block = std::get<0>(p);
                    auto pos = std::get<1>(p);
                    bool good = false;
                    for(LiveRangeData::LiveRange r : liveRange->liveRanges)
                    {
                        if(r.block != block)
                            continue;
                        if(r.end == r.start) // from one instruction to it's immediate successor
                            continue;
                        if(pos <= r.end && pos + 1 >= r.start)
                        {
                            good = true;
                            break;
                        }
                    }
                    if(good)
                    {
                        if(!liveRange->isConstant || liveRange->constantValue == nullptr) // don't store constants
                        {
                            if(spillLocation.kind != SpillLocation::Kind::LocalVariable)
                                throw std::runtime_error("register spill location kind not implemented");
                            std::shared_ptr<X86_32AsmNode> node = std::make_shared<X86_32AsmNodeStoreLocal>(VariableLocation(spillLocation.variable), liveRange->originalRegister);
                            block->instructions.insert(pos + 1, node);
                        }
                    }
                }
            }
            X86_32ConstructLivenessInfo().visitX86_32AsmFunction(function);
        }
        for(std::shared_ptr<LiveRangeData> liveRange : liveRanges)
        {
            std::shared_ptr<X86_32AsmRegister> originalRegister = liveRange->originalRegister;
            std::shared_ptr<X86_32AsmRegister> allocatedRegister = liveRange->allocatedRegister;
            if(originalRegister == allocatedRegister)
                continue;
            for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
            {
                for(std::shared_ptr<X86_32AsmNode> node : block->instructions)
                {
                    node->replaceRegister(originalRegister, allocatedRegister);
                }
            }
        }
        for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
        {
            for(auto i = block->instructions.begin(); i != block->instructions.end();)
            {
                std::shared_ptr<X86_32AsmNodeMove> node = std::dynamic_pointer_cast<X86_32AsmNodeMove>(*i);
                if(node != nullptr && node->source == node->dest)
                    i = block->instructions.erase(i);
                else
                    ++i;
            }
        }
    }
};

#endif // X86_32_REGISTER_ALLOCATOR_H_INCLUDED
