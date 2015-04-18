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
#ifndef SSA_NODE_H_INCLUDED
#define SSA_NODE_H_INCLUDED

#include <memory>
#include <list>
#include <cassert>

#include "../context.h"
#include "../types/type.h"

class SSANodeVisitor;
class SSAControlTransfer;

class SSANode : public std::enable_shared_from_this<SSANode>
{
    SSANode(const SSANode &) = delete;
    SSANode &operator =(const SSANode &) = delete;
public:
    CompilerContext *const context;
    std::weak_ptr<TypeNode> type;
    explicit SSANode(CompilerContext *context, std::shared_ptr<TypeNode> type)
        : context(context), type(type)
    {
        assert(type != nullptr);
        assert(context != nullptr);
        assert(type->context == context);
    }
    virtual ~SSANode() = default;
    virtual void visit(SSANodeVisitor &visitor) = 0;
};

class SSABasicBlock : public std::enable_shared_from_this<SSABasicBlock>
{
public:
    std::list<std::weak_ptr<SSABasicBlock>> sourceBlocks;
    std::weak_ptr<SSABasicBlock> dominator;
    std::list<std::weak_ptr<SSABasicBlock>> dominatedBlocks;
    std::list<std::weak_ptr<SSABasicBlock>> destBlocks;
    std::shared_ptr<SSAControlTransfer> controlTransferInstruction;
    std::list<std::shared_ptr<SSANode>> instructions;
};

class SSAFunction : public std::enable_shared_from_this<SSAFunction>
{
public:
    std::list<std::shared_ptr<SSABasicBlock>> blocks;
    std::shared_ptr<SSABasicBlock> startBlock;
    std::shared_ptr<SSABasicBlock> endBlock;
    std::list<std::shared_ptr<SSANode>> parameters;
    std::shared_ptr<SSANode> returnValue;
};

#include "ssa_visitor.h"
#include "ssa_control_transfer.h"

#endif // SSA_NODE_H_INCLUDED
