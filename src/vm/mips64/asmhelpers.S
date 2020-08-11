// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

#include "asmconstants.h"
#include "unixasmmacros.inc"

LEAF_ENTRY GetCurrentIP, _TEXT
    .set noreorder
    jr ra
    ori  v0, ra, 0
LEAF_END GetCurrentIP, _TEXT

//-----------------------------------------------------------------------------
// The following Macros help in WRITE_BARRIER Implemetations
// WRITE_BARRIER_ENTRY
//
// Declare the start of a write barrier function. Use similarly to NESTED_ENTRY. This is the only legal way
// to declare a write barrier function.
//
.macro WRITE_BARRIER_ENTRY name
    LEAF_ENTRY \name, _TEXT
.endm

// WRITE_BARRIER_END
//
// The partner to WRITE_BARRIER_ENTRY, used like NESTED_END.
//
.macro WRITE_BARRIER_END name
    LEAF_END_MARKED \name, _TEXT
.endm

// void JIT_UpdateWriteBarrierState(bool skipEphemeralCheck)
//
// Update shadow copies of the various state info required for barrier
//
// State info is contained in a literal pool at the end of the function
// Placed in text section so that it is close enough to use ldr literal and still
// be relocatable. Eliminates need for PREPARE_EXTERNAL_VAR in hot code.
//
// Align and group state info together so it fits in a single cache line
// and each entry can be written atomically
//
WRITE_BARRIER_ENTRY JIT_UpdateWriteBarrierState

    // a0-a7 will contain intended new state
    // t0 will preserve skipEphemeralCheck
    // t2 will be used for pointers

    move  t0, a0

    lui  t2, %hi(%neg(%gp_rel(JIT_UpdateWriteBarrierState)))
    daddu  t2, t2, t9
    daddiu  t2, t2, %lo(%neg(%gp_rel(JIT_UpdateWriteBarrierState)))
    ld  a0, %got_disp(g_card_table)(t2)
    ld  a0, 0(a0)

#ifdef FEATURE_MANUALLY_MANAGED_CARD_BUNDLES
    ld  a1, %got_disp(g_card_bundle_table)(t2)
    ld  a1, 0(a1)
#endif

#ifdef WRITE_BARRIER_CHECK
    ld  a2, %got_disp(g_GCShadow)(t2)
    ld  a2, 0(a2)
#endif

#ifdef FEATURE_USE_SOFTWARE_WRITE_WATCH_FOR_GC_HEAP
    ld  a3, %got_disp(g_sw_ww_table)(t2)
    ld  a3, 0(a3)
#endif

    ld  a4, %got_disp(g_ephemeral_low)(t2)
    ld  a4, 0(a4)

    ld  a5, %got_disp(g_ephemeral_high)(t2)
    ld  a5, 0(a5)

    beq  t0, zero, LOCAL_LABEL(EphemeralCheckEnabled)
    daddiu  a4, zero, 0
    daddiu  a5, zero, 0
LOCAL_LABEL(EphemeralCheckEnabled):

    ld  a6, %got_disp(g_lowest_address)(t2)
    ld  a6, 0(a6)

    ld  a7, %got_disp(g_highest_address)(t2)
    ld  a7, 0(a7)

    lui  t2, %hi(%neg(%gp_rel(JIT_UpdateWriteBarrierState)))
    daddu  t2, t2, t9
    daddiu  t2, t2, %lo(%neg(%gp_rel(JIT_UpdateWriteBarrierState)))
    ld  t2, %got_disp(wbs_begin)(t2)

    sd  a0, 0(t2)
    sd  a1, 8(t2)
    sd  a2, 16(t2)
    sd  a3, 24(t2)
    sd  a4, 32(t2)
    sd  a5, 40(t2)
    sd  a6, 48(t2)
    sd  a7, 56(t2)

    EPILOG_RETURN

WRITE_BARRIER_END JIT_UpdateWriteBarrierState


    // Begin patchable literal pool
    .section .data
    .balign 64  // Align to power of two at least as big as patchable literal pool so that it fits optimally in cache line
.global wbs_begin
wbs_begin:
.global wbs_card_table
wbs_card_table:
    .dword 0
.global wbs_card_bundle_table
wbs_card_bundle_table:
    .dword 0
.global wbs_GCShadow
wbs_GCShadow:
    .dword 0
.global wbs_sw_ww_table
wbs_sw_ww_table:
    .dword 0
.global wbs_ephemeral_low
wbs_ephemeral_low:
    .dword 0
.global wbs_ephemeral_high
wbs_ephemeral_high:
    .dword 0
.global wbs_lowest_address
wbs_lowest_address:
    .dword 0
.global wbs_highest_address
wbs_highest_address:
    .dword 0
    .previous

//
// If a preserved register were pushed onto the stack between
// the managed caller and the H_M_F, ptrS0_S7 will point to its
// location on the stack and it would have been updated on the
// stack by the GC already and it will be popped back into the
// appropriate register when the appropriate epilog is run.
//
// Otherwise, the register is preserved across all the code
// in this HCALL or FCALL, so we need to update those registers
// here because the GC will have updated our copies in the
// frame.
//
// So, if ptrS0_S7 points into the MachState, we need to update
// the register here.  That's what this macro does.
//
.macro RestoreRegMS regIndex, reg
    // Incoming:
    //
    // a0 = address of MachState
    //
    // $regIndex: Index of the register (s0-s7). For s0, index is 16.
    //For s1, index is 17, and so on.
    //
    // $reg: Register name (e.g. s0, s1, etc)
    //
    // Get the address of the specified captured register from machine state
    daddiu  a2, a0, (MachState__captureCalleeSavedRegisters + ((\regIndex-16)*8))

    // Get the content of specified preserved register pointer from machine state
    ld  a3, (MachState__ptrCalleeSavedRegisters + ((\regIndex-16)*8))(a0)

    bne  a2, a3, LOCAL_LABEL(NoRestore_\reg)

    ld  \reg, 0(a2)
LOCAL_LABEL(NoRestore_\reg):

.endm

// ------------------------------------------------------------------
// End of the writeable code region
LEAF_ENTRY JIT_PatchedCodeLast, _TEXT
    jr  ra

LEAF_END JIT_PatchedCodeLast, _TEXT

// void JIT_WriteBarrier(Object** dst, Object* src)
// On entry:
//   v0  : the destination address (LHS of the assignment)
//   v1  : the object reference (RHS of the assignment)
//
// On exit:
//   AT  : trashed
//   t0  : trashed
//   t1  : trashed
//   t2  : trashed
//   t3  : trashed
//   v0  : trashed (incremented by 8 to implement JIT_ByRefWriteBarrier contract)
//   v1  : trashed
//
WRITE_BARRIER_ENTRY JIT_WriteBarrier
    .set noreorder

    sync 18
    sd  v1, 0(v0)

    lui  gp, %hi(%neg(%gp_rel(JIT_WriteBarrier)))
    daddu  gp, gp, t9
    daddiu gp, gp, %lo(%neg(%gp_rel(JIT_WriteBarrier)))
#ifdef WRITE_BARRIER_CHECK
    // Update GC Shadow Heap

    // Do not perform the work if g_GCShadow is 0
    ld t1, %got_disp(wbs_GCShadow)(gp)
    ld t1, 0(t1)

    beq  t1, zero, 22f //LOCAL_LABEL(ShadowUpdateDisabled)
    nop

    // Compute address of shadow heap location:
    //   pShadow = g_GCShadow + (v0 - g_lowest_address)
    ld  t3, %got_disp(wbs_lowest_address) (gp)
    ld t3, 0(t3)

    dsubu  t3, v0, t3
    daddu  t0, t3, t1

    // if (pShadow >= g_GCShadowEnd) goto end
    ld t3, %got_disp(g_GCShadowEnd)(gp)
    ld  t3, 0(t3)

    slt  AT, t0, t3
    beq  AT, zero, 22f //LOCAL_LABEL(ShadowUpdateEnd)
    nop

    // *pShadow = v1
    sd  v1, 0(t0)

    // Ensure that the write to the shadow heap occurs before the read from the GC heap so that race
    // conditions are caught by INVALIDGCVALUE.
    sync

    // if (*v0 == v1) goto end
    ld  t3, 0(v0)
    beq  t3, v1, 22f //LOCAL_LABEL(ShadowUpdateEnd)
    nop

    // *pShadow = INVALIDGCVALUE (0xcccccccd)
    lui  t3, 0xcccc
    ori  t3, t3, 0xcccd
    sd  t3, 0(t0)
22:
//LOCAL_LABEL(ShadowUpdateEnd):
//LOCAL_LABEL(ShadowUpdateDisabled):
#endif

#ifdef FEATURE_USE_SOFTWARE_WRITE_WATCH_FOR_GC_HEAP
    // Update the write watch table if necessary

    ld  t3, %got_disp(wbs_sw_ww_table)(gp)
    ld  t3, 0(t3)
    beq  t3, zero, 1f //LOCAL_LABEL(CheckCardTable)
    nop

    dsrl  AT, v0, 0xc
    daddu  t3, t3, AT  // SoftwareWriteWatch::AddressToTableByteIndexShift
    lb  AT, 0(t3)
    bne  AT, zero, 1f //LOCAL_LABEL(CheckCardTable)
    nop

    addiu  AT, zero, 0xFF
    sb  AT, 0(t3)
1:
//LOCAL_LABEL(CheckCardTable):
#endif
    // Branch to Exit if the reference is not in the Gen0 heap
    ld  t3, %got_disp(wbs_ephemeral_low)(gp)
    ld  t3, 0(t3)
    beq  t3, zero, 2f //LOCAL_LABEL(SkipEphemeralCheck)
    nop

    slt  AT, v1, t3
    ld  t3, %got_disp(wbs_ephemeral_high)(gp)
    ld  t3, 0(t3)
    slt  t1, t3, v1
    or  AT, t1, AT
    bne  AT, zero, 800f //LOCAL_LABEL(Exit)
    nop
2:
//LOCAL_LABEL(SkipEphemeralCheck):
    // Check if we need to update the card table
    ld  t3, %got_disp(wbs_card_table)(gp)
    ld  t3, 0(t3)
    dsrl  AT, v0, 11
    daddu  v1, t3, AT
    lbu  t1, 0(v1)
    ori  AT, zero, 0xFF
    beq  t3, AT, 800f //LOCAL_LABEL(Exit)
    nop

    ori  t3, zero, 0xFF
    sb  t3, 0(v1)

#ifdef FEATURE_MANUALLY_MANAGED_CARD_BUNDLES
    // Check if we need to update the card bundle table
    ld  t3, %got_disp(wbs_card_bundle_table)(gp)
    ld  t3, 0(t3)
    dsrl  AT, v0, 21
    daddu  v1, t3, AT

    lb  t3, 0(v1)
    addiu  AT, zero, 0xFF
    ext  t3, t3, 0, 8
    beq  t3, AT, 800f //LOCAL_LABEL(Exit)
    nop

    addiu  t3, zero, 0xFF
    sb  t3, 0(v1)
#endif
800:
//LOCAL_LABEL(Exit):
    jr  ra
    daddiu  v0, v0, 8

WRITE_BARRIER_END JIT_WriteBarrier

// ------------------------------------------------------------------
// The call in fixup precode initally points to this function.
// The pupose of this function is to load the MethodDesc and forward the call to prestub.
NESTED_ENTRY PrecodeFixupThunk, _TEXT, NoHandler
    //    t2 = FixupPrecode *
    // On Exit
    //    t2 = MethodDesc*
    //    t3, t1, t0 Trashed
    // Inline computation done by FixupPrecode::GetMethodDesc()
    .set noreorder
    lbu  t3, Offset_PrecodeChunkIndex(METHODDESC_REGISTER)    //m_PrecodeChunkIndex
    lbu  t0, Offset_MethodDescChunkIndex(METHODDESC_REGISTER) //m_MethodDescChunkIndex

    dsll   t1, t3, FixupPrecode_ALIGNMENT_SHIFT_1
    daddu  METHODDESC_REGISTER, METHODDESC_REGISTER, t1
    ld  t3, SIZEOF__FixupPrecode(METHODDESC_REGISTER)
    dsll   t1, t0, MethodDesc_ALIGNMENT_SHIFT
    daddu  METHODDESC_REGISTER, t3, t1      // pMethodDesc

    b  ThePreStub
    daddiu  t9, t9, %lo(C_FUNC(ThePreStub) - C_FUNC(PrecodeFixupThunk))   //NOTE: here 16-bits is enough.
NESTED_END PrecodeFixupThunk, _TEXT

NESTED_ENTRY ThePreStub, _TEXT, NoHandler
    .set noreorder

    PROLOG_WITH_TRANSITION_BLOCK

    move  a1, METHODDESC_REGISTER // pMethodDesc

    lui  AT, %hi(%neg(%gp_rel(ThePreStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(ThePreStub)))
    ld  t9, %got_disp(PreStubWorker)(AT)
    jalr  t9
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock

    move  t9, v0

    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
    EPILOG_BRANCH_REG  t9
    nop

NESTED_END ThePreStub, _TEXT

// ------------------------------------------------------------------\

// EXTERN_C int __fastcall HelperMethodFrameRestoreState(
// INDEBUG_COMMA(HelperMethodFrame *pFrame)
// MachState *pState
// )
LEAF_ENTRY HelperMethodFrameRestoreState, _TEXT
#ifdef _DEBUG
    move  a0, a1
#endif

    // If machine state is invalid, then simply exit
    lw  a1, MachState__isValid(a0)
    beq  a1, zero, LOCAL_LABEL(Done)

    RestoreRegMS  16, s0
    RestoreRegMS  17, s1
    RestoreRegMS  18, s2
    RestoreRegMS  19, s3
    RestoreRegMS  20, s4
    RestoreRegMS  21, s5
    RestoreRegMS  22, s6
    RestoreRegMS  23, s7
    RestoreRegMS  28, gp
    RestoreRegMS  30, fp
    RestoreRegMS  31, ra
LOCAL_LABEL(Done):
    // Its imperative that the return value of HelperMethodFrameRestoreState is zero
    // as it is used in the state machine to loop until it becomes zero.
    // Refer to HELPER_METHOD_FRAME_END macro for details.
    daddiu  v0, zero, 0
    jr  ra

LEAF_END HelperMethodFrameRestoreState, _TEXT

//-----------------------------------------------------------------------------
// This routine captures the machine state. It is used by helper method frame
//-----------------------------------------------------------------------------
//void LazyMachStateCaptureState(struct LazyMachState *pState)//
LEAF_ENTRY LazyMachStateCaptureState, _TEXT
    // marks that this is not yet valid
    sw  zero, MachState__isValid(a0)

    sd  ra, LazyMachState_captureIp(a0)

    // save sp register.
    sd  sp, LazyMachState_captureSp(a0)

    // save non-volatile registers that can contain object references
    daddiu  a1, a0, LazyMachState_captureCalleeSavedRegisters
    sd  s0, 0(a1)
    sd  s1, 8(a1)
    sd  s2, 16(a1)
    sd  s3, 24(a1)
    sd  s4, 32(a1)
    sd  s5, 40(a1)
    sd  s6, 48(a1)
    sd  s7, 56(a1)
    sd  gp, 64(a1)
    sd  fp, 72(a1)
    sd  ra, 80(a1)

    jr  ra

LEAF_END LazyMachStateCaptureState, _TEXT

// ------------------------------------------------------------------
// The call in ndirect import precode points to this function.
NESTED_ENTRY NDirectImportThunk, _TEXT, NoHandler

    .set noreorder
    PROLOG_SAVE_REG_PAIR_INDEXED  fp, ra, 0xa0
    PROLOG_SAVE_REG gp, 16
    SAVE_ARGUMENT_REGISTERS  sp, 0x20
    SAVE_FLOAT_ARGUMENT_REGISTERS  sp, 0x60

    lui  t0, %hi(%neg(%gp_rel(NDirectImportThunk)))
    daddu  t0, t0, t9
    daddiu  t0, t0, %lo(%neg(%gp_rel(NDirectImportThunk)))
    ld  t9, %call16(NDirectImportWorker)(t0)
    jalr  t9
    move  a0, t2

    move  t9, v0

    // pop the stack and restore original register state
    RESTORE_FLOAT_ARGUMENT_REGISTERS  sp, 0x60
    RESTORE_ARGUMENT_REGISTERS  sp, 0x20
    EPILOG_RESTORE_REG  gp, 16
    EPILOG_RESTORE_REG_PAIR_INDEXED  fp, ra, 0xa0

    // If we got back from NDirectImportWorker, the MD has been successfully
    // linked. Proceed to execute the original DLL call.
    EPILOG_BRANCH_REG  t9
    nop

NESTED_END NDirectImportThunk, _TEXT

#ifdef FEATURE_PREJIT
//------------------------------------------------
// VirtualMethodFixupStub
//
// In NGEN images, virtual slots inherited from cross-module dependencies
// point to a jump thunk that calls into the following function that will
// call into a VM helper. The VM helper is responsible for patching up
// thunk, upon executing the precode, so that all subsequent calls go directly
// to the actual method body.
//
// This is done lazily for performance reasons.
//
// On entry:
//
// a0 = "this" pointer
// t2 = Address of thunk

NESTED_ENTRY VirtualMethodFixupStub, _TEXT, NoHandler

    // Save arguments and return address
    PROLOG_SAVE_REG_PAIR_INDEXED  fp, ra, 0xa0
    PROLOG_SAVE_REG  gp, 16
    SAVE_ARGUMENT_REGISTERS  sp, 32
    SAVE_FLOAT_ARGUMENT_REGISTERS  sp, 96


    // Call the helper in the VM to perform the actual fixup
    // and tell us where to tail call. a0 already contains
    // the this pointer.

    lui  AT, %hi(%neg(%gp_rel(VirtualMethodFixupStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(VirtualMethodFixupStub)))
    ld  t9, %call16(VirtualMethodFixupWorker)(AT)
    // Move the thunk start address in a1
    move  a1, t2
    jalr  t9

    // On return, v0 contains the target to tailcall to
    // FIXME for MIPS64: should using t2 or t9 ???
    move  t9, v0

    // pop the stack and restore original register state
    RESTORE_FLOAT_ARGUMENT_REGISTERS  sp, 96
    RESTORE_ARGUMENT_REGISTERS  sp, 32
    EPILOG_RESTORE_REG  gp, 16
    EPILOG_RESTORE_REG_PAIR_INDEXED  fp, ra, 0xa0

    PATCH_LABEL  VirtualMethodFixupPatchLabel

    // and tailcall to the actual method
    EPILOG_BRANCH_REG  t9

NESTED_END VirtualMethodFixupStub, _TEXT
#endif // FEATURE_PREJIT

// void SinglecastDelegateInvokeStub(Delegate *pThis)
LEAF_ENTRY SinglecastDelegateInvokeStub, _TEXT
    .set noreorder
    beq  a0, zero, LOCAL_LABEL(LNullThis)
    nop

    ld  t9, DelegateObject___methodPtr(a0)
    jr  t9
    ld  a0, DelegateObject___target(a0)

LOCAL_LABEL(LNullThis):
    lui  AT, %hi(%neg(%gp_rel(SinglecastDelegateInvokeStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(SinglecastDelegateInvokeStub)))
    ld  t9, %call16(JIT_InternalThrow)(AT)
    daddiu  a0, zero, CORINFO_NullReferenceException_ASM
    jr t9

LEAF_END SinglecastDelegateInvokeStub, _TEXT

// void JIT_ByRefWriteBarrier
//
// On entry:
//   t8 : the source address (points to object reference to write)
//   v0 : the destination address (object reference written here)
//
// On exit:
//   t8  : incremented by 8
//   v1  : trashed
//
WRITE_BARRIER_ENTRY JIT_ByRefWriteBarrier

    .set noreorder
    ld  v1, 0(t8)
    daddiu  t8, t8, 8
    b  C_FUNC(JIT_CheckedWriteBarrier)
    daddiu  t9, t9, %lo(C_FUNC(JIT_CheckedWriteBarrier) - C_FUNC(JIT_ByRefWriteBarrier))   //NOTE: here 16-bits is enough.

WRITE_BARRIER_END JIT_ByRefWriteBarrier

//-----------------------------------------------------------------------------
// Simple WriteBarriers
// void JIT_CheckedWriteBarrier(Object** dst, Object* src)
//
// On entry:
//   v0 : the destination address (LHS of the assignment)
//   v1 : the object reference (RHS of the assignment)
//
// On exit:
//   t1  : trashed
//   t2  : trashed
//   t3  : trashed
//   AT  : trashed
//   v0  : trashed (incremented by 8 to implement JIT_ByRefWriteBarrier contract)
//
WRITE_BARRIER_ENTRY JIT_CheckedWriteBarrier

    .set noreorder

    lui  t2, %hi(%neg(%gp_rel(JIT_CheckedWriteBarrier)))
    daddu  t2, t2, t9
    daddiu  t2, t2, %lo(%neg(%gp_rel(JIT_CheckedWriteBarrier)))
    ld  t3, %got_disp(wbs_lowest_address)(t2)
    ld  t3, 0(t3)
    slt  AT, v0, t3

    ld  t1, %got_disp(wbs_highest_address)(t2)
    ld  t1, 0(t1)
    slt  t2, t1, v0
    or  AT, t2, AT
    beq  AT, zero, C_FUNC(JIT_WriteBarrier)
    daddiu  t9, t9, %lo(C_FUNC(JIT_WriteBarrier) - C_FUNC(JIT_CheckedWriteBarrier))   //NOTE: here 16-bits is enough.

    sd  v1, 0(v0)
    jr  ra
    daddiu  v0, v0, 8
WRITE_BARRIER_END JIT_CheckedWriteBarrier

// ------------------------------------------------------------------
// ThePreStubPatch()

LEAF_ENTRY ThePreStubPatch, _TEXT
    nop
.globl C_FUNC(ThePreStubPatchLabel)
C_FUNC(ThePreStubPatchLabel):
    jr  ra
LEAF_END ThePreStubPatch, _TEXT

#ifdef FEATURE_PREJIT
// ------------------------------------------------------------------
// void StubDispatchFixupStub(args in regs a0-a7 & stack, t1:IndirectionCellAndFlags)
//
// The stub dispatch thunk which transfers control to StubDispatchFixupWorker.
NESTED_ENTRY StubDispatchFixupStub, _TEXT, NoHandler
    .set noreorder

    PROLOG_WITH_TRANSITION_BLOCK

    daddiu  AT, zero, -4
    and  a1, t8, AT // Indirection cell

    lui  AT, %hi(%neg(%gp_rel(StubDispatchFixupStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(StubDispatchFixupStub)))
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock
    ld  t9, %call16(StubDispatchFixupWorker)(AT)
    ori  a2, zero, 0 // sectionIndex
    jalr  t9
    ori  a3, zero, 0 // pModule

    move  t9, v0

    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
    PATCH_LABEL StubDispatchFixupPatchLabel
    EPILOG_BRANCH_REG  t9
    nop

NESTED_END StubDispatchFixupStub, _TEXT
#endif

//
// t2 = UMEntryThunk*
//
NESTED_ENTRY TheUMEntryPrestub, _TEXT, UnhandledExceptionHandlerUnix

    // Save arguments and return address
    PROLOG_SAVE_REG_PAIR_INDEXED  fp, ra, 0xa0
    PROLOG_SAVE_REG  gp, 16
    SAVE_ARGUMENT_REGISTERS  sp, 32
    SAVE_FLOAT_ARGUMENT_REGISTERS  sp, 96


    lui  AT, %hi(%neg(%gp_rel(TheUMEntryPrestub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(TheUMEntryPrestub)))
    ld  t9, %call16(TheUMEntryPrestubWorker)(AT)
    move  a0, t2
    jalr  t9

    move  t9, v0

    // pop the stack and restore original register state
    RESTORE_FLOAT_ARGUMENT_REGISTERS  sp, 96
    RESTORE_ARGUMENT_REGISTERS  sp, 32
    EPILOG_RESTORE_REG  gp, 16
    EPILOG_RESTORE_REG_PAIR_INDEXED  fp, ra, 0xa0

    // and tailcall to the actual method
    EPILOG_BRANCH_REG  t9

NESTED_END TheUMEntryPrestub, _TEXT

// ------------------------------------------------------------------
// void* JIT_GetSharedGCStaticBase(SIZE_T moduleDomainID, DWORD dwClassDomainID)

LEAF_ENTRY JIT_GetSharedGCStaticBase_SingleAppDomain, _TEXT
    // If class is not initialized, bail to C++ helper
    daddiu a2, a0, DomainLocalModule__m_pDataBlob
    ext a1, a1, 0, 32
    daddu a2, a2, a1
    lb a2, 0(a2)
    andi  t8, a2, 1
    beq  t8, zero, 1f //LOCAL_LABEL(JIT_GetSharedGCStaticBase_SingleAppDomain_CallHelper)

    ld  v0, DomainLocalModule__m_pGCStatics(a0)
    jr  ra

1:
//LOCAL_LABEL(JIT_GetSharedGCStaticBase_SingleAppDomain_CallHelper):
    // Tail call JIT_GetSharedGCStaticBase_Helper
    lui  AT, %hi(%neg(%gp_rel(JIT_GetSharedGCStaticBase_SingleAppDomain)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_GetSharedGCStaticBase_SingleAppDomain)))
    ld  t9, %call16(JIT_GetSharedGCStaticBase_Helper)(AT)
    jr t9

LEAF_END JIT_GetSharedGCStaticBase_SingleAppDomain, _TEXT

// ------------------------------------------------------------------
// ResolveWorkerChainLookupAsmStub
//
// This method will perform a quick chained lookup of the entry if the
//  initial cache lookup fails.
//
// On Entry:
//   t1       contains the pointer to the current ResolveCacheElem
//   t8       contains the address of the indirection (and the flags in the low two bits)
//   t2       contains our contract the DispatchToken
// Must be preserved:
//   a0       contains the instance object ref that we are making an interface call on
//   t1       Must point to a ResolveCacheElem [For Sanity]
//  [a1-a7]   contains any additional register arguments for the interface method
//
// Loaded from a0
//   t3       contains our type     the MethodTable  (from object ref in a0)
//
// On Exit:
//   a0, [a1-a7] arguments for the interface implementation target
//
// On Exit (to ResolveWorkerAsmStub):
//   t8       contains the address of the indirection and the flags in the low two bits.
//   t2       contains our contract (DispatchToken)
//   at,v0,t9 will be trashed
//

#define BACKPATCH_FLAG      1
#define PROMOTE_CHAIN_FLAG  2

NESTED_ENTRY ResolveWorkerChainLookupAsmStub, _TEXT, NoHandler

    andi  AT, t8, BACKPATCH_FLAG     // First we check if t8 has the BACKPATCH_FLAG set
    bne  AT, zero, LOCAL_LABEL(Fail) // If the BACKPATCH_FLAGS is set we will go directly to the ResolveWorkerAsmStub

    ld  t3, 0(a0)         // retrieve the MethodTable from the object ref in a0
LOCAL_LABEL(MainLoop):
    ld  t1, ResolveCacheElem__pNext(t1)     // t1 <= the next entry in the chain
    beq  t1, zero, LOCAL_LABEL(Fail)

    ld  AT, 0(t1)
    ld  v0, 8(t1)
    // compare our MT with the one in the ResolveCacheElem
    bne  AT, t3, LOCAL_LABEL(MainLoop)

    // compare our DispatchToken with one in the ResolveCacheElem
    bne  t2, v0, LOCAL_LABEL(MainLoop)

LOCAL_LABEL(Success):
    PREPARE_EXTERNAL_VAR  ResolveWorkerChainLookupAsmStub, g_dispatch_cache_chain_success_counter, t3
    ld  AT, 0(t3)
    daddiu AT, AT, -1
    sd  AT, 0(t3)
    bltz AT, LOCAL_LABEL(Promote)

    ld  t9, ResolveCacheElem__target(t1)    // get the ImplTarget
    jr  t9                // branch to interface implemenation target

LOCAL_LABEL(Promote):
                          // Move this entry to head postion of the chain
    daddiu  AT, zero, 256
    sd  AT, 0(t3)         // be quick to reset the counter so we don't get a bunch of contending threads
    ori  t8, t8, PROMOTE_CHAIN_FLAG   // set PROMOTE_CHAIN_FLAG
    move  t2, t1           // We pass the ResolveCacheElem to ResolveWorkerAsmStub instead of the DispatchToken

LOCAL_LABEL(Fail):
    lui  AT, %hi(%neg(%gp_rel(ResolveWorkerChainLookupAsmStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(ResolveWorkerChainLookupAsmStub)))
    ld  t9, %call16(ResolveWorkerAsmStub)(AT)
    b       ResolveWorkerAsmStub // call the ResolveWorkerAsmStub method to transition into the VM

NESTED_END ResolveWorkerChainLookupAsmStub, _TEXT


//NOTE:  Frame_Size = SIZEOF__ArgumentRegisters + SIZEOF__FloatArgumentRegisters + extra.
//
//     |gp   |
//     |s0   |
//     |t2   |
//     |t9   |
//     |a7   |
//     |a6   |
//     |a5   |
//     |a4   |
//     |a3   |
//     |a2   |
//     |a1   |
//     |a0   |
//     |ra   | sp+8
//     |fp   | sp
//
//     |f19  | if needed.
//     |f18  |
//     |f17  |
//     |f16  |
//     |f15  |
//     |f14  |
//     |f13  |
//     |f12  |
//
#define UMThunkStub_Offset_t9    0x50
#define UMThunkStub_Offset_Entry 0x58   // offset of saved UMEntryThunk *
#define UMThunkStub_Offset_s0    0x60
#define UMThunkStub_StackArgs    0x70   // Frame size.

//
// t2 = UMEntryThunk*
//
NESTED_ENTRY UMThunkStub, _TEXT, UnhandledExceptionHandlerUnix

    // Save arguments and return address
    PROLOG_SAVE_REG_PAIR_INDEXED   fp, ra, UMThunkStub_StackArgs
    // save callee saved reg s0, used to store thread*
    PROLOG_SAVE_REG_PAIR s0, gp, UMThunkStub_Offset_s0

    // save UMEntryThunk* and t9
    sd t9, UMThunkStub_Offset_t9(sp)
    sd t2, UMThunkStub_Offset_Entry(sp)

    SAVE_ARGUMENT_REGISTERS  sp, 16

    lui  AT, %hi(%neg(%gp_rel(UMThunkStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(UMThunkStub)))
    ld  t9, %call16(GetThread)(AT)
    // assuming GetThread does not clobber FP Args
    jalr t9

    beq  v0, zero, 800f //LOCAL_LABEL(UMThunkStub_DoThreadSetup)

LOCAL_LABEL(UMThunkStub_HaveThread):
    move  s0, v0  // s0 = Thread *

    ori  AT, zero, 1
    // m_fPreemptiveGCDisabled is 4 byte field so using 32-bit variant
    ld  t9, UMThunkStub_Offset_t9(fp)
    sw  AT, Thread__m_fPreemptiveGCDisabled(v0)

    PREPARE_EXTERNAL_VAR  UMThunkStub, g_TrapReturningThreads, a2
    lw  a3, 0(a2)
    or  a0, s0, zero
    // assuming a0 contains Thread* before jumping to UMThunkStub_DoTrapReturningThreads
    bne  a3, zero, 801f //LOCAL_LABEL(UMThunkStub_DoTrapReturningThreads)

LOCAL_LABEL(UMThunkStub_InCooperativeMode):
    ld  t2, UMThunkStub_Offset_Entry(fp) // t2 = UMEntryThunk*
    ld  a3, UMEntryThunk__m_pUMThunkMarshInfo(t2) // a3 = m_pUMThunkMarshInfo

    // m_cbActualArgSize is UINT32 and hence occupies 4 bytes
    lwu  a2, UMThunkMarshInfo__m_cbActualArgSize(a3) // a2 = Stack arg bytes
    beq  a2, zero, 803f //LOCAL_LABEL(UMThunkStub_RegArgumentsSetup)

    // Source pointer.
    daddiu  a0, fp, UMThunkStub_StackArgs

    // move source pointer to end of Stack Args
    daddu  a0, a0, a2

    // Count of stack slot to copy (divide by 8)
    srl  a1, a2, 3

    andi  AT, a1, 1
    sll  a4, AT, 3       //padding size on high-addr.
    addu  AT, AT, a1
    sll  AT, AT, 3
    dsubu  t9, sp, a4   //t9: beginning of stack slot.
    dsubu  sp, sp, AT   //stack-16byte aligned.

LOCAL_LABEL(UMThunkStub_StackLoop):
    daddiu  a0, a0, -8
    ld  a4, 0(a0)
    daddiu  t9, t9, -8
    sd  a4, 0(t9)
    daddiu  a1, a1, -1
    bne zero, a1, LOCAL_LABEL(UMThunkStub_StackLoop)
    //nop   //NOTE:here is a delay-slot nop by default.

803:
//LOCAL_LABEL(UMThunkStub_RegArgumentsSetup):
    ld  t9, UMThunkMarshInfo__m_pILStub(a3)

    RESTORE_ARGUMENT_REGISTERS fp, 16
    jalr  t9

//LOCAL_LABEL(UMThunkStub_PostCall):
    // m_fPreemptiveGCDisabled is 4 byte field so using 32-bit variant
    sw  zero, Thread__m_fPreemptiveGCDisabled(s0)

    EPILOG_STACK_RESTORE

    EPILOG_RESTORE_REG_PAIR s0, gp, UMThunkStub_Offset_s0
    EPILOG_RESTORE_REG_PAIR_INDEXED   fp, ra, UMThunkStub_StackArgs

    EPILOG_RETURN

800:
//LOCAL_LABEL(UMThunkStub_DoThreadSetup):
    daddiu sp, sp, -SIZEOF__FloatArgumentRegisters
    SAVE_FLOAT_ARGUMENT_REGISTERS  sp, 0
    ld  t9, UMThunkStub_Offset_t9(fp)

    lui  AT, %hi(%neg(%gp_rel(UMThunkStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(UMThunkStub)))
    ld  t9, %call16(CreateThreadBlockThrow)(AT)
    jalr t9

    RESTORE_FLOAT_ARGUMENT_REGISTERS  sp, 0
    daddiu sp, sp, SIZEOF__FloatArgumentRegisters
    b  LOCAL_LABEL(UMThunkStub_HaveThread)

801:
//LOCAL_LABEL(UMThunkStub_DoTrapReturningThreads):
    daddiu sp, sp, -SIZEOF__FloatArgumentRegisters
    SAVE_FLOAT_ARGUMENT_REGISTERS  sp, 0
    // a0 already contains Thread* pThread
    // UMEntryThunk* pUMEntry
    ld  t9, UMThunkStub_Offset_t9(fp)
    ld  a1, UMThunkStub_Offset_Entry(fp)

    lui  AT, %hi(%neg(%gp_rel(UMThunkStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(UMThunkStub)))
    ld  t9, %call16(UMThunkStubRareDisableWorker)(AT)
    jalr t9

    RESTORE_FLOAT_ARGUMENT_REGISTERS  sp, 0
    daddiu sp, sp, SIZEOF__FloatArgumentRegisters
    b  LOCAL_LABEL(UMThunkStub_InCooperativeMode)

NESTED_END UMThunkStub, _TEXT

// ------------------------------------------------------------------
// Start of the writeable code region
LEAF_ENTRY JIT_PatchedCodeStart, _TEXT
    jr  ra
LEAF_END JIT_PatchedCodeStart, _TEXT

// ------------------------------------------------------------------
// void ResolveWorkerAsmStub(args in regs a0-a7 & stack, t8:IndirectionCellAndFlags, t2:DispatchToken)
//
// The stub dispatch thunk which transfers control to VSD_ResolveWorker.
NESTED_ENTRY ResolveWorkerAsmStub, _TEXT, NoHandler

    PROLOG_WITH_TRANSITION_BLOCK

    lui  AT, %hi(%neg(%gp_rel(ResolveWorkerAsmStub)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(ResolveWorkerAsmStub)))
    move  a2, t2                 // DispatchToken
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock
    ld  t9, %call16(VSD_ResolveWorker)(AT)
    daddiu  AT, zero, -4
    andi  a3, t8, 3              // flag
    and  a1, t8, AT              // Indirection cell
    jalr  t9

    move  t9, v0

    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL

    EPILOG_BRANCH_REG  t9

NESTED_END ResolveWorkerAsmStub, _TEXT

// ------------------------------------------------------------------
// void* JIT_GetSharedNonGCStaticBaseNoCtor(SIZE_T moduleDomainID, DWORD dwClassDomainID)

LEAF_ENTRY JIT_GetSharedNonGCStaticBaseNoCtor_SingleAppDomain, _TEXT
    jr  ra
LEAF_END JIT_GetSharedNonGCStaticBaseNoCtor_SingleAppDomain, _TEXT

// ------------------------------------------------------------------
// void* JIT_GetSharedGCStaticBaseNoCtor(SIZE_T moduleDomainID, DWORD dwClassDomainID)

LEAF_ENTRY JIT_GetSharedGCStaticBaseNoCtor_SingleAppDomain, _TEXT
    ////FIXME for MIPS64.
    ld  v0, DomainLocalModule__m_pGCStatics(a0)
    jr  ra
LEAF_END JIT_GetSharedGCStaticBaseNoCtor_SingleAppDomain, _TEXT

// ------------------------------------------------------------------
// __declspec(naked) void F_CALL_CONV JIT_Stelem_Ref(PtrArray* array, unsigned idx, Object* val)
LEAF_ENTRY JIT_Stelem_Ref, _TEXT
    .set noreorder

    ////FIXME for MIPS64.
    // We retain arguments as they were passed and use a0 = array, a1 = idx, a2 = val

    // check for null array
    beq  a0, zero, LOCAL_LABEL(ThrowNullReferenceException)
    // idx bounds check
    ld  a3, ArrayBase__m_NumComponents(a0)
    slt  AT, a1, a3
    beq  AT, zero, LOCAL_LABEL(ThrowIndexOutOfRangeException)
    nop

    // fast path to null assignment (doesn't need any write-barriers)
    beq  a2, zero, LOCAL_LABEL(AssigningNull)
    nop

    // Verify the array-type and val-type matches before writing
    ld  t2, 0(a0) // t2 = array MT
    ld  a3, 0(a2) // a3 = val->GetMethodTable()
    ld  t2, MethodTable__m_ElementType(t2) // array->GetArrayElementTypeHandle()
    beq  a3, t2, C_FUNC(JIT_Stelem_DoWrite)
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_DoWrite) - C_FUNC(JIT_Stelem_Ref))   //NOTE: here 16-bits is enough.
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_Ref) - C_FUNC(JIT_Stelem_DoWrite))   //NOTE: here 16-bits is enough.

    // Types didnt match but allow writing into an array of objects
    lui  a3, %hi(%neg(%gp_rel(JIT_Stelem_Ref)))
    daddu  AT, a3, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_Stelem_Ref)))
    ld  a3, %got_disp(g_pObjectClass)(AT)
    ld     a3, 0(a3)  // a3 = *g_pObjectClass
    // array type matches with Object*
    beq  a3, t2, C_FUNC(JIT_Stelem_DoWrite)
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_DoWrite) - C_FUNC(JIT_Stelem_Ref))   //NOTE: here 16-bits is enough.
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_Ref) - C_FUNC(JIT_Stelem_DoWrite))   //NOTE: here 16-bits is enough.

    // array type and val type do not exactly match. Raise frame and do detailed match
    b  C_FUNC(JIT_Stelem_Ref_NotExactMatch)
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_Ref_NotExactMatch) - C_FUNC(JIT_Stelem_Ref))   //NOTE: here 16-bits is enough.

LOCAL_LABEL(AssigningNull):
    // Assigning null doesn't need write barrier
    dsll  a1, a1, 3
    daddu  a0, a0, a1           // a0 = a0 + (a1 x 8) = array->m_array[idx]
    sd  a2, PtrArray__m_Array(a0) // array->m_array[idx] = val
    jr  ra
    nop

LOCAL_LABEL(ThrowNullReferenceException):
    // Tail call JIT_InternalThrow(NullReferenceException)
    lui  AT, %hi(%neg(%gp_rel(JIT_Stelem_Ref)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_Stelem_Ref)))
    ld  t9, %call16(JIT_InternalThrow)(AT)
    li  a0, CORINFO_NullReferenceException_ASM
    jr t9
    nop

LOCAL_LABEL(ThrowIndexOutOfRangeException):
    // Tail call JIT_InternalThrow(NullReferenceException)
    lui  AT, %hi(%neg(%gp_rel(JIT_Stelem_Ref)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_Stelem_Ref)))
    ld  t9, %call16(JIT_InternalThrow)(AT)
    li  a0, CORINFO_IndexOutOfRangeException_ASM
    jr t9
    nop

LEAF_END JIT_Stelem_Ref, _TEXT

// ------------------------------------------------------------------
// __declspec(naked) void F_CALL_CONV JIT_Stelem_Ref_NotExactMatch(PtrArray* array,
//                                                       unsigned idx, Object* val)
//   t2 = array->GetArrayElementTypeHandle()
//
NESTED_ENTRY JIT_Stelem_Ref_NotExactMatch, _TEXT, NoHandler

    .set  noreorder
    PROLOG_SAVE_REG_PAIR_INDEXED   fp, ra, 48

    // Spill callee saved registers
    PROLOG_SAVE_REG_PAIR  a0, a1, 16
    PROLOG_SAVE_REG  a2, 32
    sd  t9, 40(sp)

    // allow in case val can be casted to array element type
    // call ObjIsInstanceOfNoGC(val, array->GetArrayElementTypeHandle())
    lui  AT, %hi(%neg(%gp_rel(JIT_Stelem_Ref_NotExactMatch)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_Stelem_Ref_NotExactMatch)))
    move  a1, t2 // array->GetArrayElementTypeHandle()
    ld  t9, %call16(ObjIsInstanceOfNoGC)(AT)
    move  a0, a2
    jalr  t9
    nop

    daddiu  AT, zero, TypeHandle_CanCast
    beq  AT, v0, LOCAL_LABEL(DoWrite)             // ObjIsInstance returned TypeHandle::CanCast
    nop

    // check via raising frame
LOCAL_LABEL(NeedFrame):  ////unused label.
    ld  t9, 40(sp)

    lui  AT, %hi(%neg(%gp_rel(JIT_Stelem_Ref_NotExactMatch)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_Stelem_Ref_NotExactMatch)))
    daddiu  a1, sp, 16             // a1 = &array
    ld  t9, %call16(ArrayStoreCheck)(AT)
    daddiu  a0, sp, 32             // a0 = &val
    // ArrayStoreCheck(&val, &array)
    jalr  t9
    nop

LOCAL_LABEL(DoWrite):
    ld  t9, 40(sp)
    EPILOG_RESTORE_REG_PAIR           a0, a1, 16
    EPILOG_RESTORE_REG                a2, 32
    EPILOG_RESTORE_REG_PAIR_INDEXED   fp, ra,   48
    b C_FUNC(JIT_Stelem_DoWrite)
    daddiu  t9, t9, %lo(C_FUNC(JIT_Stelem_DoWrite) - C_FUNC(JIT_Stelem_Ref_NotExactMatch))   //NOTE: here 16-bits is enough.
NESTED_END JIT_Stelem_Ref_NotExactMatch, _TEXT

// ------------------------------------------------------------------
// __declspec(naked) void F_CALL_CONV JIT_Stelem_DoWrite(PtrArray* array, unsigned idx, Object* val)
LEAF_ENTRY  JIT_Stelem_DoWrite, _TEXT
    .set noreorder

    // Setup args for JIT_WriteBarrier. v0 = &array->m_array[idx], v1 = val
    daddiu  v0, a0, PtrArray__m_Array // v0 = &array->m_array
    dsll  AT, a1, 3
    ori  v1, a2, 0  // v1 = val
    daddu  v0, AT, v0

    // Branch to the write barrier (which is already correctly overwritten with
    // single or multi-proc code based on the current CPU
    b  C_FUNC(JIT_WriteBarrier)
    daddiu  t9, t9, %lo(C_FUNC(JIT_WriteBarrier) - C_FUNC(JIT_Stelem_DoWrite))   //NOTE: here 16-bits is enough.
LEAF_END JIT_Stelem_DoWrite, _TEXT

#ifdef FEATURE_HIJACK
// ------------------------------------------------------------------
// Hijack function for functions which return a scalar type or a struct (value type)
NESTED_ENTRY OnHijackTripThread, _TEXT, NoHandler

    PROLOG_SAVE_REG_PAIR_INDEXED   fp, ra, 0x80

    // Spill callee saved registers
    PROLOG_SAVE_REG_PAIR   s0, s1, 16
    PROLOG_SAVE_REG_PAIR   s2, s3, 32
    PROLOG_SAVE_REG_PAIR   s4, s5, 48
    PROLOG_SAVE_REG_PAIR   s6, s7, 64
    PROLOG_SAVE_REG gp, 80

    // save any integral return value(s)
    sd  v0, 96(sp)
    sd  v1, 104(sp)

    // save any FP/HFA return value(s)
    sdc1  $f0, 112(sp)
    sdc1  $f2, 120(sp)

    lui  AT, %hi(%neg(%gp_rel(OnHijackTripThread)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(OnHijackTripThread)))
    ld  t9, %call16(OnHijackWorker)(AT)
    move  a0, sp
    jalr  t9

    // restore callee saved registers

    // restore any integral return value(s)
    ld  v0, 96(sp)
    ld  v1, 104(sp)

    // restore any FP/HFA return value(s)
    ldc1  $f0, 112(sp)
    ldc1  $f2, 120(sp)

    EPILOG_RESTORE_REG_PAIR   s0, s1, 16
    EPILOG_RESTORE_REG_PAIR   s2, s3, 32
    EPILOG_RESTORE_REG_PAIR   s4, s5, 48
    EPILOG_RESTORE_REG_PAIR   s6, s7, 64
    EPILOG_RESTORE_REG  gp, 80
    EPILOG_RESTORE_REG_PAIR_INDEXED  fp, ra, 0x80
    EPILOG_RETURN
NESTED_END OnHijackTripThread, _TEXT

#endif // FEATURE_HIJACK

// ------------------------------------------------------------------
// Redirection Stub for GC in fully interruptible method
//GenerateRedirectedHandledJITCaseStub GCThreadControl
// ------------------------------------------------------------------
//GenerateRedirectedHandledJITCaseStub DbgThreadControl
// ------------------------------------------------------------------
//GenerateRedirectedHandledJITCaseStub UserSuspend

#ifdef _DEBUG
// ------------------------------------------------------------------
// Redirection Stub for GC Stress
GenerateRedirectedHandledJITCaseStub GCStress
#endif


// ------------------------------------------------------------------
// This helper enables us to call into a funclet after restoring Fp register
NESTED_ENTRY CallEHFunclet, _TEXT, NoHandler
    // On entry:
    //
    // a0 = throwable
    // a1 = PC to invoke
    // a2 = address of s0 register in CONTEXT record// used to restore the non-volatile registers of CrawlFrame
    // a3 = address of the location where the SP of funclet's caller (i.e. this helper) should be saved.
    //

    .set noreorder
    PROLOG_SAVE_REG_PAIR_INDEXED   fp, ra, 96, 0

    // Spill callee saved registers
    PROLOG_SAVE_REG_PAIR   s0, s1, 16
    PROLOG_SAVE_REG_PAIR   s2, s3, 32
    PROLOG_SAVE_REG_PAIR   s4, s5, 48
    PROLOG_SAVE_REG_PAIR   s6, s7, 64
    PROLOG_SAVE_REG gp, 80

    // Save the SP of this function
    sd sp, 0(a3)

    ld  s0, 0(a2)
    ld  s1, 8(a2)
    ld  s2, 16(a2)
    ld  s3, 24(a2)
    ld  s4, 32(a2)
    ld  s5, 40(a2)
    ld  s6, 48(a2)
    ld  s7, 56(a2)
    ld  gp, 104(a2) // offset of fp in PCONTEXT relative to S0.
    ld  fp, 112(a2) // offset of fp in PCONTEXT relative to S0.

    // Invoke the funclet
    jalr a1
    move t9, a1

    EPILOG_RESTORE_REG_PAIR   s0, s1, 16
    EPILOG_RESTORE_REG_PAIR   s2, s3, 32
    EPILOG_RESTORE_REG_PAIR   s4, s5, 48
    EPILOG_RESTORE_REG_PAIR   s6, s7, 64
    EPILOG_RESTORE_REG  gp, 80
    EPILOG_RESTORE_REG_PAIR_INDEXED   fp, ra, 96
    EPILOG_RETURN

NESTED_END CallEHFunclet, _TEXT

// This helper enables us to call into a filter funclet by passing it the CallerSP to lookup the
// frame pointer for accessing the locals in the parent method.
NESTED_ENTRY CallEHFilterFunclet, _TEXT, NoHandler

    .set noreorder
    PROLOG_SAVE_REG_PAIR_INDEXED   fp, ra, 16

    // On entry:
    //
    // a0 = throwable
    // a1 = SP of the caller of the method/funclet containing the filter
    // a2 = PC to invoke
    // a3 = address of the location where the SP of funclet's caller (i.e. this helper) should be saved.
    //
    // Save the SP of this function
    sd fp, 0(a3)
    // Invoke the filter funclet
    jalr  a2
    move t9, a2

    EPILOG_RESTORE_REG_PAIR_INDEXED   fp, ra, 16
    EPILOG_RETURN
    nop

NESTED_END CallEHFilterFunclet, _TEXT

#ifdef FEATURE_COMINTEROP
// Function used by COM interop to get floating point return value (since it's not in the same
// register(s) as non-floating point values).
//
// On entry//
//   a0          : size of the FP result (4 or 8 bytes)
//   a1          : pointer to 64-bit buffer to receive result
//
// On exit:
//   buffer pointed to by a1 on entry contains the float or double argument as appropriate
//
LEAF_ENTRY getFPReturn, _TEXT
    sdc1  $f0, 0(x1)
LEAF_END getFPReturn, _TEXT

// ------------------------------------------------------------------
// Function used by COM interop to set floating point return value (since it's not in the same
// register(s) as non-floating point values).
//
// On entry:
//   a0          : size of the FP result (4 or 8 bytes)
//   a1          : 32-bit or 64-bit FP result
//
// On exit:
//   f0          : float result if x0 == 4
//   f0          : double result if x0 == 8
//
LEAF_ENTRY setFPReturn, _TEXT
    dmtc1 a1, $f0
LEAF_END setFPReturn, _TEXT

#endif // FEATURE_COMINTEROP

//
// JIT Static access helpers when coreclr host specifies single appdomain flag
//

// ------------------------------------------------------------------
// void* JIT_GetSharedNonGCStaticBase(SIZE_T moduleDomainID, DWORD dwClassDomainID)

LEAF_ENTRY JIT_GetSharedNonGCStaticBase_SingleAppDomain, _TEXT
    // If class is not initialized, bail to C++ helper
    dext a1, a1, 0, 32
    daddiu  a2, a0, DomainLocalModule__m_pDataBlob

    daddu a2, a2, a1
    lb a2, 0(a2)
    andi  AT, a2, 1
    beq  AT, zero, LOCAL_LABEL(JIT_GetSharedNonGCStaticBase_SingleAppDomain_CallHelper)

    jr  ra

LOCAL_LABEL(JIT_GetSharedNonGCStaticBase_SingleAppDomain_CallHelper):
    // Tail call JIT_GetSharedNonGCStaticBase_Helper
    lui  gp, %hi(%neg(%gp_rel(JIT_GetSharedNonGCStaticBase_SingleAppDomain)))
    daddu  gp, gp, t9
    daddiu  gp, gp, %lo(%neg(%gp_rel(JIT_GetSharedNonGCStaticBase_SingleAppDomain)))
    ld  t9, %call16(JIT_GetSharedNonGCStaticBase_Helper)(gp)
    jr  t9

LEAF_END JIT_GetSharedNonGCStaticBase_SingleAppDomain, _TEXT

//------------------------------------------------
// ExternalMethodFixupStub
//
// In NGEN images, calls to cross-module external methods initially
// point to a jump thunk that calls into the following function that will
// call into a VM helper. The VM helper is responsible for patching up the
// thunk, upon executing the precode, so that all subsequent calls go directly
// to the actual method body.
//
// This is done lazily for performance reasons.
//
// On entry:
////FIXME for MIPS: how to transfer t2 ???
// t2 = Address of thunk

NESTED_ENTRY ExternalMethodFixupStub, _TEXT, NoHandler

    PROLOG_WITH_TRANSITION_BLOCK

    lui  AT, %hi(%neg(%gp_rel(ExternalMethodFixupStub)))
    daddu  t0, AT, t9
    daddiu  AT, t0, %lo(%neg(%gp_rel(ExternalMethodFixupStub)))
    move a1, t2        // pThunk
    ld  t9, %call16(ExternalMethodFixupWorker)(AT)
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock
    jalr  t9

    // mov the address we patched to in t2 so that we can tail call to it
    ////FIXME for MIPS: should use t2 or t9 ???
    move t9, v0

    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
    PATCH_LABEL ExternalMethodFixupPatchLabel
    EPILOG_BRANCH_REG   t9

NESTED_END ExternalMethodFixupStub, _TEXT

#ifdef FEATURE_READYTORUN

NESTED_ENTRY DelayLoad_MethodCall_FakeProlog, _TEXT, NoHandler
DelayLoad_MethodCall:
    .global DelayLoad_MethodCall
    PROLOG_WITH_TRANSITION_BLOCK

    ori a1, t8, 0      // Indirection cell
    ori a2, t0, 0      // sectionIndex
    ori a3, t1, 0      // Module*

    lui  AT, %hi(%neg(%gp_rel(DelayLoad_MethodCall_FakeProlog)))
    daddu  t0, AT, t9
    daddiu  AT, t0, %lo(%neg(%gp_rel(DelayLoad_MethodCall_FakeProlog)))
    ld  t9, %call16(ExternalMethodFixupWorker)(AT)
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock
    jalr  t9

    //FIXME for MIPS: Is it needed transfering t9 or t2 ???
    move t9, v0

    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
    //// Share patch label
    b ExternalMethodFixupPatchLabel

NESTED_END DelayLoad_MethodCall_FakeProlog, _TEXT


.macro DynamicHelper frameFlags, suffix
NESTED_ENTRY DelayLoad_Helper\suffix\()_FakeProlog, _TEXT, NoHandler
DelayLoad_Helper\suffix:
    .global DelayLoad_Helper\suffix

    .set noreorder

    PROLOG_WITH_TRANSITION_BLOCK

    //DynamicHelperWorker(TransitionBlock * pTransitionBlock, TADDR * pCell,
    //                    DWORD sectionIndex, Module * pModule, INT frameFlags)
    ori  a1, t8, 0      // Indirection cell
    ori  a2, t0, 0      // sectionIndex
    ori  a3, t1, 0      // Module*
    daddiu  a4, $0, \frameFlags

    lui  AT, %hi(%neg(%gp_rel(DelayLoad_Helper\suffix)))
    daddu  t0, AT, t9
    daddiu  AT, t0, %lo(%neg(%gp_rel(DelayLoad_Helper\suffix)))
    ld  t9, %call16(DynamicHelperWorker)(AT)
    jalr  t9
    daddiu a0, sp, __PWTB_TransitionBlock        // pTransitionBlock

    bne v0, $0, LOCAL_LABEL(FakeProlog\suffix\()_0)
    nop

    ld  v0, __PWTB_ArgumentRegisters(sp)
    EPILOG_WITH_TRANSITION_BLOCK_RETURN

LOCAL_LABEL(FakeProlog\suffix\()_0):
    move t9, v0
    EPILOG_WITH_TRANSITION_BLOCK_TAILCALL
    EPILOG_BRANCH_REG  t9
    nop

NESTED_END DelayLoad_Helper\suffix\()_FakeProlog, _TEXT
.endm

DynamicHelper DynamicHelperFrameFlags_Default
DynamicHelper DynamicHelperFrameFlags_ObjectArg, _Obj
DynamicHelper DynamicHelperFrameFlags_ObjectArg | DynamicHelperFrameFlags_ObjectArg2, _ObjObj
#endif
