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
#ifndef X86_ASM_NODE_H_INCLUDED
#define X86_ASM_NODE_H_INCLUDED

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
#include "util/stable_vector.h"
#include "util/variable.h"
#include "backend/x86/x86_backend.h"

class X86AsmRegister final : public std::enable_shared_from_this<X86AsmRegister>
{
    X86AsmRegister(const X86AsmRegister &) = delete;
    X86AsmRegister &operator =(const X86AsmRegister &) = delete;
public:
    CompilerContext *const context;
    const BackendX86 *const backend;
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
            throw std::logic_error("getSpillSize called on X86AsmRegister::PhysicalRegisterKindMask::None()");
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
            throw std::logic_error("getSaveSize called on X86AsmRegister::PhysicalRegisterKindMask::None()");
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
    std::vector<std::shared_ptr<X86AsmRegister>> interferenceSet;
    std::shared_ptr<X86AsmRegister> saveRegister;
public:
    const std::vector<std::shared_ptr<X86AsmRegister>> &getPhysicalRegisterInterferenceSet() const
    {
        return interferenceSet;
    }
    std::shared_ptr<X86AsmRegister> getSaveRegister() const
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
    X86AsmRegister(CompilerContext *context, const BackendX86 *backend, RegisterType registerType, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave, constructTag)
        : context(context), backend(backend), registerType(registerType), name(name), physicalRegisterKindMask(physicalRegisterKindMask), isSpecialPurpose(isSpecialPurpose), isCalleeSave(isCalleeSave)
    {
        assert(context);
        assert(backend);
    }
private:
    typedef std::unordered_map<std::string, std::shared_ptr<X86AsmRegister>> VirtualRegisterMapType;
    static VirtualRegisterMapType &getVirtualRegisterMap(CompilerContext *context, const BackendX86 *backend)
    {
        struct tag_t
        {
        };
        std::shared_ptr<VirtualRegisterMapType> retval = context->getValue<VirtualRegisterMapType, tag_t>();
        if(retval == nullptr)
            context->setValue<VirtualRegisterMapType, tag_t>(retval = std::make_shared<VirtualRegisterMapType>());
        return *retval;
    }
    typedef std::unordered_map<std::string, std::shared_ptr<X86AsmRegister>> PhysicalRegisterMapType;
    static PhysicalRegisterMapType &getPhysicalRegisterMap(CompilerContext *context, const BackendX86 *backend)
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
    static std::shared_ptr<X86AsmRegister> getVirtualRegister(CompilerContext *context, const BackendX86 *backend, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, SpillLocation spillLocation)
    {
        std::shared_ptr<X86AsmRegister> &retval = getVirtualRegisterMap(context, backend)[name];
        if(retval == nullptr)
        {
            retval = std::make_shared<X86AsmRegister>(context, backend, RegisterType::Virtual, name, physicalRegisterKindMask, false, false, constructTag());
            retval->spillLocation = spillLocation;
        }
        return retval;
    }
private:
    static std::shared_ptr<X86AsmRegister> makePhysicalRegister(CompilerContext *context, const BackendX86 *backend, std::string name, PhysicalRegisterKindMask physicalRegisterKindMask, bool isSpecialPurpose, bool isCalleeSave)
    {
        std::shared_ptr<X86AsmRegister> &retval = getPhysicalRegisterMap(context, backend)[name];
        if(retval == nullptr)
            retval = std::make_shared<X86AsmRegister>(context, backend, RegisterType::Physical, name, physicalRegisterKindMask, isSpecialPurpose, isCalleeSave, constructTag());
        return retval;
    }
    static void constructAndAddIntegerPhysicalRegisters_32(CompilerContext *context, const BackendX86 *backend, std::shared_ptr<std::vector<std::shared_ptr<X86AsmRegister>>> retval, std::string name32, std::string name16, std::string name8l, std::string name8h, bool isSpecialPurpose, bool isCalleeSave)
    {
        assert(backend->architecture == BackendX86::X86_32);
        std::shared_ptr<X86AsmRegister> r32 = makePhysicalRegister(context, backend, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r16 = makePhysicalRegister(context, backend, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r8l = makePhysicalRegister(context, backend, name8l, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r8h = makePhysicalRegister(context, backend, name8h, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);

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
    static void constructAndAddIntegerPhysicalRegisters_64(CompilerContext *context, const BackendX86 *backend, std::shared_ptr<std::vector<std::shared_ptr<X86AsmRegister>>> retval, std::string name64, std::string name32, std::string name16, std::string name8, bool isSpecialPurpose, bool isCalleeSave)
    {
        assert(backend->architecture == BackendX86::X86_64);
        std::shared_ptr<X86AsmRegister> r64 = makePhysicalRegister(context, backend, name64, PhysicalRegisterKindMask::Int64(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r32 = makePhysicalRegister(context, backend, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r16 = makePhysicalRegister(context, backend, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r8 = makePhysicalRegister(context, backend, name8, PhysicalRegisterKindMask::Int8(), isSpecialPurpose, isCalleeSave);

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
    static void constructAndAddIntegerPhysicalRegisters_32(CompilerContext *context, const BackendX86 *backend, std::shared_ptr<std::vector<std::shared_ptr<X86AsmRegister>>> retval, std::string name32, std::string name16, bool isSpecialPurpose, bool isCalleeSave)
    {
        assert(backend->architecture == BackendX86::X86_32);
        std::shared_ptr<X86AsmRegister> r32 = makePhysicalRegister(context, backend, name32, PhysicalRegisterKindMask::Int32(), isSpecialPurpose, isCalleeSave);
        std::shared_ptr<X86AsmRegister> r16 = makePhysicalRegister(context, backend, name16, PhysicalRegisterKindMask::Int16(), isSpecialPurpose, isCalleeSave);

        r32->saveRegister = r32;
        r16->saveRegister = r32;

        r32->interferenceSet.push_back(r16);

        r16->interferenceSet.push_back(r32);

        retval->push_back(r32);
        retval->push_back(r16);
    }
public:
    static const std::vector<std::shared_ptr<X86AsmRegister>> &getPhysicalRegisters(CompilerContext *context, const BackendX86 *backend)
    {
        struct tag_t
        {
        };
        auto retval = context->getValue<std::vector<std::shared_ptr<X86AsmRegister>>, tag_t>();
        if(retval == nullptr)
        {
            retval = std::make_shared<std::vector<std::shared_ptr<X86AsmRegister>>>();
            context->setValue<std::vector<std::shared_ptr<X86AsmRegister>>, tag_t>(retval);
            switch(backend->architecture)
            {
            case BackendX86::X86_32:
            {
                for(char nameMiddle : {'a', 'c', 'd', 'b'})
                {
                    constructAndAddIntegerPhysicalRegisters_32(context,
                                                            backend,
                                                            retval,
                                                            std::string("e") + nameMiddle + "x",
                                                            nameMiddle + std::string("x"),
                                                            nameMiddle + std::string("l"),
                                                            nameMiddle + std::string("h"),
                                                            false, nameMiddle == 'b');
                }
                for(int i = 0; i < 8; i++)
                {
                    std::ostringstream ss;
                    ss << "xmm" << i;
                    std::string name = ss.str();
                    std::shared_ptr<X86AsmRegister> r = makePhysicalRegister(context, backend, name, PhysicalRegisterKindMask::Float64() | PhysicalRegisterKindMask::Float32(), false, false);
                    r->saveRegister = r;
                    retval->push_back(r);
                }
                for(std::string baseName : {"si", "di"})
                {
                    constructAndAddIntegerPhysicalRegisters_32(context,
                                                            backend,
                                                            retval,
                                                            "e" + baseName,
                                                            baseName,
                                                            false, true);
                }
                for(std::string baseName : {"sp", "bp"})
                {
                    constructAndAddIntegerPhysicalRegisters_32(context,
                                                            backend,
                                                            retval,
                                                            "e" + baseName,
                                                            baseName,
                                                            true, true);
                }
                return *retval;
            }
            case BackendX86::X86_64:
            {
                for(char nameMiddle : {'a', 'c', 'd'}) // put rbx later because it is a callee saves register
                {
                    constructAndAddIntegerPhysicalRegisters_64(context,
                                                               backend,
                                                            retval,
                                                            std::string("r") + nameMiddle + "x",
                                                            std::string("e") + nameMiddle + "x",
                                                            nameMiddle + std::string("x"),
                                                            nameMiddle + std::string("l"), // we don't use ah through dh becuase they can't be used in all instructions because they conflict with the REX (64-bit override) prefix
                                                            false, nameMiddle == 'b');
                }
                for(std::string baseName : {"si", "di"})
                {
                    constructAndAddIntegerPhysicalRegisters_64(context,
                                                               backend,
                                                            retval,
                                                            "r" + baseName,
                                                            "e" + baseName,
                                                            baseName,
                                                            baseName + "l",
                                                            false, false);
                }
                for(int i = 8; i < 16; i++)
                {
                    std::ostringstream ss;
                    ss << "r" << i;
                    std::string baseName = ss.str();
                    constructAndAddIntegerPhysicalRegisters_64(context,
                                                            backend,
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
                    std::shared_ptr<X86AsmRegister> r = makePhysicalRegister(context, backend, name, PhysicalRegisterKindMask::Float64() | PhysicalRegisterKindMask::Float32(), false, false);
                    r->saveRegister = r;
                    retval->push_back(r);
                }
                for(char nameMiddle : {'b'}) // put rbx later because it is a callee saves register
                {
                    constructAndAddIntegerPhysicalRegisters_64(context,
                                                            backend,
                                                            retval,
                                                            std::string("r") + nameMiddle + "x",
                                                            std::string("e") + nameMiddle + "x",
                                                            nameMiddle + std::string("x"),
                                                            nameMiddle + std::string("l"), // we don't use ah through dh becuase they can't be used in all instructions because they conflict with the REX (64-bit override) prefix
                                                            false, nameMiddle == 'b');
                }
                for(std::string baseName : {"sp", "bp"})
                {
                    constructAndAddIntegerPhysicalRegisters_64(context,
                                                               backend,
                                                            retval,
                                                            "r" + baseName,
                                                            "e" + baseName,
                                                            baseName,
                                                            baseName + "l",
                                                            true, true);
                }
                return *retval;
            }
            }
            assert(false);
        }
        return *retval;
    }
    static std::shared_ptr<X86AsmRegister> getPhysicalRegister(CompilerContext *context, const BackendX86 *backend, std::string name)
    {
        struct tag_t
        {
        };
        auto registerMap = context->getValue<std::unordered_map<std::string, std::shared_ptr<X86AsmRegister>>, tag_t>();
        if(registerMap == nullptr)
        {
            registerMap = std::make_shared<std::unordered_map<std::string, std::shared_ptr<X86AsmRegister>>>();
            context->setValue<std::unordered_map<std::string, std::shared_ptr<X86AsmRegister>>, tag_t>(registerMap);
            const std::vector<std::shared_ptr<X86AsmRegister>> &physicalRegisters = getPhysicalRegisters(context, backend);
            for(std::shared_ptr<X86AsmRegister> r : physicalRegisters)
            {
                registerMap->emplace(r->name, r);
            }
        }
        return registerMap->at(name);
    }
    static std::shared_ptr<X86AsmRegister> getBasePointer(CompilerContext *context, const BackendX86 *backend)
    {
        switch(backend->architecture)
        {
        case BackendX86::X86_32:
            return getPhysicalRegister(context, backend, "ebp");
        case BackendX86::X86_64:
            return getPhysicalRegister(context, backend, "rbp");
        }
        assert(false);
        return nullptr;
    }
};

namespace std
{
template <>
struct hash<X86AsmRegister::PhysicalRegisterKindMask> final
{
    std::size_t operator ()(X86AsmRegister::PhysicalRegisterKindMask v) const
    {
        return v.getHash();
    }
};
}

class X86TypeToPhysicalRegisterKindMask final : public TypeVisitor
{
    X86TypeToPhysicalRegisterKindMask(const X86TypeToPhysicalRegisterKindMask &) = delete;
    X86TypeToPhysicalRegisterKindMask &operator =(const X86TypeToPhysicalRegisterKindMask &) = delete;
private:
    bool isGood = true, isEmpty = true;
    bool isFloatingPoint = false;
    std::size_t sizeInBytes = 0;
    const BackendX86 *const backend;
    X86TypeToPhysicalRegisterKindMask(const BackendX86 *backend)
        : backend(backend)
    {
    }
    X86AsmRegister::PhysicalRegisterKindMask getMask() const
    {
        if(isEmpty || !isGood)
            throw std::runtime_error("invalid type mask");
        switch(sizeInBytes)
        {
        case 1:
            if(isFloatingPoint)
                break;
            return X86AsmRegister::PhysicalRegisterKindMask::Int8();
        case 2:
            if(isFloatingPoint)
                break;
            return X86AsmRegister::PhysicalRegisterKindMask::Int16();
        case 4:
            if(isFloatingPoint)
                return X86AsmRegister::PhysicalRegisterKindMask::Float32();
            return X86AsmRegister::PhysicalRegisterKindMask::Int32();
        case 8:
            if(isFloatingPoint)
                return X86AsmRegister::PhysicalRegisterKindMask::Float64();
            if(backend->architecture == BackendX86::X86_64)
                return X86AsmRegister::PhysicalRegisterKindMask::Int64();
            break;
        }
        throw std::runtime_error("invalid type mask");
    }
    X86TypeToPhysicalRegisterKindMask &visit(std::shared_ptr<TypeNode> node)
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
        switch(backend->architecture)
        {
        case BackendX86::X86_32:
            sizeInBytes += 4;
            break;
        case BackendX86::X86_64:
            sizeInBytes += 8;
            break;
        }
    }
    virtual void visitTypeInteger(std::shared_ptr<TypeInteger> node) override
    {
        isGood = isGood && isEmpty;
        isEmpty = false;
        switch(node->width)
        {
        case IntegerWidth::Int8:
            sizeInBytes += 1;
            break;
        case IntegerWidth::Int16:
            sizeInBytes += 2;
            break;
        case IntegerWidth::Int32:
            sizeInBytes += 4;
            break;
        case IntegerWidth::Int64:
            switch(backend->architecture)
            {
            case BackendX86::X86_32:
                throw std::runtime_error("64-bit integers not implemented on x86_32");
            case BackendX86::X86_64:
                sizeInBytes += 8;
                break;
            }
            break;
        case IntegerWidth::IntNativeSize:
            switch(backend->architecture)
            {
            case BackendX86::X86_32:
                sizeInBytes += 4;
                break;
            case BackendX86::X86_64:
                sizeInBytes += 8;
                break;
            }
            break;
        }
    }
    static X86AsmRegister::PhysicalRegisterKindMask run(std::shared_ptr<TypeNode> type, const BackendX86 *backend)
    {
        return X86TypeToPhysicalRegisterKindMask(backend).visit(type).getMask();
    }
};

class X86AsmNodeVisitor;

class X86AsmNode : public std::enable_shared_from_this<X86AsmNode>
{
    X86AsmNode(const X86AsmNode &) = delete;
    X86AsmNode &operator =(const X86AsmNode &) = delete;
public:
    CompilerContext *const context;
    const BackendX86 *const backend;
    explicit X86AsmNode(CompilerContext *context, const BackendX86 *backend)
        : context(context), backend(backend)
    {
    }
    virtual ~X86AsmNode() = default;
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const = 0;
    virtual void visit(X86AsmNodeVisitor &visitor) = 0;
    virtual bool hasSideEffects() const
    {
        return false;
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) = 0;
};

class X86AsmBasicBlock;

class X86AsmControlTransfer : public X86AsmNode
{
public:
    explicit X86AsmControlTransfer(CompilerContext *context, const BackendX86 *backend)
        : X86AsmNode(context, backend)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{};
    }
    virtual std::list<std::weak_ptr<X86AsmBasicBlock>> targets() const = 0;
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const = 0;
    virtual void visit(X86AsmNodeVisitor &visitor) = 0;
};

class X86AsmBasicBlock final : public std::enable_shared_from_this<X86AsmBasicBlock>
{
    X86AsmBasicBlock(const X86AsmBasicBlock &) = delete;
    X86AsmBasicBlock &operator =(const X86AsmBasicBlock &) = delete;
public:
    CompilerContext *const context;
    const BackendX86 *const backend;
    explicit X86AsmBasicBlock(CompilerContext *context, const BackendX86 *backend)
        : context(context), backend(backend)
    {
    }
    std::shared_ptr<X86AsmControlTransfer> controlTransferInstruction;
    typedef stable_vector<std::shared_ptr<X86AsmNode>> InstructionList;
    InstructionList instructions;
    std::list<std::weak_ptr<X86AsmBasicBlock>> sourceBlocks;
    std::list<std::weak_ptr<X86AsmBasicBlock>> destBlocks;
    std::unordered_set<std::shared_ptr<X86AsmRegister>> usedRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86AsmRegister>> assignedRegisters;
    std::unordered_set<std::shared_ptr<X86AsmRegister>> liveRegistersAtStart;
    std::unordered_set<std::shared_ptr<X86AsmRegister>> liveRegistersAtEnd;
};

class X86AsmFunction final : public std::enable_shared_from_this<X86AsmFunction>
{
    X86AsmFunction(const X86AsmFunction &) = delete;
    X86AsmFunction &operator =(const X86AsmFunction &) = delete;
public:
    CompilerContext *const context;
    const BackendX86 *const backend;
    explicit X86AsmFunction(CompilerContext *context, const BackendX86 *backend)
        : context(context), backend(backend)
    {
    }
    std::shared_ptr<X86AsmBasicBlock> startBlock;
    std::list<std::shared_ptr<X86AsmBasicBlock>> blocks;
    std::uint64_t localVariablesSize = 0;
};

class X86AsmNodeJump;
class X86AsmNodeCompareAgainstConstantAndJump;
class X86AsmNodeMove;
class X86AsmNodeLoadConstant;
class X86AsmNodeLoad;
class X86AsmNodeStore;
class X86AsmNodeLoadLocal;
class X86AsmNodeStoreLocal;
class X86AsmNodeCompare;

class X86AsmNodeVisitor
{
public:
    virtual void visitX86AsmNodeJump(std::shared_ptr<X86AsmNodeJump> node) = 0;
    virtual void visitX86AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86AsmNodeCompareAgainstConstantAndJump> node) = 0;
    virtual void visitX86AsmNodeMove(std::shared_ptr<X86AsmNodeMove> node) = 0;
    virtual void visitX86AsmNodeLoadConstant(std::shared_ptr<X86AsmNodeLoadConstant> node) = 0;
    virtual void visitX86AsmNodeLoad(std::shared_ptr<X86AsmNodeLoad> node) = 0;
    virtual void visitX86AsmNodeStore(std::shared_ptr<X86AsmNodeStore> node) = 0;
    virtual void visitX86AsmNodeLoadLocal(std::shared_ptr<X86AsmNodeLoadLocal> node) = 0;
    virtual void visitX86AsmNodeStoreLocal(std::shared_ptr<X86AsmNodeStoreLocal> node) = 0;
    virtual void visitX86AsmNodeCompare(std::shared_ptr<X86AsmNodeCompare> node) = 0;
};

class X86AsmNodeJump final : public X86AsmControlTransfer
{
public:
    std::weak_ptr<X86AsmBasicBlock> target;
    explicit X86AsmNodeJump(std::shared_ptr<X86AsmBasicBlock> target)
        : X86AsmControlTransfer(target->context, target->backend), target(target)
    {
    }
    virtual std::list<std::weak_ptr<X86AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86AsmBasicBlock>>{target};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeJump(std::static_pointer_cast<X86AsmNodeJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
    }
};

enum class X86ConditionType
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

inline X86ConditionType X86InvertCondition(X86ConditionType c)
{
    switch(c)
    {
    case X86ConditionType::Z:
        return X86ConditionType::NZ;
    case X86ConditionType::NZ:
        return X86ConditionType::Z;
    case X86ConditionType::S:
        return X86ConditionType::NS;
    case X86ConditionType::NS:
        return X86ConditionType::S;
    case X86ConditionType::O:
        return X86ConditionType::NO;
    case X86ConditionType::NO:
        return X86ConditionType::O;
    case X86ConditionType::C:
        return X86ConditionType::NC;
    case X86ConditionType::NC:
        return X86ConditionType::C;
    case X86ConditionType::B:
        return X86ConditionType::AE;
    case X86ConditionType::BE:
        return X86ConditionType::A;
    case X86ConditionType::AE:
        return X86ConditionType::B;
    case X86ConditionType::A:
        return X86ConditionType::BE;
    case X86ConditionType::L:
        return X86ConditionType::GE;
    case X86ConditionType::LE:
        return X86ConditionType::G;
    case X86ConditionType::GE:
        return X86ConditionType::L;
    case X86ConditionType::G:
        return X86ConditionType::GE;
    case X86ConditionType::E:
        return X86ConditionType::NE;
    case X86ConditionType::NE:
        return X86ConditionType::E;
    }
    assert(false);
    return X86ConditionType::NE;
}

inline std::string X86GetJmpName(X86ConditionType condition)
{
    switch(condition)
    {
    case X86ConditionType::Z:
        return "jz";
    case X86ConditionType::NZ:
        return "jnz";
    case X86ConditionType::S:
        return "js";
    case X86ConditionType::NS:
        return "jns";
    case X86ConditionType::O:
        return "jo";
    case X86ConditionType::NO:
        return "jno";
    case X86ConditionType::C:
        return "jc";
    case X86ConditionType::NC:
        return "jnc";
    case X86ConditionType::B:
        return "jb";
    case X86ConditionType::BE:
        return "jbe";
    case X86ConditionType::AE:
        return "jae";
    case X86ConditionType::A:
        return "ja";
    case X86ConditionType::L:
        return "jl";
    case X86ConditionType::LE:
        return "jle";
    case X86ConditionType::GE:
        return "jge";
    case X86ConditionType::G:
        return "jg";
    case X86ConditionType::E:
        return "je";
    case X86ConditionType::NE:
        return "jne";
    }
    assert(false);
    return "";
}

inline std::string X86GetSetName(X86ConditionType condition)
{
    switch(condition)
    {
    case X86ConditionType::Z:
        return "setz";
    case X86ConditionType::NZ:
        return "setnz";
    case X86ConditionType::S:
        return "sets";
    case X86ConditionType::NS:
        return "setns";
    case X86ConditionType::O:
        return "seto";
    case X86ConditionType::NO:
        return "setno";
    case X86ConditionType::C:
        return "setc";
    case X86ConditionType::NC:
        return "setnc";
    case X86ConditionType::B:
        return "setb";
    case X86ConditionType::BE:
        return "setbe";
    case X86ConditionType::AE:
        return "setae";
    case X86ConditionType::A:
        return "seta";
    case X86ConditionType::L:
        return "setl";
    case X86ConditionType::LE:
        return "setle";
    case X86ConditionType::GE:
        return "setge";
    case X86ConditionType::G:
        return "setg";
    case X86ConditionType::E:
        return "sete";
    case X86ConditionType::NE:
        return "setne";
    }
    assert(false);
    return "";
}

class X86AsmNodeCompareAgainstConstantAndJump final : public X86AsmControlTransfer
{
public:
    std::shared_ptr<X86AsmRegister> lhs;
    std::shared_ptr<ValueNode> rhs;
    X86ConditionType conditionType;
    std::weak_ptr<X86AsmBasicBlock> trueTarget;
    std::weak_ptr<X86AsmBasicBlock> falseTarget;
    explicit X86AsmNodeCompareAgainstConstantAndJump(std::shared_ptr<X86AsmRegister> lhs,
                                                        std::shared_ptr<ValueNode> rhs,
                                                        X86ConditionType conditionType,
                                                        std::shared_ptr<X86AsmBasicBlock> trueTarget,
                                                        std::shared_ptr<X86AsmBasicBlock> falseTarget)
        : X86AsmControlTransfer(lhs->context, lhs->backend), lhs(lhs), rhs(rhs), conditionType(conditionType), trueTarget(trueTarget), falseTarget(falseTarget)
    {
    }
    virtual std::list<std::weak_ptr<X86AsmBasicBlock>> targets() const override
    {
        return std::list<std::weak_ptr<X86AsmBasicBlock>>{trueTarget, falseTarget};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{lhs};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeCompareAgainstConstantAndJump(std::static_pointer_cast<X86AsmNodeCompareAgainstConstantAndJump>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(lhs == originalRegister)
            lhs = newRegister;
    }
};

class X86AsmNodeMove final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> dest;
    std::shared_ptr<X86AsmRegister> source;
    explicit X86AsmNodeMove(std::shared_ptr<X86AsmRegister> dest, std::shared_ptr<X86AsmRegister> source)
        : X86AsmNode(dest->context, dest->backend), dest(dest), source(source)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{source};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{dest};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeMove(std::static_pointer_cast<X86AsmNodeMove>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(source == originalRegister)
            source = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86AsmNodeCompare final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> dest;
    std::shared_ptr<X86AsmRegister> lhs;
    std::shared_ptr<X86AsmRegister> rhs;
    X86ConditionType conditionType;
    explicit X86AsmNodeCompare(std::shared_ptr<X86AsmRegister> dest, std::shared_ptr<X86AsmRegister> lhs, std::shared_ptr<X86AsmRegister> rhs, X86ConditionType conditionType)
        : X86AsmNode(dest->context, dest->backend), dest(dest), lhs(lhs), rhs(rhs), conditionType(conditionType)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{lhs, rhs};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{dest};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeCompare(std::static_pointer_cast<X86AsmNodeCompare>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
        if(lhs == originalRegister)
            lhs = newRegister;
        if(rhs == originalRegister)
            rhs = newRegister;
    }
};

class X86AsmNodeLoad final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> dest;
    std::shared_ptr<X86AsmRegister> address;
    explicit X86AsmNodeLoad(std::shared_ptr<X86AsmRegister> dest, std::shared_ptr<X86AsmRegister> address)
        : X86AsmNode(dest->context, dest->backend), dest(dest), address(address)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{address};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{dest};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeLoad(std::static_pointer_cast<X86AsmNodeLoad>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86AsmNodeLoadLocal final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> dest;
    VariableLocation location;
    explicit X86AsmNodeLoadLocal(std::shared_ptr<X86AsmRegister> dest, VariableLocation location)
        : X86AsmNode(dest->context, dest->backend), dest(dest), location(location)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{X86AsmRegister::getBasePointer(context, backend)};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{dest};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeLoadLocal(std::static_pointer_cast<X86AsmNodeLoadLocal>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

class X86AsmNodeStoreLocal final : public X86AsmNode
{
public:
    VariableLocation location;
    std::shared_ptr<X86AsmRegister> value;
    explicit X86AsmNodeStoreLocal(VariableLocation location, std::shared_ptr<X86AsmRegister> value)
        : X86AsmNode(value->context, value->backend), location(location), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{X86AsmRegister::getBasePointer(context, backend), value};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeStoreLocal(std::static_pointer_cast<X86AsmNodeStoreLocal>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86AsmNodeStore final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> address;
    std::shared_ptr<X86AsmRegister> value;
    explicit X86AsmNodeStore(std::shared_ptr<X86AsmRegister> address, std::shared_ptr<X86AsmRegister> value)
        : X86AsmNode(address->context, address->backend), address(address), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{address, value};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeStore(std::static_pointer_cast<X86AsmNodeStore>(shared_from_this()));
    }
    virtual bool hasSideEffects() const override
    {
        return true;
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(address == originalRegister)
            address = newRegister;
        if(value == originalRegister)
            value = newRegister;
    }
};

class X86AsmNodeLoadConstant final : public X86AsmNode
{
public:
    std::shared_ptr<X86AsmRegister> dest;
    std::shared_ptr<ValueNode> value;
    explicit X86AsmNodeLoadConstant(std::shared_ptr<X86AsmRegister> dest, std::shared_ptr<ValueNode> value)
        : X86AsmNode(dest->context, dest->backend), dest(dest), value(value)
    {
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> inputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{};
    }
    virtual std::unordered_set<std::shared_ptr<X86AsmRegister>> outputSet() const override
    {
        return std::unordered_set<std::shared_ptr<X86AsmRegister>>{dest};
    }
    virtual void visit(X86AsmNodeVisitor &visitor) override
    {
        visitor.visitX86AsmNodeLoadConstant(std::static_pointer_cast<X86AsmNodeLoadConstant>(shared_from_this()));
    }
    virtual void replaceRegister(std::shared_ptr<X86AsmRegister> originalRegister, std::shared_ptr<X86AsmRegister> newRegister) override
    {
        if(dest == originalRegister)
            dest = newRegister;
    }
};

#endif // X86_ASM_NODE_H_INCLUDED
