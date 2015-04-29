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

class X86_64RegisterAllocator final
{
private:
    class LiveRange final
    {
    public:
        class InstructionRange final
        {
            std::shared_ptr<X86_64AsmBasicBlock> block;
            typedef typename X86_64AsmBasicBlock::InstructionList::iterator InstructionIterator;
            InstructionIterator startIterator; // starts between *startIterator and *std::prev(startIterator)
            InstructionIterator endIterator;
            InstructionRange()
                : block(nullptr)
            {
            }
            InstructionRange(std::shared_ptr<X86_64AsmBasicBlock> block, InstructionIterator startIterator, InstructionIterator endIterator)
                : block(block), startIterator(startIterator), endIterator(endIterator)
            {
            }
            bool empty() const
            {
                return block == nullptr;
            }
            bool operator ==(const InstructionRange &rt) const
            {
                return block == rt.block && (block == nullptr || (startIterator == rt.startIterator && endIterator == rt.endIterator));
            }
            bool operator !=(const InstructionRange &rt) const
            {
                return !operator ==(rt);
            }
            friend InstructionRange intersect(const InstructionRange &l, const InstructionRange &r)
            {
                if(l.block != r.block)
                    return InstructionRange();

            }
        };
        std::unordered_map<std::shared_ptr<X86_64AsmBasicBlock>, InstructionRange> ranges;
        friend bool operator ==(const LiveRange &l, const LiveRange &r)
        {

        }
    };
public:
    void visitX86_64AsmFunction(std::shared_ptr<X86_64AsmFunction> function)
    {

    }
};

#endif // X86_64_REGISTER_ALLOCATOR_H_INCLUDED
