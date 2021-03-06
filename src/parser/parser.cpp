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
#include "parser/parser.h"
#include "tokenizer/token.h"
#include "ssa/ssa_nodes.h"
#include "types/types.h"
#include "values/values.h"
#include <unordered_map>
#include <deque>
#include "construct_basic_block_graph.h"
#include <cassert>
#include <sstream>
#include <cstdlib>
#include <cerrno>

namespace
{
struct Symbol final
{
    std::string name;
    std::shared_ptr<TypeNode> type;
    std::shared_ptr<SSANode> addressNode;
    Symbol(std::string name, std::shared_ptr<TypeNode> type, std::shared_ptr<SSAFunction> function)
        : name(name), type(type)
    {
        addressNode = std::make_shared<SSAAllocA>(type);
        function->startBlock->instructions.push_front(addressNode); // start block doesn't have any phi instructions so we don't need to insert after them
    }
};

class Parser final
{
private:
    Tokenizer tokenizer;
    CompilerContext *const context;
    std::deque<std::unordered_map<std::string, std::shared_ptr<Symbol>>> symbolTables;
    std::shared_ptr<SSAFunction> function;
    std::shared_ptr<SSABasicBlock> currentBasicBlock;
    std::shared_ptr<Symbol> findSymbolInSymbolTable(std::string name, std::unordered_map<std::string, std::shared_ptr<Symbol>> &table)
    {
        auto iter = table.find(name);
        if(iter != table.end())
        {
            return std::get<1>(*iter);
        }
        return nullptr;
    }
    std::shared_ptr<Symbol> findSymbolInSymbolTables(std::string name)
    {
        for(auto &table : symbolTables)
        {
            std::shared_ptr<Symbol> retval = findSymbolInSymbolTable(name, table);
            if(retval != nullptr)
                return retval;
        }
        return nullptr;
    }
    std::shared_ptr<Symbol> findSymbolInTopSymbolTable(std::string name)
    {
        return findSymbolInSymbolTable(name, symbolTables.front());
    }
    void addSymbolToTopSymbolTable(std::shared_ptr<Symbol> symbol)
    {
        symbolTables.front()[symbol->name] = symbol;
    }
    void pushSymbolTable()
    {
        symbolTables.emplace_front();
    }
    void popSymbolTable()
    {
        symbolTables.pop_front();
    }
    struct Value final
    {
        enum class Kind
        {
            LValue,
            RValue,
        };
        std::shared_ptr<SSANode> node;
        std::shared_ptr<TypeNode> type;
        Kind kind;
        Value(std::shared_ptr<SSANode> node, std::shared_ptr<TypeNode> type, Kind kind)
            : node(node), type(type), kind(kind)
        {
        }
    };
    std::vector<Value> valueStack;

    void convertValueToRValue()
    {
        switch(valueStack.back().kind)
        {
        case Value::Kind::LValue:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSALoad>(valueStack.back().node, nullptr);
            currentBasicBlock->instructions.push_back(node);
            valueStack.back() = Value(node, valueStack.back().type, Value::Kind::RValue);
            break;
        }
        case Value::Kind::RValue:
            break;
        }
    }

    void topLevelExpression()
    {
        switch(tokenizer.tokenType)
        {
        case TokenType::LParen:
        {
            tokenizer.readNext();
            expression();
            if(tokenizer.tokenType != TokenType::RParen)
                throw ParseError("expected )");
            tokenizer.readNext();
            return;
        }
        case TokenType::Identifier:
        {
            std::shared_ptr<Symbol> symbol = findSymbolInSymbolTables(tokenizer.tokenValue);
            if(symbol == nullptr)
                throw ParseError("undeclared symbol");
            tokenizer.readNext();
            valueStack.push_back(Value(symbol->addressNode, symbol->type, Value::Kind::LValue));
            return;
        }
        case TokenType::False:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, false), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            valueStack.push_back(Value(node, TypeBoolean::make(context), Value::Kind::RValue));
            return;
        }
        case TokenType::True:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, true), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            valueStack.push_back(Value(node, TypeBoolean::make(context), Value::Kind::RValue));
            return;
        }
        case TokenType::Null:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueNullPointer>(context), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            valueStack.push_back(Value(node, node->type, Value::Kind::RValue));
            return;
        }
        case TokenType::IntValue:
        {
            std::string str = tokenizer.tokenValue;
            int base = 10; // set base manually to avoid octal
            if(str.substr(0, 2) == "0x" || str.substr(0, 2) == "0X")
                base = 16;
            int oldErrno = errno;
            try
            {
                errno = 0;
                unsigned long long vll = std::strtoull(tokenizer.tokenValue.c_str(), nullptr, base);
                std::uint64_t vu64 = static_cast<std::uint64_t>(vll);
                std::uint64_t v = vu64;
                if(errno == ERANGE || vu64 != vll)
                {
                    throw ParseError("number too big");
                }
                bool isUnsigned = false;
                IntegerWidth width = IntegerWidth::Int64;
                std::int64_t vs64 = static_cast<std::int64_t>(vll);
                std::int32_t vs32 = static_cast<std::int32_t>(vs64);
                std::int16_t vs16 = static_cast<std::int16_t>(vs64);
                std::int8_t vs8 = static_cast<std::int8_t>(vs64);
                std::uint32_t vu32 = static_cast<std::uint32_t>(vu64);
                std::uint16_t vu16 = static_cast<std::uint16_t>(vu64);
                std::uint8_t vu8 = static_cast<std::uint8_t>(vu64);
                if(vs8 >= 0 && vs8 == vs64)
                {
                    isUnsigned = false;
                    width = IntegerWidth::Int8;
                }
                else if(vu8 == vu64)
                {
                    isUnsigned = true;
                    width = IntegerWidth::Int8;
                }
                else if(vs16 >= 0 && vs16 == vs64)
                {
                    isUnsigned = false;
                    width = IntegerWidth::Int16;
                }
                else if(vu16 == vu64)
                {
                    isUnsigned = true;
                    width = IntegerWidth::Int16;
                }
                else if(vs32 >= 0 && vs32 == vs64)
                {
                    isUnsigned = false;
                    width = IntegerWidth::Int32;
                }
                else if(vu32 == vu64)
                {
                    isUnsigned = true;
                    width = IntegerWidth::Int32;
                }
                else if(vs64 >= 0)
                {
                    isUnsigned = false;
                    width = IntegerWidth::Int64;
                }
                else
                {
                    isUnsigned = true;
                    width = IntegerWidth::Int64;
                }
                std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueInteger>(context, isUnsigned, width, v), nullptr);
                currentBasicBlock->instructions.push_back(node);
                tokenizer.readNext();
                valueStack.push_back(Value(node, TypeInteger::make(context, isUnsigned, width), Value::Kind::RValue));
            }
            catch(...)
            {
                errno = oldErrno;
                throw;
            }
            errno = oldErrno;
            break;
        }
        case TokenType::Cast:
        {
            tokenizer.readNext();
            if(tokenizer.tokenType != TokenType::LParen)
                throw ParseError("expected (");
            tokenizer.readNext();
            std::shared_ptr<TypeNode> newType = type();
            if(tokenizer.tokenType != TokenType::Comma)
                throw ParseError("expected ,");
            tokenizer.readNext();
            expression();
            convertValueToRValue();
            if(!valueStack.back().type->canTypeCastTo(newType, false))
                throw ParseError("invalid cast");
            std::shared_ptr<SSANode> node = std::make_shared<SSATypeCast>(valueStack.back().node, newType, nullptr);
            currentBasicBlock->instructions.push_back(node);
            valueStack.back() = Value(node, newType, Value::Kind::RValue);
            if(tokenizer.tokenType != TokenType::RParen)
                throw ParseError("expected )");
            tokenizer.readNext();
            break;
        }
        default:
            throw ParseError("expected (, id, true, false, or null");
        }
    }

    void prefixOperator()
    {
        if(tokenizer.tokenType == TokenType::Star)
        {
            tokenizer.readNext();
            prefixOperator();
            convertValueToRValue();
            if(valueStack.back().type->dereference() == nullptr)
                throw ParseError("can't dereference a non-pointer");
            valueStack.back() = Value(valueStack.back().node, valueStack.back().type->dereference(), Value::Kind::LValue);
            return;
        }
        if(tokenizer.tokenType == TokenType::Ampersand)
        {
            tokenizer.readNext();
            prefixOperator();
            switch(valueStack.back().kind)
            {
            case Value::Kind::LValue:
                break;
            case Value::Kind::RValue:
                throw ParseError("can't take address of a rvalue");
            }
            valueStack.back() = Value(valueStack.back().node, TypePointer::make(valueStack.back().type)->toConstant(), Value::Kind::RValue);
            return;
        }
        topLevelExpression();
    }

    void addExpression()
    {
        prefixOperator();
        while(tokenizer.tokenType == TokenType::Plus)
        {
            convertValueToRValue();
            Value lhs = valueStack.back();
            valueStack.pop_back();
            tokenizer.readNext();
            prefixOperator();
            convertValueToRValue();
            Value rhs = valueStack.back();
            TypeNode::BinaryOperatorTypeRetval type = lhs.type->getArithCombinedType(rhs.type);
            if(!type)
                throw ParseError("types not compatible");
            if(lhs.type != type.lhsType)
            {
                std::shared_ptr<SSANode> node = std::make_shared<SSATypeCast>(lhs.node, type.lhsType, nullptr);
                currentBasicBlock->instructions.push_back(node);
                lhs.node = node;
                lhs.type = type.lhsType;
            }
            if(rhs.type != type.rhsType)
            {
                std::shared_ptr<SSANode> node = std::make_shared<SSATypeCast>(rhs.node, type.rhsType, nullptr);
                currentBasicBlock->instructions.push_back(node);
                rhs.node = node;
                rhs.type = type.rhsType;
            }
            std::shared_ptr<SSANode> node = std::make_shared<SSAAdd>(lhs.node, rhs.node, nullptr, type.resultType);
            currentBasicBlock->instructions.push_back(node);
            valueStack.back() = Value(node, type.resultType, Value::Kind::RValue);
        }
    }

    void comparisonExpression()
    {
        addExpression();
        SSACompare::CompareOperator compareOperator;
        switch(tokenizer.tokenType)
        {
        case TokenType::EqualEqual:
            compareOperator = SSACompare::CompareOperator::E;
            break;
        case TokenType::NotEqual:
            compareOperator = SSACompare::CompareOperator::NE;
            break;
        case TokenType::GreaterEqual:
            compareOperator = SSACompare::CompareOperator::GE;
            break;
        case TokenType::LessEqual:
            compareOperator = SSACompare::CompareOperator::LE;
            break;
        case TokenType::Greater:
            compareOperator = SSACompare::CompareOperator::G;
            break;
        case TokenType::Less:
            compareOperator = SSACompare::CompareOperator::L;
            break;
        default:
            return;
        }
        tokenizer.readNext();
        convertValueToRValue();
        Value lhs = valueStack.back();
        valueStack.pop_back();
        addExpression();
        convertValueToRValue();
        Value rhs = valueStack.back();
        valueStack.pop_back();
        TypeNode::BinaryOperatorTypeRetval type = lhs.type->getCompareType(rhs.type);
        if(!type)
            throw ParseError("types not compatible");
        if(lhs.type != type.lhsType)
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSATypeCast>(lhs.node, type.lhsType, nullptr);
            currentBasicBlock->instructions.push_back(node);
            lhs.node = node;
            lhs.type = type.lhsType;
        }
        if(rhs.type != type.rhsType)
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSATypeCast>(rhs.node, type.rhsType, nullptr);
            currentBasicBlock->instructions.push_back(node);
            rhs.node = node;
            rhs.type = type.rhsType;
        }
        std::shared_ptr<SSANode> retval = std::make_shared<SSACompare>(lhs.node, compareOperator, rhs.node, nullptr);
        currentBasicBlock->instructions.push_back(retval);
        valueStack.push_back(Value(retval, type.resultType, Value::Kind::RValue));
        return;
    }

    void assignmentExpression()
    {
        comparisonExpression();
        if(tokenizer.tokenType == TokenType::Equal)
        {
            Value variable = valueStack.back();
            switch(variable.kind)
            {
            case Value::Kind::LValue:
                break;
            case Value::Kind::RValue:
                throw ParseError("can't assign to rvalue");
            }
            if(variable.type->isConstant)
                throw ParseError("can't assign to constant");
            tokenizer.readNext();
            assignmentExpression();
            convertValueToRValue();
            Value newValue = valueStack.back();
            valueStack.pop_back();
            if(!newValue.type->canTypeCastTo(variable.type->toVolatile()->toConstant(), true))
                throw ParseError("invalid types for =");
            if(newValue.type->toNonConstant()->toNonVolatile() != variable.type->toNonConstant()->toNonVolatile())
            {
                newValue.node = std::make_shared<SSATypeCast>(newValue.node, variable.type->toConstant(), nullptr);
                currentBasicBlock->instructions.push_back(newValue.node);
                newValue.type = variable.type->toConstant();
            }
            std::shared_ptr<SSANode> storeNode = std::make_shared<SSAStore>(variable.node, newValue.node);
            currentBasicBlock->instructions.push_back(storeNode);
        }
    }

    void commaExpression(bool ignoreComma)
    {
        assignmentExpression();
        while(tokenizer.tokenType == TokenType::Comma && !ignoreComma)
        {
            tokenizer.readNext();
            valueStack.pop_back();
            assignmentExpression();
        }
    }

    void expression(bool ignoreComma = false)
    {
        commaExpression(ignoreComma);
    }

    std::shared_ptr<TypeNode> topLevelType()
    {
        switch(tokenizer.tokenType)
        {
        case TokenType::Void:
        {
            tokenizer.readNext();
            return TypeVoid::make(context);
        }
        case TokenType::UInt8:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, true, IntegerWidth::Int8);
        }
        case TokenType::Int8:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, false, IntegerWidth::Int8);
        }
        case TokenType::UInt16:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, true, IntegerWidth::Int16);
        }
        case TokenType::Int16:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, false, IntegerWidth::Int16);
        }
        case TokenType::UInt32:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, true, IntegerWidth::Int32);
        }
        case TokenType::Int32:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, false, IntegerWidth::Int32);
        }
        case TokenType::UInt64:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, true, IntegerWidth::Int64);
        }
        case TokenType::Int64:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, false, IntegerWidth::Int64);
        }
        case TokenType::UInt:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, true, IntegerWidth::IntNativeSize);
        }
        case TokenType::Int:
        {
            tokenizer.readNext();
            return TypeInteger::make(context, false, IntegerWidth::IntNativeSize);
        }
        case TokenType::Boolean:
        {
            tokenizer.readNext();
            return TypeBoolean::make(context);
        }
        default:
            throw ParseError("expected a type");
        }
    }

    std::function<std::shared_ptr<TypeNode>(std::shared_ptr<TypeNode>)> parseTypeQualifier()
    {
        bool isConstant = false;
        bool isVolatile = false;
        for(;;)
        {
            if(tokenizer.tokenType == TokenType::Constant)
            {
                if(isConstant)
                    throw ParseError("too many \"constant\"s");
                isConstant = true;
                tokenizer.readNext();
            }
            else if(tokenizer.tokenType == TokenType::Volatile)
            {
                if(isVolatile)
                    throw ParseError("too many \"volatile\"s");
                isVolatile = true;
                tokenizer.readNext();
            }
            else
                break;
        }
        if(isConstant)
        {
            if(isVolatile)
            {
                return [](std::shared_ptr<TypeNode> node) -> std::shared_ptr<TypeNode>
                {
                    return node->toVolatile()->toConstant();
                };
            }
            else
            {
                return [](std::shared_ptr<TypeNode> node) -> std::shared_ptr<TypeNode>
                {
                    return node->toConstant();
                };
            }
        }
        else
        {
            if(isVolatile)
            {
                return [](std::shared_ptr<TypeNode> node) -> std::shared_ptr<TypeNode>
                {
                    return node->toVolatile();
                };
            }
            else
            {
                return nullptr;
            }
        }
    }

    std::shared_ptr<TypeNode> pointerType()
    {
        std::function<std::shared_ptr<TypeNode>(std::shared_ptr<TypeNode>)> currentQualifier = parseTypeQualifier();
        std::shared_ptr<TypeNode> retval = topLevelType();
        if(currentQualifier)
            retval = currentQualifier(retval);
        currentQualifier = parseTypeQualifier();
        if(currentQualifier)
            retval = currentQualifier(retval);
        while(tokenizer.tokenType == TokenType::Star)
        {
            retval = TypePointer::make(retval);
            tokenizer.readNext();
            currentQualifier = parseTypeQualifier();
            if(currentQualifier)
                retval = currentQualifier(retval);
        }
        return retval;
    }

    std::shared_ptr<TypeNode> type()
    {
        return pointerType();
    }

    void declaration(TokenType terminatingToken = TokenType::Semicolon)
    {
        std::shared_ptr<TypeNode> theType = type();
        if(tokenizer.tokenType == TokenType::Semicolon)
        {
            tokenizer.readNext();
            return;
        }
        for(;;)
        {
            if(tokenizer.tokenType != TokenType::Identifier)
                throw ParseError("expected id");
            std::string name = tokenizer.tokenValue;
            if(findSymbolInTopSymbolTable(name) != nullptr)
                throw ParseError("can't redefine symbol in same scope");
            std::shared_ptr<ValueNode> initialValue = theType->makeDefaultValue();
            if(initialValue == nullptr)
                throw ParseError("invalid type for variable");
            std::shared_ptr<Symbol> symbol = std::make_shared<Symbol>(name, theType, function);
            addSymbolToTopSymbolTable(symbol);
            tokenizer.readNext();
            if(tokenizer.tokenType == TokenType::Equal)
            {
                tokenizer.readNext();
                expression(true);
                convertValueToRValue();
                Value newValue = valueStack.back();
                valueStack.pop_back();
                if(!newValue.type->canTypeCastTo(symbol->type->toVolatile()->toConstant(), true))
                    throw ParseError("invalid types for =");
                if(newValue.type->toNonConstant()->toNonVolatile() != symbol->type->toNonConstant()->toNonVolatile())
                {
                    newValue.node = std::make_shared<SSATypeCast>(newValue.node, symbol->type->toConstant(), nullptr);
                    currentBasicBlock->instructions.push_back(newValue.node);
                    newValue.type = symbol->type->toConstant();
                }
                std::shared_ptr<SSANode> storeNode = std::make_shared<SSAStore>(symbol->addressNode, newValue.node);
                currentBasicBlock->instructions.push_back(storeNode);
            }
            else
            {
                std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(initialValue, nullptr);
                currentBasicBlock->instructions.push_back(node);
                std::shared_ptr<SSANode> storeNode = std::make_shared<SSAStore>(symbol->addressNode, node);
                currentBasicBlock->instructions.push_back(storeNode);
            }
            if(tokenizer.tokenType == terminatingToken)
            {
                tokenizer.readNext();
                return;
            }
            if(tokenizer.tokenType != TokenType::Comma)
                throw ParseError("expected ,");
            tokenizer.readNext();
        }
    }

    void ifStatement()
    {
        if(tokenizer.tokenType != TokenType::If)
            throw ParseError("expected if");
        tokenizer.readNext();
        if(tokenizer.tokenType != TokenType::LParen)
            throw ParseError("expected (");
        expression(); // handles parenthesis
        convertValueToRValue();
        std::shared_ptr<SSANode> condition = valueStack.back().node;
        if(valueStack.back().type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("if condition type must be boolean");
        valueStack.pop_back();
        std::shared_ptr<SSABasicBlock> startBlock = currentBasicBlock;
        std::shared_ptr<SSABasicBlock> thenBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> elseBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> endBlock = std::make_shared<SSABasicBlock>(context);
        currentBasicBlock = thenBlock;
        function->blocks.push_back(currentBasicBlock);
        statement();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        if(tokenizer.tokenType == TokenType::Else)
        {
            tokenizer.readNext();
            currentBasicBlock = elseBlock;
            function->blocks.push_back(currentBasicBlock);
            statement();
            currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, endBlock);
            currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
            currentBasicBlock = endBlock;
            function->blocks.push_back(currentBasicBlock);
            startBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, thenBlock, elseBlock);
            startBlock->instructions.push_back(startBlock->controlTransferInstruction);
            return;
        }
        currentBasicBlock = endBlock;
        function->blocks.push_back(currentBasicBlock);
        startBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, thenBlock, endBlock);
        startBlock->instructions.push_back(startBlock->controlTransferInstruction);
    }

    void doWhileStatement()
    {
        if(tokenizer.tokenType != TokenType::Do)
            throw ParseError("expected do");
        tokenizer.readNext();
        std::shared_ptr<SSABasicBlock> startBlock = currentBasicBlock;
        std::shared_ptr<SSABasicBlock> loopBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> endBlock = std::make_shared<SSABasicBlock>(context);
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, loopBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = loopBlock;
        function->blocks.push_back(currentBasicBlock);
        statement();
        if(tokenizer.tokenType != TokenType::While)
            throw ParseError("expected while");
        tokenizer.readNext();
        if(tokenizer.tokenType != TokenType::LParen)
            throw ParseError("expected (");
        expression(); // handles parenthesis
        convertValueToRValue();
        std::shared_ptr<SSANode> condition = valueStack.back().node;
        if(valueStack.back().type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("do while condition type must be boolean");
        valueStack.pop_back();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, loopBlock, endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = endBlock;
        function->blocks.push_back(currentBasicBlock);
        if(tokenizer.tokenType != TokenType::Semicolon)
            throw ParseError("expected ;");
        tokenizer.readNext();
    }

    void whileStatement()
    {
        if(tokenizer.tokenType != TokenType::While)
            throw ParseError("expected while");
        tokenizer.readNext();
        std::shared_ptr<SSABasicBlock> startBlock = currentBasicBlock;
        std::shared_ptr<SSABasicBlock> conditionBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> loopBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> endBlock = std::make_shared<SSABasicBlock>(context);
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, conditionBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = conditionBlock;
        function->blocks.push_back(currentBasicBlock);
        if(tokenizer.tokenType != TokenType::LParen)
            throw ParseError("expected (");
        expression(); // handles parenthesis
        convertValueToRValue();
        std::shared_ptr<SSANode> condition = valueStack.back().node;
        if(valueStack.back().type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("while condition type must be boolean");
        valueStack.pop_back();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, loopBlock, endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = loopBlock;
        function->blocks.push_back(currentBasicBlock);
        statement();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, conditionBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = endBlock;
        function->blocks.push_back(currentBasicBlock);
    }

    void forStatement()
    {
        pushSymbolTable();
        if(tokenizer.tokenType != TokenType::For)
            throw ParseError("expected for");
        tokenizer.readNext();
        if(tokenizer.tokenType != TokenType::LParen)
            throw ParseError("expected (");
        tokenizer.readNext();
        expressionOrDeclaration();
        std::shared_ptr<SSABasicBlock> startBlock = currentBasicBlock;
        std::shared_ptr<SSABasicBlock> conditionBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> updateBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> loopBlock = std::make_shared<SSABasicBlock>(context);
        std::shared_ptr<SSABasicBlock> endBlock = std::make_shared<SSABasicBlock>(context);
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, conditionBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = conditionBlock;
        function->blocks.push_back(currentBasicBlock);
        expression(); // handles parenthesis
        convertValueToRValue();
        std::shared_ptr<SSANode> condition = valueStack.back().node;
        if(valueStack.back().type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("for condition type must be boolean");
        valueStack.pop_back();
        if(tokenizer.tokenType != TokenType::Semicolon)
            throw ParseError("expected ;");
        tokenizer.readNext();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, loopBlock, endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = updateBlock;
        function->blocks.push_back(currentBasicBlock);
        expression();
        valueStack.pop_back();
        if(tokenizer.tokenType != TokenType::RParen)
            throw ParseError("expected )");
        tokenizer.readNext();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, conditionBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = loopBlock;
        function->blocks.push_back(currentBasicBlock);
        statement();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, updateBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = endBlock;
        function->blocks.push_back(currentBasicBlock);
        popSymbolTable();
    }

    void expressionOrDeclaration(TokenType terminatingToken = TokenType::Semicolon)
    {
        switch(tokenizer.tokenType)
        {
        case TokenType::Identifier:
        case TokenType::False:
        case TokenType::True:
        case TokenType::LParen:
        case TokenType::Star:
        case TokenType::Ampersand:
        case TokenType::Cast:
            expression();
            valueStack.pop_back();
            if(tokenizer.tokenType != TokenType::Semicolon)
                throw ParseError("expected " + getTokenString(terminatingToken));
            tokenizer.readNext();
            break;
        case TokenType::Constant:
        case TokenType::Volatile:
        case TokenType::Boolean:
        case TokenType::Void:
        case TokenType::Int8:
        case TokenType::UInt8:
        case TokenType::Int16:
        case TokenType::UInt16:
        case TokenType::Int32:
        case TokenType::UInt32:
        case TokenType::Int64:
        case TokenType::UInt64:
        case TokenType::Int:
        case TokenType::UInt:
            declaration(terminatingToken);
            break;
        default:
            throw ParseError("unexpected token");
        }
    }

    void statement()
    {
        switch(tokenizer.tokenType)
        {
        case TokenType::Identifier:
        case TokenType::False:
        case TokenType::True:
        case TokenType::LParen:
        case TokenType::Star:
        case TokenType::Ampersand:
        case TokenType::Cast:
            expression();
            valueStack.pop_back();
            if(tokenizer.tokenType != TokenType::Semicolon)
                throw ParseError("expected ;");
            tokenizer.readNext();
            return;
        case TokenType::LBrace:
            block();
            return;
        case TokenType::Semicolon:
            tokenizer.readNext();
            return;
        case TokenType::If:
            ifStatement();
            return;
        case TokenType::Do:
            doWhileStatement();
            return;
        case TokenType::While:
            whileStatement();
            return;
        case TokenType::For:
            forStatement();
            return;
        default:
            throw ParseError("expected statement");
        }
    }

    void blockInterior()
    {
        pushSymbolTable();
        while(tokenizer.tokenType != TokenType::EndOfFile && tokenizer.tokenType != TokenType::RBrace)
        {
            switch(tokenizer.tokenType)
            {
            case TokenType::Identifier:
            case TokenType::False:
            case TokenType::True:
            case TokenType::LParen:
            case TokenType::LBrace:
            case TokenType::Semicolon:
            case TokenType::If:
            case TokenType::While:
            case TokenType::For:
            case TokenType::Do:
            case TokenType::Star:
            case TokenType::Ampersand:
            case TokenType::Cast:
                statement();
                break;
            case TokenType::Constant:
            case TokenType::Volatile:
            case TokenType::Boolean:
            case TokenType::Void:
            case TokenType::Int8:
            case TokenType::UInt8:
            case TokenType::Int16:
            case TokenType::UInt16:
            case TokenType::Int32:
            case TokenType::UInt32:
            case TokenType::Int64:
            case TokenType::UInt64:
            case TokenType::Int:
            case TokenType::UInt:
                declaration();
                break;
            default:
                throw ParseError("unexpected token");
            }
        }
        popSymbolTable();
    }

    void block()
    {
        if(tokenizer.tokenType != TokenType::LBrace)
            throw ParseError("expected {");
        tokenizer.readNext();
        blockInterior();
        if(tokenizer.tokenType != TokenType::RBrace)
            throw ParseError("expected }");
        tokenizer.readNext();
    }

public:
    explicit Parser(CompilerContext *context, std::istream &is, bool dumpCode)
        : tokenizer(is, dumpCode), context(context)
    {
    }
    std::shared_ptr<SSAFunction> operator ()()
    {
        symbolTables.clear();
        pushSymbolTable();
        function = std::make_shared<SSAFunction>(context);
        function->startBlock = std::make_shared<SSABasicBlock>(context);
        function->blocks.push_back(function->startBlock);
        currentBasicBlock = function->startBlock;
        blockInterior();
        ConstructBasicBlockGraphVisitor().visitSSAFunction(function);
        return function;
    }
};
}

std::shared_ptr<SSAFunction> parse(CompilerContext *context, std::istream &is, bool dumpCode)
{
    Parser parser(context, is, dumpCode);
    return parser();
}
