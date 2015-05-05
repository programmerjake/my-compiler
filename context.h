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
#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include <memory>
#include <unordered_map>
#include <atomic>
#include "backend/backend.h"

class TypeNode;

class CompilerContext final
{
public:
    std::unordered_multimap<std::size_t, std::shared_ptr<TypeNode>> types;
private:
    std::shared_ptr<TypeNode> constructTypeNodeHelper(std::shared_ptr<TypeNode> retval);
    static std::size_t makeValueIndex()
    {
        static std::atomic_size_t retval(0);
        return retval++;
    }
    std::unordered_map<std::size_t, std::shared_ptr<void>> values;
    template <typename T>
    static std::size_t getValueIndex()
    {
        static std::size_t retval = makeValueIndex();
        return retval;
    }
public:
    template <typename T, typename ...Args>
    std::shared_ptr<T> constructTypeNode(Args &&...args)
    {
        return std::static_pointer_cast<T>(constructTypeNodeHelper(std::shared_ptr<T>(new T(this, std::forward<Args>(args)...))));
    }
    template <typename T>
    void setValue(std::shared_ptr<T> value)
    {
        values[getValueIndex<T>()] = std::static_pointer_cast<void>(value);
    }
    template <typename T>
    std::shared_ptr<T> getValue()
    {
        return std::static_pointer_cast<T>(values[getValueIndex<T>()]);
    }
    const Backend *const backend;
    CompilerContext(const Backend *backend)
        : backend(backend)
    {
    }
};

#endif // CONTEXT_H_INCLUDED
