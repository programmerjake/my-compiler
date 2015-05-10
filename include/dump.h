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
#ifndef DUMP_H_INCLUDED
#define DUMP_H_INCLUDED

#include "ssa/ssa_visitor.h"
#include "types/type.h"
#include "values/value.h"
#include "rtl/rtl_nodes.h"

#include <ostream>
#include <memory>
#include <unordered_map>

class DumpVisitor final : public SSANodeVisitor, public TypeVisitor, public ValueNodeVisitor, public RTLNodeVisitor
{
private:
    std::ostream &os;
    std::unordered_map<std::shared_ptr<SSANode>, std::size_t> ssaNodeMap;
    std::size_t nextSSANode = 1;
public:
    std::size_t getSSANodeDisplayValue(std::shared_ptr<SSANode> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = ssaNodeMap.find(node);
        if(iter != ssaNodeMap.end())
            return std::get<1>(*iter);
        return ssaNodeMap[node] = nextSSANode++;
    }
private:
    std::unordered_map<std::shared_ptr<SSABasicBlock>, std::size_t> ssaBasicBlockMap;
    std::size_t nextSSABasicBlock = 1;
public:
    std::size_t getSSABasicBlockDisplayValue(std::shared_ptr<SSABasicBlock> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = ssaBasicBlockMap.find(node);
        if(iter != ssaBasicBlockMap.end())
            return std::get<1>(*iter);
        return ssaBasicBlockMap[node] = nextSSABasicBlock++;
    }
private:
    std::unordered_map<std::shared_ptr<SSAFunction>, std::size_t> ssaFunctionMap;
    std::size_t nextSSAFunction = 1;
public:
    std::size_t getSSAFunctionDisplayValue(std::shared_ptr<SSAFunction> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = ssaFunctionMap.find(node);
        if(iter != ssaFunctionMap.end())
            return std::get<1>(*iter);
        return ssaFunctionMap[node] = nextSSAFunction++;
    }
private:
    std::unordered_map<std::shared_ptr<RTLNode>, std::size_t> rtlNodeMap;
    std::size_t nextRTLNode = 1;
public:
    std::size_t getRTLNodeDisplayValue(std::shared_ptr<RTLNode> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = rtlNodeMap.find(node);
        if(iter != rtlNodeMap.end())
            return std::get<1>(*iter);
        return rtlNodeMap[node] = nextRTLNode++;
    }
private:
    std::unordered_map<std::shared_ptr<RTLBasicBlock>, std::size_t> rtlBasicBlockMap;
    std::size_t nextRTLBasicBlock = 1;
public:
    std::size_t getRTLBasicBlockDisplayValue(std::shared_ptr<RTLBasicBlock> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = rtlBasicBlockMap.find(node);
        if(iter != rtlBasicBlockMap.end())
            return std::get<1>(*iter);
        return rtlBasicBlockMap[node] = nextRTLBasicBlock++;
    }
private:
    std::unordered_map<std::shared_ptr<RTLFunction>, std::size_t> rtlFunctionMap;
    std::size_t nextRTLFunction = 1;
public:
    std::size_t getRTLFunctionDisplayValue(std::shared_ptr<RTLFunction> node)
    {
        if(node == nullptr)
            return 0;
        auto iter = rtlFunctionMap.find(node);
        if(iter != rtlFunctionMap.end())
            return std::get<1>(*iter);
        return rtlFunctionMap[node] = nextRTLFunction++;
    }
private:
    void dumpInstructionName(const char *name, std::shared_ptr<SSANode> node)
    {
        os << "[" << getSSANodeDisplayValue(node) << "]" << name;
    }
    void dumpRTLRegister(std::shared_ptr<RTLRegister> r);
public:
    explicit DumpVisitor(std::ostream &os)
        : os(os)
    {
    }
    virtual void visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node) override;
    virtual void visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node) override;
    virtual void visitSSAPhi(std::shared_ptr<SSAPhi> node) override;
    virtual void visitSSAConstant(std::shared_ptr<SSAConstant> node) override;
    virtual void visitSSAMove(std::shared_ptr<SSAMove> node) override;
    virtual void visitSSALoad(std::shared_ptr<SSALoad> node) override;
    virtual void visitSSAStore(std::shared_ptr<SSAStore> node) override;
    virtual void visitSSACompare(std::shared_ptr<SSACompare> node) override;
    virtual void visitSSAAllocA(std::shared_ptr<SSAAllocA> node) override;
    virtual void visitSSATypeCast(std::shared_ptr<SSATypeCast> node) override;
    virtual void visitSSAAdd(std::shared_ptr<SSAAdd> node) override;
    virtual void visitTypeConstant(std::shared_ptr<TypeConstant> node) override;
    virtual void visitTypeVolatile(std::shared_ptr<TypeVolatile> node) override;
    virtual void visitTypeVoid(std::shared_ptr<TypeVoid> node) override;
    virtual void visitTypeBoolean(std::shared_ptr<TypeBoolean> node) override;
    virtual void visitTypePointer(std::shared_ptr<TypePointer> node) override;
    virtual void visitTypeInteger(std::shared_ptr<TypeInteger> node) override;
    virtual void visitValueBoolean(std::shared_ptr<ValueBoolean> node) override;
    virtual void visitValueUnknown(std::shared_ptr<ValueUnknown> node) override;
    virtual void visitValueVariablePointer(std::shared_ptr<ValueVariablePointer> node) override;
    virtual void visitValueNullPointer(std::shared_ptr<ValueNullPointer> node) override;
    virtual void visitValueInteger(std::shared_ptr<ValueInteger> node) override;
    virtual void visitRTLLoadConstant(std::shared_ptr<RTLLoadConstant> node) override;
    virtual void visitRTLMove(std::shared_ptr<RTLMove> node) override;
    virtual void visitRTLUnconditionalJump(std::shared_ptr<RTLUnconditionalJump> node) override;
    virtual void visitRTLConditionalJump(std::shared_ptr<RTLConditionalJump> node) override;
    virtual void visitRTLLoad(std::shared_ptr<RTLLoad> node) override;
    virtual void visitRTLStore(std::shared_ptr<RTLStore> node) override;
    virtual void visitRTLCompare(std::shared_ptr<RTLCompare> node) override;
    virtual void visitRTLTypeCast(std::shared_ptr<RTLTypeCast> node) override;
    virtual void visitRTLAdd(std::shared_ptr<RTLAdd> node) override;
    void visitSSANode(std::shared_ptr<SSANode> node)
    {
        node->visit(*this);
    }
    void visitSSABasicBlock(std::shared_ptr<SSABasicBlock> node);
    void visitSSAFunction(std::shared_ptr<SSAFunction> node);
    void visitRTLNode(std::shared_ptr<RTLNode> node)
    {
        node->visit(*this);
    }
    void visitRTLBasicBlock(std::shared_ptr<RTLBasicBlock> node);
    void visitRTLFunction(std::shared_ptr<RTLFunction> node);
};

#endif // DUMP_H_INCLUDED
