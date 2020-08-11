// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

#include "asmconstants.h"
#include "unixasmmacros.inc"

//
// hijacking stub used to perform a func-eval, see Debugger::FuncEvalSetup() for use.
//
// on entry:
//   x0  : pointer to DebuggerEval object
//

// @dbgtodo- once we port Funceval, use the ExceptionHijack stub instead of this func-eval stub.
NESTED_ENTRY FuncEvalHijack, _TEXT, UnhandledExceptionHandlerUnix

    // NOTE: FuncEvalHijackPersonalityRoutine is dependent on the stack layout so if
    //       you change the prolog you will also need to update the personality routine.

    // push arg to the stack so our personality routine can find it
    // push lr to get good stacktrace in debugger
    PROLOG_SAVE_REG_PAIR_INDEXED  fp, ra, -32
    sd  a0, 16(sp)

    lui  AT, %hi(%neg(%gp_rel(FuncEvalHijack)))
    daddu  AT, AT, t9
    daddiu  AT, AT, %lo(%neg(%gp_rel(FuncEvalHijack)))
    // FuncEvalHijackWorker returns the address we should jump to.
    ld  t9, %got_disp(FuncEvalHijackWorker)(AT)
    jalr t9


    EPILOG_STACK_FREE  32
    EPILOG_BRANCH_REG  v0
NESTED_END FuncEvalHijack

#if 0
// This is the general purpose hijacking stub. The DacDbi Hijack primitive will
// set up the stack and then set the IP here, and so this just makes the call.
NESTED_ENTRY ExceptionHijack, _TEXT, UnhandledExceptionHandlerUnix

    // make the call
    bl ExceptionHijackWorker

    // effective NOP to terminate unwind
    mov x3, x3

    // *** should never get here ***
    EMIT_BREAKPOINT

// exported label so the debugger knows where the end of this function is
PATCH_LABEL ExceptionHijackEnd

NESTED_END ExceptionHijack, _TEXT
#endif

