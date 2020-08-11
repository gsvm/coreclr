// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

//
// VirtualCallStubCpu.hpp
//
#ifndef _VIRTUAL_CALL_STUB_MIPS64_H
#define _VIRTUAL_CALL_STUB_MIPS64_H

#define DISPATCH_STUB_FIRST_DWORD 0xdc8c0000
#define RESOLVE_STUB_FIRST_DWORD  0xdc8f0000
#define VTABLECALL_STUB_FIRST_DWORD 0xF9400009

#define LOOKUP_STUB_FIRST_DWORD 0xdf2e0018

#define USES_LOOKUP_STUBS   1

struct LookupStub
{
    inline PCODE entryPoint() { LIMITED_METHOD_CONTRACT; return (PCODE)&_entryPoint[0]; }
    inline size_t token() { LIMITED_METHOD_CONTRACT; return _token; }
    inline size_t size() { LIMITED_METHOD_CONTRACT; return sizeof(LookupStub); }
private :
    friend struct LookupHolder;

    DWORD _entryPoint[4];
    PCODE _resolveWorkerTarget;
    size_t _token;
};

struct LookupHolder
{
private:
    LookupStub _stub;
public:
    static void InitializeStatic() { }

    void  Initialize(PCODE resolveWorkerTarget, size_t dispatchToken)
    {
        // ld  t2, 24(t9)
        // ld  t9, 16(t9)
        // jr  t9
        // nop
        //
        // _resolveWorkerTarget
        // _token

        _stub._entryPoint[0] = LOOKUP_STUB_FIRST_DWORD; //ld  t2,24(t9)
        _stub._entryPoint[1] = 0xdf390010; //ld  t9,16(t9)
        _stub._entryPoint[2] = 0x03200008; //jr  t9
        _stub._entryPoint[3] = 0x00000000; //nop

        _stub._resolveWorkerTarget = resolveWorkerTarget;
        _stub._token               = dispatchToken;
    }

    LookupStub*    stub()        { LIMITED_METHOD_CONTRACT; return &_stub; }
    static LookupHolder*  FromLookupEntry(PCODE lookupEntry)
    {
        return (LookupHolder*) ( lookupEntry - offsetof(LookupHolder, _stub) - offsetof(LookupStub, _entryPoint)  );
    }
};

struct DispatchStub
{
    inline PCODE entryPoint()         { LIMITED_METHOD_CONTRACT; return (PCODE)&_entryPoint[0]; }

    inline size_t expectedMT()  { LIMITED_METHOD_CONTRACT; return _expectedMT; }
    inline PCODE implTarget()  { LIMITED_METHOD_CONTRACT; return _implTarget; }

    inline TADDR implTargetSlot(EntryPointSlots::SlotType *slotTypeRef) const
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(slotTypeRef != nullptr);

        *slotTypeRef = EntryPointSlots::SlotType_Normal;
        return (TADDR)&_implTarget;
    }

    inline PCODE failTarget()  { LIMITED_METHOD_CONTRACT; return _failTarget; }
    inline size_t size()        { LIMITED_METHOD_CONTRACT; return sizeof(DispatchStub); }

private:
    friend struct DispatchHolder;

    DWORD _entryPoint[10];
    size_t  _expectedMT;
    PCODE _implTarget;
    PCODE _failTarget;
};

struct DispatchHolder
{
    static void InitializeStatic()
    {
        LIMITED_METHOD_CONTRACT;

        // Check that _implTarget is aligned in the DispatchHolder for backpatching
        static_assert_no_msg(((offsetof(DispatchHolder, _stub) + offsetof(DispatchStub, _implTarget)) % sizeof(void *)) == 0);
    }

    void  Initialize(PCODE implTarget, PCODE failTarget, size_t expectedMT)
    {
        // ld t0, 0(a0) ; methodTable from object in a0
        // ld  at, 40(t9)    at _expectedMT
        // bne at, t0, failLabel
        // nop
        // ld  t9, 48(t9)    t9 _implTarget
        // jr t9
        // nop
        // failLabel
        // ld  t9, 56(t9)    t9 _failTarget
        // jr t9
        // nop
        //
        //
        // _expectedMT
        // _implTarget
        // _failTarget

        _stub._entryPoint[0] = DISPATCH_STUB_FIRST_DWORD; //ld t0, 0(a0) ; 0xdc990000
        _stub._entryPoint[1] = 0xdf210028; //ld  at,40(t9)
        _stub._entryPoint[2] = 0x142c0004; //bne at,t0, +12
        _stub._entryPoint[3] = 0;//nop
        _stub._entryPoint[4] = 0xdf390030; //ld  t9,48(t9)
        _stub._entryPoint[5] = 0x03200008; //jr  t9
        _stub._entryPoint[6] = 0x00000000; //nop
        _stub._entryPoint[7] = 0xdf390038; //ld  t9,56(t9)
        _stub._entryPoint[8] = 0x03200008; //jr  t9
        _stub._entryPoint[9] = 0; //nop

        _stub._expectedMT = expectedMT;
        _stub._implTarget = implTarget;
        _stub._failTarget = failTarget;
    }

    DispatchStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static DispatchHolder*  FromDispatchEntry(PCODE dispatchEntry)
    {
        LIMITED_METHOD_CONTRACT;
        DispatchHolder* dispatchHolder = (DispatchHolder*) ( dispatchEntry - offsetof(DispatchHolder, _stub) - offsetof(DispatchStub, _entryPoint) );
        return dispatchHolder;
    }

private:
    DispatchStub _stub;
};

struct ResolveStub
{
    inline PCODE failEntryPoint()            { LIMITED_METHOD_CONTRACT; return (PCODE)&_failEntryPoint[0]; }
    inline PCODE resolveEntryPoint()         { LIMITED_METHOD_CONTRACT; return (PCODE)&_resolveEntryPoint[0]; }
    inline PCODE slowEntryPoint()            { LIMITED_METHOD_CONTRACT; return (PCODE)&_slowEntryPoint[0]; }
    inline size_t  token()                   { LIMITED_METHOD_CONTRACT; return _token; }
    inline INT32*  pCounter()                { LIMITED_METHOD_CONTRACT; return _pCounter; }

    inline UINT32  hashedToken()             { LIMITED_METHOD_CONTRACT; return _hashedToken >> LOG2_PTRSIZE;    }
    inline size_t  cacheAddress()            { LIMITED_METHOD_CONTRACT; return _cacheAddress;   }
    inline size_t  size()                    { LIMITED_METHOD_CONTRACT; return sizeof(ResolveStub); }

private:
    friend struct ResolveHolder;
    const static int resolveEntryPointLen = 22;
    const static int slowEntryPointLen = 7;
    const static int failEntryPointLen = 11;

    DWORD _resolveEntryPoint[resolveEntryPointLen];
    DWORD _slowEntryPoint[slowEntryPointLen];
    DWORD _failEntryPoint[failEntryPointLen];
    INT32*  _pCounter;
    size_t  _cacheAddress; // lookupCache
    size_t  _token;
    PCODE   _resolveWorkerTarget;
    UINT32  _hashedToken;
};

struct ResolveHolder
{
    static void  InitializeStatic() { }

    void Initialize(PCODE resolveWorkerTarget, PCODE patcherTarget,
                    size_t dispatchToken, UINT32 hashedToken,
                    void * cacheAddr, INT32 * counterAddr)
    {
        int n=0;
        INT32 pc_offset;

/******** Rough Convention of used in this routine
        ;;ra  temp base address of loading data region
        ;;t8  indirection cell
        ;;t3  MethodTable (from object ref in a0), out: this._token
        ;;t0  hash scratch
        ;;t1  temp
        ;;t2  temp
        ;;AT  hash scratch
        ;;cachemask => [CALL_STUB_CACHE_MASK * sizeof(void*)]

        // Called directly by JITTED code
        // ResolveStub._resolveEntryPoint(a0:Object*, a1 ...,a7, t8:IndirectionCellAndFlags)
        // {
        //    MethodTable mt = a0.m_pMethTab;
        //    int i = ((mt + mt >> 12) ^ this._hashedToken) & _cacheMask
        //    ResolveCacheElem e = this._cacheAddress + i
        //    t1 = e = this._cacheAddress + i
        //    if (mt == e.pMT && this._token == e.token)
        //    {
        //        (e.target)(a0, [a1,...,a7]);
        //    }
        //    else
        //    {
        //        t3 = this._token;
        //        (this._slowEntryPoint)(a0, [a1,.., a7], t8, t3);
        //    }
        // }
 ********/

#define DATA_OFFSET(_member) (INT32)offsetof(ResolveStub, _member)
#define PC_REL_OFFSET(_member, _index) ((((INT32)(offsetof(ResolveStub, _member) - (offsetof(ResolveStub, _resolveEntryPoint[_index]))))>>2) & 0xffff)

        ///;;resolveEntryPoint
        // Called directly by JITTED code
        // ResolveStub._resolveEntryPoint(a0:Object*, a1 ...,a7, t8:IndirectionCellAndFlags)

        // 	ld	t3,0(a0)
        _stub._resolveEntryPoint[n++] = 0xdc8f0000;//RESOLVE_STUB_FIRST_DWORD
        // 	dsrl	t0,t3,0xc
        _stub._resolveEntryPoint[n++] = 0x000f633a;
        // 	daddu	t1,t3,t0
        _stub._resolveEntryPoint[n++] = 0x01ec682d;
        // 	ori	t0,ra,0x0
        _stub._resolveEntryPoint[n++] = 0x37ec0000;
        // 	bal	+8
        _stub._resolveEntryPoint[n++] = 0x04110001;
        // 	nop
        _stub._resolveEntryPoint[n++] = 0x00000000;
        pc_offset = DATA_OFFSET(_resolveEntryPoint[n]);
        // 	lw	at,0(ra)  #AT = this._hashedToken
        _stub._resolveEntryPoint[n++] = 0x8fe10000 | ((DATA_OFFSET(_hashedToken) - pc_offset) & 0xffff);
        // 	xor	t1,t1,at
        _stub._resolveEntryPoint[n++] = 0x01a16826;
        // 	cachemask, FIXME for MIPS: why arm64 & mips64 using this imm?
        _ASSERTE(CALL_STUB_CACHE_MASK * sizeof(void*) == 0x7ff8);
        // 	andi	t1,t1,0x7ff8
        _stub._resolveEntryPoint[n++] = 0x31ad7ff8;
        // 	ld	at,0(ra)
        _stub._resolveEntryPoint[n++] = 0xdfe10000 | ((DATA_OFFSET(_cacheAddress) - pc_offset) & 0xffff);
        // 	daddu	at,at,t1
        _stub._resolveEntryPoint[n++] = 0x002d082d;
        // FIXME for MIPS: address of AT is 8-byte aligned ?
        // 	ld	t1,0(at)    # t1 = e = this._cacheAddress + i
        _stub._resolveEntryPoint[n++] = 0xdc2d0000;
        // 	ld	at,0(t1)  #  AT = Check mt == e.pMT;
        _stub._resolveEntryPoint[n++] = 0xdda10000 | (offsetof(ResolveCacheElem, pMT) & 0xffff);
        // 	ld	t2,0(ra)
        _stub._resolveEntryPoint[n++] = 0xdfee0000 | ((DATA_OFFSET(_token) - pc_offset) & 0xffff);

        // 	bne	at,t3, next
        _stub._resolveEntryPoint[n++] = 0x142f0000 | PC_REL_OFFSET(_slowEntryPoint[0], n+1);

        // 	ori	ra,t0,0x0
        _stub._resolveEntryPoint[n++] = 0x359f0000;
        // 	ld	at,0(t1)      # AT = e.token;
        _stub._resolveEntryPoint[n++] = 0xdda10000 | (offsetof(ResolveCacheElem, token) & 0xffff);
        // 	bne	at,t2, next
        _stub._resolveEntryPoint[n++] = 0x142e0000 | PC_REL_OFFSET(_slowEntryPoint[0], n+1);
        // 	nop
        _stub._resolveEntryPoint[n++] = 0x00000000;

         pc_offset = offsetof(ResolveCacheElem, target) & 0xffffffff;
         _ASSERTE(pc_offset >=0 && pc_offset%8 == 0);
        // 	ld	t9,0(t1)     # t9 = e.target;
        _stub._resolveEntryPoint[n++] = 0xddb90000 | (offsetof(ResolveCacheElem, target) & 0xffff);
        // 	jr	t9
        _stub._resolveEntryPoint[n++] = 0x03200008;
        // 	ori	t3,t9,0x0
        _stub._resolveEntryPoint[n++] = 0x372f0000;

        _ASSERTE(n == ResolveStub::resolveEntryPointLen);
        _ASSERTE(_stub._resolveEntryPoint + n == _stub._slowEntryPoint);

        // ResolveStub._slowEntryPoint(a0:MethodToken, [a1..a7], t8:IndirectionCellAndFlags)
        // {
        //     t2 = this._token;
        //     this._resolveWorkerTarget(a0, [a1..a7], t8, t2);
        // }
#undef PC_REL_OFFSET
#define PC_REL_OFFSET(_member, _index) (((INT32)(offsetof(ResolveStub, _member) - (offsetof(ResolveStub, _slowEntryPoint[_index])))) & 0xffff)
        n = 0;
        // ;;slowEntryPoint:
        // ;;fall through to the slow case

        // 	ori	t0,ra,0x0
        _stub._slowEntryPoint[n++] = 0x37ec0000;
        // 	bal	120000a9c <main+0x64>
        _stub._slowEntryPoint[n++] = 0x04110001;
        // 	nop
        _stub._slowEntryPoint[n++] = 0x00000000;
        // 	ld	t9,0(ra)     # t9 = _resolveWorkerTarget;
        pc_offset = DATA_OFFSET(_slowEntryPoint[n]);
        _stub._slowEntryPoint[n++] = 0xdff90000 | PC_REL_OFFSET(_resolveWorkerTarget, n);

        // 	ld	t2,0(ra)     # t2 = this._token;
        _stub._slowEntryPoint[n++] = 0xdfee0000 | (((DATA_OFFSET(_token) - pc_offset)) & 0xffff);
        // 	jr	t9
        _stub._slowEntryPoint[n++] = 0x03200008;
        // 	ori	ra,t0,0x0
        _stub._slowEntryPoint[n++] = 0x359f0000;

         _ASSERTE(n == ResolveStub::slowEntryPointLen);

        // ResolveStub._failEntryPoint(a0:MethodToken, a1,.., a7, t8:IndirectionCellAndFlags)
        // {
        //     if(--*(this._pCounter) < 0) t8 = t8 | SDF_ResolveBackPatch;
        //     this._resolveEntryPoint(a0, [a1..a7]);
        // }
#undef PC_REL_OFFSET
#define PC_REL_OFFSET(_member, _index) (((INT32)(offsetof(ResolveStub, _member) - (offsetof(ResolveStub, _failEntryPoint[_index])))) & 0xffff)
        n = 0;
        //;;failEntryPoint

        // 	ori	t0,ra,0x0
        _stub._failEntryPoint[n++] = 0x37ec0000;
        // 	bal	120000ab8 <main+0x80>
        _stub._failEntryPoint[n++] = 0x04110001;
        // 	nop
        _stub._failEntryPoint[n++] = 0x00000000;
        // 	ld	t1,0(ra)     # t1 = _pCounter;
        _stub._failEntryPoint[n++] = 0xdfed0000 | PC_REL_OFFSET(_pCounter, n);
        // 	lw	at,0(t1)
        _stub._failEntryPoint[n++] = 0x8da10000;
        // 	addiu	at,at,-1
        _stub._failEntryPoint[n++] = 0x2421ffff;

        // 	sw	at,0(t1)
        _stub._failEntryPoint[n++] = 0xada10000;

        pc_offset = (((DATA_OFFSET(_resolveEntryPoint)-DATA_OFFSET(_failEntryPoint[n+1]))>>2)&0xffff);
        // 	bgez	at,  _resolveEntryPoint
        _stub._failEntryPoint[n++] = 0x04210000 | pc_offset;

        //ori ra,t0,0x0
        _stub._failEntryPoint[n++] = 0x359f0000;

        pc_offset = ((DATA_OFFSET(_resolveEntryPoint) - DATA_OFFSET(_failEntryPoint[n+1]))>>2)&0xffff;
        // 	b	_resolveEntryPoint
        _stub._failEntryPoint[n++] = 0x10000000 | pc_offset;
        // ;; ori t8, t8, SDF_ResolveBackPatch
        // 	ori	t8,t8,0x1
        _stub._failEntryPoint[n++] = 0x37180001;
        _ASSERTE(SDF_ResolveBackPatch == 0x1);

        _ASSERTE(n == ResolveStub::failEntryPointLen);
         _stub._pCounter = counterAddr;
        _stub._hashedToken         = hashedToken << LOG2_PTRSIZE;
        _stub._cacheAddress        = (size_t) cacheAddr;
        _stub._token               = dispatchToken;
        _stub._resolveWorkerTarget = resolveWorkerTarget;

        _ASSERTE(resolveWorkerTarget == (PCODE)ResolveWorkerChainLookupAsmStub);
        _ASSERTE(patcherTarget == NULL);

#undef DATA_OFFSET
#undef PC_REL_OFFSET
#undef Dataregionbase
    }

    ResolveStub* stub()      { LIMITED_METHOD_CONTRACT; return &_stub; }

    static ResolveHolder*  FromFailEntry(PCODE failEntry);
    static ResolveHolder*  FromResolveEntry(PCODE resolveEntry);
private:
    ResolveStub _stub;
};


/*VTableCallStub**************************************************************************************
These are jump stubs that perform a vtable-base virtual call. These stubs assume that an object is placed
in the first argument register (this pointer). From there, the stub extracts the MethodTable pointer, followed by the
vtable pointer, and finally jumps to the target method at a given slot in the vtable.
*/
struct VTableCallStub
{
    friend struct VTableCallHolder;

    inline size_t size()
    {
        LIMITED_METHOD_CONTRACT;

        BYTE* pStubCode = (BYTE *)this;

        size_t cbSize = 4;              // First load instruction

        if (((*(DWORD*)(&pStubCode[cbSize])) & 0xFFFF0000) == 0xdc210000)
        {
            // ld at, offsetOfIndirection(at)
            cbSize += 4;
        }
        else
        {
            // These 3 instructions used when the indirection offset is >= 0x8000
            // lwu  t3, dataOffset(t9)
            // daddu at, t3, at
            // ld at, 0(at)
            cbSize += 12;
        }

        if (((*(DWORD*)(&pStubCode[cbSize])) & 0xFFFF0000) == 0xdc390000)
        {
            // ld t9, offsetAfterIndirection(at)
            cbSize += 4;
        }
        else
        {
            // These 3 instructions used when the indirection offset is >= 0x8000
            // lwu  t3, dataOffset(t9)
            // daddu at, t3, at
            // ld t9, 0(at)
            cbSize += 12;
        }

        return cbSize + 12;// add jr ,nop and the slot value.
    }

    inline PCODE        entryPoint()        const { LIMITED_METHOD_CONTRACT;  return (PCODE)&_entryPoint[0]; }

    inline size_t token()
    {
        LIMITED_METHOD_CONTRACT;
        DWORD slot = *(DWORD*)(reinterpret_cast<BYTE*>(this) + size() - 4);
        return DispatchToken::CreateDispatchToken(slot).To_SIZE_T();
    }

private:
    BYTE    _entryPoint[0];         // Dynamically sized stub. See Initialize() for more details.
};

/* VTableCallHolders are the containers for VTableCallStubs, they provide for any alignment of
stubs as necessary.  */
struct VTableCallHolder
{
    void  Initialize(unsigned slot);

    VTableCallStub* stub() { LIMITED_METHOD_CONTRACT;  return reinterpret_cast<VTableCallStub *>(this); }

    static size_t GetHolderSize(unsigned slot)
    {
        STATIC_CONTRACT_WRAPPER;
        unsigned offsetOfIndirection = MethodTable::GetVtableOffset() + MethodTable::GetIndexOfVtableIndirection(slot) * TARGET_POINTER_SIZE;
        unsigned offsetAfterIndirection = MethodTable::GetIndexAfterVtableIndirection(slot) * TARGET_POINTER_SIZE;
        int indirectionsCodeSize = (offsetOfIndirection >= 0x8000 ? 12 : 4) + (offsetAfterIndirection >= 0x8000 ? 12 : 4);
        int indirectionsDataSize = (offsetOfIndirection >= 0x8000 ? 4 : 0) + (offsetAfterIndirection >= 0x8000 ? 4 : 0);
        return 12 + indirectionsCodeSize + indirectionsDataSize + 4;
    }

    static VTableCallHolder* VTableCallHolder::FromVTableCallEntry(PCODE entry) { LIMITED_METHOD_CONTRACT; return (VTableCallHolder*)entry; }

private:
    // VTableCallStub follows here. It is dynamically sized on allocation because it could
    // use short/long instruction sizes for LDR, depending on the slot value.
};


#ifdef DECLARE_DATA

#ifndef DACCESS_COMPILE
ResolveHolder* ResolveHolder::FromFailEntry(PCODE failEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( failEntry - offsetof(ResolveHolder, _stub) - offsetof(ResolveStub, _failEntryPoint) );
    return resolveHolder;
}

ResolveHolder* ResolveHolder::FromResolveEntry(PCODE resolveEntry)
{
    LIMITED_METHOD_CONTRACT;
    ResolveHolder* resolveHolder = (ResolveHolder*) ( resolveEntry - offsetof(ResolveHolder, _stub) - offsetof(ResolveStub, _resolveEntryPoint) );
    return resolveHolder;
}

void VTableCallHolder::Initialize(unsigned slot)
{
    unsigned offsetOfIndirection = MethodTable::GetVtableOffset() + MethodTable::GetIndexOfVtableIndirection(slot) * TARGET_POINTER_SIZE;
    unsigned offsetAfterIndirection = MethodTable::GetIndexAfterVtableIndirection(slot) * TARGET_POINTER_SIZE;
    _ASSERTE(MethodTable::VTableIndir_t::isRelative == false /* TODO: NYI */);

    VTableCallStub* pStub = stub();
    BYTE* p = (BYTE*)pStub->entryPoint();

    // ld at,(a0) : at = MethodTable pointer
    *(UINT32*)p = 0xdc810000; p += 4;

    if (offsetOfIndirection >= 0x8000)
    {
        uint dataOffset = 12 + (offsetOfIndirection >= 0x8000 ? 12 : 4) + (offsetAfterIndirection >= 0x8000 ? 12 : 4);

        // lwu  t3, dataOffset(t9)
        *(DWORD*)p = 0x9f2f0000 | (UINT32)dataOffset; p += 4;
        // daddu at, t3, at
        *(DWORD*)p = 0x01e1082d; p += 4;
        // ld at, 0(at)
        *(DWORD*)p = 0xdc210000; p += 4;
    }
    else
    {
        // ld at, offsetOfIndirection(at)
        *(DWORD*)p = 0xdc210000 | (UINT32)offsetOfIndirection; p += 4;
    }

    if (offsetAfterIndirection >= 0x8000)
    {
        uint indirectionsCodeSize = (offsetOfIndirection >= 0x8000 ? 12 : 4) + (offsetAfterIndirection >= 0x8000 ? 12 : 4);
        uint indirectionsDataSize = (offsetOfIndirection >= 0x8000 ? 4 : 0);
        uint dataOffset = 12 + indirectionsCodeSize + indirectionsDataSize;

        // lwu t3, dataOffset(t9)
        *(DWORD*)p = 0x9f2f0000 | (UINT32)dataOffset; p += 4;
        // daddu at, t3, at
        *(DWORD*)p = 0x01e1082d; p += 4;
        // ld t9, 0(at)
        *(DWORD*)p = 0xdc390000; p += 4;
    }
    else
    {
        // ld t9, offsetAfterIndirection(at)
        *(DWORD*)p = 0xdc390000 | (UINT32)offsetAfterIndirection; p += 4;
    }

    // jr t9
    *(UINT32*)p = 0x03200008; p += 4;
    // nop
    *(UINT32*)p = 0; p += 4;

    // data labels:
    if (offsetOfIndirection >= 0x8000)
    {
        *(UINT32*)p = (UINT32)offsetOfIndirection;
        p += 4;
    }
    if (offsetAfterIndirection >= 0x8000)
    {
        *(UINT32*)p = (UINT32)offsetAfterIndirection;
        p += 4;
    }

    // Store the slot value here for convenience. Not a real instruction (unreachable anyways)
    // NOTE: Not counted in codeSize above.
    *(UINT32*)p = slot; p += 4;

    _ASSERT(p == (BYTE*)stub()->entryPoint() + VTableCallHolder::GetHolderSize(slot));
    _ASSERT(stub()->size() == VTableCallHolder::GetHolderSize(slot));
}

#endif // DACCESS_COMPILE

VirtualCallStubManager::StubKind VirtualCallStubManager::predictStubKind(PCODE stubStartAddress)
{

    SUPPORTS_DAC;
#ifdef DACCESS_COMPILE

    return SK_BREAKPOINT;  // Dac always uses the slower lookup

#else

    StubKind stubKind = SK_UNKNOWN;
    TADDR pInstr = PCODEToPINSTR(stubStartAddress);

    EX_TRY
    {
        // If stubStartAddress is completely bogus, then this might AV,
        // so we protect it with SEH. An AV here is OK.
        AVInRuntimeImplOkayHolder AVOkay;

        DWORD firstDword = *((DWORD*) pInstr);

        if (firstDword == DISPATCH_STUB_FIRST_DWORD) // assembly of first instruction of DispatchStub : ldr x13, [x0]
        {
            stubKind = SK_DISPATCH;
        }
        else if (firstDword == RESOLVE_STUB_FIRST_DWORD) // assembly of first instruction of ResolveStub : ldr x12, [x0,#Object.m_pMethTab ]
        {
            stubKind = SK_RESOLVE;
        }
        else if (firstDword == VTABLECALL_STUB_FIRST_DWORD) // assembly of first instruction of VTableCallStub : ldr x9, [x0]
        {
            stubKind = SK_VTABLECALL;
        }
        //else if (firstDword == 0x10000089) // assembly of first instruction of LookupStub : adr x9, _resolveWorkerTarget
        else if (firstDword == LOOKUP_STUB_FIRST_DWORD) // first instruction of LookupStub :ori t9,ra,0x0
        {
            stubKind = SK_LOOKUP;
        }
    }
    EX_CATCH
    {
        stubKind = SK_UNKNOWN;
    }
    EX_END_CATCH(SwallowAllExceptions);

    return stubKind;

#endif // DACCESS_COMPILE
}

#endif //DECLARE_DATA

#endif // _VIRTUAL_CALL_STUB_MIPS64_H
