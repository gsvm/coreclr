// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

#include "unixasmmacros.inc"
#include "asmconstants.h"

//////////////////////////////////////////////////////////////////////////
//
// This function creates a stack frame right below the target frame, restores all callee
// saved registers, SP, and RA from the passed in context.
// Then it uses the ThrowExceptionHelper to throw the passed in exception from that context.
// EXTERN_C void ThrowExceptionFromContextInternal(CONTEXT* context, PAL_SEHException* ex);
LEAF_ENTRY ThrowExceptionFromContextInternal, _TEXT
     .set noreorder
#ifdef HAS_ASAN
#pragma error("LLVM v3.9 ASAN unimplemented on MIPS yet")
#endif
    daddiu  sp, sp, -16
    .cfi_adjust_cfa_offset 16

    // Save the FP & RA to the stack so that the unwind can work at the instruction after
    // loading the FP from the context, but before loading the SP from the context.
    sd  fp, 0(sp)
    sd  ra, 8(sp)
    .cfi_rel_offset fp, 0
    .cfi_rel_offset ra, 8

    ld  s0, CONTEXT_S0(a0)
    ld  s1, CONTEXT_S1(a0)
    ld  s2, CONTEXT_S2(a0)
    ld  s3, CONTEXT_S3(a0)
    ld  s4, CONTEXT_S4(a0)
    ld  s5, CONTEXT_S5(a0)
    ld  s6, CONTEXT_S6(a0)
    ld  s7, CONTEXT_S7(a0)
    ld  gp, CONTEXT_Gp(a0)
    ld  ra, CONTEXT_Ra(a0)

    lui  AT, %hi(%neg(%gp_rel(ThrowExceptionFromContextInternal)))
    daddiu  AT, AT, %lo(%neg(%gp_rel(ThrowExceptionFromContextInternal)))
    daddu  AT, AT, t9
    ld  t9, %call16(ThrowExceptionHelper)(AT)

    ld  fp, CONTEXT_Fp(a0)
    ld  sp, CONTEXT_Sp(a0)

    jr  t9
    // The PAL_SEHException pointer
    daddu  a0, a1, zero
LEAF_END ThrowExceptionFromContextInternal, _TEXT
