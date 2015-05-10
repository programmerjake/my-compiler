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
#ifndef TYPE_H_INCLUDED
#define TYPE_H_INCLUDED

#include <memory>
#include <cassert>
#include <list>

class TypeVisitor;
class TypeNode;
class TypeConstant;
class TypeVolatile;
class TypeVoid;
class TypeBoolean;
class TypePointer;
class TypeInteger;
class ValueNode;

class TypeVisitor
{
public:
    virtual ~TypeVisitor() = default;
    virtual void visitTypeConstant(std::shared_ptr<TypeConstant> node) = 0;
    virtual void visitTypeVolatile(std::shared_ptr<TypeVolatile> node) = 0;
    virtual void visitTypeVoid(std::shared_ptr<TypeVoid> node) = 0;
    virtual void visitTypeBoolean(std::shared_ptr<TypeBoolean> node) = 0;
    virtual void visitTypePointer(std::shared_ptr<TypePointer> node) = 0;
    virtual void visitTypeInteger(std::shared_ptr<TypeInteger> node) = 0;
};

#include "context.h"

class TypeNode : public std::enable_shared_from_this<TypeNode>
{
public:
    const bool isConstant;
    const bool isVolatile;
    CompilerContext *const context;
protected:
    TypeNode(CompilerContext *context, bool isConstant, bool isVolatile)
        : isConstant(isConstant), isVolatile(isVolatile), context(context)
    {
    }
public:
    virtual ~TypeNode() = default;
    virtual void visit(TypeVisitor &visitor) = 0;
    virtual std::shared_ptr<TypeNode> toNonConstant()
    {
        return shared_from_this();
    }
    virtual std::shared_ptr<TypeNode> toNonVolatile()
    {
        return shared_from_this();
    }
    std::shared_ptr<TypeNode> toConstant();
    std::shared_ptr<TypeNode> toVolatile();
    static std::shared_ptr<TypeNode> toConstant(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        return node->toConstant();
    }
    static std::shared_ptr<TypeNode> toNonConstant(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        return node->toNonConstant();
    }
    static std::shared_ptr<TypeNode> toVolatile(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        return node->toVolatile();
    }
    static std::shared_ptr<TypeNode> toNonVolatile(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        return node->toNonVolatile();
    }
    virtual bool operator ==(const TypeNode &rt) const = 0;
    bool operator !=(const TypeNode &rt) const
    {
        return !operator ==(rt);
    }
    virtual std::size_t getHash() const = 0;
    virtual std::shared_ptr<ValueNode> makeDefaultValue() = 0;
    virtual std::shared_ptr<TypeNode> dereference()
    {
        return nullptr;
    }
private:
    TypeProperties typeProperties;
    bool hasTypeProperties;
public:
    TypeProperties getTypeProperties()
    {
        if(hasTypeProperties)
            return typeProperties;
        typeProperties = context->backend->getTypeProperties(shared_from_this());
        hasTypeProperties = true;
        return typeProperties;
    }
};

class TypeBuiltIn : public TypeNode
{
protected:
    explicit TypeBuiltIn(CompilerContext *context)
        : TypeNode(context, false, false)
    {
    }
};

class TypeConstant final : public TypeNode
{
    friend class CompilerContext;
private:
    std::shared_ptr<TypeNode> node;
    TypeConstant(CompilerContext *context, std::shared_ptr<TypeNode> node)
        : TypeNode(context, true, node->isVolatile), node(node)
    {
    }
public:
    virtual std::shared_ptr<TypeNode> toNonConstant() final
    {
        return node;
    }
    virtual std::shared_ptr<TypeNode> toNonVolatile() final
    {
        if(node->isVolatile)
            return makeConstant(node->toNonVolatile());
        return shared_from_this();
    }
    static std::shared_ptr<TypeNode> makeConstant(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        if(node->isConstant)
            return node;
        return node->context->constructTypeNode<TypeConstant>(node);
    }
    virtual bool operator ==(const TypeNode &rt) const override
    {
        const TypeConstant *prt = dynamic_cast<const TypeConstant *>(&rt);
        if(prt != nullptr)
        {
            return *node == *prt->node;
        }
        return false;
    }
    virtual std::size_t getHash() const override
    {
        return static_cast<std::size_t>(0x7321489) + node->getHash();
    }
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypeConstant(std::static_pointer_cast<TypeConstant>(shared_from_this()));
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override
    {
        return node->makeDefaultValue();
    }
    virtual std::shared_ptr<TypeNode> dereference() override
    {
        return node->dereference();
    }
};

class TypeVolatile final : public TypeNode
{
    friend class CompilerContext;
private:
    std::shared_ptr<TypeNode> node;
    TypeVolatile(CompilerContext *context, std::shared_ptr<TypeNode> node)
        : TypeNode(context, false, true), node(node)
    {
    }
public:
    virtual std::shared_ptr<TypeNode> toNonVolatile() final
    {
        return node;
    }
    static std::shared_ptr<TypeNode> makeVolatile(std::shared_ptr<TypeNode> node)
    {
        if(node == nullptr)
            return nullptr;
        if(node->isVolatile)
            return node;
        if(node->isConstant)
            return TypeConstant::makeConstant(node->context->constructTypeNode<TypeVolatile>(node->toNonConstant()));
        return node->context->constructTypeNode<TypeVolatile>(node);
    }
    virtual void visit(TypeVisitor &visitor) override
    {
        visitor.visitTypeVolatile(std::static_pointer_cast<TypeVolatile>(shared_from_this()));
    }
    virtual bool operator ==(const TypeNode &rt) const override
    {
        const TypeVolatile *prt = dynamic_cast<const TypeVolatile *>(&rt);
        if(prt != nullptr)
        {
            return *node == *prt->node;
        }
        return false;
    }
    virtual std::size_t getHash() const override
    {
        return static_cast<std::size_t>(0x71892437) + node->getHash();
    }
    virtual std::shared_ptr<ValueNode> makeDefaultValue() override
    {
        return node->makeDefaultValue();
    }
    virtual std::shared_ptr<TypeNode> dereference() override
    {
        return node->dereference();
    }
};

inline std::shared_ptr<TypeNode> TypeNode::toConstant()
{
    return TypeConstant::makeConstant(shared_from_this());
}

inline std::shared_ptr<TypeNode> TypeNode::toVolatile()
{
    return TypeVolatile::makeVolatile(shared_from_this());
}

#endif // TYPE_H_INCLUDED
