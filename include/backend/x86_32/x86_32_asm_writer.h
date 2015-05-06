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
#ifndef X86_32_ASM_WRITER_H_INCLUDED
#define X86_32_ASM_WRITER_H_INCLUDED

#include "backend/x86_32/x86_32_asm_nodes.h"
#include <ostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

class X86_32AsmWriter_GAS_Intel final : public X86_32AsmNodeVisitor
{
private:
    std::ostream &os;
    bool doBlockJoining = true;
    std::unordered_map<std::shared_ptr<X86_32AsmBasicBlock>, std::string> blockLabelMap;
    std::unordered_map<std::shared_ptr<X86_32AsmBasicBlock>, bool> blockJoinMap;
    std::size_t nextLabelIndex = 0;
    std::string getBlockLabel(std::shared_ptr<X86_32AsmBasicBlock> block)
    {
        auto iter = blockLabelMap.find(block);
        if(iter != blockLabelMap.end())
            return std::get<1>(*iter);
        std::ostringstream ss;
        ss << ".Ltmp" << ++nextLabelIndex;
        return blockLabelMap[block] = ss.str();
    }
    void writeBlockLabel(std::shared_ptr<X86_32AsmBasicBlock> block)
    {
        os << getBlockLabel(block) << ":\n";
    }
    X86_32AsmWriter_GAS_Intel(std::ostream &os)
        : os(os)
    {
    }
    enum class Phase
    {
        CreateBlockJoinMap,
        Write
    };
    Phase phase = Phase::CreateBlockJoinMap;
    std::shared_ptr<X86_32AsmBasicBlock> currentBlock, nextBlock;
    std::uint64_t alignedLocalsSize = 0;
    struct SavedRegister final
    {
        std::shared_ptr<X86_32AsmRegister> r;
        std::uint64_t saveLocationStart;
        bool isFloatingPoint;
        SavedRegister(std::shared_ptr<X86_32AsmRegister> r, std::uint64_t saveLocationStart, bool isFloatingPoint)
            : r(r), saveLocationStart(saveLocationStart), isFloatingPoint(isFloatingPoint)
        {
        }
    };
    std::list<SavedRegister> savedRegisters;
public:
    virtual void visitX86_32AsmNodeJump(std::shared_ptr<X86_32AsmNodeJump> node) override
    {
        switch(phase)
        {
        case Phase::CreateBlockJoinMap:
            if(node->target.lock() == nextBlock)
            {
                blockJoinMap[nextBlock] = true;
            }
            break;
        case Phase::Write:
            if(!doBlockJoining || node->target.lock() != nextBlock)
                os << "    jmp " << getBlockLabel(node->target.lock()) << "\n";
            break;
        }
    }
    virtual void visitX86_32AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_32AsmNodeCompareAgainstConstantAndJump> node) override
    {
        switch(phase)
        {
        case Phase::CreateBlockJoinMap:
            if(node->trueTarget.lock() == nextBlock || node->falseTarget.lock() == nextBlock)
            {
                blockJoinMap[nextBlock] = true;
            }
            break;
        case Phase::Write:
        {
            bool reversed = false;
            bool skipFinalJump = false;
            if(doBlockJoining && node->trueTarget.lock() == nextBlock)
            {
                skipFinalJump = true;
                reversed = true;
            }
            if(doBlockJoining && node->falseTarget.lock() == nextBlock)
            {
                skipFinalJump = true;
            }
            std::shared_ptr<ValueBoolean> rhs = std::dynamic_pointer_cast<ValueBoolean>(node->rhs);
            if(rhs == nullptr)
                throw NotImplementedException("type not implemented");
            os << "    cmp %" << node->lhs->name << ", " << (rhs->value ? "1" : "0") << "\n";
            if(reversed)
                os << "    " << X86_32GetJmpName(X86_32InvertCondition(node->conditionType)) << " " << getBlockLabel(node->falseTarget.lock()) << "\n";
            else
                os << "    " << X86_32GetJmpName(node->conditionType) << " " << getBlockLabel(node->trueTarget.lock()) << "\n";
            if(!skipFinalJump)
            {
                if(reversed)
                    os << "    jmp " << getBlockLabel(node->trueTarget.lock()) << "\n";
                else
                    os << "    jmp " << getBlockLabel(node->falseTarget.lock()) << "\n";
            }
            break;
        }
        }
    }
    virtual void visitX86_32AsmNodeCompare(std::shared_ptr<X86_32AsmNodeCompare> node) override
    {
        os << "    cmp %" << node->lhs->name << ", %" << node->rhs->name << "\n";
        os << "    " << X86_32GetSetName(node->conditionType) << " %" << node->dest->name << "\n";
    }
    virtual void visitX86_32AsmNodeMove(std::shared_ptr<X86_32AsmNodeMove> node) override
    {
        os << "    mov %" << node->dest->name << ", %" << node->source->name << "\n";
    }
    virtual void visitX86_32AsmNodeLoadConstant(std::shared_ptr<X86_32AsmNodeLoadConstant> node) override
    {
        if(std::shared_ptr<ValueBoolean> valueBoolean = std::dynamic_pointer_cast<ValueBoolean>(node->value))
            os << "    mov %" << node->dest->name << ", " << (valueBoolean->value ? "1" : "0") << "\n";
        else if(std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(node->value))
        {
            os << "    lea %" << node->dest->name << ", [%ebp - " << (alignedLocalsSize - valueVariablePointer->location.getStart()) << "]\n";
        }
        else if(std::shared_ptr<ValueNullPointer> valueNullPointer = std::dynamic_pointer_cast<ValueNullPointer>(node->value))
            os << "    mov %" << node->dest->name << ", 0\n";
        else
            throw NotImplementedException("type not implemented");
    }
    virtual void visitX86_32AsmNodeLoad(std::shared_ptr<X86_32AsmNodeLoad> node) override
    {
        os << "    mov %" << node->dest->name << ", [%" << node->address->name << "]\n";
    }
    virtual void visitX86_32AsmNodeStore(std::shared_ptr<X86_32AsmNodeStore> node) override
    {
        os << "    mov [%" << node->address->name << "], %" << node->value->name << "\n";
    }
    virtual void visitX86_32AsmNodeLoadLocal(std::shared_ptr<X86_32AsmNodeLoadLocal> node) override
    {
        os << "    mov %" << node->dest->name << ", [%ebp - " << (alignedLocalsSize - node->location.getStart()) << "]\n";
    }
    virtual void visitX86_32AsmNodeStoreLocal(std::shared_ptr<X86_32AsmNodeStoreLocal> node) override
    {
        os << "    mov [%ebp - " << (alignedLocalsSize - node->location.getStart()) << "], %" << node->value->name << "\n";
    }
private:
    void visitX86_32AsmBasicBlock(std::shared_ptr<X86_32AsmBasicBlock> block, bool writeAlign)
    {
        currentBlock = block;
        switch(phase)
        {
        case Phase::CreateBlockJoinMap:
        {
            if(block->controlTransferInstruction != nullptr)
            {
                block->controlTransferInstruction->visit(*this);
            }
            break;
        }
        case Phase::Write:
        {
            if(writeAlign && (!doBlockJoining || !blockJoinMap[block]))
            {
                os << "    .align 16, 0x90\n";
            }
            writeBlockLabel(block);
            for(std::shared_ptr<X86_32AsmNode> node : block->instructions)
            {
                node->visit(*this);
            }
            if(block->controlTransferInstruction == nullptr) // final block
            {
                for(const SavedRegister &r : savedRegisters)
                {
                    if(r.isFloatingPoint)
                    {
                        os << "    movaps %" << r.r->name << ", [%ebp - " << (alignedLocalsSize - r.saveLocationStart) << "]\n";
                    }
                    else
                    {
                        os << "    mov %" << r.r->name << ", [%ebp - " << (alignedLocalsSize - r.saveLocationStart) << "]\n";
                    }
                }
                os << "    mov %esp, %ebp\n";
                os << "    pop %ebp\n";
                os << "    ret\n";
            }
            os << "\n";
            break;
        }
        }
    }
    void visitX86_32AsmFunction(std::shared_ptr<X86_32AsmFunction> function)
    {
        os << "    .text\n";
        os << "    .globl main\n";
        os << "    .align 16, 0x90\n";
        os << "    .type main, @function\n";
        os << "main:\n";
        os << "    push %ebp\n";
        os << "    mov %ebp, %esp\n";
        savedRegisters.clear();
        std::unordered_set<std::shared_ptr<X86_32AsmRegister>> savedRegistersSet;
        for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<X86_32AsmNode> node : block->instructions)
            {
                for(std::shared_ptr<X86_32AsmRegister> r : node->outputSet())
                {
                    r = r->getSaveRegister();
                    if(r == nullptr || r->isSpecialPurpose || !r->isCalleeSave)
                        continue;
                    savedRegistersSet.insert(r);
                }
            }
        }
        for(std::shared_ptr<X86_32AsmRegister> r : savedRegistersSet)
        {
            savedRegisters.push_front(SavedRegister(r, r->physicalRegisterKindMask.createSaveLocation(function->localVariablesSize), static_cast<bool>(r->physicalRegisterKindMask & X86_32AsmRegister::PhysicalRegisterKindMask::Float())));
        }
        savedRegistersSet.clear();
        const std::uint64_t stackAlign = 16;
        alignedLocalsSize = ((function->localVariablesSize + (stackAlign - 1)) / stackAlign) * stackAlign;
        if(alignedLocalsSize != 0)
            os << "    sub %esp, " << alignedLocalsSize << "\n";
        for(const SavedRegister &r : savedRegisters)
        {
            if(r.isFloatingPoint)
            {
                os << "    movaps [%ebp - " << (alignedLocalsSize - r.saveLocationStart) << "], %" << r.r->name << "\n";
            }
            else
            {
                os << "    mov [%ebp - " << (alignedLocalsSize - r.saveLocationStart) << "], %" << r.r->name << "\n";
            }
        }
        os << "\n";
        std::vector<std::shared_ptr<X86_32AsmBasicBlock>> blocks;
        blocks.reserve(function->blocks.size());
        blocks.push_back(function->startBlock);
        for(std::shared_ptr<X86_32AsmBasicBlock> block : function->blocks)
        {
            blockJoinMap[block] = false;
            if(block == function->startBlock)
                continue;
            blocks.push_back(block);
        }
        phase = Phase::CreateBlockJoinMap;
        for(std::size_t i = 0; i < blocks.size(); i++)
        {
            std::shared_ptr<X86_32AsmBasicBlock> block = blocks[i];
            if(i + 1 >= blocks.size())
                nextBlock = nullptr;
            else
                nextBlock = blocks[i + 1];
            visitX86_32AsmBasicBlock(block, block != function->startBlock);
        }
        phase = Phase::Write;
        for(std::size_t i = 0; i < blocks.size(); i++)
        {
            std::shared_ptr<X86_32AsmBasicBlock> block = blocks[i];
            if(i + 1 >= blocks.size())
                nextBlock = nullptr;
            else
                nextBlock = blocks[i + 1];
            visitX86_32AsmBasicBlock(block, block != function->startBlock);
        }
        os << "\n\n";
    }
public:
    static void run(std::ostream &os, const std::list<std::shared_ptr<X86_32AsmFunction>> &functions)
    {
        X86_32AsmWriter_GAS_Intel writer(os);
        os << ".intel_syntax prefix\n\n";
        for(std::shared_ptr<X86_32AsmFunction> function : functions)
        {
            writer.visitX86_32AsmFunction(function);
        }
    }
};

#endif // X86_32_ASM_WRITER_H_INCLUDED
