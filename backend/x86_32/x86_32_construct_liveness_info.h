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
#ifndef X86_32_CONSTRUCT_LIVENESS_INFO_H_INCLUDED
#define X86_32_CONSTRUCT_LIVENESS_INFO_H_INCLUDED

#include "backend/x86_32/x86_32_asm_nodes.h"

class X86_32ConstructLivenessInfo final
{
public:
    void visitX86_32AsmFunction(std::shared_ptr<X86_32AsmFunction> function) const
    {
        for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
        {
            block->assignedRegisters.clear();
            block->usedRegistersAtStart.clear();
            block->liveRegistersAtStart.clear();
            block->liveRegistersAtEnd.clear();
            for(auto i = block->instructions.rbegin(); i != block->instructions.rend(); ++i)
            {
                for(std::shared_ptr<X86_32AsmRegister> outputRegister : (*i)->outputSet())
                {
                    block->usedRegistersAtStart.erase(outputRegister);
                    block->assignedRegisters.insert(outputRegister);
                }
                for(std::shared_ptr<X86_32AsmRegister> inputRegister : (*i)->inputSet())
                {
                    block->usedRegistersAtStart.insert(inputRegister);
                }
                block->liveRegistersAtStart = block->usedRegistersAtStart;
            }
        }
        bool done = false;
        while(!done)
        {
            done = true;
            for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
            {
                for(std::shared_ptr<X86_32AsmRegister> r : block->liveRegistersAtEnd)
                {
                    if(block->assignedRegisters.count(r) != 0)
                        continue;
                    if(std::get<1>(block->liveRegistersAtStart.insert(r)))
                    {
                        done = false;
                    }
                }
                for(std::weak_ptr<X86_32AsmBasicBlock> targetW : block->destBlocks)
                {
                    std::shared_ptr<X86_32AsmBasicBlock> target = targetW.lock();
                    for(std::shared_ptr<X86_32AsmRegister> r : target->liveRegistersAtStart)
                    {
                        if(std::get<1>(block->liveRegistersAtEnd.insert(r)))
                        {
                            done = false;
                        }
                    }
                }
            }
        }
    }
};

#endif // X86_32_CONSTRUCT_LIVENESS_INFO_H_INCLUDED
