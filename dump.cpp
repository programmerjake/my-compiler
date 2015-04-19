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
#include "dump.h"
#include "types/types.h"
#include "values/values.h"
#include "ssa/ssa_nodes.h"

void DumpVisitor::visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node)
{
    dumpInstructionName("SSAUnconditionalJump", node);
    os << "(target=" << getSSABasicBlockDisplayValue(node->destBlocks.front().lock()) << ")";
}
void DumpVisitor::visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node)
{
    dumpInstructionName("SSAConditionalJump", node);
    os << "(condition=" << getSSANodeDisplayValue(node->condition.lock()) << ",trueTarget=" << getSSABasicBlockDisplayValue(node->destBlocks.front().lock()) << ",falseTarget=" << getSSABasicBlockDisplayValue(node->destBlocks.back().lock()) << ")";
}
void DumpVisitor::visitSSAPhi(std::shared_ptr<SSAPhi> node)
{
    dumpInstructionName("SSAPhi", node);
    os << "(";
    const char *seperator = "";
    for(SSAPhi::PhiInput i : node->inputs)
    {
        os << seperator;
        seperator = ",";
        os << "(node=" << getSSANodeDisplayValue(i.node.lock()) << ",block=" << getSSABasicBlockDisplayValue(i.block.lock()) << ")";
    }
    os << ")";
}
void DumpVisitor::visitSSAConstant(std::shared_ptr<SSAConstant> node)
{
    dumpInstructionName("SSAConstant", node);
    os << "(value=";
    node->value->visit(*this);
    os << ",type=";
    node->type.lock()->visit(*this);
    os << ")";
}
void DumpVisitor::visitTypeConstant(std::shared_ptr<TypeConstant> node)
{
    os << "TypeConstant(";
    node->toNonConstant()->visit(*this);
    os << ")";
}
void DumpVisitor::visitTypeVolatile(std::shared_ptr<TypeVolatile> node)
{
    os << "TypeVolatile(";
    node->toNonVolatile()->visit(*this);
    os << ")";
}
void DumpVisitor::visitTypeVoid(std::shared_ptr<TypeVoid> node)
{
    os << "TypeVoid";
}
void DumpVisitor::visitTypeBoolean(std::shared_ptr<TypeBoolean> node)
{
    os << "TypeBoolean";
}
void DumpVisitor::visitValueBoolean(std::shared_ptr<ValueBoolean> node)
{
    os << "ValueBoolean(value=";
    if(node->value)
        os << "true";
    else
        os << "false";
    os << ")";
}
void DumpVisitor::visitValueUnknown(std::shared_ptr<ValueUnknown> node)
{
    os << "ValueUnknown()";
}

void DumpVisitor::visitSSABasicBlock(std::shared_ptr<SSABasicBlock> node)
{
    os << "  [" << getSSABasicBlockDisplayValue(node) << "]SSABasicBlock(\n    immediateDominator=";
    os << getSSABasicBlockDisplayValue(node->immediateDominator.lock()) << ",\n    dominatedBlocks=[";
    const char *seperator = "";
    for(std::weak_ptr<SSABasicBlock> dominatedBlockW : node->dominatedBlocks)
    {
        std::shared_ptr<SSABasicBlock> dominatedBlock = dominatedBlockW.lock();
        os << seperator;
        seperator = ",";
        os << getSSABasicBlockDisplayValue(dominatedBlock);
    }
    os << "]";
    for(std::shared_ptr<SSANode> i : node->instructions)
    {
        os << ",\n    ";
        visitSSANode(i);
    }
    os << "\n  )";
}

void DumpVisitor::visitSSAFunction(std::shared_ptr<SSAFunction> node)
{
    for(std::shared_ptr<SSABasicBlock> i : node->blocks)
    {
        getSSABasicBlockDisplayValue(i);
        for(std::shared_ptr<SSANode> j : i->instructions)
        {
            getSSANodeDisplayValue(j);
        }
    }
    os << "[" << getSSAFunctionDisplayValue(node) << "]SSAFunction(";
    const char *seperator = "\n";
    for(std::shared_ptr<SSABasicBlock> i : node->blocks)
    {
        os << seperator;
        seperator = ",\n";
        visitSSABasicBlock(i);
    }
    os << "\n)";
}

