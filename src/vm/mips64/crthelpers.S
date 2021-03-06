// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

#include "unixasmmacros.inc"

// JIT_MemSet/JIT_MemCpy
//
// It is IMPORTANT that the exception handling code is able to find these guys
// on the stack, but on non-windows platforms we can just defer to the platform
// implementation.
//
LEAF_ENTRY JIT_MemSet, _TEXT
    beq a2, zero, LOCAL_LABEL(JIT_MemSet_ret)

    ////FIXME for MIPS.
    //sb a1, 0(a0)       //Is this really needed ?

    lui  AT, %hi(%neg(%gp_rel(JIT_MemSet)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_MemSet)))
    ld  t9, %call16(memset)(AT)
    jr  t9

LOCAL_LABEL(JIT_MemSet_ret):
    jr ra

////NOTO: Here must use LEAF_END_MARKED!  not LEAF_END !!!
LEAF_END_MARKED JIT_MemSet, _TEXT

LEAF_ENTRY JIT_MemCpy, _TEXT
    beq a2, zero, LOCAL_LABEL(JIT_MemCpy_ret)

    ////FIXME for MIPS.
    //sb zero, 0(a0)       //Is this really needed ?
    //lb zero, 0(a1)
    lui  AT, %hi(%neg(%gp_rel(JIT_MemCpy)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(JIT_MemCpy)))
    ld  t9, %call16(memcpy)(AT)
    jr  t9

LOCAL_LABEL(JIT_MemCpy_ret):
    jr ra

////NOTO: Here must use LEAF_END_MARKED!  not LEAF_END !!!
LEAF_END_MARKED JIT_MemCpy, _TEXT
