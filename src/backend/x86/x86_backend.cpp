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
#include "backend/x86/x86_backend.h"
#include "backend/x86/x86_asm_nodes.h"
#include "backend/x86/x86_rtl_to_asm.h"
#include "backend/x86/x86_asm_writer.h"
#include "backend/x86/x86_register_allocator.h"
#include "backend/x86/x86_dead_code.h"

void BackendX86::outputAsAssembly(std::ostream &os, std::list<std::shared_ptr<RTLFunction>> functionsIn) const
{
    std::list<std::shared_ptr<X86AsmFunction>> functions = X86ConvertRTLToAsm::run(functionsIn, this);
    functionsIn.clear();
    X86DeadCodeElimination dce(this);
    for(std::shared_ptr<X86AsmFunction> function : functions)
    {
        dce.visitX86AsmFunction(function);
    }
    X86RegisterAllocator ra(this);
    for(std::shared_ptr<X86AsmFunction> function : functions)
    {
        ra.visitX86AsmFunction(function);
    }
    switch(assemblyDialect)
    {
    case AssemblyDialect::GAS_Intel:
        X86AsmWriter_GAS_Intel::run(os, functions, this);
        return;
    case AssemblyDialect::FASM:
        throw NotImplementedException("writing fasm is not implemented");
    case AssemblyDialect::GAS_AT_T:
        throw NotImplementedException("writing gas AT&T is not implemented");
    }
}

TypeProperties BackendX86::getTypeProperties(std::shared_ptr<TypeNode> type) const
{
    struct MyVisitor final : public TypeVisitor
    {
        TypeProperties retval;
        Architecture architecture;
        MyVisitor(Architecture architecture)
            : architecture(architecture)
        {
        }
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
            switch(architecture)
            {
            case Architecture::X86_32:
                retval.alignment = 4;
                retval.size = 4;
                return;
            case Architecture::X86_64:
                retval.alignment = 8;
                retval.size = 8;
                return;
            }
            assert(false);
        }
        virtual void visitTypeInteger(std::shared_ptr<TypeInteger> node) override
        {
            switch(node->width)
            {
            case IntegerWidth::Int8:
                retval.alignment = 1;
                retval.size = 1;
                return;
            case IntegerWidth::Int16:
                retval.alignment = 2;
                retval.size = 2;
                return;
            case IntegerWidth::Int32:
                retval.alignment = 4;
                retval.size = 4;
                return;
            case IntegerWidth::Int64:
                switch(architecture)
                {
                case Architecture::X86_32:
                    throw std::runtime_error("64-bit integers not implemented on x86_32");
                case Architecture::X86_64:
                    retval.alignment = 8;
                    retval.size = 8;
                    return;
                }
                break;
            case IntegerWidth::IntNativeSize:
                switch(architecture)
                {
                case Architecture::X86_32:
                    retval.alignment = 4;
                    retval.size = 4;
                    return;
                case Architecture::X86_64:
                    retval.alignment = 8;
                    retval.size = 8;
                    return;
                }
                break;
            }
            assert(false);
        }
    };
    MyVisitor v(architecture);
    type->visit(v);
    return v.retval;
}
