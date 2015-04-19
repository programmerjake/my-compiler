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

std::shared_ptr<SSAFunction> makeFunction(CompilerContext *context)
{
    auto retval = std::make_shared<SSAFunction>(context);
    retval->startBlock = std::make_shared<SSABasicBlock>(context);
    retval->blocks.push_back(retval->startBlock);
    retval->endBlock = std::make_shared<SSABasicBlock>(context);
    auto loopBlock = std::make_shared<SSABasicBlock>(context);
    retval->blocks.push_back(loopBlock);
    retval->blocks.push_back(retval->endBlock);
    auto startBlockA = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, false));
    retval->startBlock->instructions.push_back(startBlockA);
    retval->startBlock->controlTransferInstruction = std::make_shared<SSAUnconditionalJump>(context, loopBlock);
    retval->startBlock->instructions.push_back(retval->startBlock->controlTransferInstruction);
    auto loopBlockCond = std::make_shared<SSAPhi>(TypeBoolean::make(context));
    loopBlock->instructions.push_back(loopBlockCond);
    auto loopBlockA = std::make_shared<SSAConstant>(std::make_shared<ValueBoolean>(context, true));
    loopBlock->instructions.push_back(loopBlockA);
    loopBlockCond->inputs.push_back(SSAPhi::PhiInput{loopBlockA, loopBlock});
    loopBlockCond->inputs.push_back(SSAPhi::PhiInput{startBlockA, retval->startBlock});
    loopBlock->controlTransferInstruction = std::make_shared<SSAConditionalJump>(context, loopBlockCond, retval->endBlock, loopBlock);
    loopBlock->instructions.push_back(loopBlock->controlTransferInstruction);
    return retval;
}

std::string getSourceCode()
{
    return
R"(boolean e = false;
for(boolean a = true, b = true; a; a = b, b = false)
{
    boolean c = true, d = true;
    while(c)
    {
        c = d;
        d = false;
        do
        {
            if(e)
                d = true;
        }
        while(false);
    }
}
)";
}

int usage(bool isError)
{
    std::ostream *pos = &std::cout;
    if(isError)
        pos = &std::cerr;
    *pos << "Usage : my-compiler [-|<input_file>]" << std::endl;
    if(isError)
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    CompilerContext context;
    std::shared_ptr<SSAFunction> fn;
    try
    {
        std::istringstream is(getSourceCode());
        std::istream *pis = &is;
        if(argc > 1)
        {
            if(argc > 2)
                return usage(true);
            std::string arg = argv[1];
            if(arg == "--help" || arg == "-h")
            {
                return usage(false);
            }
            if(arg == "-")
                pis = &std::cin;
            else
            {
                pis = new std::ifstream(arg.c_str());
                if(!*pis)
                    return usage(true);
            }
        }
        fn = parse(&context, *pis, pis != &std::cin);
    }
    catch(ParseError &e)
    {
        std::cerr << "\nParse Error : " << e.what() << std::endl;
        return 1;
    }
    std::cout << std::endl << std::endl;
    PhiRemoval().visitSSAFunction(fn);
    ConstantPropagationAndDeadCodeElimination().visitSSAFunction(fn);
    ControlFlowSimplification().visitSSAFunction(fn);
    {
        DumpVisitor dumper(std::cout);
        dumper.visitSSAFunction(fn);
    }
    std::cout << std::endl << std::endl;
    return 0;
}
