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
#ifndef X86_64_REGISTER_ALLOCATOR_H_INCLUDED
#define X86_64_REGISTER_ALLOCATOR_H_INCLUDED

#include "x86_64_asm_nodes.h"
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <vector>
#include "../../util/random_access_list.h"
#include <iostream>

class X86_64RegisterAllocator final
{
private:
    struct LiveRangeData final
    {
        std::unordered_set<std::shared_ptr<LiveRangeData>> intersectingLiveRanges;
        const std::shared_ptr<X86_64AsmRegister> originalRegister;
        std::shared_ptr<X86_64AsmRegister> allocatedRegister;
        std::unordered_set<std::shared_ptr<LiveRangeData>> combinableLiveRanges;
        typedef typename X86_64AsmBasicBlock::InstructionList::iterator InstructionIterator;
        std::vector<InstructionIterator> spillLoadPoints;
        std::vector<InstructionIterator> spillStorePoints;
        std::vector<std::pair<InstructionIterator, InstructionIterator>> liveRanges;
        explicit LiveRangeData(std::shared_ptr<X86_64AsmRegister> originalRegister)
            : originalRegister(originalRegister)
        {
        }
    };
    std::shared_ptr<LiveRangeData> getOrMakeLiveRange(std::unordered_map<std::shared_ptr<X86_64AsmRegister>, std::shared_ptr<LiveRangeData>> &registerToLiveRangeMap, std::shared_ptr<X86_64AsmRegister> r, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        std::shared_ptr<LiveRangeData> &retval = registerToLiveRangeMap[r];
        if(retval == nullptr)
        {
            retval = std::make_shared<LiveRangeData>(r);
            liveRanges.insert(retval);
        }
        return retval;
    }
    void addAllLiveRangeIntersections(const std::unordered_set<std::shared_ptr<X86_64AsmRegister>> &currentlyLiveRegisters, std::unordered_map<std::shared_ptr<X86_64AsmRegister>, std::shared_ptr<LiveRangeData>> &registerToLiveRangeMap, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        for(std::shared_ptr<X86_64AsmRegister> r1 : currentlyLiveRegisters)
        {
            std::shared_ptr<LiveRangeData> liveRange1 = getOrMakeLiveRange(registerToLiveRangeMap, r1, liveRanges);
            for(std::shared_ptr<X86_64AsmRegister> r2 : currentlyLiveRegisters)
            {
                std::shared_ptr<LiveRangeData> liveRange2 = getOrMakeLiveRange(registerToLiveRangeMap, r2, liveRanges);
                liveRange1->intersectingLiveRanges.insert(liveRange2);
                liveRange1->combinableLiveRanges.erase(liveRange2);
            }
        }
    }
    void calculateLiveRanges(std::shared_ptr<X86_64AsmFunction> function, std::unordered_set<std::shared_ptr<LiveRangeData>> &liveRanges) const
    {
        std::unordered_map<std::shared_ptr<X86_64AsmRegister>, std::shared_ptr<LiveRangeData>> registerToLiveRangeMap;
        std::vector<std::shared_ptr<X86_64AsmRegister>> currentMoveRegisters;
        for(std::shared_ptr<X86_64AsmBasicBlock> block : function->blocks)
        {
            std::unordered_set<std::shared_ptr<X86_64AsmRegister>> currentlyLiveRegisters = block->liveRegistersAtEnd;
            addAllLiveRangeIntersections(currentlyLiveRegisters, registerToLiveRangeMap, liveRanges);
            std::unordered_map<std::shared_ptr<X86_64AsmRegister>, LiveRangeData::InstructionIterator> liveRangeEnds;
            for(std::shared_ptr<X86_64AsmRegister> r : currentlyLiveRegisters)
            {
                liveRangeEnds[r] = block->instructions.end();
            }
            for(auto i = block->instructions.end(); i != block->instructions.begin();)
            {
                std::shared_ptr<X86_64AsmNode> node = *--i;
                bool isMove = dynamic_cast<const X86_64AsmNodeMove *>(node.get()) != nullptr;
                currentMoveRegisters.clear();
                for(std::shared_ptr<X86_64AsmRegister> r : node->outputSet())
                {
                    std::shared_ptr<LiveRangeData> liveRange = getOrMakeLiveRange(registerToLiveRangeMap, r, liveRanges);
                    currentlyLiveRegisters.erase(r);
                    auto iter = liveRangeEnds.find(r);
                    if(iter != liveRangeEnds.end())
                    {
                        liveRange->liveRanges.emplace_back(i + 1, std::get<1>(*iter));
                        liveRangeEnds.erase(iter);
                    }
                    if(isMove)
                        currentMoveRegisters.push_back(r);
                    liveRange->spillStorePoints.push_back(i);
                }
                for(std::shared_ptr<X86_64AsmRegister> r : node->inputSet())
                {
                    std::shared_ptr<LiveRangeData> liveRange = getOrMakeLiveRange(registerToLiveRangeMap, r, liveRanges);
                    currentlyLiveRegisters.insert(r);
                    if(isMove)
                        currentMoveRegisters.push_back(r);
                    if(liveRangeEnds.count(r) == 0)
                        liveRangeEnds[r] = i;
                    liveRange->spillLoadPoints.push_back(i);
                }
                if(isMove)
                {
                    for(std::shared_ptr<X86_64AsmRegister> r1 : currentMoveRegisters)
                    {
                        std::shared_ptr<LiveRangeData> liveRange1 = getOrMakeLiveRange(registerToLiveRangeMap, r1, liveRanges);
                        for(std::shared_ptr<X86_64AsmRegister> r2 : currentMoveRegisters)
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
                liveRange->liveRanges.emplace_back(block->instructions.begin(), std::get<1>(p));
            }
        }
    }
    std::size_t getPhysicalRegisterCount(X86_64AsmRegister::PhysicalRegisterKindMask v,
                                         std::unordered_map<X86_64AsmRegister::PhysicalRegisterKindMask, std::size_t> &physicalRegisterCountsMap,
                                         const std::vector<std::shared_ptr<X86_64AsmRegister>> &physicalRegisters)
    {
        auto iter = physicalRegisterCountsMap.find(v);
        if(iter != physicalRegisterCountsMap.end())
            return std::get<1>(*iter);
        std::size_t &retval = physicalRegisterCountsMap[v];
        retval = 0;
        for(std::shared_ptr<X86_64AsmRegister> r : physicalRegisters)
        {
            if(r->isSpecialPurpose)
                continue;
            if(r->physicalRegisterKindMask & v)
                retval++;
        }
        return retval;
    }
public:
    void visitX86_64AsmFunction(std::shared_ptr<X86_64AsmFunction> function)
    {
        const std::vector<std::shared_ptr<X86_64AsmRegister>> &physicalRegisters = X86_64AsmRegister::getPhysicalRegisters(function->context);
        std::unordered_map<X86_64AsmRegister::PhysicalRegisterKindMask, std::size_t> physicalRegisterCountsMap;
        std::unordered_set<std::shared_ptr<LiveRangeData>> liveRanges;
        for(;;)
        {
            liveRanges.clear();
            calculateLiveRanges(function, liveRanges);
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
                if(liveRange->originalRegister->registerType != X86_64AsmRegister::RegisterType::Virtual)
                {
                    liveRange->allocatedRegister = liveRange->originalRegister;
                    continue;
                }
                std::unordered_set<std::shared_ptr<X86_64AsmRegister>> intersectingRegisters, preferredRegisters;
                for(std::shared_ptr<LiveRangeData> intersectingLiveRange : liveRange->intersectingLiveRanges)
                {
                    intersectingRegisters.insert(intersectingLiveRange->originalRegister);
                    intersectingRegisters.insert(intersectingLiveRange->allocatedRegister);
                }
                for(std::shared_ptr<LiveRangeData> prefferedLiveRange : liveRange->combinableLiveRanges)
                {
                    preferredRegisters.insert(prefferedLiveRange->originalRegister);
                    preferredRegisters.insert(prefferedLiveRange->allocatedRegister);
                }
                std::shared_ptr<X86_64AsmRegister> pickedRegister = nullptr;
                for(std::shared_ptr<X86_64AsmRegister> r : physicalRegisters)
                {
                    if(r->isSpecialPurpose)
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
                    std::cout << "spilling " << liveRange->originalRegister->name << std::endl;
                }
                else
                {
                    liveRange->allocatedRegister = pickedRegister;
                }
            }
            if(spilledLiveRanges.empty())
                break;
            #warning implement spilling registers
            throw std::runtime_error("spilling registers not implemented yet");
        }
        for(std::shared_ptr<LiveRangeData> liveRange : liveRanges)
        {
            std::shared_ptr<X86_64AsmRegister> originalRegister = liveRange->originalRegister;
            std::shared_ptr<X86_64AsmRegister> allocatedRegister = liveRange->allocatedRegister;
            if(originalRegister == allocatedRegister)
                continue;
            for(std::shared_ptr<X86_64AsmBasicBlock> block : function->blocks)
            {
                for(std::shared_ptr<X86_64AsmNode> node : block->instructions)
                {
                    node->replaceRegister(originalRegister, allocatedRegister);
                }
            }
        }
        for(std::shared_ptr<X86_64AsmBasicBlock> block : function->blocks)
        {
            for(auto i = block->instructions.begin(); i != block->instructions.end();)
            {
                std::shared_ptr<X86_64AsmNodeMove> node = std::dynamic_pointer_cast<X86_64AsmNodeMove>(*i);
                if(node != nullptr && node->source == node->dest)
                    i = block->instructions.erase(i);
                else
                    ++i;
            }
        }
    }
};

#endif // X86_64_REGISTER_ALLOCATOR_H_INCLUDED
