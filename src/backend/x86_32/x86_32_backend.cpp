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
#include "backend/x86_32/x86_32_backend.h"
#include "backend/x86_32/x86_32_asm_nodes.h"
#include "backend/x86_32/x86_32_rtl_to_asm.h"
#include "backend/x86_32/x86_32_asm_writer.h"
#include "backend/x86_32/x86_32_register_allocator.h"

void BackendX86_32::outputAsAssembly(std::ostream &os, std::list<std::shared_ptr<RTLFunction>> functionsIn) const
{
    std::list<std::shared_ptr<X86_32AsmFunction>> functions = X86_32ConvertRTLToAsm::run(functionsIn);
    functionsIn.clear();
    X86_32RegisterAllocator ra;
    for(std::shared_ptr<X86_32AsmFunction> function : functions)
    {
        ra.visitX86_32AsmFunction(function);
    }
    switch(assemblyDialect)
    {
    case AssemblyDialect::GAS_Intel:
        X86_32AsmWriter_GAS_Intel::run(os, functions);
        return;
    case AssemblyDialect::FASM:
        throw NotImplementedException("writing fasm is not implemented");
    case AssemblyDialect::GAS_AT_T:
        throw NotImplementedException("writing gas AT&T is not implemented");
    }
}

TypeProperties BackendX86_32::getTypeProperties(std::shared_ptr<TypeNode> type) const
{
    struct MyVisitor final : public TypeVisitor
    {
        TypeProperties retval;
        virtual void visitTypeConstant(std::shared_ptr<TypeConstant> node) override
        {
            node->toNonConstant()->visit(*this);
        }
        virtual void visitTypeVolatile(std::shared_ptr<TypeVolatile> node) override
        {
            node->toNonVolatile()->visit(*this);
        }
        virtual void visitTypeVoid(std::shared_ptr<TypeVoid> node) override
        {
            retval.alignment = 1;
            retval.size = 1;
        }
        virtual void visitTypeBoolean(std::shared_ptr<TypeBoolean> node) override
        {
            retval.alignment = 1;
            retval.size = 1;
        }
        virtual void visitTypePointer(std::shared_ptr<TypePointer> node) override
        {
            retval.alignment = 4;
            retval.size = 4;
        }
    };
    MyVisitor v;
    type->visit(v);
    return v.retval;
}
