// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

//

#ifndef __unwinder_mips64__
#define __unwinder_mips64__

#include "unwinder.h"

//---------------------------------------------------------------------------------------
//
// See the comment for the base class code:OOPStackUnwinder.
//

class OOPStackUnwinderMips64 : public OOPStackUnwinder
{
public:
    // Unwind the given CONTEXT to the caller CONTEXT.  The CONTEXT will be overwritten.
    BOOL Unwind(T_CONTEXT * pContext);

    //
    // Everything below comes from dbghelp.dll.
    //

protected:
    HRESULT UnwindPrologue(__in DWORD64 ImageBase,
                           __in DWORD64 ControlPc,
                           __in DWORD64 FrameBase,
                           __in _PIMAGE_RUNTIME_FUNCTION_ENTRY FunctionEntry,
                           __inout PT_CONTEXT ContextRecord);

    HRESULT VirtualUnwind(__in DWORD64 ImageBase,
                          __in DWORD64 ControlPc,
                          __in _PIMAGE_RUNTIME_FUNCTION_ENTRY FunctionEntry,
                          __inout PT_CONTEXT ContextRecord,
                          __out PDWORD64 EstablisherFrame);

    DWORD64 LookupPrimaryUnwindInfo
        (__in _PIMAGE_RUNTIME_FUNCTION_ENTRY FunctionEntry,
         __in DWORD64 ImageBase,
         __out _PIMAGE_RUNTIME_FUNCTION_ENTRY PrimaryEntry);

    _PIMAGE_RUNTIME_FUNCTION_ENTRY SameFunction
        (__in _PIMAGE_RUNTIME_FUNCTION_ENTRY FunctionEntry,
         __in DWORD64 ImageBase,
         __in DWORD64 ControlPc,
         __out _PIMAGE_RUNTIME_FUNCTION_ENTRY FunctionReturnBuffer);
};

#endif // __unwinder_mips64__

