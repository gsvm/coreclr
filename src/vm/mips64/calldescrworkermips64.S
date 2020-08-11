// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

#include "unixasmmacros.inc"
#include "asmconstants.h"

//-----------------------------------------------------------------------------
// This helper routine enregisters the appropriate arguments and makes the
// actual call.
//-----------------------------------------------------------------------------
//void CallDescrWorkerInternal(CallDescrData * pCallDescrData);

NESTED_ENTRY CallDescrWorkerInternal, _TEXT, NoHandler
    .set noreorder
    PROLOG_SAVE_REG_PAIR_INDEXED  fp, ra, 0x20
    PROLOG_SAVE_REG s0, 16
    lwu  a1, CallDescrData__numStackSlots(a0)

    beq  a1, zero, LOCAL_LABEL(donestack)
    ori  s0, a0, 0 // save pCallDescrData in s0

    dsll  a2, a1, 3
    andi  a0, a2, 0x8
    dsubu  AT, sp, a0   //padding on high-addr.
    daddu  a0, a0, a2
    dsubu  sp, sp, a0   //stack-16byte aligned.

    ld  a0, CallDescrData__pSrc(s0)

    daddu  a2, a0, a2           // pSrcEnd=pSrc+8*numStackSlots

    // This loop copies numStackSlots words
    // from [pSrcEnd-8,pSrcEnd-16,...] to [sp-8,sp-16,...]
LOCAL_LABEL(stackloop):
    daddiu  a2, a2, -8
    ld  a4, 0(a2)
    daddiu  AT, AT, -8
    sd  a4, 0(AT)
    daddiu    a1, a1, -1
    bne a1, zero, LOCAL_LABEL(stackloop)
    nop

LOCAL_LABEL(donestack):
    // If FP arguments are supplied in registers (AT != NULL)
    ld  AT, CallDescrData__pFloatArgumentRegisters(s0)
    beq  AT, zero, LOCAL_LABEL(NoFloatingPoint)
    nop
    ldc1  $f12, 0(AT)
    ldc1  $f13, 8(AT)
    ldc1  $f14, 16(AT)
    ldc1  $f15, 24(AT)
    ldc1  $f16, 32(AT)
    ldc1  $f17, 40(AT)
    ldc1  $f18, 48(AT)
    ldc1  $f19, 56(AT)
LOCAL_LABEL(NoFloatingPoint):

    // Copy [pArgumentRegisters, ..., pArgumentRegisters + 56]
    // into a0, ..., a7
    // FIXME for MIPS64: here simply initial all the arguments, but no needed!

    ld  AT, CallDescrData__pArgumentRegisters(s0)
    ld  t9, CallDescrData__pTarget(s0)
    ld  a0, 0(AT)
    ld  a1, 8(AT)
    ld  a2, 16(AT)
    ld  a3, 24(AT)
    ld  a4, 32(AT)
    ld  a5, 40(AT)
    ld  a6, 48(AT)

    // call pTarget
    jalr  t9
    ld  a7, 56(AT)

    lw  a3, CallDescrData__fpReturnSize(s0)

    // Int return case
    beq  a3, zero, LOCAL_LABEL(IntReturn)
    nop

    // Float return case
    ori  AT, zero, 4
    beq  AT, a3, LOCAL_LABEL(FloatReturn)
    nop

    // Double return case
    ori  AT, zero, 8
    bne  AT, a3, LOCAL_LABEL(TwoFloatReturn)
    nop

LOCAL_LABEL(FloatReturn):
    b  LOCAL_LABEL(ReturnDone)
    sdc1  $f0, CallDescrData__returnValue(s0)

LOCAL_LABEL(TwoFloatReturn):

    // TwoFloat or TwoDouble return case
    ori  AT, zero, 16
    bne  AT, a3, LOCAL_LABEL(NotCorrectReturn)
    nop

    sdc1  $f0, CallDescrData__returnValue(s0)
    b  LOCAL_LABEL(ReturnDone)
    sdc1  $f2, CallDescrData__returnValue+8(s0)
LOCAL_LABEL(NotCorrectReturn):

    sw  zero, 0(zero)
    EMIT_BREAKPOINT // Unreachable

LOCAL_LABEL(IntReturn):
    // Save return value into retbuf for int
    sd  v0, CallDescrData__returnValue(s0)
    sd  v1, CallDescrData__returnValue+8(s0)

LOCAL_LABEL(ReturnDone):

    EPILOG_STACK_RESTORE
    EPILOG_RESTORE_REG  s0, 16
    EPILOG_RESTORE_REG_PAIR_INDEXED  fp, ra, 0x20
    jr  ra
    nop
NESTED_END CallDescrWorkerInternal, _TEXT
