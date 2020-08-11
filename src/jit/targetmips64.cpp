// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

/*****************************************************************************/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#if defined(_TARGET_MIPS64_)

#include "target.h"

const char*            Target::g_tgtCPUName  = "mips64";
const Target::ArgOrder Target::g_tgtArgOrder = ARG_ORDER_R2L;

#endif // _TARGET_MIPS64_
