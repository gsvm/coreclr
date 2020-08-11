// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

//


#ifndef _TARGET_MIPS64_
#error Should only include "cgencpu.h" for MIPS64 builds
#endif

#ifndef __cgencpu_h__
#define __cgencpu_h__

#define INSTRFMT_K64
#include <stublink.h>

#ifndef FEATURE_PAL
#define USE_REDIRECT_FOR_GCSTRESS
#endif // FEATURE_PAL

EXTERN_C void getFPReturn(int fpSize, INT64 *pRetVal);
EXTERN_C void setFPReturn(int fpSize, INT64 retVal);


class ComCallMethodDesc;

extern PCODE GetPreStubEntryPoint();

////FIXME for MIPS.
#define COMMETHOD_PREPAD                        24   // # extra bytes to allocate in addition to sizeof(ComCallMethodDesc)
#ifdef FEATURE_COMINTEROP
#define COMMETHOD_CALL_PRESTUB_SIZE             24
#define COMMETHOD_CALL_PRESTUB_ADDRESS_OFFSET   16   // the offset of the call target address inside the prestub
#endif // FEATURE_COMINTEROP

#define STACK_ALIGN_SIZE                        16

#define JUMP_ALLOCATE_SIZE                      40  // # bytes to allocate for a jump instruction
#define BACK_TO_BACK_JUMP_ALLOCATE_SIZE         40  // # bytes to allocate for a back to back jump instruction

#define HAS_NDIRECT_IMPORT_PRECODE              1

#define USE_INDIRECT_CODEHEADER

#define HAS_FIXUP_PRECODE                       1
#define HAS_FIXUP_PRECODE_CHUNKS                1

// ThisPtrRetBufPrecode one is necessary for closed delegates over static methods with return buffer
#define HAS_THISPTR_RETBUF_PRECODE              1

#define CODE_SIZE_ALIGN                         8
#define CACHE_LINE_SIZE                         64
#define LOG2SLOT                                LOG2_PTRSIZE

#define ENREGISTERED_RETURNTYPE_MAXSIZE         16  // bytes (two FP registers: f0 and f2)
#define ENREGISTERED_RETURNTYPE_INTEGER_MAXSIZE 16  // bytes (two int registers: v0 and v1)

#define CALLDESCR_ARGREGS                       1   // CallDescrWorker has ArgumentRegister parameter
#define CALLDESCR_FPARGREGS                     1   // CallDescrWorker has FloatArgumentRegisters parameter

// Given a return address retrieved during stackwalk,
// this is the offset by which it should be decremented to arrive at the callsite.
#define STACKWALK_CONTROLPC_ADJUST_OFFSET 8

//=======================================================================
// IMPORTANT: This value is used to figure out how much to allocate
// for a fixed array of FieldMarshaler's. That means it must be at least
// as large as the largest FieldMarshaler subclass. This requirement
// is guarded by an assert.
//=======================================================================
//// FIXME for MIPS: this is not for mips.
#define MAXFIELDMARSHALERSIZE               40

//**********************************************************************
// Parameter size
//**********************************************************************

typedef INT64 StackElemType;
#define STACK_ELEM_SIZE sizeof(StackElemType)

// The expression below assumes STACK_ELEM_SIZE is a power of 2, so check that.
static_assert(((STACK_ELEM_SIZE & (STACK_ELEM_SIZE-1)) == 0), "STACK_ELEM_SIZE must be a power of 2");

#define StackElemSize(parmSize) (((parmSize) + STACK_ELEM_SIZE - 1) & ~((ULONG)(STACK_ELEM_SIZE - 1)))

//
// JIT HELPERS.
//
// Create alias for optimized implementations of helpers provided on this platform
//
#define JIT_GetSharedGCStaticBase           JIT_GetSharedGCStaticBase_SingleAppDomain
#define JIT_GetSharedNonGCStaticBase        JIT_GetSharedNonGCStaticBase_SingleAppDomain
#define JIT_GetSharedGCStaticBaseNoCtor     JIT_GetSharedGCStaticBaseNoCtor_SingleAppDomain
#define JIT_GetSharedNonGCStaticBaseNoCtor  JIT_GetSharedNonGCStaticBaseNoCtor_SingleAppDomain

#define JIT_Stelem_Ref                      JIT_Stelem_Ref

//**********************************************************************
// Frames
//**********************************************************************

//--------------------------------------------------------------------
// This represents the callee saved (non-volatile) registers saved as
// of a FramedMethodFrame.
//--------------------------------------------------------------------
typedef DPTR(struct CalleeSavedRegisters) PTR_CalleeSavedRegisters;
struct CalleeSavedRegisters {
    INT64 fp; // s8
    INT64 ra; // return register
    INT64 s0;
    INT64 s1;
    INT64 s2;
    INT64 s3;
    INT64 s4;
    INT64 s5;
    INT64 s6;
    INT64 s7;
    INT64 gp;
};

//--------------------------------------------------------------------
// This represents the arguments that are stored in volatile registers.
// This should not overlap the CalleeSavedRegisters since those are already
// saved separately and it would be wasteful to save the same register twice.
// If we do use a non-volatile register as an argument, then the ArgIterator
// will probably have to communicate this back to the PromoteCallerStack
// routine to avoid a double promotion.
//--------------------------------------------------------------------
typedef DPTR(struct ArgumentRegisters) PTR_ArgumentRegisters;
struct ArgumentRegisters {
    INT64 a[8]; // a0 ....a7
};
#define NUM_ARGUMENT_REGISTERS 8

#define ARGUMENTREGISTERS_SIZE sizeof(ArgumentRegisters)


//--------------------------------------------------------------------
// This represents the floating point argument registers which are saved
// as part of the NegInfo for a FramedMethodFrame. Note that these
// might not be saved by all stubs: typically only those that call into
// C++ helpers will need to preserve the values in these volatile
// registers.
//--------------------------------------------------------------------
typedef DPTR(struct FloatArgumentRegisters) PTR_FloatArgumentRegisters;
struct FloatArgumentRegisters {
    // armV8 supports 32 floating point registers. Each register is 128bits long.
    // It can be accessed as 128-bit value or 64-bit value(d0-d31) or as 32-bit value (s0-s31)
    // or as 16-bit value or as 8-bit values. C# only has two builtin floating datatypes float(32-bit) and
    // double(64-bit). It does not have a quad-precision floating point.So therefore it does not make sense to
    // store full 128-bit values in Frame when the upper 64 bit will not contain any values.
    double  f[8];  // f12-f19
};


//**********************************************************************
// Exception handling
//**********************************************************************

inline PCODE GetIP(const T_CONTEXT * context) {
    LIMITED_METHOD_DAC_CONTRACT;
    return context->Pc;
}

inline void SetIP(T_CONTEXT *context, PCODE ip) {
    LIMITED_METHOD_DAC_CONTRACT;
    context->Pc = ip;
}

inline TADDR GetSP(const T_CONTEXT * context) {
    LIMITED_METHOD_DAC_CONTRACT;
    return TADDR(context->Sp);
}

inline TADDR GetRA(const T_CONTEXT * context) {
    LIMITED_METHOD_DAC_CONTRACT;
    return context->Ra;
}

inline void SetRA(T_CONTEXT * context, TADDR ip) {
    LIMITED_METHOD_DAC_CONTRACT;
    context->Ra = ip;
}

inline  LPVOID __stdcall GetCurrentSP()
{
    LPVOID p;
    __asm__ volatile (
            "move %0, $29 \n"    //$29=sp
            :"=r"(p)::);
    return p;
}

inline void SetSP(T_CONTEXT *context, TADDR sp) {
    LIMITED_METHOD_DAC_CONTRACT;
    context->Sp = DWORD64(sp);
}

inline void SetFP(T_CONTEXT *context, TADDR fp) {
    LIMITED_METHOD_DAC_CONTRACT;
    context->Fp = DWORD64(fp);
}

inline TADDR GetFP(const T_CONTEXT * context)
{
    LIMITED_METHOD_DAC_CONTRACT;
    return (TADDR)(context->Fp);
}

inline TADDR GetMem(PCODE ip)
{
    TADDR mem;
    LIMITED_METHOD_DAC_CONTRACT;
    EX_TRY
    {
        mem = dac_cast<TADDR>(ip);
    }
    EX_CATCH
    {
        _ASSERTE(!"Memory read within jitted Code Failed, this should not happen!!!!");
    }
    EX_END_CATCH(SwallowAllExceptions);
    return mem;
}


#ifdef FEATURE_COMINTEROP
void emitCOMStubCall (ComCallMethodDesc *pCOMMethod, PCODE target);
#endif // FEATURE_COMINTEROP

inline BOOL ClrFlushInstructionCache(LPCVOID pCodeAddr, size_t sizeOfCode)
{
#ifdef CROSSGEN_COMPILE
    // The code won't be executed when we are cross-compiling so flush instruction cache is unnecessary
    return TRUE;
#else
    return FlushInstructionCache(GetCurrentProcess(), pCodeAddr, sizeOfCode);
#endif
}

//------------------------------------------------------------------------
inline void emitJump(LPBYTE pBuffer, LPVOID target)
{
    LIMITED_METHOD_CONTRACT;
    UINT32* pCode = (UINT32*)pBuffer;

    // We require 8-byte alignment so the LD instruction is aligned properly
    _ASSERTE(((UINT_PTR)pCode & 7) == 0);

    // ld  t9,16(t9)
    // jr  t9
    // nop
    // nop    //padding.

    pCode[0] = 0xdf390010; //ld  t9,16(t9)
    pCode[1] = 0x03200008; //jr  t9
    pCode[2] = 0; //nop
    pCode[3] = 0; //nop    //padding.

    // Ensure that the updated instructions get updated in the I-Cache
    ClrFlushInstructionCache(pCode, 16);

    *((LPVOID *)(pCode + 4)) = target;   // 64-bit target address

}

//------------------------------------------------------------------------
//  Given the same pBuffer that was used by emitJump this method
//  decodes the instructions and returns the jump target
inline PCODE decodeJump(PCODE pCode)
{
    LIMITED_METHOD_CONTRACT;

    TADDR pInstr = PCODEToPINSTR(pCode);

    return *dac_cast<PTR_PCODE>(pInstr + 16);
}

//------------------------------------------------------------------------
inline BOOL isJump(PCODE pCode)
{
    LIMITED_METHOD_DAC_CONTRACT;

    TADDR pInstr = PCODEToPINSTR(pCode);

    return *dac_cast<PTR_DWORD>(pInstr) == 0xdf390010; //ld  t9,16(t9)
}

//------------------------------------------------------------------------
inline BOOL isBackToBackJump(PCODE pBuffer)
{
    WRAPPER_NO_CONTRACT;
    SUPPORTS_DAC;
    return isJump(pBuffer);
}

//------------------------------------------------------------------------
inline void emitBackToBackJump(LPBYTE pBuffer, LPVOID target)
{
    WRAPPER_NO_CONTRACT;
    emitJump(pBuffer, target);
}

//------------------------------------------------------------------------
inline PCODE decodeBackToBackJump(PCODE pBuffer)
{
    WRAPPER_NO_CONTRACT;
    return decodeJump(pBuffer);
}

// SEH info forward declarations

inline BOOL IsUnmanagedValueTypeReturnedByRef(UINT sizeofvaluetype)
{
    // MIPS64TODO: Does this need to care about HFA. It does not for MIPS32
    return (sizeofvaluetype > ENREGISTERED_RETURNTYPE_INTEGER_MAXSIZE);
}


//----------------------------------------------------------------------

struct IntReg
{
    int reg;
    IntReg(int reg):reg(reg)
    {
        _ASSERTE(0 <= reg && reg < 32);
    }

    operator int () { return reg; }
    operator int () const { return reg; }
    int operator == (IntReg other) { return reg == other.reg; }
    int operator != (IntReg other) { return reg != other.reg; }
    WORD Mask() const { return 1 << reg; }
};

struct FloatReg
{
    int reg;
    FloatReg(int reg):reg(reg)
    {
        _ASSERTE(0 <= reg && reg < 32);
    }

    operator int () { return reg; }
    operator int () const { return reg; }
    int operator == (FloatReg other) { return reg == other.reg; }
    int operator != (FloatReg other) { return reg != other.reg; }
    WORD Mask() const { return 1 << reg; }
};

struct VecReg
{
    int reg;
    VecReg(int reg):reg(reg)
    {
        _ASSERTE(0 <= reg && reg < 32);
    }

    operator int() { return reg; }
    int operator == (VecReg other) { return reg == other.reg; }
    int operator != (VecReg other) { return reg != other.reg; }
    WORD Mask() const { return 1 << reg; }
};

const IntReg RegSp  = IntReg(29);
const IntReg RegFp  = IntReg(30);
const IntReg RegRa  = IntReg(31);

#define PRECODE_ALIGNMENT           CODE_SIZE_ALIGN

#define OFFSETOF_PRECODE_TYPE       12
#define SIZEOF_PRECODE_BASE         (OFFSETOF_PRECODE_TYPE + 4)

#ifdef CROSSGEN_COMPILE
#define GetEEFuncEntryPoint(pfn) 0x1001
#else
#define GetEEFuncEntryPoint(pfn) GFN_TADDR(pfn)
#endif


class StubLinkerCPU : public StubLinker
{

private:
    void EmitLoadStoreRegPairImm(DWORD flags, int regNum1, int regNum2, IntReg Rn, int offset, BOOL isVec);
    void EmitLoadStoreRegImm(DWORD flags, int regNum, IntReg Rn, int offset, BOOL isVec);
public:

    // BitFlags for EmitLoadStoreReg(Pair)Imm methods
    enum {
        eSTORE = 0x0,
        eLOAD  = 0x1,
    };

    static void Init();
    static bool isValidSimm16(int value) {
        return -( ((int)1) << 15 ) <= value && value < ( ((int)1) << 15 );
    }
    static int emitIns_O_R_R_I(int op, int rs, int rt, int imm) {
        _ASSERTE(isValidSimm16(imm));
        _ASSERTE(!(rs >> 5));
        _ASSERTE(!(rt >> 5));
        _ASSERTE(!(op >> 6));
        return ((op & 0x3f)<<26) | ((rs & 0x1f)<<21) | ((rt & 0x1f)<<16) | (imm & 0xffff);
    }
    static int emitIns_R_R_R_O(int rs, int rt, int rd, int op) {
        _ASSERTE(!(rs >> 5));
        _ASSERTE(!(rt >> 5));
        _ASSERTE(!(rd >> 5));
        _ASSERTE(!(op >> 6));
        return ((rs & 0x1f)<<21) | ((rt & 0x1f)<<16) | ((rd & 0x1f)<<11) | (op & 0x3f);
    }

    void EmitUnboxMethodStub(MethodDesc* pRealMD);
    void EmitCallManagedMethod(MethodDesc *pMD, BOOL fTailCall);
    void EmitCallLabel(CodeLabel *target, BOOL fTailCall, BOOL fIndirect);

    void EmitShuffleThunk(struct ShuffleEntry *pShuffleEntryArray);

    void EmitNop() { Emit32(0x00000000); }
    void EmitBreakPoint() { Emit32(0x0000000d); }
    void EmitJumpRegister(IntReg regTarget);
    void EmitMovReg(IntReg dest, IntReg source);
    void EmitMovFloatReg(FloatReg Fd, FloatReg Fs);

    void EmitSubImm(IntReg Rd, IntReg Rn, unsigned int value);
    void EmitAddImm(IntReg Rd, IntReg Rn, unsigned int value);

    void EmitLoadStoreRegPairImm(DWORD flags, IntReg Rt1, IntReg Rt2, IntReg Rn, int offset=0);
    void EmitLoadStoreRegPairImm(DWORD flags, VecReg Vt1, VecReg Vt2, IntReg Xn, int offset=0);

    void EmitLoadStoreRegImm(DWORD flags, IntReg Rt, IntReg Rn, int offset=0);
    void EmitLoadStoreRegImm(DWORD flags, VecReg Vt, IntReg Xn, int offset=0);
    void EmitLoadFloatRegImm(FloatReg ft, IntReg base, int offset);
};

extern "C" void SinglecastDelegateInvokeStub();


// preferred alignment for data
#define DATA_ALIGNMENT 8

struct DECLSPEC_ALIGN(16) UMEntryThunkCode
{
    DWORD        m_code[4];

    TADDR       m_pTargetCode;
    TADDR       m_pvSecretParam;

    void Encode(BYTE* pTargetCode, void* pvSecretParam);
    void Poison();

    LPCBYTE GetEntryPoint() const
    {
        LIMITED_METHOD_CONTRACT;

        return (LPCBYTE)this;
    }

    static int GetEntryPointOffset()
    {
        LIMITED_METHOD_CONTRACT;

        return 0;
    }
};

struct HijackArgs
{
    //FIXME for MIPS: should confirm whether v0/v1, f0/f2 are needed ???
    union
    {
        struct {
             DWORD64 V0;
             DWORD64 V1;
         };
        size_t ReturnValue[2];
    };
    union
    {
        struct {
             DWORD64 F0;
             DWORD64 F2;
         };
        size_t FPReturnValue[2];
    };
    DWORD64 S0, S1, S2, S3, S4, S5, S6, S7, Gp;
    DWORD64 Fp; // frame pointer
    union
    {
        DWORD64 Ra;
        size_t ReturnAddress;
    };
};

EXTERN_C VOID STDCALL PrecodeFixupThunk();

// Invalid precode type
struct InvalidPrecode {
    static const int Type = 0;
};

struct StubPrecode {

    static const int Type = 0x71;

    //ld  t2, 24(t9)
    //ld  t9, 16(t9)
    //jr  t9
    // ori zero,zero,0x71    //for Type encoding.
    // dcd pTarget
    // dcd pMethodDesc
    DWORD   m_rgCode[4];
    TADDR   m_pTarget;
    TADDR   m_pMethodDesc;

    void Init(MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pMethodDesc;
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pTarget;
    }

    void ResetTargetInterlocked()
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        EnsureWritableExecutablePages(&m_pTarget);
        InterlockedExchange64((LONGLONG*)&m_pTarget, (TADDR)GetPreStubEntryPoint());
    }

    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        EnsureWritableExecutablePages(&m_pTarget);
        return (TADDR)InterlockedCompareExchange64(
            (LONGLONG*)&m_pTarget, (TADDR)target, (TADDR)expected) == expected;
    }

#ifdef FEATURE_PREJIT
    void Fixup(DataImage *image);
#endif
};
typedef DPTR(StubPrecode) PTR_StubPrecode;


struct NDirectImportPrecode {

    static const int Type = 0x72;

    // ld  t2,24(t9)          ; =m_pMethodDesc
    // ld  t9,16(t9)          ; =m_pTarget
    // jr  t9
    // ori zero,zero,0x72    //for Type encoding.
    DWORD   m_rgCode[4];
    TADDR   m_pTarget;
    TADDR   m_pMethodDesc;

    void Init(MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pMethodDesc;
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pTarget;
    }

    LPVOID GetEntrypoint()
    {
        LIMITED_METHOD_CONTRACT;
        return this;
    }

#ifdef FEATURE_PREJIT
    void Fixup(DataImage *image);
#endif
};
typedef DPTR(NDirectImportPrecode) PTR_NDirectImportPrecode;


struct FixupPrecode {

    static const int Type = 0x73;

    //ori t2, t9, 0
    //ld  t9,24(t9)
    //jr  t9
    // ori zero,zero,0x73    //for Type encoding.
    //pading nop
    // 2 byte padding
    // dcb m_MethodDescChunkIndex
    // dcb m_PrecodeChunkIndex
    // dcd m_pTarget

    UINT32  m_rgCode[5];
    BYTE    padding[2];
    BYTE    m_MethodDescChunkIndex;
    BYTE    m_PrecodeChunkIndex;
    TADDR   m_pTarget;

    void Init(MethodDesc* pMD, LoaderAllocator *pLoaderAllocator, int iMethodDescChunkIndex = 0, int iPrecodeChunkIndex = 0);
    void InitCommon()
    {
        WRAPPER_NO_CONTRACT;
        int n = 0;

        m_rgCode[n++] = 0x372e0000; //ori t2, t9, 0
        m_rgCode[n++] = 0xdf390018; //ld  t9,24(t9)
        m_rgCode[n++] = 0x03200008; //jr  t9
        m_rgCode[n++] = 0x34000073; //ori zero,zero,0x73
        m_rgCode[n++] = 0x0; //nop

        _ASSERTE((UINT32*)&m_pTarget == &m_rgCode[n + 1]);

        _ASSERTE(n == _countof(m_rgCode));
    }

    TADDR GetBase()
    {
        LIMITED_METHOD_CONTRACT;
        SUPPORTS_DAC;

        return dac_cast<TADDR>(this) + (m_PrecodeChunkIndex + 1) * sizeof(FixupPrecode);
    }

    TADDR GetMethodDesc();

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pTarget;
    }

    void ResetTargetInterlocked()
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        EnsureWritableExecutablePages(&m_pTarget);
        InterlockedExchange64((LONGLONG*)&m_pTarget, (TADDR)GetEEFuncEntryPoint(PrecodeFixupThunk));
    }

    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        EnsureWritableExecutablePages(&m_pTarget);
        return (TADDR)InterlockedCompareExchange64(
            (LONGLONG*)&m_pTarget, (TADDR)target, (TADDR)expected) == expected;
    }

    static BOOL IsFixupPrecodeByASM(PCODE addr)
    {
        PTR_DWORD pInstr = dac_cast<PTR_DWORD>(PCODEToPINSTR(addr));
        return
            ((pInstr[0] == 0x372e0000) &&
             (pInstr[1] == 0xdf390018) &&
             (pInstr[2] == 0x03200008) &&
             (pInstr[3] == 0x34000073));
    }

#ifdef FEATURE_PREJIT
    // Partial initialization. Used to save regrouped chunks.
    void InitForSave(int iPrecodeChunkIndex);

    void Fixup(DataImage *image, MethodDesc * pMD);
#endif

#ifdef DACCESS_COMPILE
    void EnumMemoryRegions(CLRDataEnumMemoryFlags flags);
#endif
};
typedef DPTR(FixupPrecode) PTR_FixupPrecode;


// Precode to shuffle this and retbuf for closed delegates over static methods with return buffer
struct ThisPtrRetBufPrecode {

    static const int Type = 0x08;

    UINT32  m_rgCode[6];
    TADDR   m_pTarget;
    TADDR   m_pMethodDesc;

    void Init(MethodDesc* pMD, LoaderAllocator *pLoaderAllocator);

    TADDR GetMethodDesc()
    {
        LIMITED_METHOD_DAC_CONTRACT;

        return m_pMethodDesc;
    }

    PCODE GetTarget()
    {
        LIMITED_METHOD_DAC_CONTRACT;
        return m_pTarget;
    }

    BOOL SetTargetInterlocked(TADDR target, TADDR expected)
    {
        CONTRACTL
        {
            THROWS;
            GC_NOTRIGGER;
        }
        CONTRACTL_END;

        EnsureWritableExecutablePages(&m_pTarget);
        return (TADDR)InterlockedCompareExchange64(
            (LONGLONG*)&m_pTarget, (TADDR)target, (TADDR)expected) == expected;
    }
};
typedef DPTR(ThisPtrRetBufPrecode) PTR_ThisPtrRetBufPrecode;

#endif // __cgencpu_h__
