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
#ifndef TOKEN_H_INCLUDED
#define TOKEN_H_INCLUDED

#include <istream>
#include <string>
#include <cctype>
#include "../parser/parser.h"
#include <iostream>
#include <vector>

enum class TokenType
{
#define TOKEN_DEF_WORD(a) a,
#define TOKEN_DEF_SYMBOL(a,b) a,
#define TOKEN_DEF_SPECIAL(a,b,c) a,
#include "token_names.h"
#undef TOKEN_DEF_WORD
#undef TOKEN_DEF_SYMBOL
#undef TOKEN_DEF_SPECIAL
};

inline std::string getTokenString(TokenType type)
{
    switch(type)
    {
#define TOKEN_DEF_WORD(a) case TokenType::a: return #a;
#define TOKEN_DEF_SYMBOL(a,b) case TokenType::a: return b;
#define TOKEN_DEF_SPECIAL(a,b,c) case TokenType::a: return b;
#include "token_names.h"
#undef TOKEN_DEF_WORD
#undef TOKEN_DEF_SYMBOL
#undef TOKEN_DEF_SPECIAL
    default:
        return "<Unknown>";
    }
}

inline std::string getTokenString(TokenType type, std::string tokenValue)
{
    switch(type)
    {
#define TOKEN_DEF_WORD(a) case TokenType::a: return #a;
#define TOKEN_DEF_SYMBOL(a,b) case TokenType::a: return b;
#define TOKEN_DEF_SPECIAL(a,b,c) case TokenType::a: return c;
#include "token_names.h"
#undef TOKEN_DEF_WORD
#undef TOKEN_DEF_SYMBOL
#undef TOKEN_DEF_SPECIAL
    default:
        return "<Unknown>";
    }
}

class Tokenizer final
{
private:
    std::istream &is;
    bool dumpCode;
    std::vector<char> putBackCharacters;
    int peek()
    {
        if(!putBackCharacters.empty())
        {
            return putBackCharacters.back();
        }
        return is.peek();
    }
    int get()
    {
        if(!putBackCharacters.empty())
        {
            int retval = putBackCharacters.back();
            putBackCharacters.pop_back();
            return retval;
        }
        if(dumpCode)
            std::cout << static_cast<char>(is.peek()) << std::flush;
        return is.get();
    }
    void putBack(int ch)
    {
        putBackCharacters.push_back(static_cast<char>(ch));
    }
    static int compareCaseInsensitive(std::string a, std::string b)
    {
        for(std::size_t i = 0; i < a.size() && i < b.size(); i++)
        {
            int cha = std::toupper(a[i]);
            int chb = std::toupper(b[i]);
            if(cha < chb)
                return -1;
            if(cha > chb)
                return 1;
        }
        if(a.size() < b.size())
            return -1;
        if(a.size() > b.size())
            return 1;
        return 0;
    }
    TokenType putBackTokenType;
    std::string putBackTokenValue;
    bool hasPutBackToken = false;
public:
    explicit Tokenizer(std::istream &is, bool dumpCode)
        : is(is), dumpCode(dumpCode)
    {
        readNext();
    }
    TokenType tokenType;
    std::string tokenValue;
    void putBack(TokenType type, std::string value)
    {
        hasPutBackToken = true;
        putBackTokenType = tokenType;
        putBackTokenValue = tokenValue;
        tokenType = type;
        tokenValue = value;
    }
    void readNext()
    {
        if(hasPutBackToken)
        {
            tokenType = putBackTokenType;
            tokenValue = putBackTokenValue;
            putBackTokenValue.clear();
            hasPutBackToken = false;
            return;
        }
        tokenType = TokenType::EndOfFile;
        tokenValue = "";
        for(;;)
        {
            if(peek() == EOF)
                return;
            if(peek() == '/')
            {
                get();
                if(peek() == '*')
                {
                    get();
                    for(;;)
                    {
                        if(peek() == EOF)
                        {
                            throw ParseError("missing closing */");
                        }
                        if(peek() == '*')
                        {
                            while(peek() == '*')
                                get();
                            if(peek() == '/')
                            {
                                get();
                                break;
                            }
                        }
                        else
                            get();
                    }
                }
                else if(peek() == '/')
                {
                    while(peek() != EOF && (!std::isspace(peek()) || std::isblank(peek())))
                    {
                        get();
                    }
                }
                else
                {
                    putBack('/');
                    break;
                }
            }
            else if(isspace(peek()))
                get();
            else
                break;
        }
        if(std::isalpha(peek()) || peek() == '_')
        {
            tokenType = TokenType::Identifier;
            while(std::isalnum(peek()) || peek() == '_')
            {
                tokenValue += static_cast<char>(get());
            }
            for(char ch : tokenValue)
            {
                if(std::isupper(ch))
                    return;
            }
#define TOKEN_DEF_WORD(a) if(compareCaseInsensitive(tokenValue, #a) == 0) {tokenType = TokenType::a; return;}
#define TOKEN_DEF_SYMBOL(a,b)
#define TOKEN_DEF_SPECIAL(a,b,c)
#include "token_names.h"
#undef TOKEN_DEF_WORD
#undef TOKEN_DEF_SYMBOL
#undef TOKEN_DEF_SPECIAL
            return;
        }
        struct SymbolStruct final
        {
            std::string symbol;
            TokenType type;
        };
        static const SymbolStruct symbols[] =
        {
#define TOKEN_DEF_WORD(a)
#define TOKEN_DEF_SYMBOL(a,b) {b, TokenType::a},
#define TOKEN_DEF_SPECIAL(a,b,c)
#include "token_names.h"
#undef TOKEN_DEF_WORD
#undef TOKEN_DEF_SYMBOL
#undef TOKEN_DEF_SPECIAL
        };
        tokenValue = "";
        tokenValue += static_cast<char>(get());
        for(;;)
        {
            std::size_t matchCount = 0;
            bool lastWasExactMatch = false;
            for(SymbolStruct symbol : symbols)
            {
                if(symbol.symbol.size() < tokenValue.size())
                    continue;
                if(symbol.symbol.substr(0, tokenValue.size()) == tokenValue)
                {
                    matchCount++;
                    tokenType = symbol.type;
                    lastWasExactMatch = symbol.symbol == tokenValue;
                }
            }
            if(matchCount == 1 && lastWasExactMatch)
                return;
            if(matchCount == 0)
            {
                for(putBack(tokenValue.back()), tokenValue.erase(tokenValue.size() - 1); tokenValue.size() >= 1; putBack(tokenValue.back()), tokenValue.erase(tokenValue.size() - 1))
                {
                    for(SymbolStruct symbol : symbols)
                    {
                        if(symbol.symbol == tokenValue)
                        {
                            tokenType = symbol.type;
                            return;
                        }
                    }
                }
                break;
            }
            tokenValue += static_cast<char>(get());
        }
        throw ParseError("invalid character");
    }
};

#endif // TOKEN_H_INCLUDED
