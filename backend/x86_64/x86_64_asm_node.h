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
#ifndef X86_64_ASM_NODE_H_INCLUDED
#define X86_64_ASM_NODE_H_INCLUDED

#include <memory>
#include <string>
#include <unordered_set>
#include <list>
#include "../../context.h"
#include "../../types/types.h"
#include "../../values/values.h"
#include <cassert>

class X86_64AsmRegister final : public std::enable_shared_from_this<X86_64AsmRegister>
{
    X86_64AsmRegister(const X86_64AsmRegister &) = delete;
    X86_64AsmRegister &operator =(const X86_64AsmRegister &) = delete;
public:
    CompilerContext *const context;
    enum class RegisterType
    {
        Virtual,
        Physical,
    };
    const RegisterType registerType;
    const std::string name;
    X86_64AsmRegister(CompilerContext *context, RegisterType registerType, std::string name)
        : context(context), registerType(registerType), name(name)
    {
    }
};

class X86_64AsmNodeVisitor;

class X86_64AsmNode : public std::enable_shared_from_this<X86_64AsmNode>
{
    X86_64AsmNode(const X86_64AsmNode &) = delete;
    X86_64AsmNode &operator =(const X86_64AsmNode &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmNode(CompilerContext *context)
        : context(context)
    {
    }
    virtual ~X86_64AsmNode() = default;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const = 0;
    virtual void visit(X86_64AsmNodeVisitor &visitor) = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
};

class X86_64AsmBasicBlock;

class X86_64AsmControlTransfer : public X86_64AsmNode
{
public:
    explicit X86_64AsmControlTransfer(CompilerContext *context)
        : X86_64AsmNode(context)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const = 0;
    virtual void visit(X86_64AsmNodeVisitor &visitor) = 0;
};

class X86_64AsmBasicBlock final : public std::enable_shared_from_this<X86_64AsmBasicBlock>
{
    X86_64AsmBasicBlock(const X86_64AsmBasicBlock &) = delete;
    X86_64AsmBasicBlock &operator =(const X86_64AsmBasicBlock &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmBasicBlock(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_64AsmControlTransfer> controlTransferInstruction;
    std::list<std::shared_ptr<X86_64AsmNode>> instructions;
    std::list<std::weak_ptr<X86_64AsmBasicBlock>> sourceBlocks;
    std::list<std::weak_ptr<X86_64AsmBasicBlock>> destBlocks;
};

class X86_64AsmFunction final : public std::enable_shared_from_this<X86_64AsmFunction>
{
    X86_64AsmFunction(const X86_64AsmFunction &) = delete;
    X86_64AsmFunction &operator =(const X86_64AsmFunction &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmFunction(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_64AsmBasicBlock> startBlock;
    std::list<std::shared_ptr<X86_64AsmBasicBlock>> blocks;
};

class X86_64AsmNodeJump;
class X86_64AsmNodeCompareAgainstConstantAndJump;
class X86_64AsmNodeMove;
class X86_64AsmNodeLoadConstant;

class X86_64AsmNodeVisitor
{
public:
    virtual void visitX86_64AsmNodeJump(std::shared_ptr<X86_64AsmNodeJump> node) = 0;
    virtual void visitX86_64AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_64AsmNodeCompareAgainstConstantAndJump> node) = 0;
    virtual void visitX86_64AsmNodeMove(std::shared_ptr<X86_64AsmNodeMove> node) = 0;
    virtual void visitX86_64AsmNodeLoadConstant(std::shared_ptr<X86_64AsmNodeLoadConstant> node) = 0;
};

class X86_64AsmNodeJump final : public X86_64AsmControlTransfer
{
public:
    std::weak_ptr<X86_64AsmBasicBlock> target;
    explicit X86_64AsmNodeJump(std::shared_ptr<X86_64AsmBasicBlock> target)
        : X86_64AsmControlTransfer(target->context), target(target)
    {
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_64AsmBasicBlock>>{target};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeJump(std::static_pointer_cast<X86_64AsmNodeJump>(shared_from_this()));
    }
};

enum class X86_64ConditionType
{
    Z,
    NZ,
    S,
    NS,
    O,
    NO,
    C,
    NC,
    B,
    BE,
    AE,
    A,
    L,
    LE,
    GE,
    G,
    E,
    NE,
};

inline std::string X86_64GetJmpName(X86_64ConditionType condition)
{
    switch(condition)
    {
    case X86_64ConditionType::Z:
        return "jz";
    case X86_64ConditionType::NZ:
        return "jnz";
    case X86_64ConditionType::S:
        return "js";
    case X86_64ConditionType::NS:
        return "jns";
    case X86_64ConditionType::O:
        return "jo";
    case X86_64ConditionType::NO:
        return "jno";
    case X86_64ConditionType::C:
        return "jc";
    case X86_64ConditionType::NC:
        return "jnc";
    case X86_64ConditionType::B:
        return "jb";
    case X86_64ConditionType::BE:
        return "jbe";
    case X86_64ConditionType::AE:
        return "jae";
    case X86_64ConditionType::A:
        return "ja";
    case X86_64ConditionType::L:
        return "jl";
    case X86_64ConditionType::LE:
        return "jle";
    case X86_64ConditionType::GE:
        return "jge";
    case X86_64ConditionType::G:
        return "jg";
    case X86_64ConditionType::E:
        return "je";
    case X86_64ConditionType::NE:
        return "jne";
    }
    assert(false);
    return "";
}

class X86_64AsmNodeCompareAgainstConstantAndJump final : public X86_64AsmControlTransfer
{
public:
    std::shared_ptr<X86_64AsmRegister> lhs;
    std::shared_ptr<ValueNode> rhs;
    X86_64ConditionType conditionType;
    std::weak_ptr<X86_64AsmBasicBlock> trueTarget;
    std::weak_ptr<X86_64AsmBasicBlock> falseTarget;
    explicit X86_64AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_64AsmRegister> lhs,
                                                        std::shared_ptr<ValueNode> rhs,
                                                        X86_64ConditionType conditionType,
                                                        std::shared_ptr<X86_64AsmBasicBlock> trueTarget,
                                                        std::shared_ptr<X86_64AsmBasicBlock> falseTarget)
        : X86_64AsmControlTransfer(lhs->context), lhs(lhs), rhs(rhs), conditionType(conditionType), trueTarget(trueTarget), falseTarget(falseTarget)
    {
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_64AsmBasicBlock>>{trueTarget, falseTarget};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{lhs};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeCompareAgainstConstantAndJump(std::static_pointer_cast<X86_64AsmNodeCompareAgainstConstantAndJump>(shared_from_this()));
    }
};

class X86_64AsmNodeMove final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<X86_64AsmRegister> source;
    explicit X86_64AsmNodeMove(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<X86_64AsmRegister> source)
        : X86_64AsmNode(dest->context), dest(dest), source(source)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{source};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeMove(std::static_pointer_cast<X86_64AsmNodeMove>(shared_from_this()));
    }
};

class X86_64AsmNodeLoadConstant final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<ValueNode> value;
    explicit X86_64AsmNodeLoadConstant(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<ValueNode> value)
        : X86_64AsmNode(dest->context), dest(dest), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeLoadConstant(std::static_pointer_cast<X86_64AsmNodeLoadConstant>(shared_from_this()));
    }
};

#endif // X86_64_ASM_NODE_H_INCLUDED
