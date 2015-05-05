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
#include "parser.h"
#include "../tokenizer/token.h"
#include "../ssa/ssa_nodes.h"
#include "../types/types.h"
#include "../values/values.h"
#include <unordered_map>
#include <deque>
#include "../construct_basic_block_graph.h"
#include <cassert>

namespace
{
struct Symbol final
{
    std::string name;
    std::shared_ptr<TypeNode> type;
    VariableLocation location;
    static VariableLocation makeLocal(std::shared_ptr<SSAFunction> function, std::shared_ptr<TypeNode> type)
    {
        return VariableLocation(VariableLocation::LocationKind::LocalVariable, function->context->backend->getTypeProperties(type).allocateVariable(function->localVariablesSize));
    }
    Symbol(std::string name, std::shared_ptr<TypeNode> type, VariableLocation location)
        : name(name), type(type), location(location)
    {
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

    std::shared_ptr<SSANode> topLevelExpression()
    {
        switch(tokenizer.tokenType)
        {
        case TokenType::LParen:
        {
            tokenizer.readNext();
            std::shared_ptr<SSANode> retval = expression();
            if(tokenizer.tokenType != TokenType::RParen)
                throw ParseError("expected )");
            tokenizer.readNext();
            return retval;
        }
        case TokenType::Identifier:
        {
            std::shared_ptr<Symbol> symbol = findSymbolInSymbolTables(tokenizer.tokenValue);
            if(symbol == nullptr)
                throw ParseError("undeclared symbol");
            tokenizer.readNext();
            std::shared_ptr<SSANode> addressNode = std::make_shared<SSAConstant>(std::make_shared<ValueLocalVariablePointer>(context, symbol->location.start, symbol->type), nullptr);
            currentBasicBlock->instructions.push_back(addressNode);
            std::shared_ptr<SSANode> valueNode = std::make_shared<SSALoad>(addressNode, symbol->location);
            currentBasicBlock->instructions.push_back(valueNode);
            return valueNode;
        }
        case TokenType::False:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, false), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            return node;
        }
        case TokenType::True:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, true), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            return node;
        }
        case TokenType::Null:
        {
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(std::make_shared<ValueNullPointer>(context), nullptr);
            currentBasicBlock->instructions.push_back(node);
            tokenizer.readNext();
            return node;
        }
        default:
            throw ParseError("expected (, id, true, or false");
        }
    }

    std::shared_ptr<SSANode> comparisonExpression()
    {
        std::shared_ptr<SSANode> retval = topLevelExpression();
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
            return retval;
        }
        tokenizer.readNext();
        std::shared_ptr<SSANode> lhs = retval;
        std::shared_ptr<SSANode> rhs = topLevelExpression();
        retval = std::make_shared<SSACompare>(lhs, compareOperator, rhs, nullptr);
        currentBasicBlock->instructions.push_back(retval);
        return retval;
    }

    std::shared_ptr<SSANode> assignmentExpression()
    {
        if(tokenizer.tokenType == TokenType::Identifier)
        {
            std::string name = tokenizer.tokenValue;
            tokenizer.readNext();
            if(tokenizer.tokenType == TokenType::Equal)
            {
                std::shared_ptr<Symbol> symbol = findSymbolInSymbolTables(name);
                if(symbol == nullptr)
                    throw ParseError("undeclared symbol");
                tokenizer.readNext();
                std::shared_ptr<SSANode> newNode = assignmentExpression();
                if(newNode->type != symbol->type)
                    throw ParseError("types don't match for =");
                std::shared_ptr<SSANode> addressNode = std::make_shared<SSAConstant>(std::make_shared<ValueLocalVariablePointer>(context, symbol->location.start, symbol->type), nullptr);
                currentBasicBlock->instructions.push_back(addressNode);
                std::shared_ptr<SSANode> storeNode = std::make_shared<SSAStore>(addressNode, newNode);
                currentBasicBlock->instructions.push_back(storeNode);
                return newNode;
            }
            else
            {
                tokenizer.putBack(TokenType::Identifier, name);
            }
        }
        return comparisonExpression();
    }

    std::shared_ptr<SSANode> commaExpression(bool ignoreComma)
    {
        std::shared_ptr<SSANode> retval = assignmentExpression();
        while(tokenizer.tokenType == TokenType::Comma && !ignoreComma)
        {
            tokenizer.readNext();
            retval = assignmentExpression();
        }
        return retval;
    }

    std::shared_ptr<SSANode> expression(bool ignoreComma = false)
    {
        return commaExpression(ignoreComma);
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
            std::shared_ptr<Symbol> symbol = std::make_shared<Symbol>(name, theType, Symbol::makeLocal(function, theType));
            addSymbolToTopSymbolTable(symbol);
            std::shared_ptr<SSANode> node = std::make_shared<SSAConstant>(initialValue, symbol->location);
            currentBasicBlock->instructions.push_back(node);
            std::shared_ptr<SSANode> addressNode = std::make_shared<SSAConstant>(std::make_shared<ValueLocalVariablePointer>(context, symbol->location.start, symbol->type), nullptr);
            currentBasicBlock->instructions.push_back(addressNode);
            std::shared_ptr<SSANode> storeNode = std::make_shared<SSAStore>(addressNode, node);
            currentBasicBlock->instructions.push_back(storeNode);
            expression(true);
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
        std::shared_ptr<SSANode> condition = expression(); // handles parenthesis
        if(condition->type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("if condition type must be boolean");
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
        std::shared_ptr<SSANode> condition = expression(); // handles parenthesis
        if(condition->type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("do while condition type must be boolean");
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
        std::shared_ptr<SSANode> condition = expression(); // handles parenthesis
        if(condition->type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("while condition type must be boolean");
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
        std::shared_ptr<SSANode> condition = expression();
        if(tokenizer.tokenType != TokenType::Semicolon)
            throw ParseError("expected ;");
        tokenizer.readNext();
        if(condition->type->toNonVolatile()->toNonConstant() != TypeBoolean::make(context))
            throw ParseError("for condition type must be boolean");
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, condition, loopBlock, endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
        currentBasicBlock = updateBlock;
        function->blocks.push_back(currentBasicBlock);
        expression();
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
            expression();
            if(tokenizer.tokenType != TokenType::Semicolon)
                throw ParseError("expected " + getTokenString(terminatingToken));
            tokenizer.readNext();
            break;
        case TokenType::Constant:
        case TokenType::Volatile:
        case TokenType::Boolean:
        case TokenType::Void:
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
            expression();
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
                statement();
                break;
            case TokenType::Constant:
            case TokenType::Volatile:
            case TokenType::Boolean:
            case TokenType::Void:
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
        function->endBlock = std::make_shared<SSABasicBlock>(context);
        function->blocks.push_back(function->endBlock);
        currentBasicBlock = function->startBlock;
        blockInterior();
        currentBasicBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, function->endBlock);
        currentBasicBlock->instructions.push_back(currentBasicBlock->controlTransferInstruction);
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
