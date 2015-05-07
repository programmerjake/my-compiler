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
#ifndef X86_64_ASM_NODE_H_INCLUDED
#define X86_64_ASM_NODE_H_INCLUDED

#include <memory>
#include <string>
#include <unordered_set>
#include <list>
#include "context.h"
#include "types/types.h"
#include "values/values.h"
#include <cassert>
#include <sstream>
#include <initializer_list>
#include <vector>
#include <cstdint>
#include "util/random_access_list.h"
#include "util/variable.h"

class X86_64AsmRegister final : public std::enable_shared_from_this<X86_64AsmRegister>
{
    X86_64AsmRegister(const X86_64AsmRegister &) = delete;
    X86_64AsmRegister &operator =(const X86_64AsmRegister &) = delete;
public:
    CompilerContext *const context;
    enum class RegisterType
    {
        Virtual,
        Physical,
    };
    const RegisterType registerType;
    const std::string name;
    struct PhysicalRegisterKindMask final
    {
        bool int8 : 1;
        bool int16 : 1;
        bool int32 : 1;
        bool int64 : 1;
        bool float32 : 1;
        bool float64 : 1;
        constexpr PhysicalRegisterKindMask(bool int8,
                                            bool int16,
                                            bool int32,
                                            bool int64,
                                            bool float32,
                                            bool float64)
            : int8(int8),
              int16(int16),
              int32(int32),
              int64(int64),
              float32(float32),
              float64(float64)
        {
        }
        constexpr PhysicalRegisterKindMask()
            : int8(false),
              int16(false),
              int32(false),
              int64(false),
              float32(false),
              float64(false)
        {
        }
        static constexpr PhysicalRegisterKindMask None() {return PhysicalRegisterKindMask(false, false, false, false, false, false);}
        static constexpr PhysicalRegisterKindMask All() {return PhysicalRegisterKindMask(true, true, true, true, true, true);}
        static constexpr PhysicalRegisterKindMask Int() {return PhysicalRegisterKindMask(true, true, true, true, false, false);}
        static constexpr PhysicalRegisterKindMask Float() {return PhysicalRegisterKindMask(false, false, false, false, true, true);}
        static constexpr PhysicalRegisterKindMask Int8() {return PhysicalRegisterKindMask(true, false, false, false, false, false);}
        static constexpr PhysicalRegisterKindMask Int16() {return PhysicalRegisterKindMask(false, true, false, false, false, false);}
        static constexpr PhysicalRegisterKindMask Int32() {return PhysicalRegisterKindMask(false, false, true, false, false, false);}
        static constexpr PhysicalRegisterKindMask Int64() {return PhysicalRegisterKindMask(false, false, false, true, false, false);}
        static constexpr PhysicalRegisterKindMask Float32() {return PhysicalRegisterKindMask(false, false, false, false, true, false);}
        static constexpr PhysicalRegisterKindMask Float64() {return PhysicalRegisterKindMask(false, false, false, false, false, true);}
        constexpr PhysicalRegisterKindMask operator |(PhysicalRegisterKindMask rt) const
        {
            return PhysicalRegisterKindMask(int8 | rt.int8,
                                            int16 | rt.int16,
                                            int32 | rt.int32,
                                            int64 | rt.int64,
                                            float32 | rt.float32,
                                            float64 | rt.float64);
        }
        constexpr PhysicalRegisterKindMask operator &(PhysicalRegisterKindMask rt) const
        {
            return PhysicalRegisterKindMask(int8 & rt.int8,
                                            int16 & rt.int16,
                                            int32 & rt.int32,
                                            int64 & rt.int64,
                                            float32 & rt.float32,
                                            float64 & rt.float64);
        }
        explicit constexpr operator bool() const
        {
            return int8 | int16 | int32 | int64 | float32 | float64;
        }
        constexpr bool operator !() const
        {
            return !operator bool();
        }
        constexpr PhysicalRegisterKindMask operator ~() const
        {
            return PhysicalRegisterKindMask(~int8,
                                            ~int16,
                                            ~int32,
                                            ~int64,
                                            ~float32,
                                            ~float64);
        }
        constexpr bool operator ==(PhysicalRegisterKindMask rt) const
        {
            return int8 == rt.int8 && int16 == rt.int16 && int32 == rt.int32 && int64 == rt.int64 && float32 == rt.float32 && float64 == rt.float64;
        }
        constexpr bool operator !=(PhysicalRegisterKindMask rt) const
        {
            return !operator ==(rt);
        }
        std::size_t getHash() const
        {
            std::size_t retval = 0;
            for(bool v : {int8, int16, int32, int64, float32, float64})
            {
                retval *= 2;
                if(v)
                    retval++;
            }
            return retval;
        }
        std::uint64_t getSpillSize() const
        {
            if(float64 || int64)
                return 8;
            if(float32 || int32)
                return 4;
            if(int16)
                return 2;
            if(int8)
                return 1;
            throw std::logic_error("getSpillSize called on X86_64AsmRegister::PhysicalRegisterKindMask::None()");
        }
        std::uint64_t getSaveSize() const
        {
            if(float32 || float64)
                return 16;
            if(int64)
                return 8;
            if(int32)
                return 4;
            if(int16)
                return 2;
            if(int8)
                return 1;
            throw std::logic_error("getSaveSize called on X86_64AsmRegister::PhysicalRegisterKindMask::None()");
        }
        std::uint64_t getSpillAlignment() const
        {
            return getSpillSize();
        }
        std::uint64_t getSaveAlignment() const
        {
            return getSaveSize();
        }
        SpillLocation createSpillLocation(std::uint64_t &localsSize) const
        {
            std::uint64_t spillSize = getSpillSize();
            std::uint64_t spillAlignment = getSpillAlignment();
            localsSize += ((spillAlignment - localsSize % spillAlignment) % spillAlignment);
            std::uint64_t retval = localsSize;
            localsSize += spillSize;
            return SpillLocation(SpillLocation::Kind::LocalVariable, retval, TypeProperties(spillAlignment, spillSize));
        }
        std::uint64_t createSaveLocation(std::uint64_t &localsSize) const
        {
            std::uint64_t saveSize = getSpillSize();
            std::uint64_t saveAlignment = getSpillAlignment();
            localsSize += ((saveAlignment - localsSize % saveAlignment) % saveAlignment);
            std::uint64_t retval = localsSize;
            localsSize += saveSize;
            return retval;
        }
    };
    const PhysicalRegisterKindMask physicalRegisterKindMask;
    SpillLocation spillLocation;
private:
    std::vector<std::shared_ptr<X86_64AsmRegister>> interferenceSet;
    std::shared_ptr<X86_64AsmRegister> saveRegister;
public:
    const std::vector<std::shared_ptr<X86_64AsmRegister>> &getPhysicalRegisterInterferenceSet() const
    {
        return interferenceSet;
    }
    std::shared_ptr<X86_64AsmRegister> getSaveRegister() const
    {
        return saveRegister;
    }
    const bool isSpecialPurpose; /// if this register should not be picked by the register allocator
    const bool isCalleeSave;
private:
    struct constructTag final
    {
    };
public:
    X86_64AsmRegister(CompilerContext *context, RegisterType registerType, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave, constructTag)
        : context(context), registerType(registerType), name(name), physicalRegisterKindMask(physicalRegisterKindMask), isSpecialPurpose(isSpecialPurpose), isCalleeSave(isCalleeSave)
    {
    }
private:
    typedef std::unordered_map<std::string, std::shared_ptr<X86_64AsmRegister>> VirtualRegisterMapType;
    static VirtualRegisterMapType &getVirtualRegisterMap(CompilerContext *context)
    {
        struct tag_t
        {
        };
        std::shared_ptr<VirtualRegisterMapType> retval = context->getValue<VirtualRegisterMapType, tag_t>();
        if(retval == nullptr)
            context->setValue<VirtualRegisterMapType, tag_t>(retval = std::make_shared<VirtualRegisterMapType>());
        return *retval;
    }
    typedef std::unordered_map<std::string, std::shared_ptr<X86_64AsmRegister>> PhysicalRegisterMapType;
    static PhysicalRegisterMapType &getPhysicalRegisterMap(CompilerContext *context)
    {
        struct tag_t
        {
        };
        std::shared_ptr<PhysicalRegisterMapType> retval = context->getValue<PhysicalRegisterMapType, tag_t>();
        if(retval == nullptr)
            context->setValue<PhysicalRegisterMapType, tag_t>(retval = std::make_shared<PhysicalRegisterMapType>());
        return *retval;
    }
public:
    static std::shared_ptr<X86_64AsmRegister> getVirtualRegister(CompilerContext *context, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, SpillLocation spillLocation)
    {
        std::shared_ptr<X86_64AsmRegister> &retval = getVirtualRegisterMap(context)[name];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86_64AsmRegister>(context, RegisterType::Virtual, name, physicalRegisterKindMask, false, false, constructTag());
            retval->spillLocation = spillLocation;
        }
        return retval;
    }
private:
    static std::shared_ptr<X86_64AsmRegister> makePhysicalRegister(CompilerContext *context, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86_64AsmRegister> &retval = getPhysicalRegisterMap(context)[name];
        if(retval == nullptr)
            retval = std::make_shared<X86_64AsmRegister>(context, RegisterType::Physical, name, physicalRegisterKindMask, isSpecialPurpose, isCalleeSave, constructTag());
        return retval;
    }
    static void constructAndAddIntegerPhysicalRegisters(CompilerContext *context, std::shared_ptr<std::vector<std::shared_ptr<X86_64AsmRegister>>> retval, std::string name64, std::string name32, std::string name16, std::string name8, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86_64AsmRegister> r64 = makePhysicalRegister(context, name64, PhysicalRegisterKindMask::Int64(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_64AsmRegister> r32 = makePhysicalRegister(context, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_64AsmRegister> r16 = makePhysicalRegister(context, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_64AsmRegister> r8 = makePhysicalRegister(context, name8, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);

        r64->saveRegister = r64;
        r32->saveRegister = r64;
        r16->saveRegister = r64;
        r8->saveRegister = r64;

        r64->interferenceSet.push_back(r32);
        r64->interferenceSet.push_back(r16);
        r64->interferenceSet.push_back(r8);

        r32->interferenceSet.push_back(r64);
        r32->interferenceSet.push_back(r16);
        r32->interferenceSet.push_back(r8);

        r16->interferenceSet.push_back(r64);
        r16->interferenceSet.push_back(r32);
        r16->interferenceSet.push_back(r8);

        r8->interferenceSet.push_back(r64);
        r8->interferenceSet.push_back(r32);
        r8->interferenceSet.push_back(r16);

        retval->push_back(r64);
        retval->push_back(r32);
        retval->push_back(r16);
        retval->push_back(r8);
    }
public:
    static const std::vector<std::shared_ptr<X86_64AsmRegister>> &getPhysicalRegisters(CompilerContext *context)
    {
        struct tag_t
        {
        };
        auto retval = context->getValue<std::vector<std::shared_ptr<X86_64AsmRegister>>, tag_t>();
        if(retval == nullptr)
        {
            retval = std::make_shared<std::vector<std::shared_ptr<X86_64AsmRegister>>>();
            context->setValue<std::vector<std::shared_ptr<X86_64AsmRegister>>, tag_t>(retval);
            for(char nameMiddle : {'a', 'c', 'd', 'b'}) // we don't use ah through dh becuase they can't be used in all instructions because they conflict with the REX (64-bit override) prefix
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        std::string("r") + nameMiddle + "x",
                                                        std::string("e") + nameMiddle + "x",
                                                        nameMiddle + std::string("x"),
                                                        nameMiddle + std::string("l"),
                                                        false, nameMiddle == 'b');
            }
            for(std::string baseName : {"si", "di"})
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        "r" + baseName,
                                                        "e" + baseName,
                                                        baseName,
                                                        baseName + "l",
                                                        false, false);
            }
            for(std::string baseName : {"sp", "bp"})
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        "r" + baseName,
                                                        "e" + baseName,
                                                        baseName,
                                                        baseName + "l",
                                                        true, true);
            }
            for(int i = 8; i < 16; i++)
            {
                std::ostringstream ss;
                ss << "r" << i;
                std::string baseName = ss.str();
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        baseName,
                                                        baseName + "d",
                                                        baseName + "w",
                                                        baseName + "b",
                                                        false, i >= 12);
            }
            for(int i = 0; i < 16; i++)
            {
                std::ostringstream ss;
                ss << "xmm" << i;
                std::string name = ss.str();
                std::shared_ptr<X86_64AsmRegister> r = makePhysicalRegister(context, name, PhysicalRegisterKindMask::Float64() | PhysicalRegisterKindMask::Float32(), false, false);
                r->saveRegister = r;
                retval->push_back(r);
            }
        }
        return *retval;
    }
    static std::shared_ptr<X86_64AsmRegister> getPhysicalRegister(CompilerContext *context, std::string name)
    {
        struct tag_t
        {
        };
        auto registerMap = context->getValue<std::unordered_map<std::string, std::shared_ptr<X86_64AsmRegister>>, tag_t>();
        if(registerMap == nullptr)
        {
            registerMap = std::make_shared<std::unordered_map<std::string, std::shared_ptr<X86_64AsmRegister>>>();
            context->setValue<std::unordered_map<std::string, std::shared_ptr<X86_64AsmRegister>>, tag_t>(registerMap);
            const std::vector<std::shared_ptr<X86_64AsmRegister>> &physicalRegisters = getPhysicalRegisters(context);
            for(std::shared_ptr<X86_64AsmRegister> r : physicalRegisters)
            {
                registerMap->emplace(r->name, r);
            }
        }
        return registerMap->at(name);
    }
};

namespace std
{
template <>
struct hash<X86_64AsmRegister::PhysicalRegisterKindMask> final
{
    std::size_t operator ()(X86_64AsmRegister::PhysicalRegisterKindMask v) const
    {
        return v.getHash();
    }
};
}

class X86_64TypeToPhysicalRegisterKindMask final : public TypeVisitor
{
    X86_64TypeToPhysicalRegisterKindMask(const X86_64TypeToPhysicalRegisterKindMask &) = delete;
    X86_64TypeToPhysicalRegisterKindMask &operator =(const X86_64TypeToPhysicalRegisterKindMask &) = delete;
private:
    bool isGood = true, isEmpty = true;
    bool isFloatingPoint = false;
    std::size_t sizeInBytes = 0;
    X86_64TypeToPhysicalRegisterKindMask()
    {
    }
    X86_64AsmRegister::PhysicalRegisterKindMask getMask() const
    {
        if(isEmpty || !isGood)
            throw std::runtime_error("invalid type mask");
        switch(sizeInBytes)
        {
        case 1:
            if(isFloatingPoint)
                break;
            return X86_64AsmRegister::PhysicalRegisterKindMask::Int8();
        case 2:
            if(isFloatingPoint)
                break;
            return X86_64AsmRegister::PhysicalRegisterKindMask::Int16();
        case 4:
            if(isFloatingPoint)
                return X86_64AsmRegister::PhysicalRegisterKindMask::Float32();
            return X86_64AsmRegister::PhysicalRegisterKindMask::Int32();
        case 8:
            if(isFloatingPoint)
                return X86_64AsmRegister::PhysicalRegisterKindMask::Float64();
            return X86_64AsmRegister::PhysicalRegisterKindMask::Int64();
        }
        throw std::runtime_error("invalid type mask");
    }
    X86_64TypeToPhysicalRegisterKindMask &visit(std::shared_ptr<TypeNode> node)
    {
        node->visit(*this);
        return *this;
    }
public:
    virtual void visitTypeConstant(std::shared_ptr<TypeConstant> node) override
    {
        visit(node->toNonConstant());
    }
    virtual void visitTypeVolatile(std::shared_ptr<TypeVolatile> node) override
    {
        visit(node->toNonVolatile());
    }
    virtual void visitTypeVoid(std::shared_ptr<TypeVoid> node) override
    {
        isGood = false;
    }
    virtual void visitTypeBoolean(std::shared_ptr<TypeBoolean> node) override
    {
        isGood = isGood && isEmpty;
        isEmpty = false;
        sizeInBytes += 1;
    }
    virtual void visitTypePointer(std::shared_ptr<TypePointer> node) override
    {
        isGood = isGood && isEmpty;
        isEmpty = false;
        sizeInBytes += 8;
    }
    static X86_64AsmRegister::PhysicalRegisterKindMask run(std::shared_ptr<TypeNode> type)
    {
        return X86_64TypeToPhysicalRegisterKindMask().visit(type).getMask();
    }
};

class X86_64AsmNodeVisitor;

class X86_64AsmNode : public std::enable_shared_from_this<X86_64AsmNode>
{
    X86_64AsmNode(const X86_64AsmNode &) = delete;
    X86_64AsmNode &operator =(const X86_64AsmNode &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmNode(CompilerContext *context)
        : context(context)
    {
    }
    virtual ~X86_64AsmNode() = default;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const = 0;
    virtual void visit(X86_64AsmNodeVisitor &visitor) = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) = 0;
};

class X86_64AsmBasicBlock;

class X86_64AsmControlTransfer : public X86_64AsmNode
{
public:
    explicit X86_64AsmControlTransfer(CompilerContext *context)
        : X86_64AsmNode(context)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const = 0;
    virtual void visit(X86_64AsmNodeVisitor &visitor) = 0;
};

class X86_64AsmBasicBlock final : public std::enable_shared_from_this<X86_64AsmBasicBlock>
{
    X86_64AsmBasicBlock(const X86_64AsmBasicBlock &) = delete;
    X86_64AsmBasicBlock &operator =(const X86_64AsmBasicBlock &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmBasicBlock(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_64AsmControlTransfer> controlTransferInstruction;
    typedef random_access_list<std::shared_ptr<X86_64AsmNode>> InstructionList;
    InstructionList instructions;
    std::list<std::weak_ptr<X86_64AsmBasicBlock>> sourceBlocks;
    std::list<std::weak_ptr<X86_64AsmBasicBlock>> destBlocks;
    std::unordered_set<std::shared_ptr<X86_64AsmRegister>> usedRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86_64AsmRegister>> assignedRegisters;
    std::unordered_set<std::shared_ptr<X86_64AsmRegister>> liveRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86_64AsmRegister>> liveRegistersAtEnd;
};

class X86_64AsmFunction final : public std::enable_shared_from_this<X86_64AsmFunction>
{
    X86_64AsmFunction(const X86_64AsmFunction &) = delete;
    X86_64AsmFunction &operator =(const X86_64AsmFunction &) = delete;
public:
    CompilerContext *const context;
    explicit X86_64AsmFunction(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_64AsmBasicBlock> startBlock;
    std::list<std::shared_ptr<X86_64AsmBasicBlock>> blocks;
    std::uint64_t localVariablesSize = 0;
};

class X86_64AsmNodeJump;
class X86_64AsmNodeCompareAgainstConstantAndJump;
class X86_64AsmNodeMove;
class X86_64AsmNodeLoadConstant;
class X86_64AsmNodeLoad;
class X86_64AsmNodeStore;
class X86_64AsmNodeLoadLocal;
class X86_64AsmNodeStoreLocal;
class X86_64AsmNodeCompare;

class X86_64AsmNodeVisitor
{
public:
    virtual void visitX86_64AsmNodeJump(std::shared_ptr<X86_64AsmNodeJump> node) = 0;
    virtual void visitX86_64AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_64AsmNodeCompareAgainstConstantAndJump> node) = 0;
    virtual void visitX86_64AsmNodeMove(std::shared_ptr<X86_64AsmNodeMove> node) = 0;
    virtual void visitX86_64AsmNodeLoadConstant(std::shared_ptr<X86_64AsmNodeLoadConstant> node) = 0;
    virtual void visitX86_64AsmNodeLoad(std::shared_ptr<X86_64AsmNodeLoad> node) = 0;
    virtual void visitX86_64AsmNodeStore(std::shared_ptr<X86_64AsmNodeStore> node) = 0;
    virtual void visitX86_64AsmNodeLoadLocal(std::shared_ptr<X86_64AsmNodeLoadLocal> node) = 0;
    virtual void visitX86_64AsmNodeStoreLocal(std::shared_ptr<X86_64AsmNodeStoreLocal> node) = 0;
    virtual void visitX86_64AsmNodeCompare(std::shared_ptr<X86_64AsmNodeCompare> node) = 0;
};

class X86_64AsmNodeJump final : public X86_64AsmControlTransfer
{
public:
    std::weak_ptr<X86_64AsmBasicBlock> target;
    explicit X86_64AsmNodeJump(std::shared_ptr<X86_64AsmBasicBlock> target)
        : X86_64AsmControlTransfer(target->context), target(target)
    {
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_64AsmBasicBlock>>{target};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeJump(std::static_pointer_cast<X86_64AsmNodeJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
    }
};

enum class X86_64ConditionType
{
    Z,
    NZ,
    S,
    NS,
    O,
    NO,
    C,
    NC,
    B,
    BE,
    AE,
    A,
    L,
    LE,
    GE,
    G,
    E,
    NE,
};

inline X86_64ConditionType X86_64InvertCondition(X86_64ConditionType c)
{
    switch(c)
    {
    case X86_64ConditionType::Z:
        return X86_64ConditionType::NZ;
    case X86_64ConditionType::NZ:
        return X86_64ConditionType::Z;
    case X86_64ConditionType::S:
        return X86_64ConditionType::NS;
    case X86_64ConditionType::NS:
        return X86_64ConditionType::S;
    case X86_64ConditionType::O:
        return X86_64ConditionType::NO;
    case X86_64ConditionType::NO:
        return X86_64ConditionType::O;
    case X86_64ConditionType::C:
        return X86_64ConditionType::NC;
    case X86_64ConditionType::NC:
        return X86_64ConditionType::C;
    case X86_64ConditionType::B:
        return X86_64ConditionType::AE;
    case X86_64ConditionType::BE:
        return X86_64ConditionType::A;
    case X86_64ConditionType::AE:
        return X86_64ConditionType::B;
    case X86_64ConditionType::A:
        return X86_64ConditionType::BE;
    case X86_64ConditionType::L:
        return X86_64ConditionType::GE;
    case X86_64ConditionType::LE:
        return X86_64ConditionType::G;
    case X86_64ConditionType::GE:
        return X86_64ConditionType::L;
    case X86_64ConditionType::G:
        return X86_64ConditionType::GE;
    case X86_64ConditionType::E:
        return X86_64ConditionType::NE;
    case X86_64ConditionType::NE:
        return X86_64ConditionType::E;
    }
    assert(false);
    return X86_64ConditionType::NE;
}

inline std::string X86_64GetJmpName(X86_64ConditionType condition)
{
    switch(condition)
    {
    case X86_64ConditionType::Z:
        return "jz";
    case X86_64ConditionType::NZ:
        return "jnz";
    case X86_64ConditionType::S:
        return "js";
    case X86_64ConditionType::NS:
        return "jns";
    case X86_64ConditionType::O:
        return "jo";
    case X86_64ConditionType::NO:
        return "jno";
    case X86_64ConditionType::C:
        return "jc";
    case X86_64ConditionType::NC:
        return "jnc";
    case X86_64ConditionType::B:
        return "jb";
    case X86_64ConditionType::BE:
        return "jbe";
    case X86_64ConditionType::AE:
        return "jae";
    case X86_64ConditionType::A:
        return "ja";
    case X86_64ConditionType::L:
        return "jl";
    case X86_64ConditionType::LE:
        return "jle";
    case X86_64ConditionType::GE:
        return "jge";
    case X86_64ConditionType::G:
        return "jg";
    case X86_64ConditionType::E:
        return "je";
    case X86_64ConditionType::NE:
        return "jne";
    }
    assert(false);
    return "";
}

inline std::string X86_64GetSetName(X86_64ConditionType condition)
{
    switch(condition)
    {
    case X86_64ConditionType::Z:
        return "setz";
    case X86_64ConditionType::NZ:
        return "setnz";
    case X86_64ConditionType::S:
        return "sets";
    case X86_64ConditionType::NS:
        return "setns";
    case X86_64ConditionType::O:
        return "seto";
    case X86_64ConditionType::NO:
        return "setno";
    case X86_64ConditionType::C:
        return "setc";
    case X86_64ConditionType::NC:
        return "setnc";
    case X86_64ConditionType::B:
        return "setb";
    case X86_64ConditionType::BE:
        return "setbe";
    case X86_64ConditionType::AE:
        return "setae";
    case X86_64ConditionType::A:
        return "seta";
    case X86_64ConditionType::L:
        return "setl";
    case X86_64ConditionType::LE:
        return "setle";
    case X86_64ConditionType::GE:
        return "setge";
    case X86_64ConditionType::G:
        return "setg";
    case X86_64ConditionType::E:
        return "sete";
    case X86_64ConditionType::NE:
        return "setne";
    }
    assert(false);
    return "";
}

class X86_64AsmNodeCompareAgainstConstantAndJump final : public X86_64AsmControlTransfer
{
public:
    std::shared_ptr<X86_64AsmRegister> lhs;
    std::shared_ptr<ValueNode> rhs;
    X86_64ConditionType conditionType;
    std::weak_ptr<X86_64AsmBasicBlock> trueTarget;
    std::weak_ptr<X86_64AsmBasicBlock> falseTarget;
    explicit X86_64AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_64AsmRegister> lhs,
                                                        std::shared_ptr<ValueNode> rhs,
                                                        X86_64ConditionType conditionType,
                                                        std::shared_ptr<X86_64AsmBasicBlock> trueTarget,
                                                        std::shared_ptr<X86_64AsmBasicBlock> falseTarget)
        : X86_64AsmControlTransfer(lhs->context), lhs(lhs), rhs(rhs), conditionType(conditionType), trueTarget(trueTarget), falseTarget(falseTarget)
    {
    }
    virtual std::list<std::weak_ptr<X86_64AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_64AsmBasicBlock>>{trueTarget, falseTarget};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{lhs};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeCompareAgainstConstantAndJump(std::static_pointer_cast<X86_64AsmNodeCompareAgainstConstantAndJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(lhs == originalRegister)
            lhs = newRegister;
    }
};

class X86_64AsmNodeMove final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<X86_64AsmRegister> source;
    explicit X86_64AsmNodeMove(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<X86_64AsmRegister> source)
        : X86_64AsmNode(dest->context), dest(dest), source(source)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{source};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeMove(std::static_pointer_cast<X86_64AsmNodeMove>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(source == originalRegister)
            source = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_64AsmNodeCompare final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<X86_64AsmRegister> lhs;
    std::shared_ptr<X86_64AsmRegister> rhs;
    X86_64ConditionType conditionType;
    explicit X86_64AsmNodeCompare(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<X86_64AsmRegister> lhs, std::shared_ptr<X86_64AsmRegister> rhs, X86_64ConditionType conditionType)
        : X86_64AsmNode(dest->context), dest(dest), lhs(lhs), rhs(rhs), conditionType(conditionType)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{lhs, rhs};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeCompare(std::static_pointer_cast<X86_64AsmNodeCompare>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
        if(lhs == originalRegister)
            lhs = newRegister;
        if(rhs == originalRegister)
            rhs = newRegister;
    }
};

class X86_64AsmNodeLoad final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<X86_64AsmRegister> address;
    explicit X86_64AsmNodeLoad(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<X86_64AsmRegister> address)
        : X86_64AsmNode(dest->context), dest(dest), address(address)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{address};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeLoad(std::static_pointer_cast<X86_64AsmNodeLoad>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_64AsmNodeLoadLocal final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    VariableLocation location;
    explicit X86_64AsmNodeLoadLocal(std::shared_ptr<X86_64AsmRegister> dest, VariableLocation location)
        : X86_64AsmNode(dest->context), dest(dest), location(location)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{X86_64AsmRegister::getPhysicalRegister(context, "rbp")};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeLoadLocal(std::static_pointer_cast<X86_64AsmNodeLoadLocal>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_64AsmNodeStoreLocal final : public X86_64AsmNode
{
public:
    VariableLocation location;
    std::shared_ptr<X86_64AsmRegister> value;
    explicit X86_64AsmNodeStoreLocal(VariableLocation location, std::shared_ptr<X86_64AsmRegister> value)
        : X86_64AsmNode(value->context), location(location), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{X86_64AsmRegister::getPhysicalRegister(context, "rbp"), value};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeStoreLocal(std::static_pointer_cast<X86_64AsmNodeStoreLocal>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86_64AsmNodeStore final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> address;
    std::shared_ptr<X86_64AsmRegister> value;
    explicit X86_64AsmNodeStore(std::shared_ptr<X86_64AsmRegister> address, std::shared_ptr<X86_64AsmRegister> value)
        : X86_64AsmNode(address->context), address(address), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{address, value};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeStore(std::static_pointer_cast<X86_64AsmNodeStore>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86_64AsmNodeLoadConstant final : public X86_64AsmNode
{
public:
    std::shared_ptr<X86_64AsmRegister> dest;
    std::shared_ptr<ValueNode> value;
    explicit X86_64AsmNodeLoadConstant(std::shared_ptr<X86_64AsmRegister> dest, std::shared_ptr<ValueNode> value)
        : X86_64AsmNode(dest->context), dest(dest), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{};
    }
    virtual std::unordered_set<std::shared_ptr<X86_64AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_64AsmRegister>>{dest};
    }
    virtual void visit(X86_64AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_64AsmNodeLoadConstant(std::static_pointer_cast<X86_64AsmNodeLoadConstant>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_64AsmRegister> originalRegister, std::shared_ptr<X86_64AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

#endif // X86_64_ASM_NODE_H_INCLUDED