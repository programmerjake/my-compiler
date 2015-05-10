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
    node->type->visit(*this);
    os << ")";
}
void DumpVisitor::visitSSAAllocA(std::shared_ptr<SSAAllocA> node)
{
    dumpInstructionName("SSAAllocA", node);
    os << "(variableType=";
    node->variableType->visit(*this);
    std::shared_ptr<VariableDescriptor> variableDescriptor = node->getVariableDescriptor();
    if(variableDescriptor->getStart() != VariableDescriptor::NoStart)
        os << ",start=" << variableDescriptor->getStart();
    os << ")";
}
void DumpVisitor::visitSSAMove(std::shared_ptr<SSAMove> node)
{
    dumpInstructionName("SSAMove", node);
    os << "(source=" << getSSANodeDisplayValue(node->source.lock()) << ")";
}
void DumpVisitor::visitSSALoad(std::shared_ptr<SSALoad> node)
{
    dumpInstructionName("SSALoad", node);
    os << "(address=" << getSSANodeDisplayValue(node->address.lock()) << ")";
}
void DumpVisitor::visitSSAStore(std::shared_ptr<SSAStore> node)
{
    dumpInstructionName("SSAStore", node);
    os << "(address=" << getSSANodeDisplayValue(node->address.lock()) << ",value=" << getSSANodeDisplayValue(node->value.lock()) << ")";
}
void DumpVisitor::visitSSACompare(std::shared_ptr<SSACompare> node)
{
    dumpInstructionName("SSACompare", node);
    os << "(lhs=" << getSSANodeDisplayValue(node->lhs.lock()) << ",compareOperator=";
    switch(node->compareOperator)
    {
    case SSACompare::CompareOperator::E:
        os << "'=='";
        break;
    case SSACompare::CompareOperator::G:
        os << "'>'";
        break;
    case SSACompare::CompareOperator::GE:
        os << "'>='";
        break;
    case SSACompare::CompareOperator::L:
        os << "'<'";
        break;
    case SSACompare::CompareOperator::LE:
        os << "'<='";
        break;
    default: // NE
        os << "'!='";
        break;
    }
    os << ",rhs=" << getSSANodeDisplayValue(node->rhs.lock()) << ")";
}
void DumpVisitor::visitSSAAdd(std::shared_ptr<SSAAdd> node)
{
    dumpInstructionName("SSAAdd", node);
    os << "(lhs=" << getSSANodeDisplayValue(node->lhs.lock()) << ",rhs=";
    os << getSSANodeDisplayValue(node->rhs.lock()) << ")";
}
void DumpVisitor::visitSSATypeCast(std::shared_ptr<SSATypeCast> node)
{
    dumpInstructionName("SSATypeCast", node);
    os << "(destType=";
    node->type->visit(*this);
    os << ",sourceType=";
    node->type->visit(*this);
    os << ",arg=" << getSSANodeDisplayValue(node->arg.lock()) << ")";
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
void DumpVisitor::visitTypePointer(std::shared_ptr<TypePointer> node)
{
    os << "TypePointer(";
    node->dereference()->visit(*this);
    os << ")";
}
void DumpVisitor::visitTypeInteger(std::shared_ptr<TypeInteger> node)
{
    os << "TypeInteger(kind=";
    if(node->isUnsigned)
        os << "u";
    switch(node->width)
    {
    case IntegerWidth::Int8:
        os << "int8";
        break;
    case IntegerWidth::Int16:
        os << "int16";
        break;
    case IntegerWidth::Int32:
        os << "int32";
        break;
    case IntegerWidth::Int64:
        os << "int64";
        break;
    case IntegerWidth::IntNativeSize:
        os << "int";
        break;
    }
    os << ")";
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
void DumpVisitor::visitValueNullPointer(std::shared_ptr<ValueNullPointer> node)
{
    os << "ValueNullPointer()";
}
void DumpVisitor::visitValueVariablePointer(std::shared_ptr<ValueVariablePointer> node)
{
    VariableLocation location = node->location;
    os << "ValueVariablePointer(location=VariableLocation(start=";
    if(location.variable->getStart() != VariableDescriptor::NoStart)
        os << location.variable->getStart();
    else
        os << "<none>";
    os << ",offset=" << location.offset << "))";
}
void DumpVisitor::visitValueInteger(std::shared_ptr<ValueInteger> node)
{
    os << "ValueInteger(kind=";
    if(node->isUnsigned)
        os << "u";
    switch(node->width)
    {
    case IntegerWidth::Int8:
        os << "int8";
        break;
    case IntegerWidth::Int16:
        os << "int16";
        break;
    case IntegerWidth::Int32:
        os << "int32";
        break;
    case IntegerWidth::Int64:
        os << "int64";
        break;
    case IntegerWidth::IntNativeSize:
        os << "int";
        break;
    }
    os << ",value=";
    IntegerWidth calcWidth = node->width;
    if(calcWidth == IntegerWidth::IntNativeSize)
        calcWidth = node->context->backend->getNativeIntegerWidth();
    switch(calcWidth)
    {
    case IntegerWidth::Int8:
        if(node->isUnsigned)
            os << static_cast<unsigned>(node->getAsUInt8());
        else
            os << static_cast<int>(node->getAsInt8());
        break;
    case IntegerWidth::Int16:
        if(node->isUnsigned)
            os << static_cast<unsigned>(node->getAsUInt16());
        else
            os << static_cast<int>(node->getAsInt16());
        break;
    case IntegerWidth::Int32:
        if(node->isUnsigned)
            os << node->getAsUInt32();
        else
            os << node->getAsInt32();
        break;
    case IntegerWidth::Int64:
        if(node->isUnsigned)
            os << node->getAsUInt64();
        else
            os << node->getAsInt64();
        break;
    case IntegerWidth::IntNativeSize:
        break;
    }
    os << ")";
}
void DumpVisitor::visitRTLLoadConstant(std::shared_ptr<RTLLoadConstant> node)
{
    os << "RTLLoadConstant(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",value=";
    node->value->visit(*this);
    os << ")";
}
void DumpVisitor::visitRTLMove(std::shared_ptr<RTLMove> node)
{
    os << "RTLMove(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",sourceRegister=";
    dumpRTLRegister(node->sourceRegister);
    os << ")";
}
void DumpVisitor::visitRTLTypeCast(std::shared_ptr<RTLTypeCast> node)
{
    os << "RTLTypeCast(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",destType=";
    node->destType->visit(*this);
    os << ",sourceRegister=";
    dumpRTLRegister(node->sourceRegister);
    os << ",sourceType=";
    node->sourceType->visit(*this);
    os << ")";
}
void DumpVisitor::visitRTLLoad(std::shared_ptr<RTLLoad> node)
{
    os << "RTLLoad(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",addressRegister=";
    dumpRTLRegister(node->addressRegister);
    os << ")";
}
void DumpVisitor::visitRTLStore(std::shared_ptr<RTLStore> node)
{
    os << "RTLStore(addressRegister=";
    dumpRTLRegister(node->addressRegister);
    os << ",valueRegister=";
    dumpRTLRegister(node->valueRegister);
    os << ")";
}
void DumpVisitor::visitRTLUnconditionalJump(std::shared_ptr<RTLUnconditionalJump> node)
{
    os << "RTLUnconditionalJump(target=";
    os << getRTLBasicBlockDisplayValue(node->target.lock());
    os << ")";
}
void DumpVisitor::visitRTLConditionalJump(std::shared_ptr<RTLConditionalJump> node)
{
    os << "RTLConditionalJump(condition=";
    dumpRTLRegister(node->condition);
    os << ",trueTarget=";
    os << getRTLBasicBlockDisplayValue(node->trueTarget.lock());
    os << ",falseTarget=";
    os << getRTLBasicBlockDisplayValue(node->falseTarget.lock());
    os << ")";
}
void DumpVisitor::visitRTLCompare(std::shared_ptr<RTLCompare> node)
{
    os << "RTLCompare(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",lhsRegister=";
    dumpRTLRegister(node->lhsRegister);
    os << ",compareOperator=";
    switch(node->compareOperator)
    {
    case RTLCompare::CompareOperator::E:
        os << "'=='";
        break;
    case RTLCompare::CompareOperator::G:
        os << "'>'";
        break;
    case RTLCompare::CompareOperator::GE:
        os << "'>='";
        break;
    case RTLCompare::CompareOperator::L:
        os << "'<'";
        break;
    case RTLCompare::CompareOperator::LE:
        os << "'<='";
        break;
    default: // NE
        os << "'!='";
        break;
    }
    os << ",rhsRegister=";
    dumpRTLRegister(node->rhsRegister);
    os << ")";
}
void DumpVisitor::visitRTLAdd(std::shared_ptr<RTLAdd> node)
{
    os << "RTLAdd(destRegister=";
    dumpRTLRegister(node->destRegister);
    os << ",lhsRegister=";
    dumpRTLRegister(node->lhsRegister);
    os << ",rhsRegister=";
    dumpRTLRegister(node->rhsRegister);
    os << ")";
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

void DumpVisitor::dumpRTLRegister(std::shared_ptr<RTLRegister> r)
{
    if(r == nullptr)
        os << "nullptr";
    else
        os << r->name;
}

void DumpVisitor::visitRTLBasicBlock(std::shared_ptr<RTLBasicBlock> node)
{
    os << "  [" << getRTLBasicBlockDisplayValue(node) << "]RTLBasicBlock(";
    const char *seperator = "\n    ";
    for(std::shared_ptr<RTLNode> i : node->instructions)
    {
        os << seperator;
        seperator = ",\n    ";
        visitRTLNode(i);
    }
    os << "\n  )";
}

void DumpVisitor::visitRTLFunction(std::shared_ptr<RTLFunction> node)
{
    for(std::shared_ptr<RTLBasicBlock> i : node->blocks)
    {
        getRTLBasicBlockDisplayValue(i);
        for(std::shared_ptr<RTLNode> j : i->instructions)
        {
            getRTLNodeDisplayValue(j);
        }
    }
    os << "[" << getRTLFunctionDisplayValue(node) << "]RTLFunction(";
    const char *seperator = "\n";
    for(std::shared_ptr<RTLBasicBlock> i : node->blocks)
    {
        os << seperator;
        seperator = ",\n";
        visitRTLBasicBlock(i);
    }
    os << "\n)";
}

