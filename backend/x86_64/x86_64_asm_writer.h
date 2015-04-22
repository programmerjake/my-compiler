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
#ifndef X86_64_ASM_WRITER_H_INCLUDED
#define X86_64_ASM_WRITER_H_INCLUDED

#include "x86_64_asm_nodes.h"
#include <ostream>
#include <string>
#include <sstream>
#include <unordered_map>

class X86_64AsmWriter_GAS_Intel final : public X86_64AsmNodeVisitor
{
private:
    std::ostream &os;
    std::unordered_map<std::shared_ptr<X86_64AsmBasicBlock>, std::string> blockLabelMap;
    std::size_t nextLabelIndex = 0;
    std::string getBlockLabel(std::shared_ptr<X86_64AsmBasicBlock> block)
    {
        auto iter = blockLabelMap.find(block);
        if(iter != blockLabelMap.end())
            return std::get<1>(*iter);
        std::ostringstream ss;
        ss << ".Ltmp" << ++nextLabelIndex;
        return blockLabelMap[block] = ss.str();
    }
    void writeBlockLabel(std::shared_ptr<X86_64AsmBasicBlock> block)
    {
        os << getBlockLabel(block) << ":\n";
    }
    X86_64AsmWriter_GAS_Intel(std::ostream &os)
        : os(os)
    {
    }
public:
    virtual void visitX86_64AsmNodeJump(std::shared_ptr<X86_64AsmNodeJump> node) override
    {
        os << "    jmp " << getBlockLabel(node->target.lock()) << "\n";
    }
    virtual void visitX86_64AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_64AsmNodeCompareAgainstConstantAndJump> node) override
    {
        std::shared_ptr<ValueBoolean> rhs = std::dynamic_pointer_cast<ValueBoolean>(node->rhs);
        if(rhs == nullptr)
            throw NotImplementedException("type not implemented");
        os << "    cmp " << node->lhs->name << ", " << (rhs->value ? "1" : "0") << "\n";
        os << "    " << X86_64GetJmpName(node->conditionType) << " " << getBlockLabel(node->trueTarget.lock()) << "\n";
        os << "    jmp " << getBlockLabel(node->falseTarget.lock()) << "\n";
    }
    virtual void visitX86_64AsmNodeMove(std::shared_ptr<X86_64AsmNodeMove> node) override
    {
        os << "    mov " << node->dest->name << ", " << node->source->name << "\n";
    }
    virtual void visitX86_64AsmNodeLoadConstant(std::shared_ptr<X86_64AsmNodeLoadConstant> node) override
    {
        std::shared_ptr<ValueBoolean> value = std::dynamic_pointer_cast<ValueBoolean>(node->value);
        if(value == nullptr)
            throw NotImplementedException("type not implemented");
        os << "    mov " << node->dest->name << ", " << (value->value ? "1" : "0") << "\n";
    }
private:
    void visitX86_64AsmBasicBlock(std::shared_ptr<X86_64AsmBasicBlock> block, bool writeAlign)
    {
        if(writeAlign)
            os << "    .align 16, 0x90\n";
        writeBlockLabel(block);
        for(std::shared_ptr<X86_64AsmNode> node : block->instructions)
        {
            node->visit(*this);
        }
        if(block->controlTransferInstruction == nullptr) // final block
        {
            os << "    mov rsp, rbp\n";
            os << "    pop rbp\n";
            os << "    ret\n";
        }
        os << "\n";
    }
    void visitX86_64AsmFunction(std::shared_ptr<X86_64AsmFunction> function)
    {
        os << "    .text\n";
        os << "    .globl main\n";
        os << "    .align 16, 0x90\n";
        os << "    .type main, @function\n";
        os << "main:\n";
        os << "    push rbp\n";
        os << "    mov rbp, rsp\n";
        os << "    sub rsp, 0\n";
        visitX86_64AsmBasicBlock(function->startBlock, false);
        for(std::shared_ptr<X86_64AsmBasicBlock> block : function->blocks)
        {
            if(block == function->startBlock)
                continue;
            visitX86_64AsmBasicBlock(block, true);
        }
        os << "\n\n";
    }
public:
    static void run(std::ostream &os, const std::list<std::shared_ptr<X86_64AsmFunction>> &functions)
    {
        X86_64AsmWriter_GAS_Intel writer(os);
        os << ".intel_syntax\n\n";
        for(std::shared_ptr<X86_64AsmFunction> function : functions)
        {
            writer.visitX86_64AsmFunction(function);
        }
    }
};

#endif // X86_64_ASM_WRITER_H_INCLUDED
