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
#ifndef RTL_NODE_H_INCLUDED
#define RTL_NODE_H_INCLUDED

#include "../types/type.h"
#include "../values/value.h"
#include "../context.h"
#include <memory>

class RTLRegister final : public std::enable_shared_from_this<RTLNode>
{
public:
    enum class RegisterType
    {
        Virtual,
        Physical,
    };
    CompilerContext *const context;
    const RegisterType registerType;
    const std::string name;
    RTLRegister(CompilerContext *context, RegisterType registerType, std::string name)
        : context(context), registerType(registerType), name(name)
    {
    }
};

class RTLNode : public std::enable_shared_from_this<RTLNode>
{
    RTLNode(const RTLNode &) = delete;
    RTLNode &operator =(const RTLNode &) = delete;
public:
    CompilerContext *const context;
    std::weak_ptr<TypeNode> type;
    RTLNode(CompilerContext *context, std::shared_ptr<TypeNode> type)
        : context(context), type(type)
    {
    }
    virtual ~RTLNode() = default;
    virtual bool operator ==(const RTLNode &rt) const = 0;
    bool operator !=(const RTLNode &rt) const
    {
        return !operator ==(rt);
    }

};

#endif // RTL_NODE_H_INCLUDED
