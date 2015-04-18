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

std::shared_ptr<SSAFunction> makeFunction(CompilerContext *context)
{
    auto retval = std::make_shared<SSAFunction>();
    retval->startBlock = std::make_shared<SSABasicBlock>();
    retval->blocks.push_back(retval->startBlock);
    retval->endBlock = std::make_shared<SSABasicBlock>();
    auto loopBlock = std::make_shared<SSABasicBlock>();
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
R"(
{
    for(boolean a = true, b = true; a; a = b, b = false)
    {
        boolean c = true, d = true;
        while(c)
        {
            c = d;
            d = false;
        }
    }
}
)";
}

int main()
{
    CompilerContext context;
    std::shared_ptr<SSAFunction> fn = makeFunction(&context);
    {
        DumpVisitor dumper(std::cout);
        dumper.visitSSAFunction(fn);
    }
    std::cout << std::endl << std::endl;
    try
    {
        std::istringstream is(getSourceCode());
        fn = parse(&context, is);
    }
    catch(ParseError &e)
    {
        std::cerr << "\nParse Error : " << e.what() << std::endl;
        return 1;
    }
    std::cout << std::endl << std::endl;
    {
        DumpVisitor dumper(std::cout);
        dumper.visitSSAFunction(fn);
    }
    std::cout << std::endl << std::endl;
    return 0;
}
