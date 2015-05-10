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
#ifndef X86_RTL_TO_ASM_H_INCLUDED
#define X86_RTL_TO_ASM_H_INCLUDED

#include "rtl/rtl_nodes.h"
#include "backend/x86/x86_asm_nodes.h"
#include <unordered_map>
#include <unordered_set>
#include "construct_liveness_info.h"
#include "backend/x86/x86_construct_liveness_info.h"

class X86ConvertRTLToAsm final : public RTLNodeVisitor
{
private:
    const BackendX86 *const backend;
    std::shared_ptr<X86AsmBasicBlock> currentBlock;
    std::shared_ptr<X86AsmFunction> currentFunction;
    struct RegisterHasher final
    {
        std::size_t operator ()(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v) const
        {
            return 3 * std::hash<std::shared_ptr<RTLRegister>>()(std::get<0>(v)) + std::hash<std::shared_ptr<TypeNode>>()(std::get<1>(v));
        }
    };
    std::unordered_map<std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>, std::shared_ptr<X86AsmRegister>, RegisterHasher> registerMap;
    std::unordered_map<std::shared_ptr<RTLBasicBlock>, std::shared_ptr<X86AsmBasicBlock>> blockMap;
    std::unordered_map<std::shared_ptr<RTLFunction>, std::shared_ptr<X86AsmFunction>> functionMap;
    std::unordered_map<std::shared_ptr<RTLRegister>, VariableLocation> registerVariableLocationMap;
    std::shared_ptr<X86AsmRegister> getOrMakeRegister(std::shared_ptr<RTLRegister> reg, std::shared_ptr<TypeNode> type)
    {
        auto v = std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>>(reg, type);
        std::shared_ptr<X86AsmRegister> &retval = registerMap[v];
        if(retval == nullptr)
        {
            retval = X86AsmRegister::getVirtualRegister(reg->context, backend, reg->name, X86TypeToPhysicalRegisterKindMask::run(type, backend), reg->spillLocation);
        }
        return retval;
    }
    std::shared_ptr<X86AsmBasicBlock> getOrMakeBlock(std::shared_ptr<RTLBasicBlock> v)
    {
        std::shared_ptr<X86AsmBasicBlock> &retval = blockMap[v];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86AsmBasicBlock>(v->context, backend);
        }
        return retval;
    }
    std::shared_ptr<X86AsmFunction> getOrMakeFunction(std::shared_ptr<RTLFunction> v)
    {
        std::shared_ptr<X86AsmFunction> &retval = functionMap[v];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86AsmFunction>(v->context, backend);
        }
        return retval;
    }
    X86ConvertRTLToAsm(const BackendX86 *backend)
        : backend(backend)
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
    static void constructLivenessInfo(std::shared_ptr<X86AsmFunction> function)
    {
        X86ConstructLivenessInfo().visitX86AsmFunction(function);
    }
    std::shared_ptr<X86AsmFunction> visitRTLFunction(std::shared_ptr<RTLFunction> function)
    {
        currentFunction = getOrMakeFunction(function);
        currentFunction->localVariablesSize = function->localVariablesSize;
        currentFunction->startBlock = getOrMakeBlock(function->startBlock);
        registerVariableLocationMap.clear();
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            for(std::shared_ptr<RTLNode> node : block->instructions)
            {
                std::shared_ptr<RTLLoadConstant> loadConstant = std::dynamic_pointer_cast<RTLLoadConstant>(node);
                VariableLocation vl = nullptr;
                if(loadConstant != nullptr)
                {
                    std::shared_ptr<ValueVariablePointer> valueVariablePointer = std::dynamic_pointer_cast<ValueVariablePointer>(loadConstant->value);
                    if(valueVariablePointer != nullptr)
                        vl = valueVariablePointer->location;
                }
                for(std::pair<std::shared_ptr<RTLRegister>, std::shared_ptr<TypeNode>> v : node->getOutputRegisters())
                {
                    std::shared_ptr<RTLRegister> r = std::get<0>(v);
                    auto iter = registerVariableLocationMap.find(r);
                    if(iter == registerVariableLocationMap.end())
                    {
                        registerVariableLocationMap.emplace(r, vl);
                    }
                    else if(std::get<1>(*iter) != vl)
                    {
                        std::get<1>(*iter) = nullptr;
                    }
                }
            }
        }
        for(std::shared_ptr<RTLBasicBlock> block : function->blocks)
        {
            visitRTLBasicBlock(block);
        }
        registerMap.clear();
        blockMap.clear();
        currentBlock = nullptr;
        constructLivenessInfo(currentFunction);
        std::shared_ptr<X86AsmFunction> retval = currentFunction;
        currentFunction = nullptr;
        registerVariableLocationMap.clear();
        return retval;
    }
public:
    virtual void visitRTLLoadConstant(std::shared_ptr<RTLLoadConstant> node) override
    {
        std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeLoadConstant>(getOrMakeRegister(node->destRegister, node->value->type), node->value);
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLMove(std::shared_ptr<RTLMove> node) override
    {
        std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeMove>(getOrMakeRegister(node->destRegister, node->type), getOrMakeRegister(node->sourceRegister, node->type));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLTypeCast(std::shared_ptr<RTLTypeCast> node) override
    {
        std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeTypeCast>(getOrMakeRegister(node->destRegister, node->destType), getOrMakeRegister(node->sourceRegister, node->sourceType), node->destType, node->sourceType);
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLLoad(std::shared_ptr<RTLLoad> node) override
    {
        VariableLocation vl = registerVariableLocationMap[node->addressRegister];
        std::shared_ptr<X86AsmNode> newNode;
        if(vl.good())
            newNode = std::make_shared<X86AsmNodeLoadLocal>(getOrMakeRegister(node->destRegister, node->addressType->dereference()), vl);
        else
            newNode = std::make_shared<X86AsmNodeLoad>(getOrMakeRegister(node->destRegister, node->addressType->dereference()), getOrMakeRegister(node->addressRegister, node->addressType));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLStore(std::shared_ptr<RTLStore> node) override
    {
        VariableLocation vl = registerVariableLocationMap[node->addressRegister];
        std::shared_ptr<X86AsmNode> newNode;
        if(vl.good())
            newNode = std::make_shared<X86AsmNodeStoreLocal>(vl, getOrMakeRegister(node->valueRegister, node->addressType->dereference()));
        else
            newNode = std::make_shared<X86AsmNodeStore>(getOrMakeRegister(node->addressRegister, node->addressType), getOrMakeRegister(node->valueRegister, node->addressType->dereference()));
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLUnconditionalJump(std::shared_ptr<RTLUnconditionalJump> node) override
    {
        std::shared_ptr<X86AsmControlTransfer> newNode = std::make_shared<X86AsmNodeJump>(getOrMakeBlock(node->target.lock()));
        currentBlock->instructions.push_back(newNode);
        currentBlock->controlTransferInstruction = newNode;
    }
    virtual void visitRTLConditionalJump(std::shared_ptr<RTLConditionalJump> node) override
    {
        std::shared_ptr<X86AsmControlTransfer> newNode =
            std::make_shared<X86AsmNodeCompareAgainstConstantAndJump>(getOrMakeRegister(node->condition, TypeBoolean::make(node->context)),
                                                                         std::make_shared<ValueBoolean>(node->context, false),
                                                                         X86ConditionType::NE,
                                                                         getOrMakeBlock(node->trueTarget.lock()),
                                                                         getOrMakeBlock(node->falseTarget.lock()));
        currentBlock->instructions.push_back(newNode);
        currentBlock->controlTransferInstruction = newNode;
    }
    virtual void visitRTLCompare(std::shared_ptr<RTLCompare> node) override
    {
        X86ConditionType cond;
        bool isUnsigned = true;
        if(dynamic_cast<const TypeBoolean *>(node->operandsType->toNonConstant()->toNonVolatile().get()) != nullptr)
        {
            isUnsigned = true;
        }
        else if(dynamic_cast<const TypePointer *>(node->operandsType->toNonConstant()->toNonVolatile().get()) != nullptr)
        {
            isUnsigned = true;
        }
        else if(std::shared_ptr<TypeInteger> typeInteger = std::dynamic_pointer_cast<TypeInteger>(node->operandsType->toNonConstant()->toNonVolatile()))
        {
            isUnsigned = typeInteger->isUnsigned;
        }
        else
        {
            throw std::runtime_error("compare not implemented for type");
        }
        switch(node->compareOperator)
        {
        case RTLCompare::CompareOperator::E:
            cond = X86ConditionType::E;
            break;
        case RTLCompare::CompareOperator::G:
            if(isUnsigned)
                cond = X86ConditionType::A;
            else
                cond = X86ConditionType::G;
            break;
        case RTLCompare::CompareOperator::GE:
            if(isUnsigned)
                cond = X86ConditionType::AE;
            else
                cond = X86ConditionType::GE;
            break;
        case RTLCompare::CompareOperator::L:
            if(isUnsigned)
                cond = X86ConditionType::B;
            else
                cond = X86ConditionType::L;
            break;
        case RTLCompare::CompareOperator::LE:
            if(isUnsigned)
                cond = X86ConditionType::BE;
            else
                cond = X86ConditionType::LE;
            break;
        default: // NE
            cond = X86ConditionType::NE;
            break;
        }
        std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeCompare>(getOrMakeRegister(node->destRegister, TypeBoolean::make(node->context)),
                                                                                        getOrMakeRegister(node->lhsRegister, node->operandsType),
                                                                                        getOrMakeRegister(node->rhsRegister, node->operandsType),
                                                                                        cond);
        currentBlock->instructions.push_back(newNode);
    }
    virtual void visitRTLAdd(std::shared_ptr<RTLAdd> node) override
    {
        if(dynamic_cast<const TypePointer *>(node->lhsType.get()))
        {
            std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeLoadConstant>(getOrMakeRegister(node->destRegister, node->destType), std::make_shared<ValueInteger>(node->context, false, IntegerWidth::IntNativeSize, node->lhsType->dereference()->getTypeProperties().size));
            currentBlock->instructions.push_back(newNode);
            newNode = std::make_shared<X86AsmNodeMul>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                            getOrMakeRegister(node->rhsRegister, node->rhsType));
            currentBlock->instructions.push_back(newNode);
            newNode = std::make_shared<X86AsmNodeAdd>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                            getOrMakeRegister(node->lhsRegister, node->lhsType));
            currentBlock->instructions.push_back(newNode);
            return;
        }
        if(dynamic_cast<const TypePointer *>(node->rhsType.get()))
        {
            std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeLoadConstant>(getOrMakeRegister(node->destRegister, node->destType), std::make_shared<ValueInteger>(node->context, false, IntegerWidth::IntNativeSize, node->rhsType->dereference()->getTypeProperties().size));
            currentBlock->instructions.push_back(newNode);
            newNode = std::make_shared<X86AsmNodeMul>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                            getOrMakeRegister(node->lhsRegister, node->lhsType));
            currentBlock->instructions.push_back(newNode);
            newNode = std::make_shared<X86AsmNodeAdd>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                            getOrMakeRegister(node->rhsRegister, node->rhsType));
            currentBlock->instructions.push_back(newNode);
            return;
        }
        std::shared_ptr<X86AsmNode> newNode = std::make_shared<X86AsmNodeMove>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                        getOrMakeRegister(node->lhsRegister, node->lhsType));
        currentBlock->instructions.push_back(newNode);
        newNode = std::make_shared<X86AsmNodeAdd>(getOrMakeRegister(node->destRegister, node->destType),
                                                                                        getOrMakeRegister(node->rhsRegister, node->rhsType));
        currentBlock->instructions.push_back(newNode);
    }
    static std::list<std::shared_ptr<X86AsmFunction>> run(const std::list<std::shared_ptr<RTLFunction>> &inputFunctions, const BackendX86 *backend)
    {
        X86ConvertRTLToAsm converter(backend);
        std::list<std::shared_ptr<X86AsmFunction>> retval;
        for(std::shared_ptr<RTLFunction> fn : inputFunctions)
        {
            retval.push_back(converter.visitRTLFunction(fn));
        }
        return retval;
    }
};

#endif // X86_RTL_TO_ASM_H_INCLUDED
