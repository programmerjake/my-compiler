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
TOKEN_DEF_WORD(Boolean)
TOKEN_DEF_WORD(Void)
TOKEN_DEF_WORD(Constant)
TOKEN_DEF_WORD(Volatile)
TOKEN_DEF_SYMBOL(LBrace,"{")
TOKEN_DEF_SYMBOL(RBrace,"}")
TOKEN_DEF_SYMBOL(LParen,"(")
TOKEN_DEF_SYMBOL(RParen,")")
TOKEN_DEF_SYMBOL(Star,"*")
TOKEN_DEF_SYMBOL(Ampersand,"&")
TOKEN_DEF_WORD(If)
TOKEN_DEF_WORD(Else)
TOKEN_DEF_WORD(While)
TOKEN_DEF_WORD(Do)
TOKEN_DEF_WORD(For)
TOKEN_DEF_WORD(Break)
TOKEN_DEF_WORD(Continue)
TOKEN_DEF_WORD(UInt8)
TOKEN_DEF_WORD(Int8)
TOKEN_DEF_WORD(UInt16)
TOKEN_DEF_WORD(Int16)
TOKEN_DEF_WORD(UInt32)
TOKEN_DEF_WORD(Int32)
TOKEN_DEF_WORD(UInt64)
TOKEN_DEF_WORD(Int64)
TOKEN_DEF_WORD(UInt)
TOKEN_DEF_WORD(Int)
TOKEN_DEF_WORD(Cast)
TOKEN_DEF_SYMBOL(Colon,":")
TOKEN_DEF_WORD(GoTo)
TOKEN_DEF_SYMBOL(Semicolon,";")
TOKEN_DEF_SYMBOL(Plus,"+")
TOKEN_DEF_SYMBOL(Equal,"=")
TOKEN_DEF_SYMBOL(EqualEqual,"==")
TOKEN_DEF_SYMBOL(NotEqual,"!=")
TOKEN_DEF_SYMBOL(LessEqual,"<=")
TOKEN_DEF_SYMBOL(GreaterEqual,">=")
TOKEN_DEF_SYMBOL(Less,"<")
TOKEN_DEF_SYMBOL(Greater,">")
TOKEN_DEF_SYMBOL(Comma,",")
TOKEN_DEF_SPECIAL(Identifier,"<Id>",tokenValue)
TOKEN_DEF_SPECIAL(IntValue,"<Integer>",tokenValue)
TOKEN_DEF_SPECIAL(FloatValue,"<Float>",tokenValue)
TOKEN_DEF_SPECIAL(EndOfFile,"<EOF>","<EOF>")
TOKEN_DEF_WORD(False)
TOKEN_DEF_WORD(True)
TOKEN_DEF_WORD(Null)
