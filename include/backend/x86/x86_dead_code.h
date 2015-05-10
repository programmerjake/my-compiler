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
#ifndef X86_DEAD_CODE_H_INCLUDED
#define X86_DEAD_CODE_H_INCLUDED

#include "backend/x86/x86_backend.h"
#include "backend/x86/x86_asm_nodes.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

class X86DeadCodeElimination final
{
private:
    const BackendX86 *const backend;
public:
    explicit X86DeadCodeElimination(const BackendX86 *backend)
        : backend(backend)
    {
    }
    void visitX86AsmFunction(std::shared_ptr<X86AsmFunction> function)
    {
        std::unordered_set<std::shared_ptr<X86AsmNode>> usedNodesSet;
        std::unordered_map<std::shared_ptr<X86AsmBasicBlock>, std::unordered_set<std::shared_ptr<X86AsmRegister>>> blockUsedRegistersAtEndSetMap;
        bool done = false;
        while(!done)
        {
            done = true;
            for(std::shared_ptr<X86AsmBasicBlock> block : function->blocks)
            {
                if(block->controlTransferInstruction != nullptr)
                    if(std::get<1>(usedNodesSet.insert(block->controlTransferInstruction)))
                        done = false;
                std::unordered_set<std::shared_ptr<X86AsmRegister>> usedRegistersSet = blockUsedRegistersAtEndSetMap[block];
                for(auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
                {
                    std::shared_ptr<X86AsmNode> node = *i;
                    if(node->hasSideEffects())
                        if(std::get<1>(usedNodesSet.insert(node)))
                            done = false;
                    for(std::shared_ptr<X86AsmRegister> r : node->outputSet())
                    {
                        if(usedRegistersSet.erase(r) > 0)
                            if(std::get<1>(usedNodesSet.insert(node)))
                                done = false;
                    }
                    if(usedNodesSet.count(node) > 0)
                    {
                        for(std::shared_ptr<X86AsmRegister> r : node->inputSet())
                        {
                            usedRegistersSet.insert(r);
                        }
                    }
                }
                for(std::weak_ptr<X86AsmBasicBlock> predecessorBlockW : block->sourceBlocks)
                {
                    std::shared_ptr<X86AsmBasicBlock> predecessorBlock = predecessorBlockW.lock();
                    std::unordered_set<std::shared_ptr<X86AsmRegister>> &predecessorBlockUsedRegistersSet = blockUsedRegistersAtEndSetMap[predecessorBlock];
                    for(std::shared_ptr<X86AsmRegister> r : usedRegistersSet)
                    {
                        if(std::get<1>(predecessorBlockUsedRegistersSet.insert(r)))
                            done = false;
                    }
                }
            }
        }
        for(std::shared_ptr<X86AsmBasicBlock> block : function->blocks)
        {
            for(auto i = block->instructions.begin(); i != block->instructions.end(); )
            {
                const std::shared_ptr<X86AsmNode> &node = *i;
                if(usedNodesSet.count(node) > 0)
                    ++i;
                else
                    i = block->instructions.erase(i);
            }
        }
    }
};

#endif // X86_DEAD_CODE_H_INCLUDED
