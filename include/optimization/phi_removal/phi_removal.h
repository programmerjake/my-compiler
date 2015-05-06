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
#ifndef PHI_REMOVAL_H_INCLUDED
#define PHI_REMOVAL_H_INCLUDED

#include "ssa/ssa_nodes.h"
#include "types/types.h"
#include "values/values.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "construct_basic_block_graph.h"
#include "dump.h"
#include <cassert>

class PhiRemoval final
{
public:
    void visitSSAFunction(std::shared_ptr<SSAFunction> function)
    {
        bool done = false;
        while(!done)
        {
            done = true;
            for(std::shared_ptr<SSABasicBlock> basicBlock : function->blocks)
            {
                for(std::shared_ptr<SSANode> node : basicBlock->instructions)
                {
                    std::shared_ptr<SSAPhi> phi = std::dynamic_pointer_cast<SSAPhi>(node);
                    if(phi == nullptr)
                        continue;
                    bool isFirstNonloop = true;
                    std::shared_ptr<SSANode> lastNonloopNode = nullptr;
                    bool allNonloopNodesEqual = true;
                    for(SSAPhi::PhiInput i : phi->inputs)
                    {
                        std::shared_ptr<SSANode> inputNode = i.node.lock();
                        assert(inputNode);
                        if(inputNode == phi)
                            continue;
                        if(isFirstNonloop)
                        {
                            lastNonloopNode = inputNode;
                            isFirstNonloop = false;
                        }
                        else if(lastNonloopNode != inputNode)
                        {
                            allNonloopNodesEqual = false;
                        }
                    }
                    if(allNonloopNodesEqual)
                    {
                        std::shared_ptr<SSANode> replacementNode = lastNonloopNode;
                        assert(replacementNode != nullptr);
                        std::unordered_map<std::shared_ptr<SSANode>, SSANode::ReplacementNode> replacements;
                        replacements.emplace(node, SSANode::ReplacementNode(replacementNode, true));
                        function->replaceNodes(replacements);
                        done = false;
                        break;
                    }
                }
                if(!done)
                    break;
            }
        }
    }
};

#endif // PHI_REMOVAL_H_INCLUDED
