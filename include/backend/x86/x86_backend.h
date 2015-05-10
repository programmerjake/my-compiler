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
#ifndef X86_BACKEND_H_INCLUDED
#define X86_BACKEND_H_INCLUDED

#include "backend/backend.h"
#include <cassert>

class BackendX86 final : public Backend
{
public:
    enum AssemblyDialect
    {
        GAS_Intel,
        GAS_AT_T,
        FASM,
    };
    const AssemblyDialect assemblyDialect;
    enum Architecture
    {
        X86_32,
        X86_64,
    };
    const Architecture architecture;
    explicit BackendX86(AssemblyDialect assemblyDialect, Architecture architecture)
        : assemblyDialect(assemblyDialect), architecture(architecture)
    {
    }
    virtual void outputAsAssembly(std::ostream &os, std::list<std::shared_ptr<RTLFunction>> functions) const override;
    virtual TypeProperties getTypeProperties(std::shared_ptr<TypeNode> type) const override;
    virtual IntegerWidth getNativeIntegerWidth() const override
    {
        switch(architecture)
        {
        case Architecture::X86_32:
            return IntegerWidth::Int32;
        case Architecture::X86_64:
            return IntegerWidth::Int64;
        }
        assert(false);
        return IntegerWidth::Int32;
    }
};

#endif // X86_BACKEND_H_INCLUDED
