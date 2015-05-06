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
#ifndef X86_32_ASM_NODE_H_INCLUDED
#define X86_32_ASM_NODE_H_INCLUDED

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

class X86_32AsmRegister final : public std::enable_shared_from_this<X86_32AsmRegister>
{
    X86_32AsmRegister(const X86_32AsmRegister &) = delete;
    X86_32AsmRegister &operator =(const X86_32AsmRegister &) = delete;
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
        bool float32 : 1;
        bool float64 : 1;
        constexpr PhysicalRegisterKindMask(bool int8,
                                            bool int16,
                                            bool int32,
                                            bool float32,
                                            bool float64)
            : int8(int8),
              int16(int16),
              int32(int32),
              float32(float32),
              float64(float64)
        {
        }
        constexpr PhysicalRegisterKindMask()
            : int8(false),
              int16(false),
              int32(false),
              float32(false),
              float64(false)
        {
        }
        static constexpr PhysicalRegisterKindMask None() {return PhysicalRegisterKindMask(false, false, false, false, false);}
        static constexpr PhysicalRegisterKindMask All() {return PhysicalRegisterKindMask(true, true, true, true, true);}
        static constexpr PhysicalRegisterKindMask Int() {return PhysicalRegisterKindMask(true, true, true, false, false);}
        static constexpr PhysicalRegisterKindMask Float() {return PhysicalRegisterKindMask(false, false, false, true, true);}
        static constexpr PhysicalRegisterKindMask Int8() {return PhysicalRegisterKindMask(true, false, false, false, false);}
        static constexpr PhysicalRegisterKindMask Int16() {return PhysicalRegisterKindMask(false, true, false, false, false);}
        static constexpr PhysicalRegisterKindMask Int32() {return PhysicalRegisterKindMask(false, false, true, false, false);}
        static constexpr PhysicalRegisterKindMask Float32() {return PhysicalRegisterKindMask(false, false, false, true, false);}
        static constexpr PhysicalRegisterKindMask Float64() {return PhysicalRegisterKindMask(false, false, false, false, true);}
        constexpr PhysicalRegisterKindMask operator |(PhysicalRegisterKindMask rt) const
        {
            return PhysicalRegisterKindMask(int8 | rt.int8,
                                            int16 | rt.int16,
                                            int32 | rt.int32,
                                            float32 | rt.float32,
                                            float64 | rt.float64);
        }
        constexpr PhysicalRegisterKindMask operator &(PhysicalRegisterKindMask rt) const
        {
            return PhysicalRegisterKindMask(int8 & rt.int8,
                                            int16 & rt.int16,
                                            int32 & rt.int32,
                                            float32 & rt.float32,
                                            float64 & rt.float64);
        }
        explicit constexpr operator bool() const
        {
            return int8 | int16 | int32 | float32 | float64;
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
                                            ~float32,
                                            ~float64);
        }
        constexpr bool operator ==(PhysicalRegisterKindMask rt) const
        {
            return int8 == rt.int8 && int16 == rt.int16 && int32 == rt.int32 && float32 == rt.float32 && float64 == rt.float64;
        }
        constexpr bool operator !=(PhysicalRegisterKindMask rt) const
        {
            return !operator ==(rt);
        }
        std::size_t getHash() const
        {
            std::size_t retval = 0;
            for(bool v : {int8, int16, int32, float32, float64})
            {
                retval *= 2;
                if(v)
                    retval++;
            }
            return retval;
        }
        std::uint64_t getSpillSize() const
        {
            if(float64)
                return 8;
            if(float32 || int32)
                return 4;
            if(int16)
                return 2;
            if(int8)
                return 1;
            throw std::logic_error("getSpillSize called on X86_32AsmRegister::PhysicalRegisterKindMask::None()");
        }
        std::uint64_t getSaveSize() const
        {
            if(float32 || float64)
                return 16;
            if(int32)
                return 4;
            if(int16)
                return 2;
            if(int8)
                return 1;
            throw std::logic_error("getSaveSize called on X86_32AsmRegister::PhysicalRegisterKindMask::None()");
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
            return SpillLocation(SpillLocation::Kind::LocalVariable, retval);
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
    std::vector<std::shared_ptr<X86_32AsmRegister>> interferenceSet;
    std::shared_ptr<X86_32AsmRegister> saveRegister;
public:
    const std::vector<std::shared_ptr<X86_32AsmRegister>> &getPhysicalRegisterInterferenceSet() const
    {
        return interferenceSet;
    }
    std::shared_ptr<X86_32AsmRegister> getSaveRegister() const
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
    X86_32AsmRegister(CompilerContext *context, RegisterType registerType, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave, constructTag)
        : context(context), registerType(registerType), name(name), physicalRegisterKindMask(physicalRegisterKindMask), isSpecialPurpose(isSpecialPurpose), isCalleeSave(isCalleeSave)
    {
    }
private:
    typedef std::unordered_map<std::string, std::shared_ptr<X86_32AsmRegister>> VirtualRegisterMapType;
    static VirtualRegisterMapType &getVirtualRegisterMap(CompilerContext *context)
    {
        std::shared_ptr<VirtualRegisterMapType> retval = context->getValue<VirtualRegisterMapType>();
        if(retval == nullptr)
            context->setValue<VirtualRegisterMapType>(retval = std::make_shared<VirtualRegisterMapType>());
        return *retval;
    }
    typedef std::unordered_map<std::string, std::shared_ptr<X86_32AsmRegister>> PhysicalRegisterMapType;
    static PhysicalRegisterMapType &getPhysicalRegisterMap(CompilerContext *context)
    {
        std::shared_ptr<PhysicalRegisterMapType> retval = context->getValue<PhysicalRegisterMapType>();
        if(retval == nullptr)
            context->setValue<PhysicalRegisterMapType>(retval = std::make_shared<PhysicalRegisterMapType>());
        return *retval;
    }
public:
    static std::shared_ptr<X86_32AsmRegister> getVirtualRegister(CompilerContext *context, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, SpillLocation spillLocation)
    {
        std::shared_ptr<X86_32AsmRegister> &retval = getVirtualRegisterMap(context)[name];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86_32AsmRegister>(context, RegisterType::Virtual, name, physicalRegisterKindMask, false, false, constructTag());
            retval->spillLocation = spillLocation;
        }
        return retval;
    }
private:
    static std::shared_ptr<X86_32AsmRegister> makePhysicalRegister(CompilerContext *context, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86_32AsmRegister> &retval = getPhysicalRegisterMap(context)[name];
        if(retval == nullptr)
            retval = std::make_shared<X86_32AsmRegister>(context, RegisterType::Physical, name, physicalRegisterKindMask, isSpecialPurpose, isCalleeSave, constructTag());
        return retval;
    }
    static void constructAndAddIntegerPhysicalRegisters(CompilerContext *context, std::shared_ptr<std::vector<std::shared_ptr<X86_32AsmRegister>>> retval, std::string name32, std::string name16, std::string name8l, std::string name8h, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86_32AsmRegister> r32 = makePhysicalRegister(context, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_32AsmRegister> r16 = makePhysicalRegister(context, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_32AsmRegister> r8l = makePhysicalRegister(context, name8l, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_32AsmRegister> r8h = makePhysicalRegister(context, name8h, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);

        r32->saveRegister = r32;
        r16->saveRegister = r32;
        r8l->saveRegister = r32;
        r8h->saveRegister = r32;

        r32->interferenceSet.push_back(r16);
        r32->interferenceSet.push_back(r8l);
        r32->interferenceSet.push_back(r8h);

        r16->interferenceSet.push_back(r32);
        r16->interferenceSet.push_back(r8l);
        r16->interferenceSet.push_back(r8h);

        r8l->interferenceSet.push_back(r32);
        r8l->interferenceSet.push_back(r16);

        r8h->interferenceSet.push_back(r32);
        r8h->interferenceSet.push_back(r16);

        retval->push_back(r32);
        retval->push_back(r16);
        retval->push_back(r8l);
        retval->push_back(r8h);
    }
    static void constructAndAddIntegerPhysicalRegisters(CompilerContext *context, std::shared_ptr<std::vector<std::shared_ptr<X86_32AsmRegister>>> retval, std::string name32, std::string name16, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86_32AsmRegister> r32 = makePhysicalRegister(context, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86_32AsmRegister> r16 = makePhysicalRegister(context, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);

        r32->saveRegister = r32;
        r16->saveRegister = r32;

        r32->interferenceSet.push_back(r16);

        r16->interferenceSet.push_back(r32);

        retval->push_back(r32);
        retval->push_back(r16);
    }
public:
    static const std::vector<std::shared_ptr<X86_32AsmRegister>> &getPhysicalRegisters(CompilerContext *context)
    {
        auto retval = context->getValue<std::vector<std::shared_ptr<X86_32AsmRegister>>>();
        if(retval == nullptr)
        {
            retval = std::make_shared<std::vector<std::shared_ptr<X86_32AsmRegister>>>();
            context->setValue<std::vector<std::shared_ptr<X86_32AsmRegister>>>(retval);
            for(char nameMiddle : {'a', 'c', 'd', 'b'})
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        std::string("e") + nameMiddle + "x",
                                                        nameMiddle + std::string("x"),
                                                        nameMiddle + std::string("l"),
                                                        nameMiddle + std::string("h"),
                                                        false, nameMiddle == 'b');
            }
            for(std::string baseName : {"si", "di"})
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        "e" + baseName,
                                                        baseName,
                                                        false, true);
            }
            for(std::string baseName : {"sp", "bp"})
            {
                constructAndAddIntegerPhysicalRegisters(context,
                                                        retval,
                                                        "e" + baseName,
                                                        baseName,
                                                        true, true);
            }
            for(int i = 0; i < 8; i++)
            {
                std::ostringstream ss;
                ss << "xmm" << i;
                std::string name = ss.str();
                std::shared_ptr<X86_32AsmRegister> r = makePhysicalRegister(context, name, PhysicalRegisterKindMask::Float64() | PhysicalRegisterKindMask::Float32(), false, false);
                r->saveRegister = r;
                retval->push_back(r);
            }
        }
        return *retval;
    }
    static std::shared_ptr<X86_32AsmRegister> getPhysicalRegister(CompilerContext *context, std::string name)
    {
        auto registerMap = context->getValue<std::unordered_map<std::string, std::shared_ptr<X86_32AsmRegister>>>();
        if(registerMap == nullptr)
        {
            registerMap = std::make_shared<std::unordered_map<std::string, std::shared_ptr<X86_32AsmRegister>>>();
            context->setValue<std::unordered_map<std::string, std::shared_ptr<X86_32AsmRegister>>>(registerMap);
            const std::vector<std::shared_ptr<X86_32AsmRegister>> &physicalRegisters = getPhysicalRegisters(context);
            for(std::shared_ptr<X86_32AsmRegister> r : physicalRegisters)
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
struct hash<X86_32AsmRegister::PhysicalRegisterKindMask> final
{
    std::size_t operator ()(X86_32AsmRegister::PhysicalRegisterKindMask v) const
    {
        return v.getHash();
    }
};
}

class X86_32TypeToPhysicalRegisterKindMask final : public TypeVisitor
{
    X86_32TypeToPhysicalRegisterKindMask(const X86_32TypeToPhysicalRegisterKindMask &) = delete;
    X86_32TypeToPhysicalRegisterKindMask &operator =(const X86_32TypeToPhysicalRegisterKindMask &) = delete;
private:
    bool isGood = true, isEmpty = true;
    bool isFloatingPoint = false;
    std::size_t sizeInBytes = 0;
    X86_32TypeToPhysicalRegisterKindMask()
    {
    }
    X86_32AsmRegister::PhysicalRegisterKindMask getMask() const
    {
        if(isEmpty || !isGood)
            throw std::runtime_error("invalid type mask");
        switch(sizeInBytes)
        {
        case 1:
            if(isFloatingPoint)
                break;
            return X86_32AsmRegister::PhysicalRegisterKindMask::Int8();
        case 2:
            if(isFloatingPoint)
                break;
            return X86_32AsmRegister::PhysicalRegisterKindMask::Int16();
        case 4:
            if(isFloatingPoint)
                return X86_32AsmRegister::PhysicalRegisterKindMask::Float32();
            return X86_32AsmRegister::PhysicalRegisterKindMask::Int32();
        case 8:
            if(isFloatingPoint)
                return X86_32AsmRegister::PhysicalRegisterKindMask::Float64();
            break;
        }
        throw std::runtime_error("invalid type mask");
    }
    X86_32TypeToPhysicalRegisterKindMask &visit(std::shared_ptr<TypeNode> node)
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
        sizeInBytes += 4;
    }
    static X86_32AsmRegister::PhysicalRegisterKindMask run(std::shared_ptr<TypeNode> type)
    {
        return X86_32TypeToPhysicalRegisterKindMask().visit(type).getMask();
    }
};

class X86_32AsmNodeVisitor;

class X86_32AsmNode : public std::enable_shared_from_this<X86_32AsmNode>
{
    X86_32AsmNode(const X86_32AsmNode &) = delete;
    X86_32AsmNode &operator =(const X86_32AsmNode &) = delete;
public:
    CompilerContext *const context;
    explicit X86_32AsmNode(CompilerContext *context)
        : context(context)
    {
    }
    virtual ~X86_32AsmNode() = default;
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const = 0;
    virtual void visit(X86_32AsmNodeVisitor &visitor) = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) = 0;
};

class X86_32AsmBasicBlock;

class X86_32AsmControlTransfer : public X86_32AsmNode
{
public:
    explicit X86_32AsmControlTransfer(CompilerContext *context)
        : X86_32AsmNode(context)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{};
    }
    virtual std::list<std::weak_ptr<X86_32AsmBasicBlock>> targets() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const = 0;
    virtual void visit(X86_32AsmNodeVisitor &visitor) = 0;
};

class X86_32AsmBasicBlock final : public std::enable_shared_from_this<X86_32AsmBasicBlock>
{
    X86_32AsmBasicBlock(const X86_32AsmBasicBlock &) = delete;
    X86_32AsmBasicBlock &operator =(const X86_32AsmBasicBlock &) = delete;
public:
    CompilerContext *const context;
    explicit X86_32AsmBasicBlock(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_32AsmControlTransfer> controlTransferInstruction;
    typedef random_access_list<std::shared_ptr<X86_32AsmNode>> InstructionList;
    InstructionList instructions;
    std::list<std::weak_ptr<X86_32AsmBasicBlock>> sourceBlocks;
    std::list<std::weak_ptr<X86_32AsmBasicBlock>> destBlocks;
    std::unordered_set<std::shared_ptr<X86_32AsmRegister>> usedRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86_32AsmRegister>> assignedRegisters;
    std::unordered_set<std::shared_ptr<X86_32AsmRegister>> liveRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86_32AsmRegister>> liveRegistersAtEnd;
};

class X86_32AsmFunction final : public std::enable_shared_from_this<X86_32AsmFunction>
{
    X86_32AsmFunction(const X86_32AsmFunction &) = delete;
    X86_32AsmFunction &operator =(const X86_32AsmFunction &) = delete;
public:
    CompilerContext *const context;
    explicit X86_32AsmFunction(CompilerContext *context)
        : context(context)
    {
    }
    std::shared_ptr<X86_32AsmBasicBlock> startBlock;
    std::list<std::shared_ptr<X86_32AsmBasicBlock>> blocks;
    std::uint64_t localVariablesSize = 0;
};

class X86_32AsmNodeJump;
class X86_32AsmNodeCompareAgainstConstantAndJump;
class X86_32AsmNodeMove;
class X86_32AsmNodeLoadConstant;
class X86_32AsmNodeLoad;
class X86_32AsmNodeStore;
class X86_32AsmNodeLoadLocal;
class X86_32AsmNodeStoreLocal;
class X86_32AsmNodeCompare;

class X86_32AsmNodeVisitor
{
public:
    virtual void visitX86_32AsmNodeJump(std::shared_ptr<X86_32AsmNodeJump> node) = 0;
    virtual void visitX86_32AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_32AsmNodeCompareAgainstConstantAndJump> node) = 0;
    virtual void visitX86_32AsmNodeMove(std::shared_ptr<X86_32AsmNodeMove> node) = 0;
    virtual void visitX86_32AsmNodeLoadConstant(std::shared_ptr<X86_32AsmNodeLoadConstant> node) = 0;
    virtual void visitX86_32AsmNodeLoad(std::shared_ptr<X86_32AsmNodeLoad> node) = 0;
    virtual void visitX86_32AsmNodeStore(std::shared_ptr<X86_32AsmNodeStore> node) = 0;
    virtual void visitX86_32AsmNodeLoadLocal(std::shared_ptr<X86_32AsmNodeLoadLocal> node) = 0;
    virtual void visitX86_32AsmNodeStoreLocal(std::shared_ptr<X86_32AsmNodeStoreLocal> node) = 0;
    virtual void visitX86_32AsmNodeCompare(std::shared_ptr<X86_32AsmNodeCompare> node) = 0;
};

class X86_32AsmNodeJump final : public X86_32AsmControlTransfer
{
public:
    std::weak_ptr<X86_32AsmBasicBlock> target;
    explicit X86_32AsmNodeJump(std::shared_ptr<X86_32AsmBasicBlock> target)
        : X86_32AsmControlTransfer(target->context), target(target)
    {
    }
    virtual std::list<std::weak_ptr<X86_32AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_32AsmBasicBlock>>{target};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeJump(std::static_pointer_cast<X86_32AsmNodeJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
    }
};

enum class X86_32ConditionType
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

inline X86_32ConditionType X86_32InvertCondition(X86_32ConditionType c)
{
    switch(c)
    {
    case X86_32ConditionType::Z:
        return X86_32ConditionType::NZ;
    case X86_32ConditionType::NZ:
        return X86_32ConditionType::Z;
    case X86_32ConditionType::S:
        return X86_32ConditionType::NS;
    case X86_32ConditionType::NS:
        return X86_32ConditionType::S;
    case X86_32ConditionType::O:
        return X86_32ConditionType::NO;
    case X86_32ConditionType::NO:
        return X86_32ConditionType::O;
    case X86_32ConditionType::C:
        return X86_32ConditionType::NC;
    case X86_32ConditionType::NC:
        return X86_32ConditionType::C;
    case X86_32ConditionType::B:
        return X86_32ConditionType::AE;
    case X86_32ConditionType::BE:
        return X86_32ConditionType::A;
    case X86_32ConditionType::AE:
        return X86_32ConditionType::B;
    case X86_32ConditionType::A:
        return X86_32ConditionType::BE;
    case X86_32ConditionType::L:
        return X86_32ConditionType::GE;
    case X86_32ConditionType::LE:
        return X86_32ConditionType::G;
    case X86_32ConditionType::GE:
        return X86_32ConditionType::L;
    case X86_32ConditionType::G:
        return X86_32ConditionType::GE;
    case X86_32ConditionType::E:
        return X86_32ConditionType::NE;
    case X86_32ConditionType::NE:
        return X86_32ConditionType::E;
    }
    assert(false);
    return X86_32ConditionType::NE;
}

inline std::string X86_32GetJmpName(X86_32ConditionType condition)
{
    switch(condition)
    {
    case X86_32ConditionType::Z:
        return "jz";
    case X86_32ConditionType::NZ:
        return "jnz";
    case X86_32ConditionType::S:
        return "js";
    case X86_32ConditionType::NS:
        return "jns";
    case X86_32ConditionType::O:
        return "jo";
    case X86_32ConditionType::NO:
        return "jno";
    case X86_32ConditionType::C:
        return "jc";
    case X86_32ConditionType::NC:
        return "jnc";
    case X86_32ConditionType::B:
        return "jb";
    case X86_32ConditionType::BE:
        return "jbe";
    case X86_32ConditionType::AE:
        return "jae";
    case X86_32ConditionType::A:
        return "ja";
    case X86_32ConditionType::L:
        return "jl";
    case X86_32ConditionType::LE:
        return "jle";
    case X86_32ConditionType::GE:
        return "jge";
    case X86_32ConditionType::G:
        return "jg";
    case X86_32ConditionType::E:
        return "je";
    case X86_32ConditionType::NE:
        return "jne";
    }
    assert(false);
    return "";
}

inline std::string X86_32GetSetName(X86_32ConditionType condition)
{
    switch(condition)
    {
    case X86_32ConditionType::Z:
        return "setz";
    case X86_32ConditionType::NZ:
        return "setnz";
    case X86_32ConditionType::S:
        return "sets";
    case X86_32ConditionType::NS:
        return "setns";
    case X86_32ConditionType::O:
        return "seto";
    case X86_32ConditionType::NO:
        return "setno";
    case X86_32ConditionType::C:
        return "setc";
    case X86_32ConditionType::NC:
        return "setnc";
    case X86_32ConditionType::B:
        return "setb";
    case X86_32ConditionType::BE:
        return "setbe";
    case X86_32ConditionType::AE:
        return "setae";
    case X86_32ConditionType::A:
        return "seta";
    case X86_32ConditionType::L:
        return "setl";
    case X86_32ConditionType::LE:
        return "setle";
    case X86_32ConditionType::GE:
        return "setge";
    case X86_32ConditionType::G:
        return "setg";
    case X86_32ConditionType::E:
        return "sete";
    case X86_32ConditionType::NE:
        return "setne";
    }
    assert(false);
    return "";
}

class X86_32AsmNodeCompareAgainstConstantAndJump final : public X86_32AsmControlTransfer
{
public:
    std::shared_ptr<X86_32AsmRegister> lhs;
    std::shared_ptr<ValueNode> rhs;
    X86_32ConditionType conditionType;
    std::weak_ptr<X86_32AsmBasicBlock> trueTarget;
    std::weak_ptr<X86_32AsmBasicBlock> falseTarget;
    explicit X86_32AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86_32AsmRegister> lhs,
                                                        std::shared_ptr<ValueNode> rhs,
                                                        X86_32ConditionType conditionType,
                                                        std::shared_ptr<X86_32AsmBasicBlock> trueTarget,
                                                        std::shared_ptr<X86_32AsmBasicBlock> falseTarget)
        : X86_32AsmControlTransfer(lhs->context), lhs(lhs), rhs(rhs), conditionType(conditionType), trueTarget(trueTarget), falseTarget(falseTarget)
    {
    }
    virtual std::list<std::weak_ptr<X86_32AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86_32AsmBasicBlock>>{trueTarget, falseTarget};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{lhs};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeCompareAgainstConstantAndJump(std::static_pointer_cast<X86_32AsmNodeCompareAgainstConstantAndJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(lhs == originalRegister)
            lhs = newRegister;
    }
};

class X86_32AsmNodeMove final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> dest;
    std::shared_ptr<X86_32AsmRegister> source;
    explicit X86_32AsmNodeMove(std::shared_ptr<X86_32AsmRegister> dest, std::shared_ptr<X86_32AsmRegister> source)
        : X86_32AsmNode(dest->context), dest(dest), source(source)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{source};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{dest};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeMove(std::static_pointer_cast<X86_32AsmNodeMove>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(source == originalRegister)
            source = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_32AsmNodeCompare final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> dest;
    std::shared_ptr<X86_32AsmRegister> lhs;
    std::shared_ptr<X86_32AsmRegister> rhs;
    X86_32ConditionType conditionType;
    explicit X86_32AsmNodeCompare(std::shared_ptr<X86_32AsmRegister> dest, std::shared_ptr<X86_32AsmRegister> lhs, std::shared_ptr<X86_32AsmRegister> rhs, X86_32ConditionType conditionType)
        : X86_32AsmNode(dest->context), dest(dest), lhs(lhs), rhs(rhs), conditionType(conditionType)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{lhs, rhs};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{dest};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeCompare(std::static_pointer_cast<X86_32AsmNodeCompare>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
        if(lhs == originalRegister)
            lhs = newRegister;
        if(rhs == originalRegister)
            rhs = newRegister;
    }
};

class X86_32AsmNodeLoad final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> dest;
    std::shared_ptr<X86_32AsmRegister> address;
    explicit X86_32AsmNodeLoad(std::shared_ptr<X86_32AsmRegister> dest, std::shared_ptr<X86_32AsmRegister> address)
        : X86_32AsmNode(dest->context), dest(dest), address(address)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{address};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{dest};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeLoad(std::static_pointer_cast<X86_32AsmNodeLoad>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_32AsmNodeLoadLocal final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> dest;
    std::uint64_t start; /// byte count into local variables
    explicit X86_32AsmNodeLoadLocal(std::shared_ptr<X86_32AsmRegister> dest, std::uint64_t start)
        : X86_32AsmNode(dest->context), dest(dest), start(start)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{X86_32AsmRegister::getPhysicalRegister(context, "ebp")};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{dest};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeLoadLocal(std::static_pointer_cast<X86_32AsmNodeLoadLocal>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86_32AsmNodeStoreLocal final : public X86_32AsmNode
{
public:
    std::uint64_t start; /// byte count into local variables
    std::shared_ptr<X86_32AsmRegister> value;
    explicit X86_32AsmNodeStoreLocal(std::uint64_t start, std::shared_ptr<X86_32AsmRegister> value)
        : X86_32AsmNode(value->context), start(start), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{X86_32AsmRegister::getPhysicalRegister(context, "ebp"), value};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeStoreLocal(std::static_pointer_cast<X86_32AsmNodeStoreLocal>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86_32AsmNodeStore final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> address;
    std::shared_ptr<X86_32AsmRegister> value;
    explicit X86_32AsmNodeStore(std::shared_ptr<X86_32AsmRegister> address, std::shared_ptr<X86_32AsmRegister> value)
        : X86_32AsmNode(address->context), address(address), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{address, value};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeStore(std::static_pointer_cast<X86_32AsmNodeStore>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86_32AsmNodeLoadConstant final : public X86_32AsmNode
{
public:
    std::shared_ptr<X86_32AsmRegister> dest;
    std::shared_ptr<ValueNode> value;
    explicit X86_32AsmNodeLoadConstant(std::shared_ptr<X86_32AsmRegister> dest, std::shared_ptr<ValueNode> value)
        : X86_32AsmNode(dest->context), dest(dest), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{};
    }
    virtual std::unordered_set<std::shared_ptr<X86_32AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86_32AsmRegister>>{dest};
    }
    virtual void visit(X86_32AsmNodeVisitor &visitor) override
    {
        visitor.visitX86_32AsmNodeLoadConstant(std::static_pointer_cast<X86_32AsmNodeLoadConstant>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86_32AsmRegister> originalRegister, std::shared_ptr<X86_32AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

#endif // X86_32_ASM_NODE_H_INCLUDED
