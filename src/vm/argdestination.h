// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
//

#ifndef __ARGDESTINATION_H__
#define __ARGDESTINATION_H__

// The ArgDestination class represents a destination location of an argument.
class ArgDestination
{
    // Base address to which the m_offset is applied to get the actual argument location.
    PTR_VOID m_base;
    // Offset of the argument relative to the m_base. On AMD64 on Unix, it can have a special
    // value that represent a struct that contain both general purpose and floating point fields 
    // passed in registers.
    int m_offset;
    // For structs passed in registers, this member points to an ArgLocDesc that contains
    // details on the layout of the struct in general purpose and floating point registers.
    ArgLocDesc* m_argLocDescForStructInRegs;

public:

    // Construct the ArgDestination
    ArgDestination(PTR_VOID base, int offset, ArgLocDesc* argLocDescForStructInRegs)
    :   m_base(base),
        m_offset(offset),
        m_argLocDescForStructInRegs(argLocDescForStructInRegs)
    {
        LIMITED_METHOD_CONTRACT;
#if defined(UNIX_AMD64_ABI) || defined(_TARGET_MIPS64_)
        _ASSERTE((argLocDescForStructInRegs != NULL) || (offset != TransitionBlock::StructInRegsOffset));
#elif defined(_TARGET_ARM64_)
        // This assert is not interesting on arm64/mips64. argLocDescForStructInRegs could be
        // initialized if the args are being enregistered.
#else        
        _ASSERTE(argLocDescForStructInRegs == NULL);
#endif        
    }

    // Get argument destination address for arguments that are not structs passed in registers.
    PTR_VOID GetDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + m_offset);
    }

#if defined(_TARGET_ARM64_)
#ifndef DACCESS_COMPILE

    // Returns true if the ArgDestination represents an HFA struct
    bool IsHFA()
    {
        return m_argLocDescForStructInRegs != NULL;
    }

    // Copy struct argument into registers described by the current ArgDestination.
    // Arguments:
    //  src = source data of the structure 
    //  fieldBytes - size of the structure
    void CopyHFAStructToRegister(void *src, int fieldBytes)
    {
        // We are copying a float, double or vector HFA/HVA and need to
        // enregister each field.

        int floatRegCount = m_argLocDescForStructInRegs->m_cFloatReg;
        int hfaFieldSize = m_argLocDescForStructInRegs->m_hfaFieldSize;
        UINT64* dest = (UINT64*) this->GetDestinationAddress();

        for (int i = 0; i < floatRegCount; ++i) 
        {
            // Copy 4 or 8 bytes from src.
            UINT64 val = (hfaFieldSize == 4) ? *((UINT32*)src) : *((UINT64*)src);
            // Always store 8 bytes
            *(dest++) = val;
            // Either zero the next 8 bytes or get the next 8 bytes from src for 16-byte vector.
            *(dest++) = (hfaFieldSize == 16) ? *((UINT64*)src + 1) : 0;

            // Increment src by the appropriate amount.
            src = (void*)((char*)src + hfaFieldSize);
        }
    }

#endif // !DACCESS_COMPILE
#endif // defined(_TARGET_ARM64_)

#if defined(UNIX_AMD64_ABI)

    // Returns true if the ArgDestination represents a struct passed in registers.
    bool IsStructPassedInRegs()
    {
        LIMITED_METHOD_CONTRACT;
        return m_offset == TransitionBlock::StructInRegsOffset;
    }

    // Get destination address for floating point fields of a struct passed in registers.
    PTR_VOID GetStructFloatRegDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(IsStructPassedInRegs());
        int offset = TransitionBlock::GetOffsetOfFloatArgumentRegisters() + m_argLocDescForStructInRegs->m_idxFloatReg * 16;
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + offset);
    }

    // Get destination address for non-floating point fields of a struct passed in registers.
    PTR_VOID GetStructGenRegDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(IsStructPassedInRegs());
        int offset = TransitionBlock::GetOffsetOfArgumentRegisters() + m_argLocDescForStructInRegs->m_idxGenReg * 8;
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + offset);
    }

#ifndef DACCESS_COMPILE
    // Zero struct argument stored in registers described by the current ArgDestination.
    // Arguments:
    //  fieldBytes - size of the structure
    void ZeroStructInRegisters(int fieldBytes)
    {
        STATIC_CONTRACT_NOTHROW;
        STATIC_CONTRACT_GC_NOTRIGGER;
        STATIC_CONTRACT_FORBID_FAULT;
        STATIC_CONTRACT_MODE_COOPERATIVE;

        // To zero the struct, we create a zero filled array of large enough size and
        // then copy it to the registers. It is implemented this way to keep the complexity
        // of dealing with the eightbyte classification in single function.
        // This function is used rarely and so the overhead of reading the zeros from
        // the stack is negligible.
        long long zeros[CLR_SYSTEMV_MAX_EIGHTBYTES_COUNT_TO_PASS_IN_REGISTERS] = {};
        _ASSERTE(sizeof(zeros) >= (size_t)fieldBytes);

        CopyStructToRegisters(zeros, fieldBytes, 0);
    }

    // Copy struct argument into registers described by the current ArgDestination.
    // Arguments:
    //  src = source data of the structure 
    //  fieldBytes - size of the structure
    //  destOffset - nonzero when copying values into Nullable<T>, it is the offset
    //               of the T value inside of the Nullable<T>
    void CopyStructToRegisters(void *src, int fieldBytes, int destOffset)
    {
        STATIC_CONTRACT_NOTHROW;
        STATIC_CONTRACT_GC_NOTRIGGER;
        STATIC_CONTRACT_FORBID_FAULT;
        STATIC_CONTRACT_MODE_COOPERATIVE;

        _ASSERTE(IsStructPassedInRegs());
     
        BYTE* genRegDest = (BYTE*)GetStructGenRegDestinationAddress() + destOffset;
        BYTE* floatRegDest = (BYTE*)GetStructFloatRegDestinationAddress();
        INDEBUG(int remainingBytes = fieldBytes;)

        EEClass* eeClass = m_argLocDescForStructInRegs->m_eeClass;
        _ASSERTE(eeClass != NULL);

        // We start at the first eightByte that the destOffset didn't skip completely.
        for (int i = destOffset / 8; i < eeClass->GetNumberEightBytes(); i++)
        {
            int eightByteSize = eeClass->GetEightByteSize(i);
            SystemVClassificationType eightByteClassification = eeClass->GetEightByteClassification(i);

            // Adjust the size of the first eightByte by the destOffset
            eightByteSize -= (destOffset & 7);
            destOffset = 0;

            _ASSERTE(remainingBytes >= eightByteSize);

            if (eightByteClassification == SystemVClassificationTypeSSE)
            {
                if (eightByteSize == 8)
                {
                    *(UINT64*)floatRegDest = *(UINT64*)src;
                }
                else
                {
                    _ASSERTE(eightByteSize == 4);
                    *(UINT32*)floatRegDest = *(UINT32*)src;
                }
                floatRegDest += 16;
            }
            else
            {
                if (eightByteSize == 8)
                {
                    _ASSERTE((eightByteClassification == SystemVClassificationTypeInteger) ||
                             (eightByteClassification == SystemVClassificationTypeIntegerReference) ||
                             (eightByteClassification == SystemVClassificationTypeIntegerByRef));

                    _ASSERTE(IS_ALIGNED((SIZE_T)genRegDest, 8));
                    *(UINT64*)genRegDest = *(UINT64*)src;
                }
                else
                {
                    _ASSERTE(eightByteClassification == SystemVClassificationTypeInteger);
                    memcpyNoGCRefs(genRegDest, src, eightByteSize);
                }

                genRegDest += eightByteSize;
            }

            src = (BYTE*)src + eightByteSize;
            INDEBUG(remainingBytes -= eightByteSize;)
        }

        _ASSERTE(remainingBytes == 0);        
    }

#endif //DACCESS_COMPILE

    // Report managed object pointers in the struct in registers
    // Arguments:
    //  fn - promotion function to apply to each managed object pointer
    //  sc - scan context to pass to the promotion function
    //  fieldBytes - size of the structure
    void ReportPointersFromStructInRegisters(promote_func *fn, ScanContext *sc, int fieldBytes)
    {
        LIMITED_METHOD_CONTRACT;

        // SPAN-TODO: GC reporting - https://github.com/dotnet/coreclr/issues/8517

       _ASSERTE(IsStructPassedInRegs());

        TADDR genRegDest = dac_cast<TADDR>(GetStructGenRegDestinationAddress());
        INDEBUG(int remainingBytes = fieldBytes;)

        EEClass* eeClass = m_argLocDescForStructInRegs->m_eeClass;
        _ASSERTE(eeClass != NULL);

        for (int i = 0; i < eeClass->GetNumberEightBytes(); i++)
        {
            int eightByteSize = eeClass->GetEightByteSize(i);
            SystemVClassificationType eightByteClassification = eeClass->GetEightByteClassification(i);

            _ASSERTE(remainingBytes >= eightByteSize);

            if (eightByteClassification != SystemVClassificationTypeSSE)
            {
                if ((eightByteClassification == SystemVClassificationTypeIntegerReference) ||
                    (eightByteClassification == SystemVClassificationTypeIntegerByRef))
                {
                    _ASSERTE(eightByteSize == 8);
                    _ASSERTE(IS_ALIGNED((SIZE_T)genRegDest, 8));

                    uint32_t flags = eightByteClassification == SystemVClassificationTypeIntegerByRef ? GC_CALL_INTERIOR : 0;
                    (*fn)(dac_cast<PTR_PTR_Object>(genRegDest), sc, flags);
                }

                genRegDest += eightByteSize;
            }

            INDEBUG(remainingBytes -= eightByteSize;)
        }

        _ASSERTE(remainingBytes == 0);
    }

#endif // UNIX_AMD64_ABI

#if defined(_TARGET_MIPS64_)

    // Returns true if the ArgDestination represents a struct passed in registers.
    bool IsStructPassedInRegs()
    {
        LIMITED_METHOD_CONTRACT;
        return m_offset == TransitionBlock::StructInRegsOffset;
    }

    // Get destination address for floating point fields of a struct passed in registers.
    PTR_VOID GetStructFloatRegDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(IsStructPassedInRegs());
        int offset = TransitionBlock::GetOffsetOfFloatArgumentRegisters() + m_argLocDescForStructInRegs->m_idxFloatReg * 8;
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + offset);
    }

    // Get destination address for non-floating point fields of a struct passed in registers.
    PTR_VOID GetStructGenRegDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(IsStructPassedInRegs());
        int offset = TransitionBlock::GetOffsetOfArgumentRegisters() + m_argLocDescForStructInRegs->m_idxGenReg * 8;
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + offset);
    }

    // Get destination address for stack fields of a struct that splited.
    PTR_VOID GetStructStackDestinationAddress()
    {
        LIMITED_METHOD_CONTRACT;
        _ASSERTE(IsStructPassedInRegs());
        int offset = TransitionBlock::GetOffsetOfArgs();
        return dac_cast<PTR_VOID>(dac_cast<TADDR>(m_base) + offset);
    }


#ifndef DACCESS_COMPILE
    // Zero struct argument stored in registers described by the current ArgDestination.
    // Arguments:
    //  fieldBytes - size of the structure
    void ZeroStructInRegisters(int fieldBytes)
    {
        STATIC_CONTRACT_NOTHROW;
        STATIC_CONTRACT_GC_NOTRIGGER;
        STATIC_CONTRACT_FORBID_FAULT;
        STATIC_CONTRACT_MODE_COOPERATIVE;

        // To zero the struct, we create a zero filled array of large enough size and
        // then copy it to the registers. It is implemented this way to keep the complexity
        // of dealing with the eightbyte classification in single function.
        // This function is used rarely and so the overhead of reading the zeros from
        // the stack is negligible.
        _ASSERTE(IsStructPassedInRegs());

        BYTE* genRegDest = (BYTE*)GetStructGenRegDestinationAddress();
        BYTE* floatRegDest = (BYTE*)GetStructFloatRegDestinationAddress();
        BYTE* stackDest = (BYTE*)GetStructStackDestinationAddress();

        EEClass* eeClass = m_argLocDescForStructInRegs->m_eeClass;
        _ASSERTE(eeClass != NULL);

        int stackSlot = 0;
        for (int i = 0; i < eeClass->GetNumberEightBytes(); i++)
        {
            if (m_argLocDescForStructInRegs->m_idxGenReg + i < 8)
            {
                MIPS64ClassificationType eightByteClassification = eeClass->GetEightByteClassification(i);

                if (eightByteClassification == MIPS64ClassificationTypeDouble)
                {
                    *(UINT64*)floatRegDest = (UINT64)0;
                    floatRegDest += 8;
                }
                else
                {
                    _ASSERTE(IS_ALIGNED((SIZE_T)genRegDest, 8));
                    *(UINT64*)genRegDest = (UINT64)0;
                    genRegDest += 8;
                }
            }
            else
            {
                _ASSERTE(stackSlot < m_argLocDescForStructInRegs->m_cStack);
                *(UINT64*)stackDest = (UINT64)0;
                stackDest += 8;
                stackSlot++;
            }
        }
    }

    // Copy struct argument into registers described by the current ArgDestination.
    // Arguments:
    //  src = source data of the structure
    //  fieldBytes - size of the structure
    //  destOffset - nonzero when copying values into Nullable<T>, it is the offset
    //               of the T value inside of the Nullable<T>
    void CopyStructToRegisters(void *src, int fieldBytes, int destOffset)
    {
        STATIC_CONTRACT_NOTHROW;
        STATIC_CONTRACT_GC_NOTRIGGER;
        STATIC_CONTRACT_FORBID_FAULT;
        STATIC_CONTRACT_MODE_COOPERATIVE;

        _ASSERTE(IsStructPassedInRegs());

        BYTE* genRegDest = (BYTE*)GetStructGenRegDestinationAddress() + destOffset;
        BYTE* floatRegDest = (BYTE*)GetStructFloatRegDestinationAddress() + destOffset;
        BYTE* stackDest = (BYTE*)GetStructStackDestinationAddress();
        INDEBUG(int remainingBytes = fieldBytes;)

        EEClass* eeClass = m_argLocDescForStructInRegs->m_eeClass;
        _ASSERTE(eeClass != NULL);

        if (destOffset != 0)
        {
            _ASSERTE(destOffset <= 8); // Nullable<T>, if size of T is more than 8, need deal with it.
            if (m_argLocDescForStructInRegs->m_idxGenReg + (destOffset/8) < 8)
            {
                MIPS64ClassificationType eightByteClassification = eeClass->GetEightByteClassification(destOffset/8);
                if (eightByteClassification == MIPS64ClassificationTypeDouble)
                {
                    memcpyNoGCRefs(floatRegDest, src, fieldBytes);
                }
                else
                {
                    memcpyNoGCRefs(genRegDest, src, fieldBytes);
                }
            }
            else
            {
                memcpyNoGCRefs(stackDest, src, fieldBytes);
            }

            INDEBUG(remainingBytes -= fieldBytes;)
        }
        else
        {
            int stackSlot = 0;
            for (int i = 0; i < eeClass->GetNumberEightBytes(); i++)
            {
                if (m_argLocDescForStructInRegs->m_idxGenReg + i < 8)
                {
                    MIPS64ClassificationType eightByteClassification = eeClass->GetEightByteClassification(i);

                    if (eightByteClassification == MIPS64ClassificationTypeDouble)
                    {
                        *(UINT64*)floatRegDest = *(UINT64*)src;
                        floatRegDest += 8;
                    }
                    else
                    {
                        _ASSERTE(IS_ALIGNED((SIZE_T)genRegDest, 8));
                        *(UINT64*)genRegDest = *(UINT64*)src;
                        genRegDest += 8;
                    }
                }
                else
                {
                    _ASSERTE(stackSlot < m_argLocDescForStructInRegs->m_cStack);
                    *(UINT64*)stackDest = *(UINT64*)src;
                    stackDest += 8;
                    stackSlot++;
                }

                src = (BYTE*)src + 8;
                INDEBUG(remainingBytes -= 8;)
            }
        }

        _ASSERTE(remainingBytes <= 0);
    }

#endif //DACCESS_COMPILE

    // FIXME for MIPS: It may be not useful.
/*
    // Report managed object pointers in the struct in registers
    // Arguments:
    //  fn - promotion function to apply to each managed object pointer
    //  sc - scan context to pass to the promotion function
    //  fieldBytes - size of the structure
    void ReportPointersFromStructInRegisters(promote_func *fn, ScanContext *sc, int fieldBytes)
    {
        LIMITED_METHOD_CONTRACT;

        // SPAN-TODO: GC reporting - https://github.com/dotnet/coreclr/issues/8517

       _ASSERTE(IsStructPassedInRegs());

        TADDR genRegDest = dac_cast<TADDR>(GetStructGenRegDestinationAddress());
        TADDR stackDest = dac_cast<TADDR>(GetStructStackDestinationAddress());
        INDEBUG(int remainingBytes = fieldBytes;)

        EEClass* eeClass = m_argLocDescForStructInRegs->m_eeClass;
        _ASSERTE(eeClass != NULL);

        int stackSlot = 0;
        for (int i = 0; i < eeClass->GetNumberEightBytes() && i < 8; i++)
        {
            MIPS64ClassificationType eightByteClassification = eeClass->GetEightByteClassification(i);

            _ASSERTE(remainingBytes >= 8);

            if (m_argLocDescForStructInRegs->m_idxGenReg + i < 8)
            {
                if ((eightByteClassification == MIPS64ClassificationTypeIntegerReference) ||
                    (eightByteClassification == MIPS64ClassificationTypeIntegerByRef))
                {
                    _ASSERTE(IS_ALIGNED((SIZE_T)genRegDest, 8));

                    uint32_t flags = eightByteClassification == MIPS64ClassificationTypeIntegerByRef ? GC_CALL_INTERIOR : 0;
                    (*fn)(dac_cast<PTR_PTR_Object>(genRegDest), sc, flags);
                }
                genRegDest += 8;
            }
            else
            {
                _ASSERTE(stackSlot < m_argLocDescForStructInRegs->m_cStack);
                if ((eightByteClassification == MIPS64ClassificationTypeIntegerReference) ||
                    (eightByteClassification == MIPS64ClassificationTypeIntegerByRef))
                {

                    uint32_t flags = eightByteClassification == MIPS64ClassificationTypeIntegerByRef ? GC_CALL_INTERIOR : 0;
                    (*fn)(dac_cast<PTR_PTR_Object>(stackDest), sc, flags);
                }
                stackDest += 8;
                stackSlot++;
            }


            INDEBUG(remainingBytes -= 8;)
        }

        //_ASSERTE(remainingBytes <= 0);
    }
*/

#endif // _TARGET_MIPS64_

};

#endif // __ARGDESTINATION_H__
