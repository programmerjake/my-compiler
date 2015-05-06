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
#ifndef RTL_NODE_H_INCLUDED
#define RTL_NODE_H_INCLUDED

#include "types/type.h"
#include "values/value.h"
#include "context.h"
#include <memory>
#include <list>
#include <string>
#include <unordered_set>
#include "ssa/ssa_compare.h"

class RTLRegister final : public std::enable_shared_from_this<RTLRegister>
{
public:
    CompilerContext *const context;
    const std::string name;
    SpillLocation spillLocation;
    RTLRegister(CompilerContext *context, std::string name, SpillLocation spillLocation)
        : context(context), name(name), spillLocation(spillLocation)
    {
    }
};

class RTLNodeVisitor;

class RTLNode : public std::enable_shared_from_this<RTLNode>
{
    RTLNode(const RTLNode &) = delete;
    RTLNode &operator =(const RTLNode &) = delete;
public:
    CompilerContext *const context;
    explicit RTLNode(CompilerContext *context)
        : context(context)
    {
    }
    virtual ~RTLNode() = default;
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const = 0;
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const = 0;
    virtual void visit(RTLNodeVisitor &visitor) = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
};

class RTLControlTransfer;

class RTLBasicBlock final : public std::enable_shared_from_this<RTLBasicBlock>
{
    RTLBasicBlock(const RTLBasicBlock &) = delete;
    RTLBasicBlock &operator =(const RTLBasicBlock &) = delete;
public:
    CompilerContext *const context;
    explicit RTLBasicBlock(CompilerContext *context)
        : context(context)
    {
    }
    std::list<std::weak_ptr<RTLBasicBlock>> sourceBlocks;
    std::list<std::weak_ptr<RTLBasicBlock>> destBlocks;
    std::list<std::shared_ptr<RTLNode>> instructions; /// the only allowed RTLControlTransfer must be last
    std::shared_ptr<RTLControlTransfer> controlTransferInstruction;
    std::unordered_set<std::shared_ptr<RTLRegister>> usedRegistersAtStart;
    std::unordered_set<std::shared_ptr<RTLRegister>> assignedRegisters;
    std::unordered_set<std::shared_ptr<RTLRegister>> liveRegistersAtStart;
    std::unordered_set<std::shared_ptr<RTLRegister>> liveRegistersAtEnd;
};

class RTLFunction final : public std::enable_shared_from_this<RTLFunction>
{
    RTLFunction(const RTLFunction &) = delete;
    RTLFunction &operator =(const RTLFunction &) = delete;
public:
    CompilerContext *const context;
    explicit RTLFunction(CompilerContext *context)
        : context(context)
    {
    }
    std::list<std::shared_ptr<RTLBasicBlock>> blocks;
    std::shared_ptr<RTLBasicBlock> startBlock;
    std::uint64_t localVariablesSize = 0;
};

class RTLLoadConstant;
class RTLMove;
class RTLLoad;
class RTLStore;
class RTLUnconditionalJump;
class RTLConditionalJump;
class RTLCompare;

class RTLNodeVisitor
{
public:
    virtual void visitRTLLoadConstant(std::shared_ptr<RTLLoadConstant> node) = 0;
    virtual void visitRTLMove(std::shared_ptr<RTLMove> node) = 0;
    virtual void visitRTLUnconditionalJump(std::shared_ptr<RTLUnconditionalJump> node) = 0;
    virtual void visitRTLLoad(std::shared_ptr<RTLLoad> node) = 0;
    virtual void visitRTLStore(std::shared_ptr<RTLStore> node) = 0;
    virtual void visitRTLConditionalJump(std::shared_ptr<RTLConditionalJump> node) = 0;
    virtual void visitRTLCompare(std::shared_ptr<RTLCompare> node) = 0;
};

class RTLControlTransfer : public RTLNode
{
public:
    explicit RTLControlTransfer(CompilerContext *context)
        : RTLNode(context)
    {
    }
    virtual std::list<std::weak_ptr<RTLBasicBlock>> getTargets() const = 0;
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override = 0;
    virtual void visit(RTLNodeVisitor &visitor) override = 0;
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{};
    }
};

class RTLUnconditionalJump final : public RTLControlTransfer
{
public:
    std::weak_ptr<RTLBasicBlock> target;
    explicit RTLUnconditionalJump(std::shared_ptr<RTLBasicBlock> target)
        : RTLControlTransfer(target->context), target(target)
    {
    }
    virtual std::list<std::weak_ptr<RTLBasicBlock>> getTargets() const override
    {
        return std::list<std::weak_ptr<RTLBasicBlock>>{target};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLUnconditionalJump(std::static_pointer_cast<RTLUnconditionalJump>(shared_from_this()));
    }
};

class RTLConditionalJump final : public RTLControlTransfer
{
public:
    std::shared_ptr<RTLRegister> condition;
    std::weak_ptr<RTLBasicBlock> trueTarget;
    std::weak_ptr<RTLBasicBlock> falseTarget;
    explicit RTLConditionalJump(std::shared_ptr<RTLRegister> condition, std::shared_ptr<RTLBasicBlock> trueTarget, std::shared_ptr<RTLBasicBlock> falseTarget)
        : RTLControlTransfer(condition->context), condition(condition), trueTarget(trueTarget), falseTarget(falseTarget)
    {
    }
    virtual std::list<std::weak_ptr<RTLBasicBlock>> getTargets() const override
    {
        return std::list<std::weak_ptr<RTLBasicBlock>>{trueTarget, falseTarget};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{condition, TypeBoolean::make(context)}};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLConditionalJump(std::static_pointer_cast<RTLConditionalJump>(shared_from_this()));
    }
};

class RTLLoadConstant final : public RTLNode
{
public:
    std::shared_ptr<RTLRegister> destRegister;
    std::shared_ptr<ValueNode> value;
    RTLLoadConstant(std::shared_ptr<RTLRegister> destRegister, std::shared_ptr<ValueNode> value)
        : RTLNode(destRegister->context), destRegister(destRegister), value(value)
    {
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{destRegister, value->type}};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLLoadConstant(std::static_pointer_cast<RTLLoadConstant>(shared_from_this()));
    }
};

class RTLMove final : public RTLNode
{
public:
    std::shared_ptr<RTLRegister> destRegister;
    std::shared_ptr<RTLRegister> sourceRegister;
    std::shared_ptr<TypeNode> type;
    RTLMove(std::shared_ptr<RTLRegister> destRegister, std::shared_ptr<RTLRegister> sourceRegister, std::shared_ptr<TypeNode> type)
        : RTLNode(type->context), destRegister(destRegister), sourceRegister(sourceRegister), type(type)
    {
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{destRegister, type}};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{sourceRegister, type}};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLMove(std::static_pointer_cast<RTLMove>(shared_from_this()));
    }
};

class RTLLoad final : public RTLNode
{
public:
    std::shared_ptr<RTLRegister> destRegister;
    std::shared_ptr<RTLRegister> addressRegister;
    std::shared_ptr<TypeNode> addressType;
    RTLLoad(std::shared_ptr<RTLRegister> destRegister, std::shared_ptr<RTLRegister> addressRegister, std::shared_ptr<TypeNode> addressType)
        : RTLNode(addressType->context), destRegister(destRegister), addressRegister(addressRegister), addressType(addressType)
    {
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{destRegister, addressType->dereference()}};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{addressRegister, addressType}};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLLoad(std::static_pointer_cast<RTLLoad>(shared_from_this()));
    }
};

class RTLStore final : public RTLNode
{
public:
    std::shared_ptr<RTLRegister> addressRegister;
    std::shared_ptr<RTLRegister> valueRegister;
    std::shared_ptr<TypeNode> addressType;
    RTLStore(std::shared_ptr<RTLRegister> addressRegister, std::shared_ptr<RTLRegister> valueRegister, std::shared_ptr<TypeNode> addressType)
        : RTLNode(addressType->context), addressRegister(addressRegister), valueRegister(valueRegister), addressType(addressType)
    {
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{addressRegister, addressType}, std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{valueRegister, addressType->dereference()}};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLStore(std::static_pointer_cast<RTLStore>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
};

class RTLCompare final : public RTLNode
{
public:
    typedef SSACompare::CompareOperator CompareOperator;
    std::shared_ptr<RTLRegister> destRegister;
    std::shared_ptr<RTLRegister> lhsRegister;
    std::shared_ptr<RTLRegister> rhsRegister;
    CompareOperator compareOperator;
    std::shared_ptr<TypeNode> operandsType;
    RTLCompare(std::shared_ptr<RTLRegister> destRegister, std::shared_ptr<RTLRegister> lhsRegister, std::shared_ptr<RTLRegister> rhsRegister, CompareOperator compareOperator, std::shared_ptr<TypeNode> operandsType)
        : RTLNode(destRegister->context), destRegister(destRegister), lhsRegister(lhsRegister), rhsRegister(rhsRegister), compareOperator(compareOperator), operandsType(operandsType)
    {
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getOutputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{destRegister, TypeBoolean::make(context)}};
    }
    virtual std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>> getInputRegisters() const override
    {
        return std::list<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>>{std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{lhsRegister, operandsType}, std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>{rhsRegister, operandsType}};
    }
    virtual void visit(RTLNodeVisitor &visitor) override
    {
        visitor.visitRTLCompare(std::static_pointer_cast<RTLCompare>(shared_from_this()));
    }
};

#endif // RTL_NODE_H_INCLUDED
