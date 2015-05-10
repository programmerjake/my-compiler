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
#include "ssa/ssa_node.h"
#ifndef SSA_VISITOR_H_INCLUDED
#define SSA_VISITOR_H_INCLUDED

class SSAUnconditionalJump;
class SSAConditionalJump;
class SSAPhi;
class SSAConstant;
class SSAMove;
class SSALoad;
class SSAStore;
class SSACompare;
class SSAAllocA;
class SSATypeCast;
class SSAAdd;

class SSANodeVisitor
{
public:
    virtual ~SSANodeVisitor() = default;
    virtual void visitSSAUnconditionalJump(std::shared_ptr<SSAUnconditionalJump> node) = 0;
    virtual void visitSSAConditionalJump(std::shared_ptr<SSAConditionalJump> node) = 0;
    virtual void visitSSAPhi(std::shared_ptr<SSAPhi> node) = 0;
    virtual void visitSSAConstant(std::shared_ptr<SSAConstant> node) = 0;
    virtual void visitSSAMove(std::shared_ptr<SSAMove> node) = 0;
    virtual void visitSSALoad(std::shared_ptr<SSALoad> node) = 0;
    virtual void visitSSAStore(std::shared_ptr<SSAStore> node) = 0;
    virtual void visitSSACompare(std::shared_ptr<SSACompare> node) = 0;
    virtual void visitSSAAllocA(std::shared_ptr<SSAAllocA> node) = 0;
    virtual void visitSSATypeCast(std::shared_ptr<SSATypeCast> node) = 0;
    virtual void visitSSAAdd(std::shared_ptr<SSAAdd> node) = 0;
};

#endif // SSA_VISITOR_H_INCLUDED
