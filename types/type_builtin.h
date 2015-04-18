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
#ifndef TYPE_BUILTIN_H_INCLUDED
#define TYPE_BUILTIN_H_INCLUDED

#include "type.h"

template <typename T, std::size_t hashValue>
class TypeGenericBuiltIn : public TypeBuiltIn
{
protected:
    explicit TypeGenericBuiltIn(CompilerContext *context)
        : TypeBuiltIn(context)
    {
    }
public:
    static std::shared_ptr<T> make(CompilerContext *context)
    {
        return context->constructTypeNode<T>();
    }
    virtual bool operator ==(const TypeNode &rt) const override final
    {
        const T *prt = dynamic_cast<const T *>(&rt);
        if(prt != nullptr)
        {
            return true;
        }
        return false;
    }
    virtual std::size_t getHash() const override
    {
        return hashValue;
    }
};

class TypeVoid final : public TypeGenericBuiltIn<TypeVoid, 0>
{
    friend CompilerContext;
private:
    using TypeGenericBuiltIn::TypeGenericBuiltIn;
public:
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypeVoid(std::static_pointer_cast<TypeVoid>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override
    {
        return nullptr;
    }
};

class TypeBoolean final : public TypeGenericBuiltIn<TypeBoolean, 0>
{
    friend CompilerContext;
private:
    using TypeGenericBuiltIn::TypeGenericBuiltIn;
public:
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypeBoolean(std::static_pointer_cast<TypeBoolean>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override;
};

#endif // TYPE_BUILTIN_H_INCLUDED
