; Licensed to the .NET Foundation under one or more agreements.
; The .NET Foundation licenses this file to you under the MIT license.
; See the LICENSE file in the project root for more information.

; Copyright (c) Loongson Technology. All rights reserved.


;; ==++==
;;

;;
;; ==--==
#include "ksmips64.h"

;; Arguments
;;     input: (in X0) the _NEON128 value to be converted to a double
;;     output: the double corresponding to the _NEON128 input value
#if 0
    LEAF_ENTRY FPFillR8
        LDR  Q0, [X0]
        ret     lr
    LEAF_END

;; Must be at very end of file
    END
#endif
