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
#include "types/type_builtin.h"
#include "values/values.h"

std::shared_ptr<ValueNode> TypeBoolean::makeDefaultValue()
{
    return std::make_shared<ValueBoolean>(context, false);
}

bool TypeBoolean::canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const
{
    if(dynamic_cast<const TypeBoolean *>(destType->toNonConstant()->toNonVolatile().get()))
        return true;
    if(dynamic_cast<const TypeInteger *>(destType->toNonConstant()->toNonVolatile().get()))
        return !isImplicit;
    return false;
}

TypeNode::BinaryOperatorTypeRetval TypeBoolean::getCompareType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypeBoolean *>(rt->toNonConstant()->toNonVolatile().get()))
        return BinaryOperatorTypeRetval(shared_from_this(), rt, TypeBoolean::make(context));
    return BinaryOperatorTypeRetval();
}

std::shared_ptr<ValueNode> TypePointer::makeDefaultValue()
{
    return std::make_shared<ValueNullPointer>(context);
}

std::shared_ptr<ValueNode> TypeInteger::makeDefaultValue()
{
    return std::make_shared<ValueInteger>(context, isUnsigned, width, 0);
}

TypeNode::BinaryOperatorTypeRetval TypePointer::getArithCombinedType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypeInteger *>(rt->toNonConstant()->toNonVolatile().get()))
        return BinaryOperatorTypeRetval(shared_from_this(), TypeInteger::make(context, false, IntegerWidth::IntNativeSize), shared_from_this());
    return BinaryOperatorTypeRetval();
}

TypeNode::BinaryOperatorTypeRetval TypePointer::getCompareType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypePointer *>(rt->toNonConstant()->toNonVolatile().get()))
        return BinaryOperatorTypeRetval(shared_from_this(), rt, TypeBoolean::make(context));
    return BinaryOperatorTypeRetval();
}

bool TypePointer::canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const
{
    if(dynamic_cast<const TypeBoolean *>(destType->toNonConstant()->toNonVolatile().get()))
        return true;
    if(dynamic_cast<const TypeInteger *>(destType->toNonConstant()->toNonVolatile().get()))
        return !isImplicit;
    if(TypePointer *typePointer = dynamic_cast<TypePointer *>(destType->toNonConstant()->toNonVolatile().get()))
    {
        if(typePointer == this)
            return true;
        if(!isImplicit)
            return true;
        if(node->isConstant && !typePointer->node->isConstant)
            return false;
        if(node->isVolatile && !typePointer->node->isVolatile)
            return false;
        TypeNode *fromTypeDereferenced = node.get();
        TypeNode *toTypeDereferenced = typePointer->node.get();
        bool needConstant = (fromTypeDereferenced->isConstant != toTypeDereferenced->isConstant);
        while(fromTypeDereferenced != toTypeDereferenced)
        {
            if(needConstant && !toTypeDereferenced->isConstant)
                return false;
            TypePointer *fromTypeDereferencedPointer = dynamic_cast<TypePointer *>(fromTypeDereferenced->toNonConstant()->toNonVolatile().get());
            TypePointer *toTypeDereferencedPointer = dynamic_cast<TypePointer *>(toTypeDereferenced->toNonConstant()->toNonVolatile().get());
            if(!fromTypeDereferencedPointer || !toTypeDereferencedPointer)
            {
                if(fromTypeDereferenced->isConstant && !toTypeDereferenced->isConstant)
                    return false;
                if(fromTypeDereferenced->isVolatile && !toTypeDereferenced->isVolatile)
                    return false;
                return fromTypeDereferenced->toNonConstant()->toNonVolatile() == toTypeDereferenced->toNonConstant()->toNonVolatile();
            }
            fromTypeDereferenced = fromTypeDereferencedPointer->node.get();
            toTypeDereferenced = toTypeDereferencedPointer->node.get();
            if(fromTypeDereferenced->isConstant && !toTypeDereferenced->isConstant)
                return false;
            if(fromTypeDereferenced->isVolatile && !toTypeDereferenced->isVolatile)
                return false;
            if(!needConstant)
                needConstant = (fromTypeDereferenced->isConstant != toTypeDereferenced->isConstant);
        }
        if(needConstant && !toTypeDereferenced->isConstant)
            return false;
        return true;
    }
    return false;
}

TypeNode::BinaryOperatorTypeRetval TypeInteger::getArithCombinedType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypePointer *>(rt->toNonConstant()->toNonVolatile().get()))
        return BinaryOperatorTypeRetval(make(context, false, Width::IntNativeSize), rt, rt);
    if(std::shared_ptr<TypeInteger> rtInteger = std::dynamic_pointer_cast<TypeInteger>(rt))
    {
        bool resultIsUnsigned = false;
        Width resultWidth = width;
        switch(width)
        {
        case Width::Int8:
        case Width::Int16:
            switch(rtInteger->width)
            {
            case Width::Int8:
            case Width::Int16:
                resultIsUnsigned = false;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int32:
                resultIsUnsigned = rtInteger->isUnsigned;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int64:
            case Width::IntNativeSize:
                resultIsUnsigned = rtInteger->isUnsigned;
                resultWidth = rtInteger->width;
                break;
            }
            break;
        case Width::Int32:
            switch(rtInteger->width)
            {
            case Width::Int8:
            case Width::Int16:
                resultIsUnsigned = false;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int32:
                resultIsUnsigned = isUnsigned || rtInteger->isUnsigned;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int64:
            case Width::IntNativeSize:
                resultIsUnsigned = rtInteger->isUnsigned;
                resultWidth = rtInteger->width;
                break;
            }
            break;
        case Width::Int64:
            switch(rtInteger->width)
            {
            case Width::Int8:
            case Width::Int16:
            case Width::Int32:
            case Width::IntNativeSize:
                resultIsUnsigned = isUnsigned;
                resultWidth = Width::Int64;
                break;
            case Width::Int64:
                resultIsUnsigned = isUnsigned || rtInteger->isUnsigned;
                resultWidth = Width::Int64;
                break;
            }
            break;
        case Width::IntNativeSize:
            switch(rtInteger->width)
            {
            case Width::Int8:
            case Width::Int16:
                resultIsUnsigned = isUnsigned;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int32:
                resultIsUnsigned = isUnsigned;
                resultWidth = Width::IntNativeSize;
                break;
            case Width::Int64:
                resultIsUnsigned = rtInteger->isUnsigned;
                resultWidth = rtInteger->width;
                break;
            case Width::IntNativeSize:
                resultIsUnsigned = isUnsigned || rtInteger->isUnsigned;
                resultWidth = rtInteger->width;
                break;
            }
            break;
        }
        std::shared_ptr<TypeNode> resultType = make(context, resultIsUnsigned, resultWidth);
        return BinaryOperatorTypeRetval(resultType, resultType, resultType);
    }
    return BinaryOperatorTypeRetval();
}

TypeNode::BinaryOperatorTypeRetval TypeInteger::getCompareType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypeInteger *>(rt->toNonConstant()->toNonVolatile().get()))
    {
        BinaryOperatorTypeRetval retval = getArithCombinedType(rt);
        retval.resultType = TypeBoolean::make(context);
        return retval;
    }
    return BinaryOperatorTypeRetval();
}

bool TypeInteger::canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const
{
    if(dynamic_cast<const TypeBoolean *>(destType->toNonConstant()->toNonVolatile().get()))
        return true;
    if(dynamic_cast<const TypeInteger *>(destType->toNonConstant()->toNonVolatile().get()))
        return true;
    if(dynamic_cast<const TypePointer *>(destType->toNonConstant()->toNonVolatile().get()))
        return !isImplicit;
    return false;
}


