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
#ifndef X86_ASM_WRITER_H_INCLUDED
#define X86_ASM_WRITER_H_INCLUDED

#include "backend/x86/x86_asm_nodes.h"
#include <ostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

class X86AsmWriter_GAS_Intel final : public X86AsmNodeVisitor
{
private:
    const BackendX86 *const backend;
    std::ostream &os;
    bool doBlockJoining = true;
    std::unordered_map<std::shared_ptr<X86AsmBasicBlock>, std::string> blockLabelMap;
    std::unordered_map<std::shared_ptr<X86AsmBasicBlock>, bool> blockJoinMap;
    std::size_t nextLabelIndex = 0;
    std::string getBlockLabel(std::shared_ptr<X86AsmBasicBlock> block)
    {
        auto iter = blockLabelMap.find(block);
        if(iter != blockLabelMap.end())
            return std::get<1>(*iter);
        std::ostringstream ss;
        ss << ".Ltmp" << ++nextLabelIndex;
        return blockLabelMap[block] = ss.str();
    }
    void writeBlockLabel(std::shared_ptr<X86AsmBasicBlock> block)
    {
        os << getBlockLabel(block) << ":\n";
    }
    X86AsmWriter_GAS_Intel(std::ostream &os, const BackendX86 *backend)
        : backend(backend), os(os)
    {
    }
    enum class Phase
    {
        CreateBlockJoinMap,
        Write
    };
    Phase phase = Phase::CreateBlockJoinMap;
    std::shared_ptr<X86AsmBasicBlock> currentBlock, nextBlock;
    std::uint64_t alignedLocalsSize = 0;
    struct SavedRegister final
    {
        std::shared_ptr<X86AsmRegister> r;
        std::uint64_t saveLocationStart;
        bool isFloatingPoint;
        SavedRegister(std::shared_ptr<X86AsmRegister> r, std::uint64_t saveLocationStart, bool isFloatingPoint)
            : r(r), saveLocationStart(saveLocationStart), isFloatingPoint(isFloatingPoint)
        {
        }
    };
    std::list<SavedRegister> savedRegisters;
public:
    virtual void visitX86AsmNodeJump(std::shared_ptr<X86AsmNodeJump> node) override
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
    virtual void visitX86AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86AsmNodeCompareAgainstConstantAndJump> node) override
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
                os << "    " << X86GetJmpName(X86InvertCondition(node->conditionType)) << " " << getBlockLabel(node->falseTarget.lock()) << "\n";
            else
                os << "    " << X86GetJmpName(node->conditionType) << " " << getBlockLabel(node->trueTarget.lock()) << "\n";
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
    virtual void visitX86AsmNodeCompare(std::shared_ptr<X86AsmNodeCompare> node) override
    {
        os << "    cmp %" << node->lhs->name << ", %" << node->rhs->name << "\n";
        os << "    " << X86GetSetName(node->conditionType) << " %" << node->dest->name << "\n";
    }
    virtual void visitX86AsmNodeMove(std::shared_ptr<X86AsmNodeMove> node) override
    {
        os << "    mov %" << node->dest->name << ", %" << node->source->name << "\n";
    }
    virtual void visitX86AsmNodeLoadConstant(std::shared_ptr<X86AsmNodeLoadConstant> node) override
    {
        if(std::shared_ptr<ValueBoolean> valueBoolean = std::dynamic_pointer_cast<ValueBoolean>(node->value))
            os << "    mov %" << node->dest->name << ", " << (valueBoolean->value ? "1" : "0") << "\n";
        else if(std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(node->value))
        {
            os << "    lea %" << node->dest->name << ", [%" << X86AsmRegister::getBasePointer(node->context, backend)->name << " - " << (alignedLocalsSize - valueVariablePointer->location.getStart()) << "]\n";
        }
        else if(std::shared_ptr<ValueNullPointer> valueNullPointer = std::dynamic_pointer_cast<ValueNullPointer>(node->value))
            os << "    mov %" << node->dest->name << ", 0\n";
        else if(std::shared_ptr<ValueInteger> valueInteger = std::dynamic_pointer_cast<ValueInteger>(node->value))
        {
            if(valueInteger->isUnsigned)
                os << "    mov %" << node->dest->name << ", " << valueInteger->getUnsignedValue() << "\n";
            else
                os << "    mov %" << node->dest->name << ", " << valueInteger->getSignedValue() << "\n";
        }
        else
            throw NotImplementedException("type not implemented");
    }
    virtual void visitX86AsmNodeLoad(std::shared_ptr<X86AsmNodeLoad> node) override
    {
        os << "    mov %" << node->dest->name << ", [%" << node->address->name << "]\n";
    }
    virtual void visitX86AsmNodeStore(std::shared_ptr<X86AsmNodeStore> node) override
    {
        os << "    mov [%" << node->address->name << "], %" << node->value->name << "\n";
    }
    virtual void visitX86AsmNodeLoadLocal(std::shared_ptr<X86AsmNodeLoadLocal> node) override
    {
        os << "    mov %" << node->dest->name << ", [%" << X86AsmRegister::getBasePointer(node->context, backend)->name << " - " << (alignedLocalsSize - node->location.getStart()) << "]\n";
    }
    virtual void visitX86AsmNodeStoreLocal(std::shared_ptr<X86AsmNodeStoreLocal> node) override
    {
        os << "    mov [%" << X86AsmRegister::getBasePointer(node->context, backend)->name << " - " << (alignedLocalsSize - node->location.getStart()) << "], %" << node->value->name << "\n";
    }
private:
    void visitX86AsmBasicBlock(std::shared_ptr<X86AsmBasicBlock> block, bool writeAlign)
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
            for(std::shared_ptr<X86AsmNode> node : block->instructions)
            {
                node->visit(*this);
            }
            if(block->controlTransferInstruction == nullptr) // final block
            {
                os << "    .cfi_remember_state\n";
                for(const SavedRegister &r : savedRegisters)
                {
                    if(r.isFloatingPoint)
                    {
                        os << "    movaps %" << r.r->name << ", [%" << X86AsmRegister::getBasePointer(block->context, backend)->name << " - " << (alignedLocalsSize - r.saveLocationStart) << "]\n";
                    }
                    else
                    {
                        os << "    mov %" << r.r->name << ", [%" << X86AsmRegister::getBasePointer(block->context, backend)->name << " - " << (alignedLocalsSize - r.saveLocationStart) << "]\n";
                        os << "    .cfi_restore %" << r.r->name << "\n";
                    }
                }
                switch(backend->architecture)
                {
                case BackendX86::X86_32:
                    os << "    mov %esp, %ebp\n";
                    os << "    pop %ebp\n";
                    os << "    .cfi_def_cfa %esp, 4\n";
                    os << "    ret\n";
                    os << "    .cfi_restore_state\n";
                    break;
                case BackendX86::X86_64:
                    os << "    mov %rsp, %rbp\n";
                    os << "    pop %rbp\n";
                    os << "    .cfi_def_cfa %rsp, 8\n";
                    os << "    ret\n";
                    os << "    .cfi_restore_state\n";
                    break;
                }
            }
            os << "\n";
            break;
        }
        }
    }
    void visitX86AsmFunction(std::shared_ptr<X86AsmFunction> function)
    {
        os << "    .text\n";
        os << "    .globl main\n";
        os << "    .align 16, 0x90\n";
        os << "    .type main, @function\n";
        os << "main:\n";
        os << "    .cfi_startproc\n";
        switch(backend->architecture)
        {
        case BackendX86::X86_32:
            os << "    push %ebp\n";
            os << "    .cfi_def_cfa_offset 8\n";
            os << "    mov %ebp, %esp\n";
            os << "    .cfi_offset %ebp, -8\n";
            os << "    .cfi_def_cfa_register %ebp\n";
            break;
        case BackendX86::X86_64:
            os << "    push %rbp\n";
            os << "    .cfi_def_cfa_offset 16\n";
            os << "    .cfi_offset %rbp, -16\n";
            os << "    mov %rbp, %rsp\n";
            os << "    .cfi_def_cfa_register %rbp\n";
            break;
        }
        savedRegisters.clear();
        std::unordered_set<std::shared_ptr<X86AsmRegister>> savedRegistersSet;
        for(std::shared_ptr<X86AsmBasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<X86AsmNode> node : block->instructions)
            {
                for(std::shared_ptr<X86AsmRegister> r : node->outputSet())
                {
                    r = r->getSaveRegister();
                    if(r == nullptr || r->isSpecialPurpose || !r->isCalleeSave)
                        continue;
                    savedRegistersSet.insert(r);
                }
            }
        }
        for(std::shared_ptr<X86AsmRegister> r : savedRegistersSet)
        {
            savedRegisters.push_front(SavedRegister(r, r->physicalRegisterKindMask.createSaveLocation(function->localVariablesSize), static_cast<bool>(r->physicalRegisterKindMask & X86AsmRegister::PhysicalRegisterKindMask::Float())));
        }
        savedRegistersSet.clear();
        const std::uint64_t stackAlign = 16;
        alignedLocalsSize = ((function->localVariablesSize + (stackAlign - 1)) / stackAlign) * stackAlign;
        switch(backend->architecture)
        {
        case BackendX86::X86_32:
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
                    os << "    .cfi_rel_offset %" << r.r->name << ", " << (alignedLocalsSize - r.saveLocationStart) << "\n";
                }
            }
            break;
        case BackendX86::X86_64:
            if(alignedLocalsSize != 0)
                os << "    sub %rsp, " << alignedLocalsSize << "\n";
            for(const SavedRegister &r : savedRegisters)
            {
                if(r.isFloatingPoint)
                {
                    os << "    movaps [%rbp - " << (alignedLocalsSize - r.saveLocationStart) << "], %" << r.r->name << "\n";
                }
                else
                {
                    os << "    mov [%rbp - " << (alignedLocalsSize - r.saveLocationStart) << "], %" << r.r->name << "\n";
                    os << "    .cfi_rel_offset %" << r.r->name << ", " << (alignedLocalsSize - r.saveLocationStart) << "\n";
                }
            }
            break;
        }
        os << "\n";
        std::vector<std::shared_ptr<X86AsmBasicBlock>> blocks;
        blocks.reserve(function->blocks.size());
        blocks.push_back(function->startBlock);
        for(std::shared_ptr<X86AsmBasicBlock> block : function->blocks)
        {
            blockJoinMap[block] = false;
            if(block == function->startBlock)
                continue;
            blocks.push_back(block);
        }
        phase = Phase::CreateBlockJoinMap;
        for(std::size_t i = 0; i < blocks.size(); i++)
        {
            std::shared_ptr<X86AsmBasicBlock> block = blocks[i];
            if(i + 1 >= blocks.size())
                nextBlock = nullptr;
            else
                nextBlock = blocks[i + 1];
            visitX86AsmBasicBlock(block, block != function->startBlock);
        }
        phase = Phase::Write;
        for(std::size_t i = 0; i < blocks.size(); i++)
        {
            std::shared_ptr<X86AsmBasicBlock> block = blocks[i];
            if(i + 1 >= blocks.size())
                nextBlock = nullptr;
            else
                nextBlock = blocks[i + 1];
            visitX86AsmBasicBlock(block, block != function->startBlock);
        }
        os << "    .cfi_endproc\n";
        os << "\n\n";
    }
public:
    static void run(std::ostream &os, const std::list<std::shared_ptr<X86AsmFunction>> &functions, const BackendX86 *backend)
    {
        X86AsmWriter_GAS_Intel writer(os, backend);
        os << ".intel_syntax prefix\n\n";
        for(std::shared_ptr<X86AsmFunction> function : functions)
        {
            writer.visitX86AsmFunction(function);
        }
    }
};

#endif // X86_ASM_WRITER_H_INCLUDED
