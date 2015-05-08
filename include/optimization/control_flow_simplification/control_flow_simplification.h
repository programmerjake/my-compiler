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
#ifndef CONTROL_FLOW_SIMPLIFICATION_H_INCLUDED
#define CONTROL_FLOW_SIMPLIFICATION_H_INCLUDED

#include "ssa/ssa_nodes.h"
#include "types/types.h"
#include "values/values.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "construct_basic_block_graph.h"
#include "dump.h"
#include <iostream>
#include <cassert>

/** simplify control flow
 * @note this doesn't change phi functions : required for use in ConvertSSAToRTL
 */
class ControlFlowSimplification final
{
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        bool done = false;
        DumpVisitor dumper(std::cout);
        while(!done)
        {
            function->verify();
            dumper.visitSSAFunction(function);
            std::cout << std::endl << std::endl;
            done = true;
            for(std::shared_ptr<SSABasicBlock> firstBlock : function->blocks)
            {
                if(firstBlock->destBlocks.size() != 1)
                    continue;
                std::shared_ptr<SSABasicBlock> secondBlock = firstBlock->destBlocks.front().lock();
                if(secondBlock->sourceBlocks.size() != 1)
                {
                    if(firstBlock->instructions.size() != 1)
                        continue;
                    function->replaceBlock(firstBlock, secondBlock);
                    std::cout << "replaceBlock(" << dumper.getSSABasicBlockDisplayValue(firstBlock) << ", " << dumper.getSSABasicBlockDisplayValue(secondBlock) << ")" << std::endl;
                    done = false;
                    break;
                }
                function->mergeBlocks(firstBlock, secondBlock);
                std::cout << "mergeBlocks(" << dumper.getSSABasicBlockDisplayValue(firstBlock) << ", " << dumper.getSSABasicBlockDisplayValue(secondBlock) << ")" << std::endl;
                done = false;
                break;
            }
        }
    }
};

#endif // CONTROL_FLOW_SIMPLIFICATION_H_INCLUDED
