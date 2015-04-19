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

#include <ostream>
#include <memory>
#include <unordered_map>

class DumpVisitor final : public SSANodeVisitor, public TypeVisitor, public ValueNodeVisitor
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
    void dumpInstructionName(const char *name, std::shared_ptr<SSANode> node)
    {
        os << "[" << getSSANodeDisplayValue(node) << "]" << name;
    }
public:
    explicit DumpVisitor(std::ostream &os)
        : os(os)
    {
    }
    virtual void visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node) override;
    virtual void visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node) override;
    virtual void visitSSAPhi(std::shared_ptr<SSAPhi> node) override;
    virtual void visitSSAConstant(std::shared_ptr<SSAConstant> node) override;
    virtual void visitTypeConstant(std::shared_ptr<TypeConstant> node) override;
    virtual void visitTypeVolatile(std::shared_ptr<TypeVolatile> node) override;
    virtual void visitTypeVoid(std::shared_ptr<TypeVoid> node) override;
    virtual void visitTypeBoolean(std::shared_ptr<TypeBoolean> node) override;
    virtual void visitValueBoolean(std::shared_ptr<ValueBoolean> node) override;
    virtual void visitValueUnknown(std::shared_ptr<ValueUnknown> node) override;
    void visitSSANode(std::shared_ptr<SSANode> node)
    {
        node->visit(*this);
    }
    void visitSSABasicBlock(std::shared_ptr<SSABasicBlock> node);
    void visitSSAFunction(std::shared_ptr<SSAFunction> node);
};

#endif // DUMP_H_INCLUDED