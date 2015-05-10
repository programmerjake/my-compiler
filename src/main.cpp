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
#include <iostream>
#include "dump.h"
#include "ssa/ssa_nodes.h"
#include "types/types.h"
#include "values/values.h"
#include "parser/parser.h"
#include <sstream>
#include <fstream>
#include "optimization/const_dead_code/const_dead_code.h"
#include "optimization/phi_removal/phi_removal.h"
#include "optimization/control_flow_simplification/control_flow_simplification.h"
#include "convert_ssa_to_rtl.h"
#include "backend/backend.h"
#include "backend/x86/x86_backend.h"
#include "optimization/memory_to_register/memory_to_register.h"
#include <getopt.h>

std::string getSourceCode()
{
    return
        "for(boolean x = true, y = true, z = true; z; z = y, y = x, x = false)\n"
        "{\n"
        "    volatile int v = cast(int, 0);\n"
        "    volatile int v2 = cast(int, 1);\n"
        "    volatile int *a = &v;\n"
        "    if(x)\n"
        "        a = &v2;\n"
        "    *a = cast(int, 1) + cast(int, 2);\n"
        "}\n"
        "for(int i = cast(int, 0); i < cast(int, 10); i = i + 1)\n"
        "{\n"
        "}\n"
        "";
}

struct ArchitectureDescriptor final
{
    const char *name;
    std::shared_ptr<Backend> (*backendMaker)();
};

const ArchitectureDescriptor architectures[] =
{
    {"x86_64", []()->std::shared_ptr<Backend>
        {
            return std::make_shared<BackendX86>(BackendX86::AssemblyDialect::GAS_Intel, BackendX86::X86_64);
        }
    },
    {"x86_32", []()->std::shared_ptr<Backend>
        {
            return std::make_shared<BackendX86>(BackendX86::AssemblyDialect::GAS_Intel, BackendX86::X86_32);
        }
    },
};

int usage(bool isError)
{
    std::ostream *pos = &std::cout;
    if(isError)
        pos = &std::cerr;
    *pos << "Usage : my-compiler [options] [-|<input_file>]\n"
        "Options:\n"
        "-h|--help                       show this help.\n"
        "-a <arch>|--arch=<arch>         use the specified architecture.\n"
        "\n"
        "Architectures:\n";
    const char *seperator = "";
    for(const ArchitectureDescriptor &arch : architectures)
    {
        *pos << seperator << arch.name;
        seperator = " ";
    }
    *pos << std::endl;
    if(isError)
        return 1;
    return 0;
}

int usageAndError(std::string msg)
{
    std::cerr << msg << "\n";
    return usage(true);
}

int main(int argc, char **argv)
{
    std::shared_ptr<Backend> backend;
    std::shared_ptr<CompilerContext> context;
    std::shared_ptr<SSAFunction> fn;
    try
    {
        std::istringstream is(getSourceCode());
        std::istream *pis = &is;
        std::string archName = "";
        bool gotArch = false;
        for(;;)
        {
            static const option longOptions[] =
            {
                {"help", no_argument, nullptr, 'h'},
                {"arch", required_argument, nullptr, 'a'},
                {nullptr, 0, nullptr, 0}
            };
            int longOptionIndex = -1;
            int c = getopt_long(argc, argv, "ha:", longOptions, &longOptionIndex);
            if(c == -1)
                break;
            switch(c)
            {
            case 'h':
                return usage(false);
            case 'a':
                if(gotArch)
                {
                    return usageAndError("too many --arch options");
                }
                archName = optarg;
                gotArch = true;
                break;
            default:
                return usageAndError("invalid option");
            }
        }
        std::string fileName = "";
        if(optind < argc)
        {
            fileName = argv[optind];
            if(argc - optind > 1)
                return usageAndError("too many input files");
        }
        if(archName == "")
            archName = architectures[0].name;
        for(const ArchitectureDescriptor &arch : architectures)
        {
            if(archName == arch.name)
            {
                backend = arch.backendMaker();
                break;
            }
        }
        if(backend == nullptr)
        {
            return usageAndError("invalid architecture");
        }
        context = std::make_shared<CompilerContext>(backend.get());
        if(fileName == "")
            pis = &is;
        else if(fileName == "-")
            pis = &std::cin;
        else
        {
            pis = new std::ifstream(fileName.c_str());
            if(!*pis)
            {
                return usageAndError("can't open input file");
            }
        }
        fn = parse(context.get(), *pis, pis != &std::cin);
    }
    catch(ParseError &e)
    {
        std::cerr << "\nParse Error : " << e.what() << std::endl;
        return 1;
    }
    std::cout << std::endl << std::endl;
    DumpVisitor(std::cout).visitSSAFunction(fn);
    std::cout << std::endl << std::endl;
    fn->verify();
    for(std::size_t i = 0; i < 3; i++)
    {
        MemoryToRegister().visitSSAFunction(fn);
        ConstructBasicBlockGraphVisitor().visitSSAFunction(fn);
        fn->verify();
        PhiRemoval().visitSSAFunction(fn);
        ConstructBasicBlockGraphVisitor().visitSSAFunction(fn);
        fn->verify();
        ConstantPropagationAndDeadCodeElimination().visitSSAFunction(fn);
        ConstructBasicBlockGraphVisitor().visitSSAFunction(fn);
        fn->verify();
    }
    std::shared_ptr<RTLFunction> rtlFn = ConvertSSAToRTL().visitSSAFunction(fn);
    ConstantPropagationAndDeadCodeElimination().visitRTLFunction(rtlFn);
    backend->outputAsAssembly(std::cout, std::list<std::shared_ptr<RTLFunction>>{rtlFn});
    return 0;
}
