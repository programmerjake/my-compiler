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

std::shared_ptr<ValueNode> TypePointer::makeDefaultValue()
{
    return std::make_shared<ValueNullPointer>(context);
}

std::shared_ptr<ValueNode> TypeInteger::makeDefaultValue()
{
    return std::make_shared<ValueInteger>(context, isUnsigned, width, 0);
}

std::shared_ptr<TypeNode> TypePointer::getArithCombinedType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypeInteger *>(rt->toNonConstant()->toNonVolatile().get()))
        return shared_from_this();
    return nullptr;
}

std::shared_ptr<TypeNode> TypeInteger::getArithCombinedType(std::shared_ptr<TypeNode> rt)
{
    if(dynamic_cast<const TypePointer *>(rt->toNonConstant()->toNonVolatile().get()))
        return rt;
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
        return make(context, resultIsUnsigned, resultWidth);
    }
    return nullptr;
}

