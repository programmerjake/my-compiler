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
#ifndef X86_64_RTL_TO_ASM_H_INCLUDED
#define X86_64_RTL_TO_ASM_H_INCLUDED

#include "../../rtl/rtl_nodes.h"
#include "x86_64_asm_nodes.h"
#include <unordered_map>
#include <unordered_set>
#include "../../construct_liveness_info.h"
#include "x86_64_construct_liveness_info.h"

class X86_64ConvertRTLToAsm final : public RTLNodeVisitor
{
private:
    std::shared_ptr<X86_64AsmBasicBlock> currentBlock;
    std::shared_ptr<X86_64AsmFunction> currentFunction;
    struct RegisterHasher final
    {
        std::size_t operator ()(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v) const
        {
            return 3 * std::hash<std::shared_ptr<RTLRegister>>()(std::get<0>(v)) + std::hash<std::shared_ptr<TypeNode>>()(std::get<1>(v));
        }
    };
    std::unordered_map<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>, std::shared_ptr<X86_64AsmRegister>, RegisterHasher> registerMap;
    std::unordered_map<std::shared_ptr<RTLBasicBlock>, std::shared_ptr<X86_64AsmBasicBlock>> blockMap;
    std::unordered_map<std::shared_ptr<RTLFunction>, std::shared_ptr<X86_64AsmFunction>> functionMap;
    std::shared_ptr<X86_64AsmRegister> getOrMakeRegister(std::shared_ptr<RTLRegister> reg, std::shared_ptr<TypeNode> type)
    {
        auto v = std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>(reg, type);
        std::shared_ptr<X86_64AsmRegister> &retval = registerMap[v];
        if(retval == nullptr)
        {
            retval = X86_64AsmRegister::getVirtualRegister(reg->context, reg->name, X86_64TypeToPhysicalRegisterKindMask::run(type));
        }
        return retval;
    }
    std::shared_ptr<X86_64AsmBasicBlock> getOrMakeBlock(std::shared_ptr<RTLBasicBlock> v)
    {
        std::shared_ptr<X86_64AsmBasicBlock> &retval = blockMap[v];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86_64AsmBasicBlock>(v->context);
        }
        return retval;
    }
    std::shared_ptr<X86_64AsmFunction> getOrMakeFunction(std::shared_ptr<RTLFunction> v)
    {
        std::shared_ptr<X86_64AsmFunction> &retval = functionMap[v];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86_64AsmFunction>(v->context);
        }
        return retval;
    }
    X86_64ConvertRTLToAsm()
    {
    }
    void visitRTLNode(std::shared_ptr<RTLNode> node)
    {
        node->visit(*this);
    }
    void visitRTLBasicBlock(std::shared_ptr<RTLBasicBlock> block)
    {
        currentBlock = getOrMakeBlock(block);
        currentFunction->blocks.push_back(currentBlock);
        for(std::weak_ptr<RTLBasicBlock> vW : block->destBlocks)
        {
            std::shared_ptr<RTLBasicBlock> v = vW.lock();
            currentBlock->destBlocks.push_back(getOrMakeBlock(v));
        }
        for(std::weak_ptr<RTLBasicBlock> vW : block->sourceBlocks)
        {
            std::shared_ptr<RTLBasicBlock> v = vW.lock();
            currentBlock->sourceBlocks.push_back(getOrMakeBlock(v));
        }
        for(std::shared_ptr<RTLNode> node : block->instructions)
        {
            visitRTLNode(node);
        }
    }
    static void constructLivenessInfo(std::shared_ptr<X86_64AsmFunction> function)
    {
        X86_64ConstructLivenessInfo().visitX86_64AsmFunction(function);
    }
    std::shared_ptr<X86_64AsmFunction> visitRTLFunction(std::shared_ptr<RTLFunction> function)
    {
        currentFunction = getOrMakeFunction(function);
        currentFunction->localVariablesSize = function->localVariablesSize;
        currentFunction->startBlock = getOrMakeBlock(function->startBlock);
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            visitRTLBasicBlock(block);
        }
        registerMap.clear();
        blockMap.clear();
        currentBlock = nullptr;
        constructLivenessInfo(currentFunction);
        std::shared_ptr<X86_64AsmFunction> retval = currentFunction;
        currentFunction = nullptr;
        return retval;
    }
public:
    virtual void visitRTLLoadConstant(std::shared_ptr<RTLLoadConstant> node) override
    {
        std::shared_ptr<X86_64AsmNode> newNode = std::make_shared<X86_64AsmNodeLoadConstant>(getOrMakeRegister(node->destRegister, node->value->type), node->value);
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLMove(std::shared_ptr<RTLMove> node) override
    {
        std::shared_ptr<X86_64AsmNode> newNode = std::make_shared<X86_64AsmNodeMove>(getOrMakeRegister(node->destRegister, node->type), getOrMakeRegister(node->sourceRegister, node->type));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLLoad(std::shared_ptr<RTLLoad> node) override
    {
        std::shared_ptr<X86_64AsmNode> newNode = std::make_shared<X86_64AsmNodeLoad>(getOrMakeRegister(node->destRegister, node->addressType->dereference()), getOrMakeRegister(node->addressRegister, node->addressType));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLStore(std::shared_ptr<RTLStore> node) override
    {
        std::shared_ptr<X86_64AsmNode> newNode = std::make_shared<X86_64AsmNodeStore>(getOrMakeRegister(node->addressRegister, node->addressType), getOrMakeRegister(node->valueRegister, node->addressType->dereference()));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLUnconditionalJump(std::shared_ptr<RTLUnconditionalJump> node) override
    {
        std::shared_ptr<X86_64AsmControlTransfer> newNode = std::make_shared<X86_64AsmNodeJump>(getOrMakeBlock(node->target.lock()));
        currentBlock->instructions.push_back(newNode);
        currentBlock->controlTransferInstruction = newNode;
    }
    virtual void visitRTLConditionalJump(std::shared_ptr<RTLConditionalJump> node) override
    {
        std::shared_ptr<X86_64AsmControlTransfer> newNode =
            std::make_shared<X86_64AsmNodeCompareAgainstConstantAndJump>(getOrMakeRegister(node->condition, TypeBoolean::make(node->context)),
                                                                         std::make_shared<ValueBoolean>(node->context, false),
                                                                         X86_64ConditionType::NE,
                                                                         getOrMakeBlock(node->trueTarget.lock()),
                                                                         getOrMakeBlock(node->falseTarget.lock()));
        currentBlock->instructions.push_back(newNode);
        currentBlock->controlTransferInstruction = newNode;
    }
    virtual void visitRTLCompare(std::shared_ptr<RTLCompare> node) override
    {
        X86_64ConditionType cond;
        bool isUnsigned = true;
        if(dynamic_cast<const TypeBoolean *>(node->operandsType->toNonConstant()->toNonVolatile().get()) != nullptr)
        {
            isUnsigned = true;
        }
        else if(dynamic_cast<const TypePointer *>(node->operandsType->toNonConstant()->toNonVolatile().get()) != nullptr)
        {
            isUnsigned = true;
        }
        else
        {
            throw std::runtime_error("compare not implemented for type");
        }
        switch(node->compareOperator)
        {
        case RTLCompare::CompareOperator::E:
            cond = X86_64ConditionType::E;
            break;
        case RTLCompare::CompareOperator::G:
            if(isUnsigned)
                cond = X86_64ConditionType::A;
            else
                cond = X86_64ConditionType::G;
            break;
        case RTLCompare::CompareOperator::GE:
            if(isUnsigned)
                cond = X86_64ConditionType::AE;
            else
                cond = X86_64ConditionType::GE;
            break;
        case RTLCompare::CompareOperator::L:
            if(isUnsigned)
                cond = X86_64ConditionType::B;
            else
                cond = X86_64ConditionType::L;
            break;
        case RTLCompare::CompareOperator::LE:
            if(isUnsigned)
                cond = X86_64ConditionType::BE;
            else
                cond = X86_64ConditionType::LE;
            break;
        default: // NE
            cond = X86_64ConditionType::NE;
            break;
        }
        std::shared_ptr<X86_64AsmNode> newNode = std::make_shared<X86_64AsmNodeCompare>(getOrMakeRegister(node->destRegister, TypeBoolean::make(node->context)),
                                                                                        getOrMakeRegister(node->lhsRegister, node->operandsType),
                                                                                        getOrMakeRegister(node->rhsRegister, node->operandsType),
                                                                                        cond);
        currentBlock->instructions.push_back(newNode);
    }
    static std::list<std::shared_ptr<X86_64AsmFunction>> run(const std::list<std::shared_ptr<RTLFunction>> &inputFunctions)
    {
        X86_64ConvertRTLToAsm converter;
        std::list<std::shared_ptr<X86_64AsmFunction>> retval;
        for(std::shared_ptr<RTLFunction> fn : inputFunctions)
        {
            retval.push_back(converter.visitRTLFunction(fn));
        }
        return retval;
    }
};

#endif // X86_64_RTL_TO_ASM_H_INCLUDED
