// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

//
// Implementation of _CONTEXT_CaptureContext for the MIPS platform.
// This function is processor dependent.  It is used by exception handling,
// and is always apply to the current thread.
//

#include "unixasmmacros.inc"
#include "asmconstants.h"

// FIXME for MIPS: the stack frame.
// Incoming:
//  a0: Context*
//  a1: Exception*
//
LEAF_ENTRY RtlRestoreContext, _TEXT

#ifdef HAS_ASAN
    lw  AT, CONTEXT_ContextFlags(a0)
    ext t1, AT, CONTEXT_FLOATING_POINT_BIT, 1
    beq t1, $0, LOCAL_LABEL(Restore_CONTEXT_FLOATING_POINT)

    daddiu  sp, sp, -16
    sd  a0, 0(sp)
    sd  a1, 8(sp)

    lui  t0, %hi(%neg(%gp_rel(RtlRestoreContext)))
    daddu  t0, t0, t9
    daddiu  t0, t0, %lo(%neg(%gp_rel(RtlRestoreContext)))
    ld  t9, %call16(__asan_handle_no_return)(t0)
    jalr  t9

    ld  a0, 0(sp)
    ld  a1, 8(sp)
    daddiu  sp, sp, 16

LOCAL_LABEL(Restore_CONTEXT_FLOATING_POINT):
#endif

    ori t9, a0, 0
    lw  AT, CONTEXT_ContextFlags(t9)
    ext t1, AT, CONTEXT_FLOATING_POINT_BIT, 1
    beq t1, $0, LOCAL_LABEL(No_Restore_CONTEXT_FLOATING_POINT)

    //64-bits FPR.
    daddiu t0, t9,   CONTEXT_FPU_OFFSET
#ifdef FEATURE_LOONGSONISA
    //for mips64-loongson.
    //NOTE: CONTEXT_F0 must be 16-bytes align on loognson.
    gslqc1 $f1, $f0,   CONTEXT_F0(t0)
    gslqc1 $f3, $f2,   CONTEXT_F2(t0)
    gslqc1 $f5, $f4,   CONTEXT_F4(t0)
    gslqc1 $f7, $f6,   CONTEXT_F6(t0)
    gslqc1 $f9, $f8,   CONTEXT_F8(t0)
    gslqc1 $f11, $f10, CONTEXT_F10(t0)
    gslqc1 $f13, $f12, CONTEXT_F12(t0)
    gslqc1 $f15, $f14, CONTEXT_F14(t0)
    gslqc1 $f17, $f16, CONTEXT_F16(t0)
    gslqc1 $f19, $f18, CONTEXT_F18(t0)
    gslqc1 $f21, $f20, CONTEXT_F20(t0)
    gslqc1 $f23, $f22, CONTEXT_F22(t0)
    gslqc1 $f25, $f24, CONTEXT_F24(t0)
    gslqc1 $f27, $f26, CONTEXT_F26(t0)
    gslqc1 $f29, $f28, CONTEXT_F28(t0)
    gslqc1 $f31, $f30, CONTEXT_F30(t0)
#else
    ldc1 $f0, CONTEXT_F0(t0)
    ldc1 $f1, CONTEXT_F1(t0)
    ldc1 $f2, CONTEXT_F2(t0)
    ldc1 $f3, CONTEXT_F3(t0)
    ldc1 $f4, CONTEXT_F4(t0)
    ldc1 $f5, CONTEXT_F5(t0)
    ldc1 $f6, CONTEXT_F6(t0)
    ldc1 $f7, CONTEXT_F7(t0)
    ldc1 $f8, CONTEXT_F8(t0)
    ldc1 $f9, CONTEXT_F9(t0)
    ldc1 $f10, CONTEXT_F10(t0)
    ldc1 $f11, CONTEXT_F11(t0)
    ldc1 $f12, CONTEXT_F12(t0)
    ldc1 $f13, CONTEXT_F13(t0)
    ldc1 $f14, CONTEXT_F14(t0)
    ldc1 $f15, CONTEXT_F15(t0)
    ldc1 $f16, CONTEXT_F16(t0)
    ldc1 $f17, CONTEXT_F17(t0)
    ldc1 $f18, CONTEXT_F18(t0)
    ldc1 $f19, CONTEXT_F19(t0)
    ldc1 $f20, CONTEXT_F20(t0)
    ldc1 $f21, CONTEXT_F21(t0)
    ldc1 $f22, CONTEXT_F22(t0)
    ldc1 $f23, CONTEXT_F23(t0)
    ldc1 $f24, CONTEXT_F24(t0)
    ldc1 $f25, CONTEXT_F25(t0)
    ldc1 $f26, CONTEXT_F26(t0)
    ldc1 $f27, CONTEXT_F27(t0)
    ldc1 $f28, CONTEXT_F28(t0)
    ldc1 $f29, CONTEXT_F29(t0)
    ldc1 $f30, CONTEXT_F30(t0)
    ldc1 $f31, CONTEXT_F31(t0)
#endif
    lw t1, CONTEXT_FLOAT_CONTROL_OFFSET(t0)
    ctc1 t1, $31

LOCAL_LABEL(No_Restore_CONTEXT_FLOATING_POINT):

    ext t1, AT, CONTEXT_INTEGER_BIT, 1
    beq t1, $0, LOCAL_LABEL(No_Restore_CONTEXT_INTEGER)

#ifdef FEATURE_LOONGSONISA
    //for mips64-loongson.
    //NOTE: CONTEXT_V0 must be 16-bytes align on loognson.

    gslq $2, $1, CONTEXT_At(t9)
    gslq $4, $3, CONTEXT_V1(t9)
    gslq $6, $5, CONTEXT_A1(t9)
    gslq $8, $7, CONTEXT_A3(t9)
    gslq $10, $9, CONTEXT_A5(t9)
    gslq $12, $11, CONTEXT_A7(t9)
    gslq $14, $13, CONTEXT_T1(t9)
    gslq $16, $15, CONTEXT_T3(t9)
    gslq $18, $17, CONTEXT_S1(t9)
    gslq $20, $19, CONTEXT_S3(t9)
    gslq $22, $21, CONTEXT_S5(t9)
    gslq $24, $23, CONTEXT_S7(t9)
    //ld   $25, CONTEXT_T9(t9)
    ld   $28, CONTEXT_Gp(t9)
#else
    ld $1,  CONTEXT_At(t9)
    ld $2,  CONTEXT_V0(t9)
    ld $3,  CONTEXT_V1(t9)
    ld $4,  CONTEXT_A0(t9)
    ld $5,  CONTEXT_A1(t9)
    ld $6,  CONTEXT_A2(t9)
    ld $7,  CONTEXT_A3(t9)
    ld $8,  CONTEXT_A4(t9)
    ld $9,  CONTEXT_A5(t9)
    ld $10, CONTEXT_A6(t9)
    ld $11, CONTEXT_A7(t9)
    ld $12, CONTEXT_T0(t9)
    ld $13, CONTEXT_T1(t9)
    ld $14, CONTEXT_T2(t9)
    ld $15, CONTEXT_T3(t9)
    ld $16, CONTEXT_S0(t9)
    ld $17, CONTEXT_S1(t9)
    ld $18, CONTEXT_S2(t9)
    ld $19, CONTEXT_S3(t9)
    ld $20, CONTEXT_S4(t9)
    ld $21, CONTEXT_S5(t9)
    ld $22, CONTEXT_S6(t9)
    ld $23, CONTEXT_S7(t9)
    ld $24, CONTEXT_T8(t9)
    //ld $25, CONTEXT_T9(t9)
    ld $28, CONTEXT_Gp(t9)
#endif

LOCAL_LABEL(No_Restore_CONTEXT_INTEGER):

    lw  AT, CONTEXT_ContextFlags(t9)
    ext t1, AT, CONTEXT_CONTROL_BIT, 1
    beq t1, $0, LOCAL_LABEL(No_Restore_CONTEXT_CONTROL)

    /* FIXME for MIPS: How to translate restore the cpsr?  */
    ld  ra, CONTEXT_Ra(t9)
    ld  fp, CONTEXT_Fp(t9)
    ld  sp, CONTEXT_Sp(t9)
    ld  t9, CONTEXT_Pc(t9)
    jr  t9


LOCAL_LABEL(No_Restore_CONTEXT_CONTROL):
   jr ra

LEAF_END RtlRestoreContext, _TEXT

// Incoming:
//  a0: Context*

LEAF_ENTRY RtlCaptureContext, _TEXT
    .set noreorder
    daddiu  sp, sp, -8
    sd  AT, 0(sp)
    li  AT, CONTEXT_FULL
    sw  AT, CONTEXT_ContextFlags(a0)
    ld  AT, 0(sp)
    b  C_FUNC(CONTEXT_CaptureContext)
    daddiu  sp, sp, 8
LEAF_END RtlCaptureContext, _TEXT

// Incoming:
//  a0: Context* t2
//
LEAF_ENTRY CONTEXT_CaptureContext, _TEXT
    daddiu  sp, sp, -24
    sd  t0, 0(sp)
    sd  t1, 8(sp)
    sd  t3, 16(sp)

    lw  t1, CONTEXT_ContextFlags(a0)
    li  t0, CONTEXT_CONTROL
    and  t3, t1, t0
    bne t3, t0, LOCAL_LABEL(Done_CONTEXT_CONTROL)

    /* FIXME for MIPS: How to translate save the cpsr?  */
    daddiu  t0, sp, 24
    sd  fp, CONTEXT_Fp(a0)
    sd  t0, CONTEXT_Sp(a0)
    sd  ra, CONTEXT_Ra(a0)
    sd  ra, CONTEXT_Pc(a0)

LOCAL_LABEL(Done_CONTEXT_CONTROL):

    li  t0, CONTEXT_INTEGER
    and  t3, t1, t0
    bne  t3, t0, LOCAL_LABEL(Done_CONTEXT_INTEGER)

    ld  t0, 0(sp)
    ld  t1, 8(sp)
    ld  t3, 16(sp)

    sd  AT, CONTEXT_At(a0)
    sd  v0, CONTEXT_V0(a0)
    sd  v1, CONTEXT_V1(a0)
    sd  a0, CONTEXT_A0(a0)
    sd  a1, CONTEXT_A1(a0)
    sd  a2, CONTEXT_A2(a0)
    sd  a3, CONTEXT_A3(a0)
    sd  a4, CONTEXT_A4(a0)
    sd  a5, CONTEXT_A5(a0)
    sd  a6, CONTEXT_A6(a0)
    sd  a7, CONTEXT_A7(a0)
    sd  t0, CONTEXT_T0(a0)
    sd  t1, CONTEXT_T1(a0)
    sd  t2, CONTEXT_T2(a0)
    sd  t3, CONTEXT_T3(a0)
    sd  s0, CONTEXT_S0(a0)
    sd  s1, CONTEXT_S1(a0)
    sd  s2, CONTEXT_S2(a0)
    sd  s3, CONTEXT_S3(a0)
    sd  s4, CONTEXT_S4(a0)
    sd  s5, CONTEXT_S5(a0)
    sd  s6, CONTEXT_S6(a0)
    sd  s7, CONTEXT_S7(a0)
    sd  t8, CONTEXT_T8(a0)
    sd  t9, CONTEXT_T9(a0)
    sd  gp, CONTEXT_Gp(a0)


LOCAL_LABEL(Done_CONTEXT_INTEGER):
    lw  t1, CONTEXT_ContextFlags(a0)

    li  t0, CONTEXT_FLOATING_POINT
    and  t3, t1, t0
    bne  t3, t0, LOCAL_LABEL(Done_CONTEXT_FLOATING_POINT)

    daddiu  a0, a0, CONTEXT_FPU_OFFSET

    sdc1  $f0 , CONTEXT_F0 (a0)
    sdc1  $f1 , CONTEXT_F1 (a0)
    sdc1  $f2 , CONTEXT_F2 (a0)
    sdc1  $f3 , CONTEXT_F3 (a0)
    sdc1  $f4 , CONTEXT_F4 (a0)
    sdc1  $f5 , CONTEXT_F5 (a0)
    sdc1  $f6 , CONTEXT_F6 (a0)
    sdc1  $f7 , CONTEXT_F7 (a0)
    sdc1  $f8 , CONTEXT_F8 (a0)
    sdc1  $f9 , CONTEXT_F9 (a0)
    sdc1  $f10, CONTEXT_F10(a0)
    sdc1  $f11, CONTEXT_F11(a0)
    sdc1  $f12, CONTEXT_F12(a0)
    sdc1  $f13, CONTEXT_F13(a0)
    sdc1  $f14, CONTEXT_F14(a0)
    sdc1  $f15, CONTEXT_F15(a0)
    sdc1  $f16, CONTEXT_F16(a0)
    sdc1  $f17, CONTEXT_F17(a0)
    sdc1  $f18, CONTEXT_F18(a0)
    sdc1  $f19, CONTEXT_F19(a0)
    sdc1  $f20, CONTEXT_F20(a0)
    sdc1  $f21, CONTEXT_F21(a0)
    sdc1  $f22, CONTEXT_F22(a0)
    sdc1  $f23, CONTEXT_F23(a0)
    sdc1  $f24, CONTEXT_F24(a0)
    sdc1  $f25, CONTEXT_F25(a0)
    sdc1  $f26, CONTEXT_F26(a0)
    sdc1  $f27, CONTEXT_F27(a0)
    sdc1  $f28, CONTEXT_F28(a0)
    sdc1  $f29, CONTEXT_F29(a0)
    sdc1  $f30, CONTEXT_F30(a0)
    sdc1  $f31, CONTEXT_F31(a0)
    cfc1  t0, $31
    sd  t0, CONTEXT_FLOAT_CONTROL_OFFSET(a0)

LOCAL_LABEL(Done_CONTEXT_FLOATING_POINT):

    daddiu sp, sp, 24
    jr ra
LEAF_END CONTEXT_CaptureContext, _TEXT
