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

#include "types/type.h"

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
    virtual BinaryOperatorTypeRetval getCompareType(std::shared_ptr<TypeNode> rt) override;
    virtual bool canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const override;
};

class TypePointer final : public TypeNode
{
    friend class CompilerContext;
private:
    std::shared_ptr<TypeNode> node;
    TypePointer(CompilerContext *context, std::shared_ptr<TypeNode> node)
        : TypeNode(context, false, false), node(node)
    {
    }
public:
    static std::shared_ptr<TypeNode> make(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        return node->context->constructTypeNode<TypePointer>(node);
    }
    virtual std::shared_ptr<TypeNode> dereference() override
    {
        return node;
    }
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypePointer(std::static_pointer_cast<TypePointer>(shared_from_this()));
    }
    virtual bool operator ==(const TypeNode &rt) const override
    {
        const TypePointer *prt = dynamic_cast<const TypePointer *>(&rt);
        if(prt != nullptr)
        {
            return *node == *prt->node;
        }
        return false;
    }
    virtual std::size_t getHash() const override
    {
        return static_cast<std::size_t>(0x24395729) + node->getHash();
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override;
    virtual BinaryOperatorTypeRetval getArithCombinedType(std::shared_ptr<TypeNode> rt) override;
    virtual BinaryOperatorTypeRetval getCompareType(std::shared_ptr<TypeNode> rt) override;
    virtual bool canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const override;
};

class TypeInteger final : public TypeNode
{
    friend class CompilerContext;
public:
    const bool isUnsigned;
    typedef IntegerWidth Width;
    const Width width;
private:
    TypeInteger(CompilerContext *context, bool isUnsigned, Width width)
        : TypeNode(context, false, false), isUnsigned(isUnsigned), width(width)
    {
    }
public:
    static std::shared_ptr<TypeNode> make(CompilerContext *context, bool isUnsigned, Width width)
    {
        return context->constructTypeNode<TypeInteger>(isUnsigned, width);
    }
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypeInteger(std::static_pointer_cast<TypeInteger>(shared_from_this()));
    }
    virtual bool operator ==(const TypeNode &rt) const override
    {
        const TypeInteger *prt = dynamic_cast<const TypeInteger *>(&rt);
        if(prt != nullptr)
        {
            return isUnsigned == prt->isUnsigned && width == prt->width;
        }
        return false;
    }
    virtual std::size_t getHash() const override
    {
        std::size_t retval = static_cast<std::size_t>(width);
        retval = retval * 2 + (isUnsigned ? 1 : 0);
        return retval;
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override;
    virtual BinaryOperatorTypeRetval getArithCombinedType(std::shared_ptr<TypeNode> rt) override;
    virtual BinaryOperatorTypeRetval getCompareType(std::shared_ptr<TypeNode> rt) override;
    virtual bool canTypeCastTo(std::shared_ptr<TypeNode> destType, bool isImplicit) const override;
};

#endif // TYPE_BUILTIN_H_INCLUDED
