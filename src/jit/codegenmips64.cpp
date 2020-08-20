// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                        MIPS64 Code Generator                              XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/
#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifdef _TARGET_MIPS64_
#include "emit.h"
#include "codegen.h"
#include "lower.h"
#include "gcinfo.h"
#include "gcinfoencoder.h"

static short splitLow(int value) {
    return (value & 0xffff);
}

// Returns true if 'value' is a legal signed immediate 16 bit encoding.
static bool isValidSimm16(ssize_t value)
{
    return -( ((int)1) << 15 ) <= value && value < ( ((int)1) << 15 );
};

static inline void set_Reg_To_Imm(emitter* emit, emitAttr size, regNumber reg, ssize_t imm)
{
    // reg cannot be a FP register
    assert(genIsValidIntReg(reg));

    size = EA_SIZE(size);

    if (-1 == (imm >> 15) || 0 == (imm >> 15)) {
        emit->emitIns_R_R_I(INS_addiu, size, reg, REG_R0, imm);
        return;
    }

    if (0 == (imm >> 16)) {
        emit->emitIns_R_R_I(INS_ori, size, reg, REG_R0, imm);
        return;
    }

    if (-1 == (imm >> 31) || 0 == (imm >> 31)) {
        emit->emitIns_R_I(INS_lui, size, reg, static_cast<uint16_t>(imm >> 16));
    } else if (0 == (imm >> 32)) {
        emit->emitIns_R_I(INS_lui, size, reg, static_cast<uint16_t>(imm >> 16));
        emit->emitIns_R_R_I_I(INS_dinsu, size, reg, REG_R0, 32, 32);
    } else if (-1 == (imm >> 47) || 0 == (imm >> 47)) {
        emit->emitIns_R_I(INS_lui, size, reg, static_cast<uint16_t>(imm >> 32));
        if (static_cast<uint16_t>(imm >> 16))
            emit->emitIns_R_R_I(INS_ori, size, reg, reg, static_cast<uint16_t>(imm >> 16));
        emit->emitIns_R_R_I(INS_dsll, size, reg, reg, 16);
    } else if (0 == (imm >> 48)) {
        emit->emitIns_R_I(INS_lui, size, reg, static_cast<uint16_t>(imm >> 32));
        emit->emitIns_R_R_I_I(INS_dinsu, size, reg, REG_R0, 32, 32);
        if (static_cast<uint16_t>(imm >> 16))
            emit->emitIns_R_R_I(INS_ori, size, reg, reg, static_cast<uint16_t>(imm >> 16));
        emit->emitIns_R_R_I(INS_dsll, size, reg, reg, 16);
    } else {
        emit->emitIns_R_I(INS_lui, size, reg, static_cast<uint16_t>(imm >> 48));
        if (static_cast<uint16_t>(imm >> 32))
            emit->emitIns_R_R_I(INS_ori, size, reg, reg, static_cast<uint16_t>(imm >> 32));
        if (static_cast<uint16_t>(imm >> 16)) {
            emit->emitIns_R_R_I(INS_dsll, size, reg, reg, 16);
            emit->emitIns_R_R_I(INS_ori, size, reg, reg, static_cast<uint16_t>(imm >> 16));
            emit->emitIns_R_R_I(INS_dsll, size, reg, reg, 16);
        } else {
            emit->emitIns_R_R_I(INS_dsll32, size, reg, reg, 0);
        }
    }
    if (static_cast<uint16_t>(imm))
        emit->emitIns_R_R_I(INS_ori, size, reg, reg, static_cast<uint16_t>(imm));
}

/*
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                           Prolog / Epilog                                 XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

//------------------------------------------------------------------------
// genInstrWithConstant:   we will typically generate one instruction
//
//    ins  reg1, reg2, imm
//
// However the imm might not fit as a directly encodable immediate,
// when it doesn't fit we generate extra instruction(s) that sets up
// the 'regTmp' with the proper immediate value.
//
//     li64  regTmp, imm
//     ins  reg1, reg2, regTmp
//
// Arguments:
//    ins                 - instruction
//    attr                - operation size and GC attribute
//    reg1, reg2          - first and second register operands
//    imm                 - immediate value (third operand when it fits)
//    tmpReg              - temp register to use when the 'imm' doesn't fit. Can be REG_NA
//                          if caller knows for certain the constant will fit.
//    inUnwindRegion      - true if we are in a prolog/epilog region with unwind codes.
//                          Default: false.
//
// Return Value:
//    returns true if the immediate was too large and tmpReg was used and modified.
//
bool CodeGen::genInstrWithConstant(instruction ins,
                                   emitAttr    attr,
                                   regNumber   reg1,
                                   regNumber   reg2,
                                   ssize_t     imm,
                                   regNumber   tmpReg,
                                   bool        inUnwindRegion /* = false */)
{
    bool     immFitsInIns = false;
    emitAttr size         = EA_SIZE(attr);

    // reg1 is usually a dest register
    // reg2 is always source register
    assert(tmpReg != reg2); // regTmp can not match any source register

    switch (ins)
    {
        case INS_daddiu:
            immFitsInIns = emitter::emitIns_valid_imm_for_add(imm, size);
            ins = immFitsInIns ? INS_daddiu : INS_daddu;
            break;

        case INS_sb:
        case INS_sh:
        case INS_sw:
        case INS_swc1:
        case INS_sd:
        case INS_sdc1:
            // reg1 is a source register for store instructions
            assert(tmpReg != reg1); // regTmp can not match any source register
            immFitsInIns = emitter::emitIns_valid_imm_for_ldst_offset(imm, size);
            break;

        case INS_lb:
        case INS_lh:
        case INS_lw:
        case INS_lwc1:
        case INS_ld:
        case INS_ldc1:
            immFitsInIns = emitter::emitIns_valid_imm_for_ldst_offset(imm, size);
            break;

        default:
            assert(!"Unexpected instruction in genInstrWithConstant");
            break;
    }

    if (immFitsInIns)
    {
        // generate a single instruction that encodes the immediate directly
        getEmitter()->emitIns_R_R_I(ins, attr, reg1, reg2, imm);
    }
    else
    {
        // caller can specify REG_NA  for tmpReg, when it "knows" that the immediate will always fit
        assert(tmpReg != REG_NA);

        // generate two or more instructions

        // first we load the immediate into tmpReg
        instGen_Set_Reg_To_Imm(size, tmpReg, imm);
        regSet.verifyRegUsed(tmpReg);

        // when we are in an unwind code region
        // we record the extra instructions using unwindPadding()
        if (inUnwindRegion)
        {
            compiler->unwindPadding();
        }

        switch (ins)
        {
            case INS_daddu:
                // generate the instruction using a three register encoding with the immediate in tmpReg
                getEmitter()->emitIns_R_R_R(ins, attr, reg1, reg2, tmpReg);
                break;

            case INS_sb:
            case INS_sh:
            case INS_sw:
            case INS_swc1:
            case INS_sd:
            case INS_sdc1:
            case INS_lb:
            case INS_lh:
            case INS_lw:
            case INS_lwc1:
            case INS_ld:
            case INS_ldc1:
                getEmitter()->emitIns_R_R_R(INS_daddu, attr, tmpReg, reg2, tmpReg);
                if (inUnwindRegion) compiler->unwindPadding();
                getEmitter()->emitIns_R_R_I(ins, attr, reg1, tmpReg, 0);
                break;

            default:
                assert(!"Unexpected instruction in genInstrWithConstant");
                break;
        }
    }
    return immFitsInIns;
}

//------------------------------------------------------------------------
// genStackPointerAdjustment: add a specified constant value to the stack pointer in either the prolog
// or the epilog. The unwind codes for the generated instructions are produced. An available temporary
// register is required to be specified, in case the constant is too large to encode in an "daddu"
// instruction (or "dsubu" instruction if we choose to use one), such that we need to load the constant
// into a register first, before using it.
//
// Arguments:
//    spDelta                 - the value to add to SP (can be negative)
//    tmpReg                  - an available temporary register
//    pTmpRegIsZero           - If we use tmpReg, and pTmpRegIsZero is non-null, we set *pTmpRegIsZero to 'false'.
//                              Otherwise, we don't touch it.
//    reportUnwindData        - If true, report the change in unwind data. Otherwise, do not report it.
//
// Return Value:
//    None.

void CodeGen::genStackPointerAdjustment(ssize_t spDelta, regNumber tmpReg, bool* pTmpRegIsZero, bool reportUnwindData)
{
    // Even though INS_daddiu is specified here, the encoder will choose either
    // an INS_daddu or an INS_daddiu and encode the immediate as a positive value
    //
    if (genInstrWithConstant(INS_daddiu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, spDelta, tmpReg, true))
    {
        if (pTmpRegIsZero != nullptr)
        {
            *pTmpRegIsZero = false;
        }
    }

    if (reportUnwindData)
    {
        // spDelta is negative in the prolog, positive in the epilog,
        // but we always tell the unwind codes the positive value.
        ssize_t  spDeltaAbs    = abs(spDelta);
        unsigned unwindSpDelta = (unsigned)spDeltaAbs;
        assert((ssize_t)unwindSpDelta == spDeltaAbs); // make sure that it fits in a unsigned

        compiler->unwindAllocStack(unwindSpDelta);
    }
}

//------------------------------------------------------------------------
// genPrologSaveRegPair: Save a pair of general-purpose or floating-point/SIMD registers in a function or funclet
// prolog. If possible, we use pre-indexed addressing to adjust SP and store the registers with a single instruction.
// The caller must ensure that we can use the STP instruction, and that spOffset will be in the legal range for that
// instruction.
//
// Arguments:
//    reg1                     - First register of pair to save.
//    reg2                     - Second register of pair to save.
//    spOffset                 - The offset from SP to store reg1 (must be positive or zero).
//    spDelta                  - If non-zero, the amount to add to SP before the register saves (must be negative or
//                               zero).
//    useSaveNextPair          - True if the last prolog instruction was to save the previous register pair. This
//                               allows us to emit the "save_next" unwind code.
//    tmpReg                   - An available temporary register. Needed for the case of large frames.
//    pTmpRegIsZero            - If we use tmpReg, and pTmpRegIsZero is non-null, we set *pTmpRegIsZero to 'false'.
//                               Otherwise, we don't touch it.
//
// Return Value:
//    None.

void CodeGen::genPrologSaveRegPair(regNumber reg1,
                                   regNumber reg2,
                                   int       spOffset,
                                   int       spDelta,
                                   bool      useSaveNextPair,
                                   regNumber tmpReg,
                                   bool*     pTmpRegIsZero)
{
    assert(spOffset >= 0);
    assert(spDelta <= 0);
    assert((spDelta % 16) == 0);                                  // SP changes must be 16-byte aligned
    assert(genIsValidFloatReg(reg1) == genIsValidFloatReg(reg2)); // registers must be both general-purpose, or both
                                                                  // FP/SIMD

    instruction ins = INS_sd;
    if (genIsValidFloatReg(reg1))
        ins = INS_sdc1;

    if (spDelta != 0)
    {
        // generate daddiu SP,SP,-imm
        genStackPointerAdjustment(spDelta, tmpReg, pTmpRegIsZero, /* reportUnwindData */ true);

        assert((spDelta+spOffset+16)<=0);

        assert(spOffset <= 32751);//32767-16
    }

    getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
    compiler->unwindSaveReg(reg1, spOffset);

    getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg2, REG_SPBASE, spOffset+8);
    compiler->unwindSaveReg(reg2, spOffset+8);
}

//------------------------------------------------------------------------
// genPrologSaveReg: Like genPrologSaveRegPair, but for a single register. Save a single general-purpose or
// floating-point/SIMD register in a function or funclet prolog. Note that if we wish to change SP (i.e., spDelta != 0),
// then spOffset must be 8. This is because otherwise we would create an alignment hole above the saved register, not
// below it, which we currently don't support. This restriction could be loosened if the callers change to handle it
// (and this function changes to support using pre-indexed SD addressing). The caller must ensure that we can use the
// SD instruction, and that spOffset will be in the legal range for that instruction.
//
// Arguments:
//    reg1                     - Register to save.
//    spOffset                 - The offset from SP to store reg1 (must be positive or zero).
//    spDelta                  - If non-zero, the amount to add to SP before the register saves (must be negative or
//                               zero).
//    tmpReg                   - An available temporary register. Needed for the case of large frames.
//    pTmpRegIsZero            - If we use tmpReg, and pTmpRegIsZero is non-null, we set *pTmpRegIsZero to 'false'.
//                               Otherwise, we don't touch it.
//
// Return Value:
//    None.

void CodeGen::genPrologSaveReg(regNumber reg1, int spOffset, int spDelta, regNumber tmpReg, bool* pTmpRegIsZero)
{
    assert(spOffset >= 0);
    assert(spDelta <= 0);
    assert((spDelta % 16) == 0); // SP changes must be 16-byte aligned

    instruction ins = INS_sd;
    if (genIsValidFloatReg(reg1))
        ins = INS_sdc1;

    if (spDelta != 0)
    {
        // generate daddiu SP,SP,-imm
        genStackPointerAdjustment(spDelta, tmpReg, pTmpRegIsZero, /* reportUnwindData */ true);
    }

    getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
    compiler->unwindSaveReg(reg1, spOffset);

}

//------------------------------------------------------------------------
// genEpilogRestoreRegPair: This is the opposite of genPrologSaveRegPair(), run in the epilog instead of the prolog.
// The stack pointer adjustment, if requested, is done after the register restore, using post-index addressing.
// The caller must ensure that we can use the LDP instruction, and that spOffset will be in the legal range for that
// instruction.
//
// Arguments:
//    reg1                     - First register of pair to restore.
//    reg2                     - Second register of pair to restore.
//    spOffset                 - The offset from SP to load reg1 (must be positive or zero).
//    spDelta                  - If non-zero, the amount to add to SP after the register restores (must be positive or
//                               zero).
//    useSaveNextPair          - True if the last prolog instruction was to save the previous register pair. This
//                               allows us to emit the "save_next" unwind code.
//    tmpReg                   - An available temporary register. Needed for the case of large frames.
//    pTmpRegIsZero            - If we use tmpReg, and pTmpRegIsZero is non-null, we set *pTmpRegIsZero to 'false'.
//                               Otherwise, we don't touch it.
//
// Return Value:
//    None.

void CodeGen::genEpilogRestoreRegPair(regNumber reg1,
                                      regNumber reg2,
                                      int       spOffset,
                                      int       spDelta,
                                      bool      useSaveNextPair,
                                      regNumber tmpReg,
                                      bool*     pTmpRegIsZero)
{
    assert(spOffset >= 0);
    assert(spDelta >= 0);
    assert((spDelta % 16) == 0);                                  // SP changes must be 16-byte aligned
    assert(genIsValidFloatReg(reg1) == genIsValidFloatReg(reg2)); // registers must be both general-purpose, or both
                                                                  // FP/SIMD

    instruction ins = INS_ld;
    if (genIsValidFloatReg(reg1))
        ins = INS_ldc1;

    if (spDelta != 0)
    {
        assert(!useSaveNextPair);

        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg2, REG_SPBASE, spOffset+8);
        compiler->unwindSaveReg(reg2, spOffset+8);

        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
        compiler->unwindSaveReg(reg1, spOffset);

        // generate daddiu SP,SP,imm
        genStackPointerAdjustment(spDelta, tmpReg, pTmpRegIsZero, /* reportUnwindData */ true);
    }
    else
    {
        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg2, REG_SPBASE, spOffset+8);
        compiler->unwindSaveReg(reg2, spOffset+8);

        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
        compiler->unwindSaveReg(reg1, spOffset);
    }
}

//------------------------------------------------------------------------
// genEpilogRestoreReg: The opposite of genPrologSaveReg(), run in the epilog instead of the prolog.
//
// Arguments:
//    reg1                     - Register to restore.
//    spOffset                 - The offset from SP to restore reg1 (must be positive or zero).
//    spDelta                  - If non-zero, the amount to add to SP after the register restores (must be positive or
//                               zero).
//    tmpReg                   - An available temporary register. Needed for the case of large frames.
//    pTmpRegIsZero            - If we use tmpReg, and pTmpRegIsZero is non-null, we set *pTmpRegIsZero to 'false'.
//                               Otherwise, we don't touch it.
//
// Return Value:
//    None.

void CodeGen::genEpilogRestoreReg(regNumber reg1, int spOffset, int spDelta, regNumber tmpReg, bool* pTmpRegIsZero)
{
    assert(spOffset >= 0);
    assert(spDelta >= 0);
    assert((spDelta % 16) == 0); // SP changes must be 16-byte aligned

    instruction ins = INS_ld;
    if (genIsValidFloatReg(reg1))
        ins = INS_ldc1;

    if (spDelta != 0)
    {
        // ld reg1, offset(SP)
        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
        compiler->unwindSaveReg(reg1, spOffset);

        // generate add SP,SP,imm
        genStackPointerAdjustment(spDelta, tmpReg, pTmpRegIsZero, /* reportUnwindData */ true);
    }
    else
    {
        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg1, REG_SPBASE, spOffset);
        compiler->unwindSaveReg(reg1, spOffset);
    }
}

//------------------------------------------------------------------------
// genBuildRegPairsStack: Build a stack of register pairs for prolog/epilog save/restore for the given mask.
// The first register pair will contain the lowest register. Register pairs will combine neighbor
// registers in pairs. If it can't be done (for example if we have a hole or this is the last reg in a mask with
// odd number of regs) then the second element of that RegPair will be REG_NA.
//
// Arguments:
//   regsMask - a mask of registers for prolog/epilog generation;
//   regStack - a regStack instance to build the stack in, used to save temp copyings.
//
// Return value:
//   no return value; the regStack argument is modified.
//
// static
void CodeGen::genBuildRegPairsStack(regMaskTP regsMask, ArrayStack<RegPair>* regStack)
{
    assert(regStack != nullptr);
    assert(regStack->Height() == 0);

    unsigned regsCount = genCountBits(regsMask);

    while (regsMask != RBM_NONE)
    {
        regMaskTP reg1Mask = genFindLowestBit(regsMask);
        regNumber reg1     = genRegNumFromMask(reg1Mask);
        regsMask &= ~reg1Mask;
        regsCount -= 1;

        bool isPairSave = false;
        if (regsCount > 0)
        {
            regMaskTP reg2Mask = genFindLowestBit(regsMask);
            regNumber reg2     = genRegNumFromMask(reg2Mask);
            if (reg2 == REG_NEXT(reg1))
            {
                // The JIT doesn't allow saving pair (S7,FP), even though the
                // save_regp register pair unwind code specification allows it.
                // The JIT always saves (FP,RA) as a pair, and uses the save_fpra
                // unwind code. This only comes up in stress mode scenarios
                // where callee-saved registers are not allocated completely
                // from lowest-to-highest, without gaps.
                if (reg1 != REG_GP)
                {
                    // Both registers must have the same type to be saved as pair.
                    if (genIsValidFloatReg(reg1) == genIsValidFloatReg(reg2))
                    {
                        isPairSave = true;

                        regsMask &= ~reg2Mask;
                        regsCount -= 1;

                        regStack->Push(RegPair(reg1, reg2));
                    }
                }
            }
        }
        if (!isPairSave)
        {
            regStack->Push(RegPair(reg1));
        }
    }
    assert(regsCount == 0 && regsMask == RBM_NONE);

    genSetUseSaveNextPairs(regStack);
}

//------------------------------------------------------------------------
// genSetUseSaveNextPairs: Set useSaveNextPair for each RegPair on the stack which unwind info can be encoded as
// save_next code.
//
// Arguments:
//   regStack - a regStack instance to set useSaveNextPair.
//
// Notes:
// We can use save_next for RegPair(N, N+1) only when we have sequence like (N-2, N-1), (N, N+1).
// In this case in the prolog save_next for (N, N+1) refers to save_pair(N-2, N-1);
// in the epilog the unwinder will search for the first save_pair (N-2, N-1)
// and then go back to the first save_next (N, N+1) to restore it first.
//
// static
void CodeGen::genSetUseSaveNextPairs(ArrayStack<RegPair>* regStack)
{
    for (int i = 1; i < regStack->Height(); ++i)
    {
        RegPair& curr = regStack->BottomRef(i);
        RegPair  prev = regStack->Bottom(i - 1);

        if (prev.reg2 == REG_NA || curr.reg2 == REG_NA)
        {
            continue;
        }

        if (REG_NEXT(prev.reg2) != curr.reg1)
        {
            continue;
        }

        if (genIsValidFloatReg(prev.reg2) != genIsValidFloatReg(curr.reg1))
        {
            // It is possible to support changing of the last int pair with the first float pair,
            // but it is very rare case and it would require superfluous changes in the unwinder.
            continue;
        }
        curr.useSaveNextPair = true;
    }
}

//------------------------------------------------------------------------
// genGetSlotSizeForRegsInMask: Get the stack slot size appropriate for the register type from the mask.
//
// Arguments:
//   regsMask - a mask of registers for prolog/epilog generation.
//
// Return value:
//   stack slot size in bytes.
//
// Note: Because int and float register type sizes match we can call this function with a mask that includes both.
//
// static
int CodeGen::genGetSlotSizeForRegsInMask(regMaskTP regsMask)
{
    assert((regsMask & (RBM_CALLEE_SAVED | RBM_FP | RBM_RA)) == regsMask); // Do not expect anything else.

    static_assert_no_msg(REGSIZE_BYTES == FPSAVE_REGSIZE_BYTES);
    return REGSIZE_BYTES;
}

//------------------------------------------------------------------------
// genSaveCalleeSavedRegisterGroup: Saves the group of registers described by the mask.
//
// Arguments:
//   regsMask             - a mask of registers for prolog generation;
//   spDelta              - if non-zero, the amount to add to SP before the first register save (or together with it);
//   spOffset             - the offset from SP that is the beginning of the callee-saved register area;
//
void CodeGen::genSaveCalleeSavedRegisterGroup(regMaskTP regsMask, int spDelta, int spOffset)
{
    const int slotSize = genGetSlotSizeForRegsInMask(regsMask);

    ArrayStack<RegPair> regStack(compiler->getAllocator(CMK_Codegen));
    genBuildRegPairsStack(regsMask, &regStack);

    for (int i = 0; i < regStack.Height(); ++i)
    {
        RegPair regPair = regStack.Bottom(i);
        if (regPair.reg2 != REG_NA)
        {
            // We can use two SD instructions.
            genPrologSaveRegPair(regPair.reg1, regPair.reg2, spOffset, spDelta, regPair.useSaveNextPair, REG_AT,
                                 nullptr);

            spOffset += 2 * slotSize;
        }
        else
        {
            // No register pair; we use a SD instruction.
            genPrologSaveReg(regPair.reg1, spOffset, spDelta, REG_AT, nullptr);
            spOffset += slotSize;
        }

        spDelta = 0; // We've now changed SP already, if necessary; don't do it again.
    }
}

//------------------------------------------------------------------------
// genSaveCalleeSavedRegistersHelp: Save the callee-saved registers in 'regsToSaveMask' to the stack frame
// in the function or funclet prolog. Registers are saved in register number order from low addresses
// to high addresses. This means that integer registers are saved at lower addresses than floatint-point/SIMD
// registers.
//
// If establishing frame pointer chaining, it must be done after saving the callee-saved registers.
//
// We can only use the instructions that are allowed by the unwind codes. The caller ensures that
// there is enough space on the frame to store these registers, and that the store instructions
// we need to use (SD) are encodable with the stack-pointer immediate offsets we need to use.
//
// The caller can tell us to fold in a stack pointer adjustment, which we will do with the first instruction.
// Note that the stack pointer adjustment must be by a multiple of 16 to preserve the invariant that the
// stack pointer is always 16 byte aligned. If we are saving an odd number of callee-saved
// registers, though, we will have an empty aligment slot somewhere. It turns out we will put
// it below (at a lower address) the callee-saved registers, as that is currently how we
// do frame layout. This means that the first stack offset will be 8 and the stack pointer
// adjustment must be done by a SUB, and not folded in to a pre-indexed store.
//
// Arguments:
//    regsToSaveMask          - The mask of callee-saved registers to save. If empty, this function does nothing.
//    lowestCalleeSavedOffset - The offset from SP that is the beginning of the callee-saved register area. Note that
//                              if non-zero spDelta, then this is the offset of the first save *after* that
//                              SP adjustment.
//    spDelta                 - If non-zero, the amount to add to SP before the register saves (must be negative or
//                              zero).
//
// Notes:
//    The save set can not contain FP/RA in which case FP/RA is saved along with the other callee-saved registers.
//
void CodeGen::genSaveCalleeSavedRegistersHelp(regMaskTP regsToSaveMask, int lowestCalleeSavedOffset, int spDelta)
{
    assert(spDelta <= 0);

    unsigned regsToSaveCount = genCountBits(regsToSaveMask);
    if (regsToSaveCount == 0)
    {
        if (spDelta != 0)
        {
            // Currently this is the case for varargs only
            // whose size is MAX_REG_ARG * REGSIZE_BYTES = 64 bytes.
            genStackPointerAdjustment(spDelta, REG_AT, nullptr, /* reportUnwindData */ true);
        }
        return;
    }

    assert((spDelta % 16) == 0);

    assert(regsToSaveCount <= genCountBits(RBM_CALLEE_SAVED));

    // Save integer registers at higher addresses than floating-point registers.

    regMaskTP maskSaveRegsFloat = regsToSaveMask & RBM_ALLFLOAT;
    regMaskTP maskSaveRegsInt   = regsToSaveMask & ~maskSaveRegsFloat;

    if (maskSaveRegsFloat != RBM_NONE)
    {
        genSaveCalleeSavedRegisterGroup(maskSaveRegsFloat, spDelta, lowestCalleeSavedOffset);
        spDelta = 0;
        lowestCalleeSavedOffset += genCountBits(maskSaveRegsFloat) * FPSAVE_REGSIZE_BYTES;
    }

    if (maskSaveRegsInt != RBM_NONE)
    {
        genSaveCalleeSavedRegisterGroup(maskSaveRegsInt, spDelta, lowestCalleeSavedOffset);
        // No need to update spDelta, lowestCalleeSavedOffset since they're not used after this.
    }
}

//------------------------------------------------------------------------
// genRestoreCalleeSavedRegisterGroup: Restores the group of registers described by the mask.
//
// Arguments:
//   regsMask             - a mask of registers for epilog generation;
//   spDelta              - if non-zero, the amount to add to SP after the last register restore (or together with it);
//   spOffset             - the offset from SP that is the beginning of the callee-saved register area;
//
void CodeGen::genRestoreCalleeSavedRegisterGroup(regMaskTP regsMask, int spDelta, int spOffset)
{
    const int slotSize = genGetSlotSizeForRegsInMask(regsMask);

    ArrayStack<RegPair> regStack(compiler->getAllocator(CMK_Codegen));
    genBuildRegPairsStack(regsMask, &regStack);

    int stackDelta = 0;
    for (int i = 0; i < regStack.Height(); ++i)
    {
        bool lastRestoreInTheGroup = (i == regStack.Height() - 1);
        bool updateStackDelta      = lastRestoreInTheGroup && (spDelta != 0);
        if (updateStackDelta)
        {
            // Update stack delta only if it is the last restore (the first save).
            assert(stackDelta == 0);
            stackDelta = spDelta;
        }

        RegPair regPair = regStack.Index(i);
        if (regPair.reg2 != REG_NA)
        {
            spOffset -= 2 * slotSize;

            genEpilogRestoreRegPair(regPair.reg1, regPair.reg2, spOffset, stackDelta, regPair.useSaveNextPair, REG_AT,
                                    nullptr);
        }
        else
        {
            spOffset -= slotSize;
            genEpilogRestoreReg(regPair.reg1, spOffset, stackDelta, REG_AT, nullptr);
        }
    }
}

//------------------------------------------------------------------------
// genRestoreCalleeSavedRegistersHelp: Restore the callee-saved registers in 'regsToRestoreMask' from the stack frame
// in the function or funclet epilog. This exactly reverses the actions of genSaveCalleeSavedRegistersHelp().
//
// Arguments:
//    regsToRestoreMask       - The mask of callee-saved registers to restore. If empty, this function does nothing.
//    lowestCalleeSavedOffset - The offset from SP that is the beginning of the callee-saved register area.
//    spDelta                 - If non-zero, the amount to add to SP after the register restores (must be positive or
//                              zero).
//
// Here's an example restore sequence:
//      ld     s7, 88(sp)
//      ld     s6, 80(sp)
//      ld     s5, 72(sp)
//      ld     s4, 64(sp)
//      ld     s3, 56(sp)
//      ld     s2, 48(sp)
//      ld     s1, 40(sp)
//      ld     s0, 32(sp)
//
// For the case of non-zero spDelta, we assume the base of the callee-save registers to restore is at SP, and
// the last restore adjusts SP by the specified amount. For example:
//      ld     s7, 56(sp)
//      ld     s6, 48(sp)
//      ld     s5, 40(sp)
//      ld     s4, 32(sp)
//      ld     s3, 24(sp)
//      ld     s2, 16(sp)
//      ld     s1, 88(sp)
//      ld     s0, 80(sp)
//
// Note you call the unwind functions specifying the prolog operation that is being un-done. So, for example, when
// generating a post-indexed load, you call the unwind function for specifying the corresponding preindexed store.
//
// Return Value:
//    None.

void CodeGen::genRestoreCalleeSavedRegistersHelp(regMaskTP regsToRestoreMask, int lowestCalleeSavedOffset, int spDelta)
{
    assert(spDelta >= 0);
    unsigned regsToRestoreCount = genCountBits(regsToRestoreMask);
    if (regsToRestoreCount == 0)
    {
        if (spDelta != 0)
        {
            // Currently this is the case for varargs only
            // whose size is MAX_REG_ARG * REGSIZE_BYTES = 64 bytes.
            genStackPointerAdjustment(spDelta, REG_AT, nullptr, /* reportUnwindData */ true);
        }
        return;
    }

    assert((spDelta % 16) == 0);

    // We also can restore FP and RA, even though they are not in RBM_CALLEE_SAVED.
    assert(regsToRestoreCount <= genCountBits(RBM_CALLEE_SAVED | RBM_FP | RBM_RA));

    // Point past the end, to start. We predecrement to find the offset to load from.
    static_assert_no_msg(REGSIZE_BYTES == FPSAVE_REGSIZE_BYTES);
    int spOffset = lowestCalleeSavedOffset + regsToRestoreCount * REGSIZE_BYTES;

    // Save integer registers at higher addresses than floating-point registers.

    regMaskTP maskRestoreRegsFloat = regsToRestoreMask & RBM_ALLFLOAT;
    regMaskTP maskRestoreRegsInt   = regsToRestoreMask & ~maskRestoreRegsFloat;

    // Restore in the opposite order of saving.

    if (maskRestoreRegsInt != RBM_NONE)
    {
        int spIntDelta = (maskRestoreRegsFloat != RBM_NONE) ? 0 : spDelta; // should we delay the SP adjustment?
        genRestoreCalleeSavedRegisterGroup(maskRestoreRegsInt, spIntDelta, spOffset);
        spOffset -= genCountBits(maskRestoreRegsInt) * REGSIZE_BYTES;
    }

    if (maskRestoreRegsFloat != RBM_NONE)
    {
        // If there is any spDelta, it must be used here.
        genRestoreCalleeSavedRegisterGroup(maskRestoreRegsFloat, spDelta, spOffset);
        // No need to update spOffset since it's not used after this.
    }
}

// clang-format off
/*****************************************************************************
 *
 *  Generates code for an EH funclet prolog.
 *
 *  Funclets have the following incoming arguments:
 *
 *      catch:          a0 = the exception object that was caught (see GT_CATCH_ARG)
 *      filter:         a0 = the exception object to filter (see GT_CATCH_ARG), a1 = CallerSP of the containing function
 *      finally/fault:  none
 *
 *  Funclets set the following registers on exit:
 *
 *      catch:          v0 = the address at which execution should resume (see BBJ_EHCATCHRET)
 *      filter:         v0 = non-zero if the handler should handle the exception, zero otherwise (see GT_RETFILT)
 *      finally/fault:  none
 *
 *  The MIPS64 funclet prolog sequence is one of the following (Note: #framesz is total funclet frame size,
 *  including everything; #outsz is outgoing argument space. #framesz must be a multiple of 16):
 *
 *  Frame type 1:
 *     For #framesz <= 32760 and FP/RA at bottom:
 *     daddiu sp,sp,-#framesz    ; establish the frame (predecrement by #framesz), save FP/RA
 *     sd fp,#outsz(sp)
 *     sd ra,#outsz+8(sp)
 *     sd s0,#xxx-8(sp)          ; save callee-saved registers, as necessary
 *     sd s1,#xxx(sp)
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in CoreRT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |      Saved FP, RA     | // 16 bytes
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes; if required (i.e., #outsz != 0)
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 2:
 *     For #framesz <= 32760 and FP/RA at top:
 *     daddiu sp,sp,-#framesz          ; establish the frame
 *     sd s0,xxx(sp)                 ; save callee-saved registers, as necessary
 *     sd s1,xxx+8(sp)
 *     sd s?,xxx+?(sp)
 *     sd fp,xxx+?(sp)              ; save FP/RA.
 *     sd ra,xxx+?(sp)
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |      Saved FP, RA     | // 16 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in CoreRT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes; if required (i.e., #outsz != 0)
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 3:
 *     For #framesz > 32760 and FP/RA at bottom:
 *     ; for funclet, #framesz-#outsz will be less than 32760.
 *
 *     daddiu sp,sp,-(#framesz-#FPRA_delta)     ; note maybe 16byte-alignment.
 *     sd fp, pad(sp)                           ; pad is depended on stack-16byte-alignment..
 *     sd ra, pad+8(sp)
 *     sd s0,#xxx(sp)                         ; save callee-saved registers, as necessary,
 *     sd s1,#xxx+8(sp)
 *     daddiu sp,sp,-#outsz                     ; create space for outgoing argument space, mabye 16byte-alignment.
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in CoreRT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~
 *      |-----------------------|
 *      |      Saved FP, RA     | // 16 bytes
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *  Frame type 4:
 *     For #framesz > 32760 and FP/RA at top:
 *     daddiu sp,sp,-#framesz+PSP_offset  ; establish the frame, maybe 16byte-alignment.
 *     sd s0,xxx(sp)                      ; save callee-saved registers, as necessary
 *     sd s1,xxx+8(sp)
 *     sd s?,xxx+?(sp)
 *     sd fp,xxx+?(sp)              ; save FP/RA.
 *     sd ra,xxx+?(sp)
 *
 *     daddiu sp,sp,-#PSP_offset    ; establish the frame, maybe 16byte-alignment.
 *
 *  The funclet frame is thus:
 *
 *      |                       |
 *      |-----------------------|
 *      |  incoming arguments   |
 *      +=======================+ <---- Caller's SP
 *      |  Varargs regs space   | // Only for varargs main functions; 64 bytes
 *      |-----------------------|
 *      |      Saved FP, RA     | // 16 bytes
 *      |-----------------------|
 *      |Callee saved registers | // multiple of 8 bytes
 *      |-----------------------|
 *      |        PSP slot       | // 8 bytes (omitted in CoreRT ABI)
 *      |-----------------------|
 *      ~  alignment padding    ~ // To make the whole frame 16 byte aligned.
 *      |-----------------------|
 *      |   Outgoing arg space  | // multiple of 8 bytes; if required (i.e., #outsz != 0)
 *      |-----------------------| <---- Ambient SP
 *      |       |               |
 *      ~       | Stack grows   ~
 *      |       | downward      |
 *              V
 *
 *
 * Both #1 and #2 only change SP once. That means that there will be a maximum of one alignment slot needed. For the general case, #3,
 * it is possible that we will need to add alignment to both changes to SP, leading to 16 bytes of alignment. Remember that the stack
 * pointer needs to be 16 byte aligned at all times. The size of the PSP slot plus callee-saved registers space is a maximum of 224 bytes:
 *
 *     FP,RA registers
 *     8 int callee-saved register s0-s7
 *     8 float callee-saved registers f24-f31
 *     8 saved integer argument registers a0-a7, if varargs function
 *     1 PSP slot
 *     1 alignment slot, future maybe add gp
 *     == 28 slots * 8 bytes = 224 bytes.
 *
 * The outgoing argument size, however, can be very large, if we call a function that takes a large number of
 * arguments (note that we currently use the same outgoing argument space size in the funclet as for the main
 * function, even if the funclet doesn't have any calls, or has a much smaller, or larger, maximum number of
 * outgoing arguments for any call). In that case, we need to 16-byte align the initial change to SP, before
 * saving off the callee-saved registers and establishing the PSPsym, so we can use the limited immediate offset
 * encodings we have available, before doing another 16-byte aligned SP adjustment to create the outgoing argument
 * space. Both changes to SP might need to add alignment padding.
 *
 * In addition to the above "standard" frames, we also need to support a frame where the saved FP/RA are at the
 * highest addresses. This is to match the frame layout (specifically, callee-saved registers including FP/RA
 * and the PSPSym) that is used in the main function when a GS cookie is required due to the use of localloc.
 * (Note that localloc cannot be used in a funclet.) In these variants, not only has the position of FP/RA
 * changed, but where the alignment padding is placed has also changed.
 *
 *
 * Note that in all cases, the PSPSym is in exactly the same position with respect to Caller-SP, and that location is the same relative to Caller-SP
 * as in the main function.
 *
 * Funclets do not have varargs arguments. However, because the PSPSym must exist at the same offset from Caller-SP as in the main function, we
 * must add buffer space for the saved varargs/argument registers here, if the main function did the same.
 *
 *     ; After this header, fill the PSP slot, for use by the VM (it gets reported with the GC info), or by code generation of nested filters.
 *     ; This is not part of the "OS prolog"; it has no associated unwind data, and is not reversed in the funclet epilog.
 *
 *     if (this is a filter funclet)
 *     {
 *          // a1 on entry to a filter funclet is CallerSP of the containing function:
 *          // either the main function, or the funclet for a handler that this filter is dynamically nested within.
 *          // Note that a filter can be dynamically nested within a funclet even if it is not statically within
 *          // a funclet. Consider:
 *          //
 *          //    try {
 *          //        try {
 *          //            throw new Exception();
 *          //        } catch(Exception) {
 *          //            throw new Exception();     // The exception thrown here ...
 *          //        }
 *          //    } filter {                         // ... will be processed here, while the "catch" funclet frame is still on the stack
 *          //    } filter-handler {
 *          //    }
 *          //
 *          // Because of this, we need a PSP in the main function anytime a filter funclet doesn't know whether the enclosing frame will
 *          // be a funclet or main function. We won't know any time there is a filter protecting nested EH. To simplify, we just always
 *          // create a main function PSP for any function with a filter.
 *
 *          ld a1, CallerSP_to_PSP_slot_delta(a1)  ; Load the CallerSP of the main function (stored in the PSP of the dynamically containing funclet or function)
 *          sd a1, SP_to_PSP_slot_delta(sp)        ; store the PSP
 *          daddiu fp, a1, Function_CallerSP_to_FP_delta ; re-establish the frame pointer
 *     }
 *     else
 *     {
 *          // This is NOT a filter funclet. The VM re-establishes the frame pointer on entry.
 *          // TODO-MIPS64-CQ: if VM set x1 to CallerSP on entry, like for filters, we could save an instruction.
 *
 *          daddiu a3, fp, Function_FP_to_CallerSP_delta  ; compute the CallerSP, given the frame pointer. a3 is scratch?
 *          sd a3, SP_to_PSP_slot_delta(sp)         ; store the PSP
 *     }
 *
 *  An example epilog sequence is then:
 *
 *     daddiu sp,sp,#outsz             ; if any outgoing argument space
 *     ...                          ; restore callee-saved registers
 *     ld s0,#xxx-8(sp)
 *     ld s1,#xxx(sp)
 *     ld fp,#framesz-8(sp)
 *     ld ra,#framesz(sp)
 *     daddiu  sp,sp,#framesz
 *     jr  ra
 *
 */
// clang-format on

void CodeGen::genFuncletProlog(BasicBlock* block)
{
#ifdef DEBUG
    if (verbose)
        printf("*************** In genFuncletProlog()\n");
#endif

    assert(block != NULL);
    assert(block->bbFlags & BBF_FUNCLET_BEG);

    ScopedSetVariable<bool> _setGeneratingProlog(&compiler->compGeneratingProlog, true);

    gcInfo.gcResetForBB();

    compiler->unwindBegProlog();

    regMaskTP maskSaveRegsFloat = genFuncletInfo.fiSaveRegs & RBM_ALLFLOAT;
    regMaskTP maskSaveRegsInt   = genFuncletInfo.fiSaveRegs & ~maskSaveRegsFloat;

    // Funclets must always save RA and FP, since when we have funclets we must have an FP frame.
    assert((maskSaveRegsInt & RBM_RA) != 0);
    assert((maskSaveRegsInt & RBM_FP) != 0);

    bool isFilter = (block->bbCatchTyp == BBCT_FILTER);
    int frameSize  = genFuncletInfo.fiSpDelta1;

    regMaskTP maskArgRegsLiveIn;
    if (isFilter)
    {
        maskArgRegsLiveIn = RBM_A0 | RBM_A1;
    }
    else if ((block->bbCatchTyp == BBCT_FINALLY) || (block->bbCatchTyp == BBCT_FAULT))
    {
        maskArgRegsLiveIn = RBM_NONE;
    }
    else
    {
        maskArgRegsLiveIn = RBM_A0;
    }

#ifdef DEBUG
    if (compiler->opts.disAsm)
        printf("DEBUG: CodeGen::genFuncletProlog, frameType:%d\n\n", genFuncletInfo.fiFrameType);
#endif

    int offset = 0;
    if (genFuncletInfo.fiFrameType == 1)
    {
        // fiFrameType constraints:
        assert(frameSize < 0);
        assert(frameSize >= -32768);

        assert(genFuncletInfo.fiSP_to_FPRA_save_delta < 32760);
        genStackPointerAdjustment(frameSize, REG_AT, nullptr, /* reportUnwindData */ true);

        getEmitter()->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_FP, REG_SPBASE, genFuncletInfo.fiSP_to_FPRA_save_delta);
        compiler->unwindSaveReg(REG_FP, genFuncletInfo.fiSP_to_FPRA_save_delta);

        getEmitter()->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_RA, REG_SPBASE, genFuncletInfo.fiSP_to_FPRA_save_delta + 8);
        compiler->unwindSaveReg(REG_RA, genFuncletInfo.fiSP_to_FPRA_save_delta + 8);

        maskSaveRegsInt &= ~(RBM_RA | RBM_FP); // We've saved these now

        genSaveCalleeSavedRegistersHelp(maskSaveRegsInt | maskSaveRegsFloat, genFuncletInfo.fiSP_to_PSP_slot_delta + 8, 0);
    }
    else if (genFuncletInfo.fiFrameType == 2)
    {
        // fiFrameType constraints:
        assert(frameSize < 0);
        assert(frameSize >= -32768);

        assert(genFuncletInfo.fiSP_to_FPRA_save_delta < 32760);
        genStackPointerAdjustment(frameSize, REG_AT, nullptr, /* reportUnwindData */ true);

        genSaveCalleeSavedRegistersHelp(maskSaveRegsInt | maskSaveRegsFloat, genFuncletInfo.fiSP_to_PSP_slot_delta + 8, 0);
    }
    else if (genFuncletInfo.fiFrameType == 3)
    {
        // fiFrameType constraints:
        assert(frameSize < -32768);

        offset = -frameSize - genFuncletInfo.fiSP_to_FPRA_save_delta;
        int SP_delta = roundUp((UINT)offset, STACK_ALIGN);
        offset = SP_delta - offset;

        genStackPointerAdjustment(-SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);

        getEmitter()->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_FP, REG_SPBASE, offset);
        compiler->unwindSaveReg(REG_FP, offset);

        getEmitter()->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_RA, REG_SPBASE, offset + 8);
        compiler->unwindSaveReg(REG_RA, offset + 8);

        maskSaveRegsInt &= ~(RBM_RA | RBM_FP); // We've saved these now

        offset = frameSize + SP_delta + genFuncletInfo.fiSP_to_PSP_slot_delta + 8;
        genSaveCalleeSavedRegistersHelp(maskSaveRegsInt | maskSaveRegsFloat, offset, 0);

        genStackPointerAdjustment(frameSize + SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else if (genFuncletInfo.fiFrameType == 4)
    {
        // fiFrameType constraints:
        assert(frameSize < -32768);

        offset = -frameSize - (genFuncletInfo.fiSP_to_PSP_slot_delta + 8);
        int SP_delta = roundUp((UINT)offset, STACK_ALIGN);
        offset = SP_delta - offset;

        genStackPointerAdjustment(-SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);

        genSaveCalleeSavedRegistersHelp(maskSaveRegsInt | maskSaveRegsFloat, offset, 0);

        genStackPointerAdjustment(frameSize + SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else
    {
        unreached();
    }

    // This is the end of the OS-reported prolog for purposes of unwinding
    compiler->unwindEndProlog();

    // If there is no PSPSym (CoreRT ABI), we are done. Otherwise, we need to set up the PSPSym in the functlet frame.
    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        if (isFilter)
        {
            // This is the first block of a filter
            // Note that register a1 = CallerSP of the containing function
            // A1 is overwritten by the first Load (new callerSP)
            // A2 is scratch when we have a large constant offset

            // Load the CallerSP of the main function (stored in the PSP of the dynamically containing funclet or
            // function)
            genInstrWithConstant(INS_ld, EA_PTRSIZE, REG_A1, REG_A1, genFuncletInfo.fiCallerSP_to_PSP_slot_delta,
                                 REG_A2, false);
            regSet.verifyRegUsed(REG_A1);

            // Store the PSP value (aka CallerSP)
            genInstrWithConstant(INS_sd, EA_PTRSIZE, REG_A1, REG_SPBASE, genFuncletInfo.fiSP_to_PSP_slot_delta, REG_A2,
                                 false);

            // re-establish the frame pointer
            genInstrWithConstant(INS_daddiu, EA_PTRSIZE, REG_FPBASE, REG_A1,
                                 genFuncletInfo.fiFunction_CallerSP_to_FP_delta, REG_A2, false);
        }
        else // This is a non-filter funclet
        {
            // A3 is scratch, A2 can also become scratch.

            // compute the CallerSP, given the frame pointer. a3 is scratch?
            genInstrWithConstant(INS_daddiu, EA_PTRSIZE, REG_A3, REG_FPBASE,
                                 -genFuncletInfo.fiFunction_CallerSP_to_FP_delta, REG_A2, false);
            regSet.verifyRegUsed(REG_A3);

            genInstrWithConstant(INS_sd, EA_PTRSIZE, REG_A3, REG_SPBASE, genFuncletInfo.fiSP_to_PSP_slot_delta, REG_A2,
                                 false);
        }
    }
}

/*****************************************************************************
 *
 *  Generates code for an EH funclet epilog.
 */

void CodeGen::genFuncletEpilog()
{
#ifdef DEBUG
    if (verbose)
        printf("*************** In genFuncletEpilog()\n");
#endif

    ScopedSetVariable<bool> _setGeneratingEpilog(&compiler->compGeneratingEpilog, true);

    bool unwindStarted = false;
    int frameSize  = genFuncletInfo.fiSpDelta1;

    if (!unwindStarted)
    {
        // We can delay this until we know we'll generate an unwindable instruction, if necessary.
        compiler->unwindBegEpilog();
        unwindStarted = true;
    }

    regMaskTP maskRestoreRegsFloat = genFuncletInfo.fiSaveRegs & RBM_ALLFLOAT;
    regMaskTP maskRestoreRegsInt   = genFuncletInfo.fiSaveRegs & ~maskRestoreRegsFloat;

    // Funclets must always save RA and FP, since when we have funclets we must have an FP frame.
    assert((maskRestoreRegsInt & RBM_RA) != 0);
    assert((maskRestoreRegsInt & RBM_FP) != 0);

#ifdef DEBUG
    if (compiler->opts.disAsm)
        printf("DEBUG: CodeGen::genFuncletEpilog, frameType:%d\n\n", genFuncletInfo.fiFrameType);
#endif

    regMaskTP regsToRestoreMask = maskRestoreRegsInt | maskRestoreRegsFloat;

    assert(frameSize < 0);
    if (genFuncletInfo.fiFrameType == 1)
    {
        // fiFrameType constraints:
        assert(frameSize >= -32768);
        assert(genFuncletInfo.fiSP_to_FPRA_save_delta < 32760);

        regsToRestoreMask &= ~(RBM_RA | RBM_FP); // We restore FP/RA at the end

        genRestoreCalleeSavedRegistersHelp(regsToRestoreMask, genFuncletInfo.fiSP_to_PSP_slot_delta + 8, 0);

        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_RA, REG_SPBASE, genFuncletInfo.fiSP_to_FPRA_save_delta + 8);
        compiler->unwindSaveReg(REG_RA, genFuncletInfo.fiSP_to_FPRA_save_delta + 8);

        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, genFuncletInfo.fiSP_to_FPRA_save_delta);
        compiler->unwindSaveReg(REG_FP, genFuncletInfo.fiSP_to_FPRA_save_delta);

        // generate daddiu SP,SP,imm
        genStackPointerAdjustment(-frameSize, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else if (genFuncletInfo.fiFrameType == 2)
    {
        // fiFrameType constraints:
        assert(frameSize >= -32768);
        assert(genFuncletInfo.fiSP_to_FPRA_save_delta < 32760);

        genRestoreCalleeSavedRegistersHelp(regsToRestoreMask, genFuncletInfo.fiSP_to_PSP_slot_delta + 8, 0);

        // generate daddiu SP,SP,imm
        genStackPointerAdjustment(-frameSize, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else if (genFuncletInfo.fiFrameType == 3)
    {
        // fiFrameType constraints:
        assert(frameSize < -32768);


        int offset = -frameSize - genFuncletInfo.fiSP_to_FPRA_save_delta;
        int SP_delta = roundUp((UINT)offset, STACK_ALIGN);
        offset = SP_delta - offset;

        //first, generate daddiu SP,SP,imm
        genStackPointerAdjustment(-frameSize - SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);

        int offset2 = frameSize + SP_delta + genFuncletInfo.fiSP_to_PSP_slot_delta + 8;
        assert(offset2 < 32760);//can amend.

        regsToRestoreMask &= ~(RBM_RA | RBM_FP); // We restore FP/RA at the end
        genRestoreCalleeSavedRegistersHelp(regsToRestoreMask, offset2, 0);

        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_RA, REG_SPBASE, offset + 8);
        compiler->unwindSaveReg(REG_RA, offset + 8);

        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, REG_FP, REG_SPBASE, offset);
        compiler->unwindSaveReg(REG_FP, offset);

        //second, generate daddiu SP,SP,imm for remaine space.
        genStackPointerAdjustment(SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else if (genFuncletInfo.fiFrameType == 4)
    {
        // fiFrameType constraints:
        assert(frameSize < -32768);

        int offset = -frameSize - (genFuncletInfo.fiSP_to_PSP_slot_delta + 8);
        int SP_delta = roundUp((UINT)offset, STACK_ALIGN);
        offset = SP_delta - offset;

        genStackPointerAdjustment(-frameSize - SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);

        genRestoreCalleeSavedRegistersHelp(regsToRestoreMask, offset, 0);

        genStackPointerAdjustment(SP_delta, REG_AT, nullptr, /* reportUnwindData */ true);
    }
    else
    {
        unreached();
    }

    inst_RV(INS_jr, REG_RA, TYP_I_IMPL);
    instGen(INS_nop);
    compiler->unwindReturn(REG_RA);

    compiler->unwindEndEpilog();
}

/*****************************************************************************
 *
 *  Capture the information used to generate the funclet prologs and epilogs.
 *  Note that all funclet prologs are identical, and all funclet epilogs are
 *  identical (per type: filters are identical, and non-filters are identical).
 *  Thus, we compute the data used for these just once.
 *
 *  See genFuncletProlog() for more information about the prolog/epilog sequences.
 */

void CodeGen::genCaptureFuncletPrologEpilogInfo()
{
    if (!compiler->ehAnyFunclets())
        return;

    assert(isFramePointerUsed());

    // The frame size and offsets must be finalized
    assert(compiler->lvaDoneFrameLayout == Compiler::FINAL_FRAME_LAYOUT);

    genFuncletInfo.fiFunction_CallerSP_to_FP_delta = genCallerSPtoFPdelta();

    regMaskTP rsMaskSaveRegs = regSet.rsMaskCalleeSaved;
    assert((rsMaskSaveRegs & RBM_RA) != 0);
    assert((rsMaskSaveRegs & RBM_FP) != 0);

    unsigned PSPSize = (compiler->lvaPSPSym != BAD_VAR_NUM) ? 8 : 0;

    unsigned saveRegsCount = genCountBits(rsMaskSaveRegs);
    assert(saveRegsCount == compiler->compCalleeRegsPushed);

    unsigned saveRegsPlusPSPSize;
    if (!IsSaveFpRaWithAllCalleeSavedRegisters())
        saveRegsPlusPSPSize = roundUp((UINT)genTotalFrameSize(), STACK_ALIGN) - compiler->compLclFrameSize +PSPSize/* -2*8*/;
    else
        saveRegsPlusPSPSize = roundUp((UINT)genTotalFrameSize(), STACK_ALIGN) - compiler->compLclFrameSize +PSPSize;

    if (compiler->info.compIsVarArgs)
    {
        // For varargs we always save all of the integer register arguments
        // so that they are contiguous with the incoming stack arguments.
        saveRegsPlusPSPSize += MAX_REG_ARG * REGSIZE_BYTES;
    }
    unsigned saveRegsPlusPSPSizeAligned = roundUp(saveRegsPlusPSPSize, STACK_ALIGN);

    assert(compiler->lvaOutgoingArgSpaceSize % REGSIZE_BYTES == 0);
    unsigned outgoingArgSpaceAligned = roundUp(compiler->lvaOutgoingArgSpaceSize, STACK_ALIGN);

    unsigned maxFuncletFrameSizeAligned = saveRegsPlusPSPSizeAligned + outgoingArgSpaceAligned;
    assert((maxFuncletFrameSizeAligned % STACK_ALIGN) == 0);

    int SP_to_FPRA_save_delta = compiler->lvaOutgoingArgSpaceSize;

    unsigned funcletFrameSize        = saveRegsPlusPSPSize + compiler->lvaOutgoingArgSpaceSize;
    unsigned funcletFrameSizeAligned = roundUp(funcletFrameSize, STACK_ALIGN);
    assert(funcletFrameSizeAligned <= maxFuncletFrameSizeAligned);

    unsigned funcletFrameAlignmentPad = funcletFrameSizeAligned - funcletFrameSize;
    assert((funcletFrameAlignmentPad == 0) || (funcletFrameAlignmentPad == REGSIZE_BYTES));

    if (maxFuncletFrameSizeAligned <= (32768-8))
    {
        if (!IsSaveFpRaWithAllCalleeSavedRegisters())
        {
            genFuncletInfo.fiFrameType = 1;
            saveRegsPlusPSPSize -= 2*8;// FP/RA
        }
        else
        {
            genFuncletInfo.fiFrameType = 2;
            SP_to_FPRA_save_delta += (saveRegsCount-2) * REGSIZE_BYTES + PSPSize;
        }
    }
    else
    {
        unsigned saveRegsPlusPSPAlignmentPad = saveRegsPlusPSPSizeAligned - saveRegsPlusPSPSize;
        assert((saveRegsPlusPSPAlignmentPad == 0) || (saveRegsPlusPSPAlignmentPad == REGSIZE_BYTES));

        if (!IsSaveFpRaWithAllCalleeSavedRegisters())
        {
            genFuncletInfo.fiFrameType = 3;
            saveRegsPlusPSPSize -= 2*8;// FP/RA
        }
        else
        {
            genFuncletInfo.fiFrameType = 4;
            SP_to_FPRA_save_delta += (saveRegsCount-2) * REGSIZE_BYTES + PSPSize;
        }
    }


    int CallerSP_to_PSP_slot_delta = -(int)saveRegsPlusPSPSize;
    genFuncletInfo.fiSpDelta1 = -(int)funcletFrameSizeAligned;
    int SP_to_PSP_slot_delta = funcletFrameSizeAligned - saveRegsPlusPSPSize;

    /* Now save it for future use */
    genFuncletInfo.fiSaveRegs                   = rsMaskSaveRegs;
    genFuncletInfo.fiSP_to_FPRA_save_delta      = SP_to_FPRA_save_delta;

    genFuncletInfo.fiSP_to_PSP_slot_delta       = SP_to_PSP_slot_delta;
    genFuncletInfo.fiCallerSP_to_PSP_slot_delta = CallerSP_to_PSP_slot_delta;

#ifdef DEBUG
    if (verbose)
    {
        printf("\n");
        printf("Funclet prolog / epilog info\n");
        printf("                        Save regs: ");
        dspRegMask(genFuncletInfo.fiSaveRegs);
        printf("\n");
        printf("    Function CallerSP-to-FP delta: %d\n", genFuncletInfo.fiFunction_CallerSP_to_FP_delta);
        printf("  SP to FP/RA save location delta: %d\n", genFuncletInfo.fiSP_to_FPRA_save_delta);
        printf("                       Frame type: %d\n", genFuncletInfo.fiFrameType);
        printf("                       SP delta 1: %d\n", genFuncletInfo.fiSpDelta1);

        if (compiler->lvaPSPSym != BAD_VAR_NUM)
        {
            if (CallerSP_to_PSP_slot_delta !=
                compiler->lvaGetCallerSPRelativeOffset(compiler->lvaPSPSym)) // for debugging
            {
                printf("lvaGetCallerSPRelativeOffset(lvaPSPSym): %d\n",
                       compiler->lvaGetCallerSPRelativeOffset(compiler->lvaPSPSym));
            }
        }
    }

    assert(genFuncletInfo.fiSP_to_FPRA_save_delta >= 0);
#endif // DEBUG
}

/*
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                           End Prolog / Epilog                             XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

BasicBlock* CodeGen::genCallFinally(BasicBlock* block)
{
    // Generate a call to the finally, like this:
    //      mov  a0,qword ptr [fp + 10H] / sp    // Load a0 with PSPSym, or sp if PSPSym is not used
    //      bal  finally-funclet
    //      b    finally-return                  // Only for non-retless finally calls
    // The 'b' can be a NOP if we're going to the next block.

    if (compiler->lvaPSPSym != BAD_VAR_NUM)
    {
        getEmitter()->emitIns_R_S(INS_ld, EA_PTRSIZE, REG_A0, compiler->lvaPSPSym, 0);
    }
    else
    {
        getEmitter()->emitIns_R_R(INS_mov, EA_PTRSIZE, REG_A0, REG_SPBASE);
    }
    getEmitter()->emitIns_J(INS_bal, block->bbJumpDest);

    if (block->bbFlags & BBF_RETLESS_CALL)
    {
        // We have a retless call, and the last instruction generated was a call.
        // If the next block is in a different EH region (or is the end of the code
        // block), then we need to generate a breakpoint here (since it will never
        // get executed) to get proper unwind behavior.

        if ((block->bbNext == nullptr) || !BasicBlock::sameEHRegion(block, block->bbNext))
        {
            instGen(INS_break); // This should never get executed
        }
    }
    else
    {
        // Because of the way the flowgraph is connected, the liveness info for this one instruction
        // after the call is not (can not be) correct in cases where a variable has a last use in the
        // handler.  So turn off GC reporting for this single instruction.
        getEmitter()->emitDisableGC();

        // Now go to where the finally funclet needs to return to.
        if (block->bbNext->bbJumpDest == block->bbNext->bbNext)
        {
            // Fall-through.
            // TODO-MIPS64-CQ: Can we get rid of this instruction, and just have the call return directly
            // to the next instruction? This would depend on stack walking from within the finally
            // handler working without this instruction being in this special EH region.
            instGen(INS_nop);
        }
        else
        {
            inst_JMP(EJ_jmp, block->bbNext->bbJumpDest);//no branch-delay.
        }

        getEmitter()->emitEnableGC();
    }

    // The BBJ_ALWAYS is used because the BBJ_CALLFINALLY can't point to the
    // jump target using bbJumpDest - that is already used to point
    // to the finally block. So just skip past the BBJ_ALWAYS unless the
    // block is RETLESS.
    if (!(block->bbFlags & BBF_RETLESS_CALL))
    {
        assert(block->isBBCallAlwaysPair());
        block = block->bbNext;
    }
    return block;
}

void CodeGen::genEHCatchRet(BasicBlock* block)
{
    getEmitter()->emitIns_R_L(INS_lea, EA_PTRSIZE, block->bbJumpDest, REG_INTRET);
}

//  move an immediate value into an integer register

void CodeGen::instGen_Set_Reg_To_Imm(emitAttr size, regNumber reg, ssize_t imm, insFlags flags)
{
    emitter* emit = getEmitter();

    if (!compiler->opts.compReloc)
    {
        size = EA_SIZE(size); // Strip any Reloc flags from size if we aren't doing relocs.
    }

    if (EA_IS_RELOC(size))
    {
        assert(genIsValidIntReg(reg));
        emit->emitIns_R_AI(INS_bal, size, reg, imm);//for example: EA_PTR_DSP_RELOC
    }
    else
    {
        set_Reg_To_Imm(emit, size, reg, imm);
    }

    regSet.verifyRegUsed(reg);
}

/***********************************************************************************
 *
 * Generate code to set a register 'targetReg' of type 'targetType' to the constant
 * specified by the constant (GT_CNS_INT or GT_CNS_DBL) in 'tree'. This does not call
 * genProduceReg() on the target register.
 */
void CodeGen::genSetRegToConst(regNumber targetReg, var_types targetType, GenTree* tree)
{
    switch (tree->gtOper)
    {
        case GT_CNS_INT:
        {
            // relocatable values tend to come down as a CNS_INT of native int type
            // so the line between these two opcodes is kind of blurry
            GenTreeIntConCommon* con    = tree->AsIntConCommon();
            ssize_t              cnsVal = con->IconValue();

            if (con->ImmedValNeedsReloc(compiler))
            {
                instGen_Set_Reg_To_Imm(EA_HANDLE_CNS_RELOC, targetReg, cnsVal);
                regSet.verifyRegUsed(targetReg);
            }
            else
            {
                genSetRegToIcon(targetReg, cnsVal, targetType);
            }
        }
        break;

        case GT_CNS_DBL:
        {
            emitter* emit       = getEmitter();
            emitAttr size       = emitActualTypeSize(tree);
            double   constValue = tree->AsDblCon()->gtDconVal;

            // Make sure we use "daddiu reg, zero, 0x00"  only for positive zero (0.0)
            // and not for negative zero (-0.0)
            if (*(__int64*)&constValue == 0)
            {
                // A faster/smaller way to generate 0.0
                // We will just zero out the entire vector register for both float and double
                emit->emitIns_R_R(INS_dmtc1, EA_8BYTE, REG_R0, targetReg);
            }
            /*else if (emitter::emitIns_valid_imm_for_fmov(constValue))
            {// MIPS64 doesn't need this.
                assert(!"unimplemented on MIPS yet");
            }*/
            else
            {
                // Get a temp integer register to compute long address.
                regNumber addrReg = tree->GetSingleTempReg();

                // We must load the FP constant from the constant pool
                // Emit a data section constant for the float or double constant.
                CORINFO_FIELD_HANDLE hnd = emit->emitFltOrDblConst(constValue, size);

                // Compute the address of the FP constant.
                emit->emitIns_R_C(INS_bal, size, addrReg, REG_NA, hnd, 0);

                instruction ins;
                // Load the FP constant.
                if (targetReg > REG_RA)
                {
                    if (size == EA_4BYTE)
                    {
                        ins = INS_lwc1;
                    }
                    else
                    {
                        ins = INS_ldc1;
                    }
                }
                else
                {
                    if (size == EA_4BYTE)
                    {
                        ins = INS_lw;
                    }
                    else
                    {
                        ins = INS_ld;
                    }
                }
                emit->emitIns_R_R_I(ins, size, targetReg, addrReg, 0);
            }
        }
        break;

        default:
            unreached();
    }
}

// Generate code to get the high N bits of a N*N=2N bit multiplication result
void CodeGen::genCodeForMulHi(GenTreeOp* treeNode)
{
    assert(!treeNode->gtOverflowEx());

    genConsumeOperands(treeNode);

    regNumber targetReg  = treeNode->gtRegNum;
    var_types targetType = treeNode->TypeGet();
    emitter*  emit       = getEmitter();
    emitAttr  attr       = emitActualTypeSize(treeNode);
    unsigned  isUnsigned = (treeNode->gtFlags & GTF_UNSIGNED);

    GenTree* op1 = treeNode->gtGetOp1();
    GenTree* op2 = treeNode->gtGetOp2();

    assert(!varTypeIsFloating(targetType));

    // op1 and op2 can only be a reg at present, will amend in the future.
    assert(!op1->isContained());
    assert(!op2->isContained());

    // The arithmetic node must be sitting in a register (since it's not contained)
    assert(targetReg != REG_NA);

    if (EA_SIZE(attr) == EA_8BYTE)
    {
        instruction ins = isUnsigned ? INS_dmultu : INS_dmult;

        emit->emitIns_R_R(ins, attr, op1->gtRegNum, op2->gtRegNum);

        emit->emitIns_R(INS_mfhi, attr, targetReg);
    }
    else
    {
        assert(EA_SIZE(attr) == EA_4BYTE);
        regNumber tmpRegOp1 = treeNode->ExtractTempReg();
        regNumber tmpRegOp2 = treeNode->ExtractTempReg();
        emit->emitIns_R_R_I(INS_sll, attr, tmpRegOp1, op1->gtRegNum, 0);
        emit->emitIns_R_R_I(INS_sll, attr, tmpRegOp2, op2->gtRegNum, 0);

        instruction ins = isUnsigned ? INS_multu : INS_mult;

        emit->emitIns_R_R(ins, attr, tmpRegOp1, tmpRegOp2);

        emit->emitIns_R(INS_mfhi, attr, targetReg);
    }

    genProduceReg(treeNode);
}

// Generate code for ADD, SUB, MUL, DIV, UDIV, AND, OR and XOR
// This method is expected to have called genConsumeOperands() before calling it.
void CodeGen::genCodeForBinary(GenTreeOp* treeNode)
{
    const genTreeOps oper       = treeNode->OperGet();
    regNumber        targetReg  = treeNode->gtRegNum;
    emitter*         emit       = getEmitter();

    assert(oper == GT_ADD || oper == GT_SUB || oper == GT_MUL || oper == GT_DIV || oper == GT_UDIV || oper == GT_AND ||
           oper == GT_OR || oper == GT_XOR || oper == GT_MOD || oper == GT_UMOD);

    GenTree*    op1 = treeNode->gtGetOp1();
    GenTree*    op2 = treeNode->gtGetOp2();
    instruction ins = genGetInsForOper(treeNode);

    // The arithmetic node must be sitting in a register (since it's not contained)
    assert(targetReg != REG_NA);

    regNumber r = emit->emitInsTernary(ins, emitActualTypeSize(treeNode), treeNode, op1, op2);
    assert(r == targetReg);

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCodeForLclVar: Produce code for a GT_LCL_VAR node.
//
// Arguments:
//    tree - the GT_LCL_VAR node
//
void CodeGen::genCodeForLclVar(GenTreeLclVar* tree)
{
    var_types targetType = tree->TypeGet();
    emitter*  emit       = getEmitter();

    unsigned varNum = tree->gtLclNum;
    assert(varNum < compiler->lvaCount);
    LclVarDsc* varDsc         = &(compiler->lvaTable[varNum]);
    bool       isRegCandidate = varDsc->lvIsRegCandidate();

    // lcl_vars are not defs
    assert((tree->gtFlags & GTF_VAR_DEF) == 0);

    // If this is a register candidate that has been spilled, genConsumeReg() will
    // reload it at the point of use.  Otherwise, if it's not in a register, we load it here.

    if (!isRegCandidate && !(tree->gtFlags & GTF_SPILLED))
    {
        // targetType must be a normal scalar type and not a TYP_STRUCT
        assert(targetType != TYP_STRUCT);

        instruction ins  = ins_Load(targetType);
        emitAttr    attr = emitTypeSize(targetType);

        emit->emitIns_R_S(ins, attr, tree->gtRegNum, varNum, 0);
        genProduceReg(tree);
    }
}

//------------------------------------------------------------------------
// genCodeForStoreLclFld: Produce code for a GT_STORE_LCL_FLD node.
//
// Arguments:
//    tree - the GT_STORE_LCL_FLD node
//
void CodeGen::genCodeForStoreLclFld(GenTreeLclFld* tree)
{
    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;
    emitter*  emit       = getEmitter();
    noway_assert(targetType != TYP_STRUCT);

#ifdef FEATURE_SIMD
    // storing of TYP_SIMD12 (i.e. Vector3) field
    if (tree->TypeGet() == TYP_SIMD12)
    {
        genStoreLclTypeSIMD12(tree);
        return;
    }
#endif // FEATURE_SIMD

    // record the offset
    unsigned offset = tree->gtLclOffs;

    // We must have a stack store with GT_STORE_LCL_FLD
    noway_assert(targetReg == REG_NA);

    unsigned varNum = tree->gtLclNum;
    assert(varNum < compiler->lvaCount);
    LclVarDsc* varDsc = &(compiler->lvaTable[varNum]);

    // Ensure that lclVar nodes are typed correctly.
    assert(!varDsc->lvNormalizeOnStore() || targetType == genActualType(varDsc->TypeGet()));

    GenTree* data = tree->gtOp1;
    genConsumeRegs(data);

    regNumber dataReg = REG_NA;
    if (data->isContainedIntOrIImmed())
    {
        assert(data->IsIntegralConst(0));
        dataReg = REG_R0;
    }
    else
    {
        assert(!data->isContained());
        dataReg = data->gtRegNum;
    }
    assert(dataReg != REG_NA);

    instruction ins = ins_Store(targetType);

    emitAttr attr = emitTypeSize(targetType);

    emit->emitIns_S_R(ins, attr, dataReg, varNum, offset);

    genUpdateLife(tree);

    varDsc->lvRegNum = REG_STK;
}

//------------------------------------------------------------------------
// genCodeForStoreLclVar: Produce code for a GT_STORE_LCL_VAR node.
//
// Arguments:
//    tree - the GT_STORE_LCL_VAR node
//
void CodeGen::genCodeForStoreLclVar(GenTreeLclVar* tree)
{
    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;
    emitter*  emit       = getEmitter();

    unsigned varNum = tree->gtLclNum;
    assert(varNum < compiler->lvaCount);
    LclVarDsc* varDsc = &(compiler->lvaTable[varNum]);

    // Ensure that lclVar nodes are typed correctly.
    assert(!varDsc->lvNormalizeOnStore() || targetType == genActualType(varDsc->TypeGet()));

    GenTree* data = tree->gtOp1;

    // var = call, where call returns a multi-reg return value
    // case is handled separately.
    if (data->gtSkipReloadOrCopy()->IsMultiRegCall())
    {
        genMultiRegCallStoreToLocal(tree);
    }
    else
    {
#ifdef FEATURE_SIMD
        // storing of TYP_SIMD12 (i.e. Vector3) field
        if (tree->TypeGet() == TYP_SIMD12)
        {
            genStoreLclTypeSIMD12(tree);
            return;
        }
#endif // FEATURE_SIMD

        genConsumeRegs(data);

        regNumber dataReg = REG_NA;
        if (data->isContainedIntOrIImmed())
        {
            // This is only possible for a zero-init.
            assert(data->IsIntegralConst(0));

            if (varTypeIsSIMD(targetType))
            {
                /* FIXME for MIPS */
                assert(targetReg != REG_NA);
                getEmitter()->emitIns_R_R(INS_dmtc1, EA_16BYTE, REG_R0, targetReg);
                genProduceReg(tree);
                return;
            }

            dataReg = REG_R0;
        }
        else
        {
            assert(!data->isContained());
            dataReg = data->gtRegNum;
        }
        assert(dataReg != REG_NA);

        if (targetReg == REG_NA) // store into stack based LclVar
        {
            inst_set_SV_var(tree);

            instruction ins  = ins_Store(targetType);
            emitAttr    attr = emitTypeSize(targetType);

            emit->emitIns_S_R(ins, attr, dataReg, varNum, /* offset */ 0);

            genUpdateLife(tree);

            varDsc->lvRegNum = REG_STK;
        }
        else // store into register (i.e move into register)
        {
            if (dataReg != targetReg)
            {
                // Assign into targetReg when dataReg (from op1) is not the same register
                inst_RV_RV(ins_Copy(targetType), targetReg, dataReg, targetType);
            }
            genProduceReg(tree);
        }
    }
}

//------------------------------------------------------------------------
// genSimpleReturn: Generates code for simple return statement for mips64.
//
// Note: treeNode's and op1's registers are already consumed.
//
// Arguments:
//    treeNode - The GT_RETURN or GT_RETFILT tree node with non-struct and non-void type
//
// Return Value:
//    None
//
void CodeGen::genSimpleReturn(GenTree* treeNode)
{
    assert(treeNode->OperGet() == GT_RETURN || treeNode->OperGet() == GT_RETFILT);
    GenTree*  op1        = treeNode->gtGetOp1();
    var_types targetType = treeNode->TypeGet();

    assert(targetType != TYP_STRUCT);
    assert(targetType != TYP_VOID);

    regNumber retReg = varTypeUsesFloatArgReg(treeNode) ? REG_FLOATRET : REG_INTRET;

    bool movRequired = (op1->gtRegNum != retReg);

    if (!movRequired)
    {
        if (op1->OperGet() == GT_LCL_VAR)
        {
            GenTreeLclVarCommon* lcl            = op1->AsLclVarCommon();
            bool                 isRegCandidate = compiler->lvaTable[lcl->gtLclNum].lvIsRegCandidate();
            if (isRegCandidate && ((op1->gtFlags & GTF_SPILLED) == 0))
            {
                // We may need to generate a zero-extending mov instruction to load the value from this GT_LCL_VAR

                unsigned   lclNum  = lcl->gtLclNum;
                LclVarDsc* varDsc  = &(compiler->lvaTable[lclNum]);
                var_types  op1Type = genActualType(op1->TypeGet());
                var_types  lclType = genActualType(varDsc->TypeGet());

                if (genTypeSize(op1Type) < genTypeSize(lclType))
                {
                    movRequired = true;
                }
            }
        }
    }
    if (movRequired)
    {
        emitAttr attr = emitActualTypeSize(targetType);
        if (varTypeUsesFloatArgReg(treeNode))
        {
            if (attr == EA_4BYTE)
                getEmitter()->emitIns_R_R(INS_mov_s, attr, retReg, op1->gtRegNum);
            else
                getEmitter()->emitIns_R_R(INS_mov_d, attr, retReg, op1->gtRegNum);
        }
        else
        {
            getEmitter()->emitIns_R_R(INS_mov, attr, retReg, op1->gtRegNum);
        }
    }
}

/***********************************************************************************************
 *  Generate code for localloc
 */
void CodeGen::genLclHeap(GenTree* tree)
{
    assert(tree->OperGet() == GT_LCLHEAP);
    assert(compiler->compLocallocUsed);

    emitter* emit = getEmitter();
    GenTree* size = tree->gtOp.gtOp1;
    noway_assert((genActualType(size->gtType) == TYP_INT) || (genActualType(size->gtType) == TYP_I_IMPL));

    regNumber            targetReg                = tree->gtRegNum;
    regNumber            regCnt                   = REG_NA;
    regNumber            pspSymReg                = REG_NA;
    var_types            type                     = genActualType(size->gtType);
    emitAttr             easz                     = emitTypeSize(type);
    BasicBlock*          endLabel                 = nullptr;//can optimize for mips.
    unsigned             stackAdjustment          = 0;
    const target_ssize_t ILLEGAL_LAST_TOUCH_DELTA = (target_ssize_t)-1;
    target_ssize_t       lastTouchDelta =
        ILLEGAL_LAST_TOUCH_DELTA; // The number of bytes from SP to the last stack address probed.

    noway_assert(isFramePointerUsed()); // localloc requires Frame Pointer to be established since SP changes
    noway_assert(genStackLevel == 0);   // Can't have anything on the stack

    // compute the amount of memory to allocate to properly STACK_ALIGN.
    size_t amount = 0;
    if (size->IsCnsIntOrI())
    {
        // If size is a constant, then it must be contained.
        assert(size->isContained());

        // If amount is zero then return null in targetReg
        amount = size->gtIntCon.gtIconVal;
        if (amount == 0)
        {
            instGen_Set_Reg_To_Zero(EA_PTRSIZE, targetReg);
            goto BAILOUT;
        }

        // 'amount' is the total number of bytes to localloc to properly STACK_ALIGN
        amount = AlignUp(amount, STACK_ALIGN);
    }
    else
    {
        // If 0 bail out by returning null in targetReg
        genConsumeRegAndCopy(size, targetReg);
        endLabel = genCreateTempLabel();

        ssize_t imm = 7 << 2;
        emit->emitIns_R_R_I(INS_bne, easz, targetReg, REG_R0, imm);
        instGen(INS_nop);
        inst_JMP(EJ_jmp, endLabel);//no branch-delay.

        // Compute the size of the block to allocate and perform alignment.
        // If compInitMem=true, we can reuse targetReg as regcnt,
        // since we don't need any internal registers.
        if (compiler->info.compInitMem)
        {
            assert(tree->AvailableTempRegCount() == 0);
            regCnt = targetReg;
        }
        else
        {
            regCnt = tree->ExtractTempReg();
            if (regCnt != targetReg)
            {
                emit->emitIns_R_R(INS_mov, easz, regCnt, targetReg);
            }
        }

        // Align to STACK_ALIGN
        // regCnt will be the total number of bytes to localloc
        inst_RV_IV(INS_daddiu, regCnt, (STACK_ALIGN - 1), emitActualTypeSize(type));

        assert(regCnt != REG_AT);
        ssize_t imm2 = ~(STACK_ALIGN - 1);
        emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, REG_AT, REG_R0, imm2);
        emit->emitIns_R_R_R(INS_and, emitActualTypeSize(type), regCnt, regCnt, REG_AT);
    }

    // If we have an outgoing arg area then we must adjust the SP by popping off the
    // outgoing arg area. We will restore it right before we return from this method.
    //
    // Localloc returns stack space that aligned to STACK_ALIGN bytes. The following
    // are the cases that need to be handled:
    //   i) Method has out-going arg area.
    //      It is guaranteed that size of out-going arg area is STACK_ALIGN'ed (see fgMorphArgs).
    //      Therefore, we will pop off the out-going arg area from the stack pointer before allocating the localloc
    //      space.
    //  ii) Method has no out-going arg area.
    //      Nothing to pop off from the stack.
    if (compiler->lvaOutgoingArgSpaceSize > 0)
    {
        unsigned outgoingArgSpaceAligned = roundUp(compiler->lvaOutgoingArgSpaceSize, STACK_ALIGN);
        //assert((compiler->lvaOutgoingArgSpaceSize % STACK_ALIGN) == 0); // This must be true for the stack to remain
        //                                                                // aligned
        genInstrWithConstant(INS_daddiu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, outgoingArgSpaceAligned,
                             rsGetRsvdReg());
        stackAdjustment += outgoingArgSpaceAligned;
    }

    if (size->IsCnsIntOrI())
    {
        // We should reach here only for non-zero, constant size allocations.
        assert(amount > 0);
        ssize_t imm = -16;

        // For small allocations we will generate up to four stp instructions, to zero 16 to 64 bytes.
        static_assert_no_msg(STACK_ALIGN == (REGSIZE_BYTES * 2));
        assert(amount % (REGSIZE_BYTES * 2) == 0); // stp stores two registers at a time
        size_t stpCount = amount / (REGSIZE_BYTES * 2);
        if (stpCount <= 4)
        {
            imm = -16 * stpCount;
            emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, imm);

            imm = -imm;
            while (stpCount != 0)
            {
                imm -= 8;
                emit->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_R0, REG_SPBASE, imm);
                imm -= 8;
                emit->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_R0, REG_SPBASE, imm);
                stpCount -= 1;
            }

            lastTouchDelta = 0;

            goto ALLOC_DONE;
        }
        else if (!compiler->info.compInitMem && (amount < compiler->eeGetPageSize())) // must be < not <=
        {
            // Since the size is less than a page, simply adjust the SP value.
            // The SP might already be in the guard page, so we must touch it BEFORE
            // the alloc, not after.

            // lw k0, 0(SP)
            emit->emitIns_R_R_I(INS_lw, EA_4BYTE, REG_K0, REG_SP, 0);

            lastTouchDelta = amount;
            imm = -(ssize_t)amount;
            assert(-32768 <= imm && imm < 0);
            emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, imm);

            goto ALLOC_DONE;
        }

        // else, "mov regCnt, amount"
        // If compInitMem=true, we can reuse targetReg as regcnt.
        // Since size is a constant, regCnt is not yet initialized.
        assert(regCnt == REG_NA);
        if (compiler->info.compInitMem)
        {
            assert(tree->AvailableTempRegCount() == 0);
            regCnt = targetReg;
        }
        else
        {
            regCnt = tree->ExtractTempReg();
        }
        genSetRegToIcon(regCnt, amount, ((unsigned int)amount == amount) ? TYP_INT : TYP_LONG);
    }

    if (compiler->info.compInitMem)
    {
        // At this point 'regCnt' is set to the total number of bytes to locAlloc.
        // Since we have to zero out the allocated memory AND ensure that the stack pointer is always valid
        // by tickling the pages, we will just push 0's on the stack.
        //
        // Note: regCnt is guaranteed to be even on Amd64 since STACK_ALIGN/TARGET_POINTER_SIZE = 2
        // and localloc size is a multiple of STACK_ALIGN.

        // Loop:
        ssize_t imm = -16;
        emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, REG_SPBASE, REG_SPBASE, imm);

        emit->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_R0, REG_SPBASE, 8);
        emit->emitIns_R_R_I(INS_sd, EA_PTRSIZE, REG_R0, REG_SPBASE, 0);

        // If not done, loop
        // Note that regCnt is the number of bytes to stack allocate.
        // Therefore we need to subtract 16 from regcnt here.
        assert(genIsValidIntReg(regCnt));

        emit->emitIns_R_R_I(INS_daddiu, emitActualTypeSize(type), regCnt, regCnt, -16);

        imm = -5<<2;//goto loop.
        emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, regCnt, REG_R0, imm);
        instGen(INS_nop);

        lastTouchDelta = 0;
    }
    else
    {
        // At this point 'regCnt' is set to the total number of bytes to localloc.
        //
        // We don't need to zero out the allocated memory. However, we do have
        // to tickle the pages to ensure that SP is always valid and is
        // in sync with the "stack guard page".  Note that in the worst
        // case SP is on the last byte of the guard page.  Thus you must
        // touch SP-0 first not SP-0x1000.
        //
        // This is similar to the prolog code in CodeGen::genAllocLclFrame().
        //
        // Note that we go through a few hoops so that SP never points to
        // illegal pages at any time during the tickling process.
        //
        //       dsubu  regCnt, SP, regCnt      // regCnt now holds ultimate SP
        //       sltu   AT, SP, regCnt
        //       movn   regCnt, R0, AT          // Overflow, pick lowest possible value
        //
        //  Loop:
        //       lw     r0, 0(SP)               // tickle the page - read from the page
        //       daddiu regTmp, SP, -PAGE_SIZE  // decrement SP by eeGetPageSize()
        //       sltu   AT, regTmp, regCnt
        //       bne    AT, R0,  Done
        //       movz   SP, regTmp, AT
        //       b     Loop
        //
        //  Done:
        //       mov   SP, regCnt
        //

        // Setup the regTmp
        regNumber regTmp = tree->GetSingleTempReg();

        assert(regCnt != REG_AT);
        emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, REG_AT, REG_SPBASE, regCnt);

        //// dsubu  regCnt, SP, regCnt      // regCnt now holds ultimate SP
        emit->emitIns_R_R_R(INS_dsubu, EA_PTRSIZE, regCnt, REG_SPBASE, regCnt);

        // Overflow, set regCnt to lowest possible value
        emit->emitIns_R_R_R(INS_movn, EA_PTRSIZE, regCnt, REG_R0, REG_AT);

        //genDefineTempLabel(loop);

        // tickle the page - Read from the updated SP - this triggers a page fault when on the guard page
        emit->emitIns_R_R_I(INS_lw, EA_4BYTE, REG_K0, REG_SPBASE, 0);

        // decrement SP by eeGetPageSize()
        emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, regTmp, REG_SPBASE, -compiler->eeGetPageSize());

        assert(regTmp != REG_AT);

        emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, REG_AT, regTmp, regCnt);

        ssize_t imm = 3 << 2;//goto done.
        emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);

        emit->emitIns_R_R_R(INS_movz, EA_PTRSIZE, REG_SPBASE, regTmp, REG_AT);

        // Jump to loop and tickle new stack address
        imm = -6 << 2;//goto done.
        emit->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_R0, REG_R0, imm);
        instGen(INS_nop);

        // Done with stack tickle loop
        //genDefineTempLabel(done);

        // Now just move the final value to SP
        emit->emitIns_R_R(INS_mov, EA_PTRSIZE, REG_SPBASE, regCnt);

        // lastTouchDelta is dynamic, and can be up to a page. So if we have outgoing arg space,
        // we're going to assume the worst and probe.
    }

ALLOC_DONE:
    // Re-adjust SP to allocate outgoing arg area. We must probe this adjustment.
    if (stackAdjustment != 0)
    {
        assert((stackAdjustment % STACK_ALIGN) == 0); // This must be true for the stack to remain aligned
        assert((lastTouchDelta == ILLEGAL_LAST_TOUCH_DELTA) || (lastTouchDelta >= 0));

        if ((lastTouchDelta == ILLEGAL_LAST_TOUCH_DELTA) ||
            (stackAdjustment + (unsigned)lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES >
             compiler->eeGetPageSize()))
        {
            genStackPointerConstantAdjustmentLoopWithProbe(-(ssize_t)stackAdjustment, REG_R0);
        }
        else
        {
            genStackPointerConstantAdjustment(-(ssize_t)stackAdjustment);
        }

        // Return the stackalloc'ed address in result register.
        // TargetReg = SP + stackAdjustment.
        //
        genInstrWithConstant(INS_daddiu, EA_PTRSIZE, targetReg, REG_SPBASE, (ssize_t)stackAdjustment, rsGetRsvdReg());
    }
    else // stackAdjustment == 0
    {
        // Move the final value of SP to targetReg
        inst_RV_RV(INS_mov, targetReg, REG_SPBASE);
    }

BAILOUT:
    if (endLabel != nullptr)
        genDefineTempLabel(endLabel);

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForNegNot: Produce code for a GT_NEG/GT_NOT node.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForNegNot(GenTree* tree)
{
    assert(tree->OperIs(GT_NEG, GT_NOT));

    var_types targetType = tree->TypeGet();

    assert(!tree->OperIs(GT_NOT) || !varTypeIsFloating(targetType));

    regNumber   targetReg = tree->gtRegNum;
    instruction ins       = genGetInsForOper(tree);

    // The arithmetic node must be sitting in a register (since it's not contained)
    assert(!tree->isContained());
    // The dst can only be a register.
    assert(targetReg != REG_NA);

    GenTree* operand = tree->gtGetOp1();
    assert(!operand->isContained());
    // The src must be a register.
    regNumber operandReg = genConsumeReg(operand);

    emitAttr attr = emitActualTypeSize(tree);
    getEmitter()->emitIns_R_R(ins, attr, targetReg, operandReg);

    if (ins == INS_not && attr == EA_4BYTE)
    {
        // MIPS needs to sign-extend dst when deal with 32bit data
        getEmitter()->emitIns_R_R_I(INS_sll, attr, targetReg, targetReg, 0);
    }

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForDivMod: Produce code for a GT_DIV/GT_UDIV node. We don't see MOD:
// (1) integer MOD is morphed into a sequence of sub, mul, div in fgMorph;
// (2) float/double MOD is morphed into a helper call by front-end.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForDivMod(GenTreeOp* tree)
{
    assert(tree->OperIs(GT_MOD, GT_UMOD, GT_DIV, GT_UDIV));

    var_types targetType = tree->TypeGet();
    emitter*  emit       = getEmitter();

    genConsumeOperands(tree);

    if (varTypeIsFloating(targetType))
    {
        // Floating point divide never raises an exception
        genCodeForBinary(tree);
    }
    else // an integer divide operation
    {
        GenTree* divisorOp = tree->gtGetOp2();
        emitAttr size      = EA_ATTR(genTypeSize(genActualType(tree->TypeGet())));

        if (divisorOp->IsIntegralConst(0) || divisorOp->gtRegNum == REG_R0)
        {
            // We unconditionally throw a divide by zero exception
            genJumpToThrowHlpBlk(EJ_jmp, SCK_DIV_BY_ZERO);
            //emit->emitIns(INS_nop);

            // We still need to call genProduceReg
            genProduceReg(tree);
        }
        else // the divisor is not the constant zero
        {
            regNumber divisorReg = divisorOp->gtRegNum;

            // Generate the require runtime checks for GT_DIV or GT_UDIV
            if (tree->gtOper == GT_DIV || tree->gtOper == GT_MOD)
            {
                BasicBlock* sdivLabel = genCreateTempLabel();//can optimize for mips64.

                // Two possible exceptions:
                //     (AnyVal /  0) => DivideByZeroException
                //     (MinInt / -1) => ArithmeticException
                //
                bool checkDividend = true;

                // Do we have an immediate for the 'divisorOp'?
                //
                if (divisorOp->IsCnsIntOrI())
                {
                    GenTreeIntConCommon* intConstTree  = divisorOp->AsIntConCommon();
                    ssize_t              intConstValue = intConstTree->IconValue();
                    assert(intConstValue != 0); // already checked above by IsIntegralConst(0)
                    if (intConstValue != -1)
                    {
                        checkDividend = false; // We statically know that the dividend is not -1
                    }
                }
                else // insert check for divison by zero
                {
                    // Check if the divisor is zero throw a DivideByZeroException
                    ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    getEmitter()->emitIns_R_R_I(INS_bne, EA_PTRSIZE, divisorReg, REG_R0, imm);
                    emit->emitIns(INS_nop);

                    genJumpToThrowHlpBlk(EJ_jmp, SCK_DIV_BY_ZERO);//EJ_jmp is 6/8-ins
                    //emit->emitIns(INS_nop);
                }

                if (checkDividend)
                {
                    // Check if the divisor is not -1 branch to 'sdivLabel'
                    emit->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, REG_AT, REG_R0, -1);
                    ssize_t imm = 7 << 2;
                    getEmitter()->emitIns_R_R_I(INS_beq, size, REG_AT, divisorReg, imm);
                    emit->emitIns(INS_nop);

                    inst_JMP(EJ_jmp, sdivLabel);//no branch-delay. 6-ins.
                    // If control flow continues past here the 'divisorReg' is known to be -1

                    regNumber dividendReg = tree->gtGetOp1()->gtRegNum;
                    // At this point the divisor is known to be -1
                    //
                    // Wether dividendReg is MinInt or not
                    //

                    imm = compiler->fgUseThrowHelperBlocks() ? (10<<2) : (12 << 2);//10/12=1+1+1+6/8 +1.
                    getEmitter()->emitIns_R_R_I(INS_beq, size, dividendReg, REG_R0, imm);
                    emit->emitIns(INS_nop);

                    getEmitter()->emitIns_R_R_R(size == EA_4BYTE ? INS_addu : INS_daddu, size, REG_AT, dividendReg, dividendReg);

                    imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    getEmitter()->emitIns_R_R_I(INS_bne, size, REG_AT, REG_R0, imm);
                    emit->emitIns(INS_nop);

                    genJumpToThrowHlpBlk(EJ_jmp, SCK_ARITH_EXCPN); //EJ_jmp is 6/8-ins.
                    //emit->emitIns(INS_nop);

                    genDefineTempLabel(sdivLabel);
                }
                genCodeForBinary(tree); // Generate the sdiv instruction
            }
            else //if (tree->gtOper == GT_UDIV) GT_UMOD
            {
                // Only one possible exception
                //     (AnyVal /  0) => DivideByZeroException
                //
                // Note that division by the constant 0 was already checked for above by the
                // op2->IsIntegralConst(0) check
                //
                if (!divisorOp->IsCnsIntOrI())
                {
                    // divisorOp is not a constant, so it could be zero
                    //
                    ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    getEmitter()->emitIns_R_R_I(INS_bne, size, divisorReg, REG_R0, imm);
                    emit->emitIns(INS_nop);

                    genJumpToThrowHlpBlk(EJ_jmp, SCK_DIV_BY_ZERO); //EJ_jmp is 6/8-ins.
                    //emit->emitIns(INS_nop);
                }
                genCodeForBinary(tree);
            }
        }
    }
}

// Generate code for InitBlk by performing a loop unroll
// Preconditions:
//   a) Both the size and fill byte value are integer constants.
//   b) The size of the struct to initialize is smaller than INITBLK_UNROLL_LIMIT bytes.
void CodeGen::genCodeForInitBlkUnroll(GenTreeBlk* initBlkNode)
{
    // Make sure we got the arguments of the initblk/initobj operation in the right registers
    unsigned size    = initBlkNode->Size();
    GenTree* dstAddr = initBlkNode->Addr();
    GenTree* initVal = initBlkNode->Data();
    if (initVal->OperIsInitVal())
    {
        initVal = initVal->gtGetOp1();
    }

    assert(dstAddr->isUsedFromReg());
    assert((initVal->isUsedFromReg() && !initVal->IsIntegralConst(0)) || initVal->IsIntegralConst(0));
    assert(size != 0);
    assert(size <= INITBLK_UNROLL_LIMIT);

    emitter* emit = getEmitter();

    genConsumeOperands(initBlkNode);

    if (initBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a full memory barrier before a volatile initBlockUnroll operation
        instGen_MemoryBarrier();
    }

    regNumber valReg = initVal->IsIntegralConst(0) ? REG_R0 : initVal->gtRegNum;

    assert(!initVal->IsIntegralConst(0) || (valReg == REG_R0));

    unsigned offset = 0;

    // Perform an unroll using 2 SD.
    if (size >= 2 * REGSIZE_BYTES)
    {
        // Determine how many 16 byte slots
        size_t slots = size / (2 * REGSIZE_BYTES);

        while (slots-- > 0)
        {
            emit->emitIns_R_R_I(INS_sd, EA_8BYTE, valReg, dstAddr->gtRegNum, offset);
            emit->emitIns_R_R_I(INS_sd, EA_8BYTE, valReg, dstAddr->gtRegNum, offset + 8);
            offset += (2 * REGSIZE_BYTES);
        }
    }

    // Fill the remainder (15 bytes or less) if there's any.
    if ((size & 0xf) != 0)
    {
        if ((size & 8) != 0)
        {
            emit->emitIns_R_R_I(INS_sd, EA_8BYTE, valReg, dstAddr->gtRegNum, offset);
            offset += 8;
        }
        if ((size & 4) != 0)
        {
            emit->emitIns_R_R_I(INS_sw, EA_4BYTE, valReg, dstAddr->gtRegNum, offset);
            offset += 4;
        }
        if ((size & 2) != 0)
        {
            emit->emitIns_R_R_I(INS_sh, EA_2BYTE, valReg, dstAddr->gtRegNum, offset);
            offset += 2;
        }
        if ((size & 1) != 0)
        {
            emit->emitIns_R_R_I(INS_sb, EA_1BYTE, valReg, dstAddr->gtRegNum, offset);
        }
    }
}

#if 0
// Generate code for a load pair from some address + offset
//   base: tree node which can be either a local address or arbitrary node
//   offset: distance from the base from which to load
void CodeGen::genCodeForLoadPairOffset(regNumber dst, regNumber dst2, GenTree* base, unsigned offset)
{
    emitter* emit = getEmitter();

    if (base->OperIsLocalAddr())
    {
        if (base->gtOper == GT_LCL_FLD_ADDR)
            offset += base->gtLclFld.gtLclOffs;

        emit->emitIns_R_S(INS_ld, EA_8BYTE, dst, base->gtLclVarCommon.gtLclNum, offset);
        emit->emitIns_R_S(INS_ld, EA_8BYTE, dst2, base->gtLclVarCommon.gtLclNum, offset + 8);
    }
    else
    {
        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, dst, base->gtRegNum, offset);
        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, dst2, base->gtRegNum, offset + 8);
    }
}
#endif

#if 0
// Generate code for a store pair to some address + offset
//   base: tree node which can be either a local address or arbitrary node
//   offset: distance from the base from which to load
void CodeGen::genCodeForStorePairOffset(regNumber src, regNumber src2, GenTree* base, unsigned offset)
{
    emitter* emit = getEmitter();

    if (base->OperIsLocalAddr())
    {
        if (base->gtOper == GT_LCL_FLD_ADDR)
            offset += base->gtLclFld.gtLclOffs;

        emit->emitIns_S_R(INS_sd, EA_8BYTE, src, base->gtLclVarCommon.gtLclNum, offset);
        emit->emitIns_S_R(INS_sd, EA_8BYTE, src2, base->gtLclVarCommon.gtLclNum, offset + 8);
    }
    else
    {
        emit->emitIns_R_R_I(INS_sd, EA_8BYTE, src, base->gtRegNum, offset);
        emit->emitIns_R_R_I(INS_sd, EA_8BYTE, src2, base->gtRegNum, offset + 8);
    }
}
#endif

// Generate code for CpObj nodes wich copy structs that have interleaved
// GC pointers.
// For this case we'll generate a sequence of loads/stores in the case of struct
// slots that don't contain GC pointers.  The generated code will look like:
// ld tempReg, 8(A5)
// sd tempReg, 8(A6)
//
// In the case of a GC-Pointer we'll call the ByRef write barrier helper
// who happens to use the same registers as the previous call to maintain
// the same register requirements and register killsets:
// bal CORINFO_HELP_ASSIGN_BYREF
//
// So finally an example would look like this:
// ld tempReg, 8(A5)
// sd tempReg, 8(A6)
// bal CORINFO_HELP_ASSIGN_BYREF
// ld tempReg, 8(A5)
// sd tempReg, 8(A6)
// bal CORINFO_HELP_ASSIGN_BYREF
// ld tempReg, 8(A5)
// sd tempReg, 8(A6)
void CodeGen::genCodeForCpObj(GenTreeObj* cpObjNode)
{
    GenTree*  dstAddr       = cpObjNode->Addr();
    GenTree*  source        = cpObjNode->Data();
    var_types srcAddrType   = TYP_BYREF;
    bool      sourceIsLocal = false;

    assert(source->isContained());
    if (source->gtOper == GT_IND)
    {
        GenTree* srcAddr = source->gtGetOp1();
        assert(!srcAddr->isContained());
        srcAddrType = srcAddr->TypeGet();
    }
    else
    {
        noway_assert(source->IsLocal());
        sourceIsLocal = true;
    }

    bool dstOnStack = dstAddr->gtSkipReloadOrCopy()->OperIsLocalAddr();

#ifdef DEBUG
    assert(!dstAddr->isContained());

    // This GenTree node has data about GC pointers, this means we're dealing
    // with CpObj.
    assert(cpObjNode->gtGcPtrCount > 0);
#endif // DEBUG

    // Consume the operands and get them into the right registers.
    // They may now contain gc pointers (depending on their type; gcMarkRegPtrVal will "do the right thing").
    genConsumeBlockOp(cpObjNode, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_SRC_BYREF, REG_NA);
    gcInfo.gcMarkRegPtrVal(REG_WRITE_BARRIER_SRC_BYREF, srcAddrType);
    gcInfo.gcMarkRegPtrVal(REG_WRITE_BARRIER_DST_BYREF, dstAddr->TypeGet());

    unsigned slots = cpObjNode->gtSlots;

    // Temp register(s) used to perform the sequence of loads and stores.
    regNumber tmpReg  = cpObjNode->ExtractTempReg();
    regNumber tmpReg2 = REG_NA;

    assert(genIsValidIntReg(tmpReg));
    assert(tmpReg != REG_WRITE_BARRIER_SRC_BYREF);
    assert(tmpReg != REG_WRITE_BARRIER_DST_BYREF);

    if (slots > 1)
    {
        tmpReg2 = cpObjNode->GetSingleTempReg();
        assert(tmpReg2 != tmpReg);
        assert(genIsValidIntReg(tmpReg2));
        assert(tmpReg2 != REG_WRITE_BARRIER_DST_BYREF);
        assert(tmpReg2 != REG_WRITE_BARRIER_SRC_BYREF);
    }

    if (cpObjNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a full memory barrier before a volatile CpObj operation
        instGen_MemoryBarrier();
    }

    emitter* emit = getEmitter();

    BYTE* gcPtrs = cpObjNode->gtGcPtrs;
    emitAttr attrSrcAddr = emitActualTypeSize(srcAddrType);
    emitAttr attrDstAddr = emitActualTypeSize(dstAddr->TypeGet());

    // If we can prove it's on the stack we don't need to use the write barrier.
    if (dstOnStack)
    {
        unsigned i = 0;
        // Check if two or more remaining slots and use two ld/sd sequence
        while (i < slots - 1)
        {
            emitAttr attr0 = emitTypeSize(compiler->getJitGCType(gcPtrs[i + 0]));
            emitAttr attr1 = emitTypeSize(compiler->getJitGCType(gcPtrs[i + 1]));

            emit->emitIns_R_R_I(INS_ld, attr0, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
            emit->emitIns_R_R_I(INS_ld, attr1, tmpReg2, REG_WRITE_BARRIER_SRC_BYREF, TARGET_POINTER_SIZE);
            emit->emitIns_R_R_I(INS_daddiu, attrSrcAddr, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF, 2 * TARGET_POINTER_SIZE);
            emit->emitIns_R_R_I(INS_sd, attr0, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
            emit->emitIns_R_R_I(INS_sd, attr1, tmpReg2, REG_WRITE_BARRIER_DST_BYREF, TARGET_POINTER_SIZE);
            emit->emitIns_R_R_I(INS_daddiu, attrDstAddr, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF, 2 * TARGET_POINTER_SIZE);
            i += 2;
        }

        // Use a ld/sd sequence for the last remainder
        if (i < slots)
        {
            emitAttr attr0 = emitTypeSize(compiler->getJitGCType(gcPtrs[i + 0]));

            emit->emitIns_R_R_I(INS_ld, attr0, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
            emit->emitIns_R_R_I(INS_daddiu, attrSrcAddr, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF, TARGET_POINTER_SIZE);
            emit->emitIns_R_R_I(INS_sd, attr0, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
            emit->emitIns_R_R_I(INS_daddiu, attrDstAddr, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF, TARGET_POINTER_SIZE);
        }
    }
    else
    {
        unsigned gcPtrCount = cpObjNode->gtGcPtrCount;

        unsigned i = 0;
        while (i < slots)
        {
            switch (gcPtrs[i])
            {
                case TYPE_GC_NONE:
                    // Check if the next slot's type is also TYP_GC_NONE and use two ld/sd
                    if ((i + 1 < slots) && (gcPtrs[i + 1] == TYPE_GC_NONE))
                    {
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg2, REG_WRITE_BARRIER_SRC_BYREF, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_daddiu, attrSrcAddr, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF, 2 * TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_sd, EA_8BYTE, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
                        emit->emitIns_R_R_I(INS_sd, EA_8BYTE, tmpReg2, REG_WRITE_BARRIER_DST_BYREF, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_daddiu, attrDstAddr, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF, 2 * TARGET_POINTER_SIZE);
                        ++i; // extra increment of i, since we are copying two items
                    }
                    else
                    {
                        emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg, REG_WRITE_BARRIER_SRC_BYREF, 0);
                        emit->emitIns_R_R_I(INS_daddiu, attrSrcAddr, REG_WRITE_BARRIER_SRC_BYREF, REG_WRITE_BARRIER_SRC_BYREF, TARGET_POINTER_SIZE);
                        emit->emitIns_R_R_I(INS_sd, EA_8BYTE, tmpReg, REG_WRITE_BARRIER_DST_BYREF, 0);
                        emit->emitIns_R_R_I(INS_daddiu, attrDstAddr, REG_WRITE_BARRIER_DST_BYREF, REG_WRITE_BARRIER_DST_BYREF, TARGET_POINTER_SIZE);
                    }
                    break;

                default:
                    // In the case of a GC-Pointer we'll call the ByRef write barrier helper
                    genEmitHelperCall(CORINFO_HELP_ASSIGN_BYREF, 0, EA_PTRSIZE);

                    gcPtrCount--;
                    break;
            }
            ++i;
        }
        assert(gcPtrCount == 0);
    }

    if (cpObjNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a INS_BARRIER_RMB after a volatile CpObj operation
        instGen_MemoryBarrier(INS_BARRIER_RMB);
    }

    // Clear the gcInfo for REG_WRITE_BARRIER_SRC_BYREF and REG_WRITE_BARRIER_DST_BYREF.
    // While we normally update GC info prior to the last instruction that uses them,
    // these actually live into the helper call.
    gcInfo.gcMarkRegSetNpt(RBM_WRITE_BARRIER_SRC_BYREF | RBM_WRITE_BARRIER_DST_BYREF);
}

// generate code do a switch statement based on a table of ip-relative offsets
void CodeGen::genTableBasedSwitch(GenTree* treeNode)
{
    genConsumeOperands(treeNode->AsOp());
    regNumber idxReg  = treeNode->gtOp.gtOp1->gtRegNum;
    regNumber baseReg = treeNode->gtOp.gtOp2->gtRegNum;

    regNumber tmpReg = treeNode->GetSingleTempReg();

    // load the ip-relative offset (which is relative to start of fgFirstBB)
    getEmitter()->emitIns_R_R_I(INS_dsll, EA_8BYTE, REG_AT, idxReg, 2);
    getEmitter()->emitIns_R_R_R(INS_daddu, EA_8BYTE, baseReg, baseReg, REG_AT);
    getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, baseReg, baseReg, 0);

    // add it to the absolute address of fgFirstBB
    compiler->fgFirstBB->bbFlags |= BBF_JMP_TARGET;
    getEmitter()->emitIns_R_L(INS_lea, EA_PTRSIZE, compiler->fgFirstBB, tmpReg);
    getEmitter()->emitIns_R_R_R(INS_daddu, EA_PTRSIZE, baseReg, baseReg, tmpReg);

    // jr baseReg
    getEmitter()->emitIns_R(INS_jr, emitActualTypeSize(TYP_I_IMPL), baseReg);
    //branch-delay-slot
    if (baseReg != REG_T9)
    {
        getEmitter()->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_T9, baseReg, 0);
    }
    else
    {
        instGen(INS_nop);
    }
}

// emits the table and an instruction to get the address of the first element
void CodeGen::genJumpTable(GenTree* treeNode)
{
    noway_assert(compiler->compCurBB->bbJumpKind == BBJ_SWITCH);
    assert(treeNode->OperGet() == GT_JMPTABLE);

    unsigned     jumpCount = compiler->compCurBB->bbJumpSwt->bbsCount;
    BasicBlock** jumpTable = compiler->compCurBB->bbJumpSwt->bbsDstTab;
    unsigned     jmpTabOffs;
    unsigned     jmpTabBase;

    jmpTabBase = getEmitter()->emitBBTableDataGenBeg(jumpCount, true);

    jmpTabOffs = 0;

    JITDUMP("\n      J_M%03u_DS%02u LABEL   DWORD\n", Compiler::s_compMethodsCount, jmpTabBase);

    for (unsigned i = 0; i < jumpCount; i++)
    {
        BasicBlock* target = *jumpTable++;
        noway_assert(target->bbFlags & BBF_JMP_TARGET);

        JITDUMP("            DD      L_M%03u_" FMT_BB "\n", Compiler::s_compMethodsCount, target->bbNum);

        getEmitter()->emitDataGenData(i, target);
    };

    getEmitter()->emitDataGenEnd();

    // Access to inline data is 'abstracted' by a special type of static member
    // (produced by eeFindJitDataOffs) which the emitter recognizes as being a reference
    // to constant data, not a real static field.
    getEmitter()->emitIns_R_C(INS_bal, emitActualTypeSize(TYP_I_IMPL), treeNode->gtRegNum, REG_NA,
                              compiler->eeFindJitDataOffs(jmpTabBase), 0);
    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genLockedInstructions: Generate code for a GT_XADD or GT_XCHG node.
//
// Arguments:
//    treeNode - the GT_XADD/XCHG node
//
void CodeGen::genLockedInstructions(GenTreeOp* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  data      = treeNode->gtOp.gtOp2;
    GenTree*  addr      = treeNode->gtOp.gtOp1;
    regNumber targetReg = treeNode->gtRegNum;
    regNumber dataReg   = data->gtRegNum;
    regNumber addrReg   = addr->gtRegNum;

    genConsumeAddress(addr);
    genConsumeRegs(data);

    emitAttr dataSize = emitActualTypeSize(data);

    if (compiler->compSupports(InstructionSet_Atomics))
    {
        assert(!data->isContainedIntOrIImmed());

        switch (treeNode->gtOper)
        {
            case GT_XCHG:
                getEmitter()->emitIns_R_R_R(INS_swpal, dataSize, dataReg, targetReg, addrReg);
                break;
            case GT_XADD:
                if ((targetReg == REG_NA) || (targetReg == REG_R0))
                {
                    getEmitter()->emitIns_R_R(INS_staddl, dataSize, dataReg, addrReg);
                }
                else
                {
                    getEmitter()->emitIns_R_R_R(INS_ldaddal, dataSize, dataReg, targetReg, addrReg);
                }
                break;
            default:
                assert(!"Unexpected treeNode->gtOper");
        }

        instGen_MemoryBarrier(INS_BARRIER_ISH);
    }
    else
    {
        regNumber exResultReg  = treeNode->ExtractTempReg(RBM_ALLINT);
        regNumber storeDataReg = (treeNode->OperGet() == GT_XCHG) ? dataReg : treeNode->ExtractTempReg(RBM_ALLINT);
        regNumber loadReg      = (targetReg != REG_NA) ? targetReg : storeDataReg;

        // Check allocator assumptions
        //
        // The register allocator should have extended the lifetimes of all input and internal registers so that
        // none interfere with the target.
        noway_assert(addrReg != targetReg);

        noway_assert(addrReg != loadReg);
        noway_assert(dataReg != loadReg);

        noway_assert(addrReg != storeDataReg);
        noway_assert((treeNode->OperGet() == GT_XCHG) || (addrReg != dataReg));

        assert(addr->isUsedFromReg());
        noway_assert(exResultReg != REG_NA);
        noway_assert(exResultReg != targetReg);
        noway_assert((targetReg != REG_NA) || (treeNode->OperGet() != GT_XCHG));

        // Store exclusive unpredictable cases must be avoided
        noway_assert(exResultReg != storeDataReg);
        noway_assert(exResultReg != addrReg);

        // NOTE: `genConsumeAddress` marks the consumed register as not a GC pointer, as it assumes that the input
        // registers
        // die at the first instruction generated by the node. This is not the case for these atomics as the  input
        // registers are multiply-used. As such, we need to mark the addr register as containing a GC pointer until
        // we are finished generating the code for this node.

        gcInfo.gcMarkRegPtrVal(addrReg, addr->TypeGet());

        // Emit code like this:
        //   retry:
        //     ldxr loadReg, [addrReg]
        //     add storeDataReg, loadReg, dataReg         # Only for GT_XADD
        //                                                # GT_XCHG storeDataReg === dataReg
        //     stxr exResult, storeDataReg, [addrReg]
        //     cbnz exResult, retry
        //     dmb ish

        BasicBlock* labelRetry = genCreateTempLabel();
        genDefineTempLabel(labelRetry);

        // The following instruction includes a acquire half barrier
        getEmitter()->emitIns_R_R(INS_ldaxr, dataSize, loadReg, addrReg);

        switch (treeNode->OperGet())
        {
            case GT_XADD:
                if (data->isContainedIntOrIImmed())
                {
                    // Even though INS_add is specified here, the encoder will choose either
                    // an INS_add or an INS_sub and encode the immediate as a positive value
                    genInstrWithConstant(INS_add, dataSize, storeDataReg, loadReg, data->AsIntConCommon()->IconValue(),
                                         REG_NA);
                }
                else
                {
                    getEmitter()->emitIns_R_R_R(INS_add, dataSize, storeDataReg, loadReg, dataReg);
                }
                break;
            case GT_XCHG:
                assert(!data->isContained());
                storeDataReg = dataReg;
                break;
            default:
                unreached();
        }

        // The following instruction includes a release half barrier
        //getEmitter()->emitIns_R_R_R(INS_stlxr, dataSize, exResultReg, storeDataReg, addrReg);

        getEmitter()->emitIns_J_R(INS_cbnz, EA_4BYTE, labelRetry, exResultReg);

        instGen_MemoryBarrier(INS_BARRIER_ISH);

        gcInfo.gcMarkRegSetNpt(addr->gtGetRegMask());
    }

    if (treeNode->gtRegNum != REG_NA)
    {
        genProduceReg(treeNode);
    }
#endif
}

//------------------------------------------------------------------------
// genCodeForCmpXchg: Produce code for a GT_CMPXCHG node.
//
// Arguments:
//    tree - the GT_CMPXCHG node
//
void CodeGen::genCodeForCmpXchg(GenTreeCmpXchg* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(treeNode->OperIs(GT_CMPXCHG));

    GenTree* addr      = treeNode->gtOpLocation;  // arg1
    GenTree* data      = treeNode->gtOpValue;     // arg2
    GenTree* comparand = treeNode->gtOpComparand; // arg3

    regNumber targetReg    = treeNode->gtRegNum;
    regNumber dataReg      = data->gtRegNum;
    regNumber addrReg      = addr->gtRegNum;
    regNumber comparandReg = comparand->gtRegNum;

    genConsumeAddress(addr);
    genConsumeRegs(data);
    genConsumeRegs(comparand);

    if (compiler->compSupports(InstructionSet_Atomics))
    {
        emitAttr dataSize = emitActualTypeSize(data);

        // casal use the comparand as the target reg
        if (targetReg != comparandReg)
        {
            getEmitter()->emitIns_R_R(INS_mov, dataSize, targetReg, comparandReg);

            // Catch case we destroyed data or address before use
            noway_assert(addrReg != targetReg);
            noway_assert(dataReg != targetReg);
        }
        getEmitter()->emitIns_R_R_R(INS_casal, dataSize, targetReg, dataReg, addrReg);

        instGen_MemoryBarrier(INS_BARRIER_ISH);
    }
    else
    {
        regNumber exResultReg = treeNode->ExtractTempReg(RBM_ALLINT);

        // Check allocator assumptions
        //
        // The register allocator should have extended the lifetimes of all input and internal registers so that
        // none interfere with the target.
        noway_assert(addrReg != targetReg);
        noway_assert(dataReg != targetReg);
        noway_assert(comparandReg != targetReg);
        noway_assert(addrReg != dataReg);
        noway_assert(targetReg != REG_NA);
        noway_assert(exResultReg != REG_NA);
        noway_assert(exResultReg != targetReg);

        assert(addr->isUsedFromReg());
        assert(data->isUsedFromReg());
        assert(!comparand->isUsedFromMemory());

        // Store exclusive unpredictable cases must be avoided
        noway_assert(exResultReg != dataReg);
        noway_assert(exResultReg != addrReg);

        // NOTE: `genConsumeAddress` marks the consumed register as not a GC pointer, as it assumes that the input
        // registers
        // die at the first instruction generated by the node. This is not the case for these atomics as the  input
        // registers are multiply-used. As such, we need to mark the addr register as containing a GC pointer until
        // we are finished generating the code for this node.

        gcInfo.gcMarkRegPtrVal(addrReg, addr->TypeGet());


        // Emit code like this:
        //   retry:
        //     ldxr targetReg, [addrReg]
        //     cmp targetReg, comparandReg
        //     bne compareFail
        //     stxr exResult, dataReg, [addrReg]
        //     cbnz exResult, retry
        //   compareFail:
        //     dmb ish

        BasicBlock* labelRetry       = genCreateTempLabel();
        BasicBlock* labelCompareFail = genCreateTempLabel();
        genDefineTempLabel(labelRetry);

        // The following instruction includes a acquire half barrier
        getEmitter()->emitIns_R_R(INS_ldaxr, emitTypeSize(treeNode), targetReg, addrReg);

        if (comparand->isContainedIntOrIImmed())
        {
            if (comparand->IsIntegralConst(0))
            {
                getEmitter()->emitIns_J_R(INS_cbnz, emitActualTypeSize(treeNode), labelCompareFail, targetReg);
            }
            else
            {
                getEmitter()->emitIns_R_I(INS_cmp, emitActualTypeSize(treeNode), targetReg,
                                          comparand->AsIntConCommon()->IconValue());
                getEmitter()->emitIns_J(INS_bne, labelCompareFail);
            }
        }
        else
        {
            getEmitter()->emitIns_R_R(INS_cmp, emitActualTypeSize(treeNode), targetReg, comparandReg);
            getEmitter()->emitIns_J(INS_bne, labelCompareFail);
        }

        // The following instruction includes a release half barrier
        getEmitter()->emitIns_R_R_R(INS_stlxr, emitTypeSize(treeNode), exResultReg, dataReg, addrReg);

        getEmitter()->emitIns_J_R(INS_cbnz, EA_4BYTE, labelRetry, exResultReg);

        genDefineTempLabel(labelCompareFail);

        instGen_MemoryBarrier(INS_BARRIER_ISH);

        gcInfo.gcMarkRegSetNpt(addr->gtGetRegMask());
    }

    genProduceReg(treeNode);
#endif
}

static inline bool isImmed(GenTree* treeNode)
{
        if (treeNode->gtGetOp1()->isContainedIntOrIImmed())
        {
            return true;
        }
        else if (treeNode->OperIsBinary())
        {
            if (treeNode->gtGetOp2()->isContainedIntOrIImmed())
                return true;
        }

        return false;
}

instruction CodeGen::genGetInsForOper(GenTree* treeNode)
{
    var_types  type = treeNode->TypeGet();
    genTreeOps oper = treeNode->OperGet();
    GenTree*   op1  = treeNode->gtGetOp1();
    GenTree*   op2;
    emitAttr   attr = emitActualTypeSize(treeNode);
    bool isImm = false;

    instruction ins = INS_break;

    if (varTypeIsFloating(type))
    {
        /* FIXME for MIPS: should amend type-size. */
        switch (oper)
        {
            case GT_ADD:
                if (attr == EA_4BYTE)
                    ins = INS_add_s;
                else
                    ins = INS_add_d;
                break;
            case GT_SUB:
                if (attr == EA_4BYTE)
                    ins = INS_sub_s;
                else
                    ins = INS_sub_d;
                break;
            case GT_MUL:
                if (attr == EA_4BYTE)
                    ins = INS_mul_s;
                else
                    ins = INS_mul_d;
                break;
            case GT_DIV:
                if (attr == EA_4BYTE)
                    ins = INS_div_s;
                else
                    ins = INS_div_d;
                break;
            case GT_NEG:
                if (attr == EA_4BYTE)
                    ins = INS_neg_s;
                else
                    ins = INS_neg_d;
                break;

            default:
                NYI("Unhandled oper in genGetInsForOper() - float");
                unreached();
                break;
        }
    }
    else
    {
        switch (oper)
        {
            case GT_ADD:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                    {
                        ins = INS_daddiu;
                    }
                    else
                    {
                        assert(attr == EA_4BYTE);
                        ins = INS_addiu;
                    }
                }
                else
                {
                    if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                    {
                        ins = INS_daddu;
                    }
                    else
                    {
                        assert(attr == EA_4BYTE);
                        ins = INS_addu;
                    }
                }
                break;

            case GT_SUB:
                isImm = isImmed(treeNode);
                if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                {
                    ins = INS_dsubu;
                }
                else
                {
                    assert(attr == EA_4BYTE);
                    ins = INS_subu;
                }
                break;

            case GT_MOD:
            case GT_DIV:
                if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                {
                    ins = INS_ddiv;
                }
                else
                {
                    assert(attr == EA_4BYTE);
                    ins = INS_div;
                }
                break;

            case GT_UMOD:
            case GT_UDIV:
                if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                {
                    ins = INS_ddivu;
                }
                else
                {
                    assert(attr == EA_4BYTE);
                    ins = INS_divu;
                }
                break;

            case GT_MUL:
                if ((attr == EA_8BYTE) || (attr == EA_BYREF))
                {
                    if ((treeNode->gtFlags & GTF_UNSIGNED) != 0)
                        ins = INS_dmultu;
                    else
                        ins = INS_dmult;
                }
                else
                {
                    if ((treeNode->gtFlags & GTF_UNSIGNED) != 0)
                        ins = INS_multu;
                    else
                        ins = INS_mult;
                }
                break;

            case GT_NEG:
                if (attr == EA_8BYTE)
                {
                    ins = INS_dneg;
                }
                else
                {
                    assert(attr == EA_4BYTE);
                    ins = INS_neg;
                }
                break;

            case GT_NOT:
                ins = INS_not;
                break;

            case GT_AND:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    ins = INS_andi;
                }
                else
                {
                    ins = INS_and;
                }
                break;

            case GT_OR:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    ins = INS_ori;
                }
                else
                {
                    ins = INS_or;
                }
                break;

            case GT_LSH:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    //it's better to check sa.
                    if (attr == EA_4BYTE)
                        ins = INS_sll;
                    else
                        ins = INS_dsll;
                }
                else
                {
                    if (attr == EA_4BYTE)
                        ins = INS_sllv;
                    else
                        ins = INS_dsllv;
                }
                break;

            case GT_RSZ:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    //it's better to check sa.
                    if (attr == EA_4BYTE)
                        ins = INS_srl;
                    else
                        ins = INS_dsrl;
                }
                else
                {
                    if (attr == EA_4BYTE)
                        ins = INS_srlv;
                    else
                        ins = INS_dsrlv;
                }
                break;

            case GT_RSH:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    //it's better to check sa.
                    if (attr == EA_4BYTE)
                        ins = INS_sra;
                    else
                        ins = INS_dsra;
                }
                else
                {
                    if (attr == EA_4BYTE)
                        ins = INS_srav;
                    else
                        ins = INS_dsrav;
                }
                break;

            case GT_ROR:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    //it's better to check sa.
                    if (attr == EA_4BYTE)
                        ins = INS_rotr;
                    else
                        ins = INS_drotr;
                }
                else
                {
                    if (attr == EA_4BYTE)
                        ins = INS_rotrv;
                    else
                        ins = INS_drotrv;
                }
                break;

            case GT_XOR:
                isImm = isImmed(treeNode);
                if (isImm)
                {
                    ins = INS_xori;
                }
                else
                {
                    ins = INS_xor;
                }
                break;

            default:
                NYI("Unhandled oper in genGetInsForOper() - integer");
                unreached();
                break;
        }
    }
    return ins;
}

//------------------------------------------------------------------------
// genCodeForReturnTrap: Produce code for a GT_RETURNTRAP node.
//
// Arguments:
//    tree - the GT_RETURNTRAP node
//
void CodeGen::genCodeForReturnTrap(GenTreeOp* tree)
{
    assert(tree->OperGet() == GT_RETURNTRAP);

    // this is nothing but a conditional call to CORINFO_HELP_STOP_FOR_GC
    // based on the contents of 'data'

    GenTree* data = tree->gtOp1;
    genConsumeRegs(data);
    ssize_t imm = 7 << 2;
    getEmitter()->emitIns_R_R_I(INS_bne, EA_PTRSIZE, data->gtRegNum, REG_R0, imm);
    getEmitter()->emitIns(INS_nop);//can optimize for mips.

    BasicBlock* skipLabel = genCreateTempLabel();

    inst_JMP(EJ_jmp, skipLabel);//no branch-delay.
    // emit the call to the EE-helper that stops for GC (or other reasons)

    genEmitHelperCall(CORINFO_HELP_STOP_FOR_GC, 0, EA_UNKNOWN);//no branch-delay.
    genDefineTempLabel(skipLabel);
}

//------------------------------------------------------------------------
// genCodeForStoreInd: Produce code for a GT_STOREIND node.
//
// Arguments:
//    tree - the GT_STOREIND node
//
void CodeGen::genCodeForStoreInd(GenTreeStoreInd* tree)
{
    /* FIXME for MIPS */
#ifdef FEATURE_SIMD
    // Storing Vector3 of size 12 bytes through indirection
    if (tree->TypeGet() == TYP_SIMD12)
    {
        genStoreIndTypeSIMD12(tree);
        return;
    }
#endif // FEATURE_SIMD

    GenTree* data = tree->Data();
    GenTree* addr = tree->Addr();

    GCInfo::WriteBarrierForm writeBarrierForm = gcInfo.gcIsWriteBarrierCandidate(tree, data);
    if (writeBarrierForm != GCInfo::WBF_NoBarrier)
    {
        // data and addr must be in registers.
        // Consume both registers so that any copies of interfering
        // registers are taken care of.
        genConsumeOperands(tree);

        // At this point, we should not have any interference.
        // That is, 'data' must not be in REG_WRITE_BARRIER_DST_BYREF,
        //  as that is where 'addr' must go.
        noway_assert(data->gtRegNum != REG_WRITE_BARRIER_DST_BYREF);

        // 'addr' goes into x14 (REG_WRITE_BARRIER_DST)
        genCopyRegIfNeeded(addr, REG_WRITE_BARRIER_DST);

        // 'data' goes into x15 (REG_WRITE_BARRIER_SRC)
        genCopyRegIfNeeded(data, REG_WRITE_BARRIER_SRC);

        genGCWriteBarrier(tree, writeBarrierForm);
    }
    else // A normal store, not a WriteBarrier store
    {
        // We must consume the operands in the proper execution order,
        // so that liveness is updated appropriately.
        genConsumeAddress(addr);

        if (!data->isContained())
        {
            genConsumeRegs(data);
        }

        regNumber dataReg;
        if (data->isContainedIntOrIImmed())
        {
            assert(data->IsIntegralConst(0));
            dataReg = REG_R0;
        }
        else // data is not contained, so evaluate it into a register
        {
            assert(!data->isContained());
            dataReg = data->gtRegNum;
        }

        var_types   type = tree->TypeGet();
        instruction ins  = ins_Store(type);

        if ((tree->gtFlags & GTF_IND_VOLATILE) != 0)
        {
            bool addrIsInReg   = addr->isUsedFromReg();
            bool addrIsAligned = ((tree->gtFlags & GTF_IND_UNALIGNED) == 0);

            if ((ins == INS_sb) && addrIsInReg)
            {
                instGen_MemoryBarrier(INS_BARRIER_REL);
                ins = INS_sb;
            }
            else if ((ins == INS_sh) && addrIsInReg && addrIsAligned)
            {
                instGen_MemoryBarrier(INS_BARRIER_REL);
                ins = INS_sh;
            }
            else if ((ins == INS_sw) && genIsValidIntReg(dataReg) && addrIsInReg && addrIsAligned)
            {
                instGen_MemoryBarrier(INS_BARRIER_REL);
                ins = INS_sw;
            }
            else if ((ins == INS_sd) && genIsValidIntReg(dataReg) && addrIsInReg && addrIsAligned)
            {
                instGen_MemoryBarrier(INS_BARRIER_REL);
                ins = INS_sd;
            }
            else
            {
                // issue a full memory barrier before a volatile StInd
                instGen_MemoryBarrier();
            }
        }

        getEmitter()->emitInsLoadStoreOp(ins, emitActualTypeSize(type), dataReg, tree);
    }
}

//------------------------------------------------------------------------
// genCodeForSwap: Produce code for a GT_SWAP node.
//
// Arguments:
//    tree - the GT_SWAP node
//
void CodeGen::genCodeForSwap(GenTreeOp* tree)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(tree->OperIs(GT_SWAP));

    // Swap is only supported for lclVar operands that are enregistered
    // We do not consume or produce any registers.  Both operands remain enregistered.
    // However, the gc-ness may change.
    assert(genIsRegCandidateLocal(tree->gtOp1) && genIsRegCandidateLocal(tree->gtOp2));

    GenTreeLclVarCommon* lcl1    = tree->gtOp1->AsLclVarCommon();
    LclVarDsc*           varDsc1 = &(compiler->lvaTable[lcl1->gtLclNum]);
    var_types            type1   = varDsc1->TypeGet();
    GenTreeLclVarCommon* lcl2    = tree->gtOp2->AsLclVarCommon();
    LclVarDsc*           varDsc2 = &(compiler->lvaTable[lcl2->gtLclNum]);
    var_types            type2   = varDsc2->TypeGet();

    // We must have both int or both fp regs
    assert(!varTypeIsFloating(type1) || varTypeIsFloating(type2));

    // FP swap is not yet implemented (and should have NYI'd in LSRA)
    assert(!varTypeIsFloating(type1));

    regNumber oldOp1Reg     = lcl1->gtRegNum;
    regMaskTP oldOp1RegMask = genRegMask(oldOp1Reg);
    regNumber oldOp2Reg     = lcl2->gtRegNum;
    regMaskTP oldOp2RegMask = genRegMask(oldOp2Reg);

    // We don't call genUpdateVarReg because we don't have a tree node with the new register.
    varDsc1->lvRegNum = oldOp2Reg;
    varDsc2->lvRegNum = oldOp1Reg;

    // Do the xchg
    emitAttr size = EA_PTRSIZE;
    if (varTypeGCtype(type1) != varTypeGCtype(type2))
    {
        // If the type specified to the emitter is a GC type, it will swap the GC-ness of the registers.
        // Otherwise it will leave them alone, which is correct if they have the same GC-ness.
        size = EA_GCREF;
    }

    NYI("register swap");
    // inst_RV_RV(INS_xchg, oldOp1Reg, oldOp2Reg, TYP_I_IMPL, size);

    // Update the gcInfo.
    // Manually remove these regs for the gc sets (mostly to avoid confusing duplicative dump output)
    gcInfo.gcRegByrefSetCur &= ~(oldOp1RegMask | oldOp2RegMask);
    gcInfo.gcRegGCrefSetCur &= ~(oldOp1RegMask | oldOp2RegMask);

    // gcMarkRegPtrVal will do the appropriate thing for non-gc types.
    // It will also dump the updates.
    gcInfo.gcMarkRegPtrVal(oldOp2Reg, type1);
    gcInfo.gcMarkRegPtrVal(oldOp1Reg, type2);
#endif
}

//------------------------------------------------------------------------
// genIntToFloatCast: Generate code to cast an int/long to float/double
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    SrcType= int32/uint32/int64/uint64 and DstType=float/double.
//
void CodeGen::genIntToFloatCast(GenTree* treeNode)
{
    // int type --> float/double conversions are always non-overflow ones
    assert(treeNode->OperGet() == GT_CAST);
    assert(!treeNode->gtOverflow());

    regNumber targetReg = treeNode->gtRegNum;
    assert(genIsValidFloatReg(targetReg));

    GenTree* op1 = treeNode->gtOp.gtOp1;
    assert(!op1->isContained());             // Cannot be contained
    assert(genIsValidIntReg(op1->gtRegNum)); // Must be a valid int reg.

    var_types dstType = treeNode->CastToType();
    var_types srcType = genActualType(op1->TypeGet());
    assert(!varTypeIsFloating(srcType) && varTypeIsFloating(dstType));

    emitter *emit = getEmitter();
    emitAttr attr = emitActualTypeSize(dstType);

    // We should never see a srcType whose size is neither EA_4BYTE or EA_8BYTE
    emitAttr srcSize = EA_ATTR(genTypeSize(srcType));
    noway_assert((srcSize == EA_4BYTE) || (srcSize == EA_8BYTE));

    bool IsUnsigned = treeNode->gtFlags & GTF_UNSIGNED;
    instruction ins = INS_invalid;

    genConsumeOperands(treeNode->AsOp());

    if (IsUnsigned)
    {
        emit->emitIns_R_R(INS_dmtc1, EA_8BYTE, op1->gtRegNum, REG_F1); // save op1

        if (srcSize == EA_8BYTE)
        {
            ssize_t imm = 4 << 2;
            emit->emitIns_R_I(INS_bgez, EA_8BYTE, op1->gtRegNum, imm);
            emit->emitIns(INS_nop);

            emit->emitIns_R_R_I(INS_andi, EA_8BYTE, REG_AT, op1->gtRegNum, 1);
            emit->emitIns_R_R_I(INS_dsrl, EA_8BYTE, op1->gtRegNum, op1->gtRegNum, 1);
            emit->emitIns_R_R_R(INS_or, EA_8BYTE, op1->gtRegNum, op1->gtRegNum, REG_AT);
        }
        else
        {
            srcSize = EA_8BYTE;
            emit->emitIns_R_R_I_I(INS_dinsu, EA_8BYTE, op1->gtRegNum, REG_R0, 32, 32);
        }
    }

    ins = srcSize == EA_8BYTE ? INS_dmtc1 : INS_mtc1;
    emit->emitIns_R_R(ins, attr, op1->gtRegNum, treeNode->gtRegNum);

    if (dstType == TYP_DOUBLE)
    {
        if (srcSize == EA_4BYTE)
        {
            ins = INS_cvt_d_w;
        }
        else
        {
            assert(srcSize == EA_8BYTE);
            ins = INS_cvt_d_l;
        }
    }
    else
    {
        assert(dstType == TYP_FLOAT);
        if (srcSize == EA_4BYTE)
        {
            ins = INS_cvt_s_w;
        }
        else
        {
            assert(srcSize == EA_8BYTE);
            ins = INS_cvt_s_l;
        }
    }

    emit->emitIns_R_R(ins, attr, treeNode->gtRegNum, treeNode->gtRegNum);

    if (IsUnsigned)
    {
        srcSize = EA_ATTR(genTypeSize(srcType));
        emit->emitIns_R_R(INS_dmfc1, attr, op1->gtRegNum, REG_F1); // recover op1

        if (srcSize == EA_8BYTE)
        {
            ssize_t imm = 3 << 2;
            emit->emitIns_R_I(INS_bgez, EA_8BYTE, op1->gtRegNum, imm);
            emit->emitIns(INS_nop);

            emit->emitIns_R_R(dstType == TYP_DOUBLE ? INS_mov_d : INS_mov_s, attr, REG_F1, treeNode->gtRegNum);
            emit->emitIns_R_R_R(dstType == TYP_DOUBLE ? INS_add_d : INS_add_s, attr, treeNode->gtRegNum, REG_F1, treeNode->gtRegNum);
        }
    }

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genFloatToIntCast: Generate code to cast float/double to int/long
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    SrcType=float/double and DstType= int32/uint32/int64/uint64
//
void CodeGen::genFloatToIntCast(GenTree* treeNode)
{
    // we don't expect to see overflow detecting float/double --> int type conversions here
    // as they should have been converted into helper calls by front-end.
    assert(treeNode->OperGet() == GT_CAST);
    assert(!treeNode->gtOverflow());

    regNumber targetReg = treeNode->gtRegNum;
    assert(genIsValidIntReg(targetReg)); // Must be a valid int reg.

    GenTree* op1 = treeNode->gtOp.gtOp1;
    assert(!op1->isContained());               // Cannot be contained
    assert(genIsValidFloatReg(op1->gtRegNum)); // Must be a valid float reg.

    var_types dstType = treeNode->CastToType();
    var_types srcType = op1->TypeGet();
    assert(varTypeIsFloating(srcType) && !varTypeIsFloating(dstType));

    // We should never see a dstType whose size is neither EA_4BYTE or EA_8BYTE
    // For conversions to small types (byte/sbyte/int16/uint16) from float/double,
    // we expect the front-end or lowering phase to have generated two levels of cast.
    //
    emitAttr dstSize = EA_ATTR(genTypeSize(dstType));
    noway_assert((dstSize == EA_4BYTE) || (dstSize == EA_8BYTE));

    instruction ins1 = INS_invalid;
    instruction ins2 = INS_invalid;
    bool IsUnsigned = varTypeIsUnsigned(dstType);

    regNumber tmpReg = REG_F1; // FIXME: NOTE: REG_F1 is a scratch-float-reg.
    assert(tmpReg != op1->gtRegNum);

    if (srcType == TYP_DOUBLE)
    {
        if (dstSize == EA_4BYTE)
        {
            ins1 = INS_trunc_w_d;
            ins2 = INS_mfc1;
        }
        else
        {
            assert(dstSize == EA_8BYTE);
            ins1 = INS_trunc_l_d;
            ins2 = INS_dmfc1;
        }
    }
    else
    {
        assert(srcType == TYP_FLOAT);
        if (dstSize == EA_4BYTE)
        {
            ins1 = INS_trunc_w_s;
            ins2 = INS_mfc1;
        }
        else
        {
            assert(dstSize == EA_8BYTE);
            ins1 = INS_trunc_l_s;
            ins2 = INS_dmfc1;
        }
    }

    genConsumeOperands(treeNode->AsOp());

    if (IsUnsigned)
    {
        ssize_t imm = 0;

        if (srcType == TYP_DOUBLE)
        {
            if (dstSize == EA_4BYTE)
            {
                imm = 0x41e0;
            }
            else
            {
                imm = 0x43e0;
            }
        }
        else
        {
            assert(srcType == TYP_FLOAT);
            if (dstSize == EA_4BYTE)
            {
                imm = 0x4f00;
            }
            else
            {
                imm = 0x5f00;
            }
        }

        /* FIXME for MIPS: How about the case: op1->gtRegNum < 0 ? */
        //{
        //    getEmitter()->emitIns_R_R(INS_dmtc1, EA_8BYTE, REG_R0, tmpReg);

        //    getEmitter()->emitIns_R_R_I(srcType == TYP_DOUBLE ? INS_c_olt_d : INS_c_olt_s, EA_8BYTE, op1->gtRegNum, tmpReg, 2);
        //    getEmitter()->emitIns_I_I(INS_bc1f, EA_PTRSIZE, 2, 4 << 2);
        //    getEmitter()->emitIns(INS_nop);

        //    getEmitter()->emitIns_R_R_I(INS_ori, EA_PTRSIZE, treeNode->gtRegNum, REG_R0, 0);
        //    getEmitter()->emitIns_I(INS_b, EA_PTRSIZE, srcType == TYP_DOUBLE ? 14 << 2 : 13 << 2);
        //    getEmitter()->emitIns(INS_nop);
        //}

        getEmitter()->emitIns_R_I(INS_lui, EA_PTRSIZE, REG_AT, imm);
        if (srcType == TYP_DOUBLE)
            getEmitter()->emitIns_R_R_I(INS_dsll32, EA_8BYTE, REG_AT, REG_AT, 0);

        getEmitter()->emitIns_R_R(srcType == TYP_DOUBLE ? INS_dmtc1 : INS_mtc1, EA_8BYTE, REG_AT, tmpReg);

        getEmitter()->emitIns_R_R_I(srcType == TYP_DOUBLE ? INS_c_olt_d : INS_c_olt_s, EA_8BYTE, op1->gtRegNum, tmpReg, 2);

        getEmitter()->emitIns_I_I(INS_bc1t, EA_PTRSIZE, 2, 4 << 2);
        getEmitter()->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_R0, 0);

        getEmitter()->emitIns_R_R_R(srcType == TYP_DOUBLE ? INS_sub_d : INS_sub_s, EA_8BYTE, tmpReg, op1->gtRegNum, tmpReg);

        getEmitter()->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_R0, 1);
        getEmitter()->emitIns_R_R_I(dstSize == EA_8BYTE ? INS_dsll32 : INS_dsll, EA_PTRSIZE, REG_AT, REG_AT, 31);

        getEmitter()->emitIns_R_R_I(srcType == TYP_DOUBLE ? INS_movt_d : INS_movt_s, EA_PTRSIZE, tmpReg, op1->gtRegNum, 2);

        getEmitter()->emitIns_R_R(ins1, dstSize, tmpReg, tmpReg);
        getEmitter()->emitIns_R_R(ins2, dstSize, treeNode->gtRegNum, tmpReg);

        getEmitter()->emitIns_R_R_R(INS_or, dstSize, treeNode->gtRegNum, REG_AT, treeNode->gtRegNum);
    }
    else
    {
        getEmitter()->emitIns_R_R(ins1, dstSize, tmpReg, op1->gtRegNum);
        getEmitter()->emitIns_R_R(ins2, dstSize, treeNode->gtRegNum, tmpReg);
    }

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCkfinite: Generate code for ckfinite opcode.
//
// Arguments:
//    treeNode - The GT_CKFINITE node
//
// Return Value:
//    None.
//
// Assumptions:
//    GT_CKFINITE node has reserved an internal register.
//
void CodeGen::genCkfinite(GenTree* treeNode)
{
    assert(treeNode->OperGet() == GT_CKFINITE);

    GenTree*  op1         = treeNode->gtOp.gtOp1;
    var_types targetType  = treeNode->TypeGet();
    ssize_t   expMask     = (targetType == TYP_FLOAT) ? 0xFF : 0x7FF; // Bit mask to extract exponent.
    ssize_t   size        = (targetType == TYP_FLOAT) ? 8 : 11;  // Bit size to extract exponent.
    ssize_t   pos         = (targetType == TYP_FLOAT) ? 23 : 52; // Bit pos of exponent.

    emitter* emit = getEmitter();
    emitAttr attr = emitActualTypeSize(treeNode);

    // Extract exponent into a register.
    regNumber intReg = treeNode->GetSingleTempReg();
    regNumber fpReg  = genConsumeReg(op1);

    emit->emitIns_R_R(attr == EA_8BYTE ? INS_dmfc1 : INS_mfc1, attr, intReg, fpReg);

    // Mask of exponent with all 1's and check if the exponent is all 1's
    instruction ins = (targetType == TYP_FLOAT) ? INS_ext : INS_dextu;
    emit->emitIns_R_R_I_I(ins, EA_PTRSIZE, intReg, intReg, pos, size);
    emit->emitIns_R_R_I(INS_xori, attr, intReg, intReg, expMask);

    ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
    emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, intReg, REG_R0, imm);
    emit->emitIns(INS_nop);

    // If exponent is all 1's, throw ArithmeticException
    genJumpToThrowHlpBlk(EJ_jmp, SCK_ARITH_EXCPN); //EJ_jmp is 6/8-ins.
    //emit->emitIns(INS_nop);

    // if it is a finite value copy it to targetReg
    if (treeNode->gtRegNum != fpReg)
    {
        emit->emitIns_R_R(ins_Copy(targetType), attr, treeNode->gtRegNum, fpReg);
    }
    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCodeForCompare: Produce code for a GT_EQ/GT_NE/GT_LT/GT_LE/GT_GE/GT_GT node.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForCompare(GenTreeOp* tree)
{
/* FIXME for MIPS: should re-design for mips64. */

    ssize_t cc = 1;
    bool cc_true = true;

    regNumber targetReg = tree->gtRegNum;
    emitter* emit = getEmitter();

    GenTree*  op1     = tree->gtOp1;
    GenTree*  op2     = tree->gtOp2;
    var_types op1Type = genActualType(op1->TypeGet());
    var_types op2Type = genActualType(op2->TypeGet());

    assert(!op1->isUsedFromMemory());
    assert(!op2->isUsedFromMemory());

    genConsumeOperands(tree);

    emitAttr cmpSize = EA_ATTR(genTypeSize(op1Type));

    GenCondition condition = GenCondition::FromRelop(tree);
    assert(genTypeSize(op1Type) == genTypeSize(op2Type));

    regNumber SaveCcResultReg = targetReg != REG_NA ? targetReg : REG_AT;
    if (varTypeIsFloating(op1Type))
    {
        /* FIXME for MIPS: */
        assert(tree->OperIs(GT_LT, GT_LE, GT_EQ, GT_NE, GT_GT, GT_GE));
        bool IsUnordered = condition.IsUnordered();

        if (tree->OperIs(GT_EQ))
        {
            cc_true = true;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ueq_s : INS_c_eq_s, EA_4BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ueq_d : INS_c_eq_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }
        else if (tree->OperIs(GT_NE))
        {
            cc_true = false;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_eq_s : INS_c_ueq_s, EA_4BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_eq_d : INS_c_ueq_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }
        else if (tree->OperIs(GT_LT))
        {
            cc_true = true;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ult_s : INS_c_olt_s, EA_4BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ult_d : INS_c_olt_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }
        else if (tree->OperIs(GT_LE))
        {
            cc_true = true;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ule_s : INS_c_ole_s, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ule_d : INS_c_ole_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }
        else if (tree->OperIs(GT_GE))
        {
            cc_true = false;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_olt_s : INS_c_ult_s, EA_4BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_olt_d : INS_c_ult_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }
        else if (tree->OperIs(GT_GT))
        {
            cc_true = false;
            if (cmpSize == EA_4BYTE)
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ole_s : INS_c_ule_s, EA_4BYTE, op1->gtRegNum, op2->gtRegNum, cc);
            else
                emit->emitIns_R_R_I(IsUnordered ? INS_c_ole_d : INS_c_ule_d, EA_8BYTE, op1->gtRegNum, op2->gtRegNum, cc);
        }

        emit->emitIns_R_R(INS_cfc1, EA_PTRSIZE, SaveCcResultReg, regNumber(25));

        ssize_t imm = 1 << cc;
        emit->emitIns_R_R_I(INS_andi, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, imm);

        emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, SaveCcResultReg, REG_R0, SaveCcResultReg);

        if (!cc_true)
        {
            imm = 1;
            emit->emitIns_R_R_I(INS_xori, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, imm);
        }

        assert(0 <= cc && cc < 8);
    }
    else
    {
        assert(tree->OperIs(GT_CMP, GT_LT, GT_LE, GT_EQ, GT_NE, GT_GT, GT_GE));

        bool IsUnsigned = condition.IsUnsigned();
        regNumber tmpRegOp1 = tree->ExtractTempReg();
        regNumber tmpRegOp2 = tree->ExtractTempReg();
        regNumber regOp1 = op1->gtRegNum;
        regNumber regOp2 = op2->gtRegNum;

        if (op2->isContainedIntOrIImmed())
        {
            /* FIXME for MIPS: should modify intConst. Best is design a new func. */
            GenTreeIntConCommon* intConst = op2->AsIntConCommon();
            ssize_t imm = intConst->IconValue();

            if (IsUnsigned)
            {
                switch (cmpSize)
                {
                case EA_1BYTE:
                    imm = static_cast<uint8_t>(imm);
                    break;
                case EA_2BYTE:
                    imm = static_cast<uint16_t>(imm);
                    break;
                case EA_4BYTE:
                    imm = static_cast<uint32_t>(imm);
                    break;
                default:
                    break;
                }
            }

            if (cmpSize == EA_4BYTE)
            {
                regOp1 = tmpRegOp1;
                if (IsUnsigned)
                    emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, tmpRegOp1, op1->gtRegNum, 0, 32);
                else
                    emit->emitIns_R_R_I(INS_sll, EA_4BYTE, tmpRegOp1, op1->gtRegNum, 0);
            }

            if (tree->OperIs(GT_CMP))
            {
                assert(!"------------should comfirm.");
                //emit->emitIns_R_R_I(IsUnsigned ? INS_sltiu : INS_slti, EA_PTRSIZE, SaveCcResultReg, regOp1, imm);
            }
            else if (tree->OperIs(GT_LT))
            {
                if (isValidSimm16(imm))
                {
                    emit->emitIns_R_R_I(IsUnsigned ? INS_sltiu : INS_slti, EA_PTRSIZE, SaveCcResultReg, regOp1, imm);
                }
                else
                {
                    set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);
                    emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_PTRSIZE, SaveCcResultReg, regOp1, REG_AT);
                }
            }
            else if (tree->OperIs(GT_LE))
            {
                set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);

                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_PTRSIZE, SaveCcResultReg, REG_AT, regOp1);

                emit->emitIns_R_R_I(INS_xori, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
            }
            else if (tree->OperIs(GT_GT))
            {
                set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);

                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_PTRSIZE, SaveCcResultReg, REG_AT, regOp1);
            }
            else if (tree->OperIs(GT_GE))
            {
                if (isValidSimm16(imm))
                {
                    emit->emitIns_R_R_I(IsUnsigned ? INS_sltiu : INS_slti, EA_PTRSIZE, SaveCcResultReg, regOp1, imm);
                }
                else
                {
                    set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);

                    emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_PTRSIZE, SaveCcResultReg, regOp1, REG_AT);
                }

                emit->emitIns_R_R_I(INS_xori, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
            }
            else if (tree->OperIs(GT_NE))
            {
                if (imm)
                {
                    set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);

                    emit->emitIns_R_R_R(INS_xor, EA_PTRSIZE, SaveCcResultReg, regOp1, REG_AT);

                    emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, SaveCcResultReg, REG_R0, SaveCcResultReg);
                }
                else
                {
                    emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, SaveCcResultReg, REG_R0, regOp1);
                }
            }
            else if (tree->OperIs(GT_EQ))
            {
                if (imm)
                {
                    set_Reg_To_Imm(emit, EA_PTRSIZE, REG_AT, imm);

                    emit->emitIns_R_R_R(INS_xor, EA_PTRSIZE, SaveCcResultReg, regOp1, REG_AT);

                    emit->emitIns_R_R_I(INS_sltiu, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
                }
                else
                {
                    emit->emitIns_R_R_I(INS_sltiu, EA_PTRSIZE, SaveCcResultReg, regOp1, 1);
                }
            }
            else
            {
                assert(!"unimplemented on MIPS yet");
            }
        }
        else
        {
            if (cmpSize == EA_4BYTE)
            {
                regOp1 = tmpRegOp1;
                regOp2 = tmpRegOp2;
                if (IsUnsigned)
                {
                    emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, tmpRegOp1, op1->gtRegNum, 0, 32);
                    emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, tmpRegOp2, op2->gtRegNum, 0, 32);
                }
                else
                {
                    emit->emitIns_R_R_I(INS_sll, EA_4BYTE, tmpRegOp1, op1->gtRegNum, 0);
                    emit->emitIns_R_R_I(INS_sll, EA_4BYTE, tmpRegOp2, op2->gtRegNum, 0);
                }
            }

            if (tree->OperIs(GT_CMP))
            {
                assert(!"------------should comfirm-3.");
            }
            else if (/*tree->OperIs(GT_CMP) ||*/ tree->OperIs(GT_LT))
            {
                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_8BYTE, SaveCcResultReg, regOp1, regOp2);
            }
            else if (tree->OperIs(GT_LE))
            {
                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_8BYTE, SaveCcResultReg, regOp2, regOp1);

                emit->emitIns_R_R_I(INS_xori, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
            }
            else if (tree->OperIs(GT_GT))
            {
                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_8BYTE, SaveCcResultReg, regOp2, regOp1);
            }
            else if (tree->OperIs(GT_GE))
            {
                emit->emitIns_R_R_R(IsUnsigned ? INS_sltu : INS_slt, EA_8BYTE, SaveCcResultReg, regOp1, regOp2);

                emit->emitIns_R_R_I(INS_xori, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
            }
            else if (tree->OperIs(GT_NE))
            {
                emit->emitIns_R_R_R(INS_xor, EA_PTRSIZE, SaveCcResultReg, regOp1, regOp2);
                emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, SaveCcResultReg, REG_R0, SaveCcResultReg);
            }
            else if (tree->OperIs(GT_EQ))
            {
                emit->emitIns_R_R_R(INS_xor, EA_PTRSIZE, SaveCcResultReg, regOp1, regOp2);

                emit->emitIns_R_R_I(INS_sltiu, EA_PTRSIZE, SaveCcResultReg, SaveCcResultReg, 1);
            }
            else
            {
                assert(!"unimplemented on MIPS yet");
            }
        }

        genProduceReg(tree);
    }
}

//------------------------------------------------------------------------
// genCodeForJumpTrue: Generate code for a GT_JTRUE node.
//
// Arguments:
//    jtrue - The node
//
void CodeGen::genCodeForJumpTrue(GenTreeOp* jtrue)
{
    assert(compiler->compCurBB->bbJumpKind == BBJ_COND);
    assert(jtrue->OperIs(GT_JTRUE));

    emitter* emit = getEmitter();
    regNumber SaveCcResultReg = jtrue->gtGetOp1()->gtRegNum;
    SaveCcResultReg = SaveCcResultReg != REG_NA ? SaveCcResultReg : REG_AT;

    ssize_t imm = 7 << 2;//size of Jump-placeholders in bytes liking within emitter::emitIns_J() .
    emit->emitIns_R_R_I(INS_beq, EA_PTRSIZE, SaveCcResultReg, REG_R0, imm);
    instGen(INS_nop);

    inst_JMP(EJ_jmp, compiler->compCurBB->bbJumpDest);//no branch-delay.
}

//------------------------------------------------------------------------
// genCodeForJumpCompare: Generates code for jmpCompare statement.
//
// A GT_JCMP node is created when a comparison and conditional branch
// can be executed in a single instruction.
//
// MIPS64 has a few instructions with this behavior.
//   - beq/bne -- Compare and branch register equal/not equal
//
// The beq/bne supports the normal +/- 2^15 branch range for conditional branches
//
// A GT_JCMP beq/bne node is created when there is a GT_EQ or GT_NE
// integer/unsigned comparison against the value of Rt register which is used by
// a GT_JTRUE condition jump node.
//
// This node is repsonsible for consuming the register, and emitting the
// appropriate fused compare/test and branch instruction
//
// Two flags guide code generation
//    GTF_JCMP_EQ  -- Set if this is beq rather than bne
//
// Arguments:
//    tree - The GT_JCMP tree node.
//
// Return Value:
//    None
//
void CodeGen::genCodeForJumpCompare(GenTreeOp* tree)
{
    assert(compiler->compCurBB->bbJumpKind == BBJ_COND);

    GenTree* op1 = tree->gtGetOp1();
    GenTree* op2 = tree->gtGetOp2();

    assert(tree->OperIs(GT_JCMP));
    assert(!varTypeIsFloating(tree));
    assert(!op1->isUsedFromMemory());
    assert(!op2->isUsedFromMemory());
    assert(op2->IsCnsIntOrI());
    assert(op2->isContained());

    genConsumeOperands(tree);

    regNumber reg  = op1->gtRegNum;
    emitAttr  attr = emitActualTypeSize(op1->TypeGet());

    /* FIXME for MIPS: */
    //if (tree->gtFlags & GTF_JCMP_TST)
    //{
    //    assert(!"unimplemented on MIPS yet");
    //    //ssize_t compareImm = op2->gtIntCon.IconValue();

    //    //assert(isPow2(compareImm));

    //    //instruction ins = (tree->gtFlags & GTF_JCMP_EQ) ? INS_tbz : INS_tbnz;
    //    //int         imm = genLog2((size_t)compareImm);

    //    //getEmitter()->emitIns_J_R_I(ins, attr, compiler->compCurBB->bbJumpDest, reg, imm);
    //}
    //else
    {
        assert(op2->IsIntegralConst(0));

        instruction ins = (tree->gtFlags & GTF_JCMP_EQ) ? INS_bne : INS_beq;

        ssize_t imm = 7 << 2;//size of Jump-placeholders in bytes liking within emitter::emitIns_J() .
        getEmitter()->emitIns_R_R_I(ins, EA_PTRSIZE, reg, REG_R0, imm);
        instGen(INS_nop);

        inst_JMP(EJ_jmp, compiler->compCurBB->bbJumpDest);//no branch-delay.
    }
}

//---------------------------------------------------------------------
// genSPtoFPdelta - return offset from the stack pointer (Initial-SP) to the frame pointer. The frame pointer
// will point to the saved frame pointer slot (i.e., there will be frame pointer chaining).
//
int CodeGenInterface::genSPtoFPdelta() const
{
    assert(isFramePointerUsed());

    int delta;
    if (IsSaveFpRaWithAllCalleeSavedRegisters())
    {
        delta = (compiler->compCalleeRegsPushed -2)* REGSIZE_BYTES + compiler->compLclFrameSize;
        assert(delta == genTotalFrameSize() - compiler->lvaArgSize - 2*8);
    }
    else
    {
        delta = compiler->lvaOutgoingArgSpaceSize;
    }

    assert(delta >= 0);
    return delta;
}

//---------------------------------------------------------------------
// genTotalFrameSize - return the total size of the stack frame, including local size,
// callee-saved register size, etc.
//
// Return value:
//    Total frame size
//

int CodeGenInterface::genTotalFrameSize() const
{
    // For varargs functions, we home all the incoming register arguments. They are not
    // included in the compCalleeRegsPushed count. This is like prespill on ARM32, but
    // since we don't use "push" instructions to save them, we don't have to do the
    // save of these varargs register arguments as the first thing in the prolog.

    assert(!IsUninitialized(compiler->compCalleeRegsPushed));

    int totalFrameSize = /*(compiler->info.compIsVarArgs ? MAX_REG_ARG * REGSIZE_BYTES : 0) +*/compiler->lvaArgSize +
                         compiler->compCalleeRegsPushed * REGSIZE_BYTES + compiler->compLclFrameSize;

    assert(totalFrameSize > 0);
    return totalFrameSize;
}

//---------------------------------------------------------------------
// genCallerSPtoFPdelta - return the offset from Caller-SP to the frame pointer.
// This number is going to be negative, since the Caller-SP is at a higher
// address than the frame pointer.
//
// There must be a frame pointer to call this function!

int CodeGenInterface::genCallerSPtoFPdelta() const
{
    assert(isFramePointerUsed());
    int callerSPtoFPdelta;

    callerSPtoFPdelta = genCallerSPtoInitialSPdelta() + genSPtoFPdelta();

    assert(callerSPtoFPdelta <= 0);
    return callerSPtoFPdelta;
}

//---------------------------------------------------------------------
// genCallerSPtoInitialSPdelta - return the offset from Caller-SP to Initial SP.
//
// This number will be negative.

int CodeGenInterface::genCallerSPtoInitialSPdelta() const
{
    int callerSPtoSPdelta = 0;

    callerSPtoSPdelta -= genTotalFrameSize();

    assert(callerSPtoSPdelta <= 0);
    return callerSPtoSPdelta;
}

//---------------------------------------------------------------------
// SetSaveFpRaWithAllCalleeSavedRegisters - Set the variable that indicates if FP/RA registers
// are stored with the rest of the callee-saved registers.
void CodeGen::SetSaveFpRaWithAllCalleeSavedRegisters(bool value)
{
    JITDUMP("Setting genSaveFpRaWithAllCalleeSavedRegisters to %s\n", dspBool(value));
    genSaveFpRaWithAllCalleeSavedRegisters = value;
}

//---------------------------------------------------------------------
// IsSaveFpRaWithAllCalleeSavedRegisters - Return the value that indicates where FP/RA registers
// are stored in the prolog.
bool CodeGen::IsSaveFpRaWithAllCalleeSavedRegisters() const
{
    return genSaveFpRaWithAllCalleeSavedRegisters;
}

/*****************************************************************************
 *  Emit a call to a helper function.
 *
 *  NOTE: no branch-delay!!!  delay-slot will be generated with jump together.
 */

void CodeGen::genEmitHelperCall(unsigned helper, int argSize, emitAttr retSize, regNumber callTargetReg /*= REG_NA */)
{
    /* FIXME for MIPS */
    void* addr  = nullptr;
    void* pAddr = nullptr;

    emitter::EmitCallType callType = emitter::EC_FUNC_TOKEN;
    addr                           = compiler->compGetHelperFtn((CorInfoHelpFunc)helper, &pAddr);
    regNumber callTarget           = REG_NA;

    if (addr == nullptr)
    {
        // This is call to a runtime helper.
        // add x, x, [reloc:page offset]
        // ld x, [x]
        // jr x

        if (callTargetReg == REG_NA)
        {
            // If a callTargetReg has not been explicitly provided, we will use REG_DEFAULT_HELPER_CALL_TARGET, but
            // this is only a valid assumption if the helper call is known to kill REG_DEFAULT_HELPER_CALL_TARGET.
            callTargetReg = REG_DEFAULT_HELPER_CALL_TARGET;
        }

        regMaskTP callTargetMask = genRegMask(callTargetReg);
        regMaskTP callKillSet    = compiler->compHelperCallKillSet((CorInfoHelpFunc)helper);

        // assert that all registers in callTargetMask are in the callKillSet
        noway_assert((callTargetMask & callKillSet) == callTargetMask);

        callTarget = callTargetReg;

        instGen_Set_Reg_To_Imm(EA_PTR_DSP_RELOC, callTarget, (ssize_t)pAddr);
        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, callTarget, callTarget, 0);
        callType = emitter::EC_INDIR_R;
    }

    getEmitter()->emitIns_Call(callType, compiler->eeFindHelper(helper), INDEBUG_LDISASM_COMMA(nullptr) addr, argSize,
                               retSize, EA_UNKNOWN, gcInfo.gcVarPtrSetCur, gcInfo.gcRegGCrefSetCur,
                               gcInfo.gcRegByrefSetCur, BAD_IL_OFFSET, /* IL offset */
                               callTarget,                             /* ireg */
                               REG_NA, 0, 0,                           /* xreg, xmul, disp */
                               false                                   /* isJump */
                               );

    regMaskTP killMask = compiler->compHelperCallKillSet((CorInfoHelpFunc)helper);
    regSet.verifyRegistersUsed(killMask);
}

#ifdef FEATURE_SIMD

//------------------------------------------------------------------------
// genSIMDIntrinsic: Generate code for a SIMD Intrinsic.  This is the main
// routine which in turn calls appropriate genSIMDIntrinsicXXX() routine.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
// Notes:
//    Currently, we only recognize SIMDVector<float> and SIMDVector<int>, and
//    a limited set of methods.
//
// TODO-CLEANUP Merge all versions of this function and move to new file simdcodegencommon.cpp.
void CodeGen::genSIMDIntrinsic(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    // NYI for unsupported base types
    if (simdNode->gtSIMDBaseType != TYP_INT && simdNode->gtSIMDBaseType != TYP_LONG &&
        simdNode->gtSIMDBaseType != TYP_FLOAT && simdNode->gtSIMDBaseType != TYP_DOUBLE &&
        simdNode->gtSIMDBaseType != TYP_USHORT && simdNode->gtSIMDBaseType != TYP_UBYTE &&
        simdNode->gtSIMDBaseType != TYP_SHORT && simdNode->gtSIMDBaseType != TYP_BYTE &&
        simdNode->gtSIMDBaseType != TYP_UINT && simdNode->gtSIMDBaseType != TYP_ULONG)
    {
        // We don't need a base type for the Upper Save & Restore intrinsics, and we may find
        // these implemented over lclVars created by CSE without full handle information (and
        // therefore potentially without a base type).
        if ((simdNode->gtSIMDIntrinsicID != SIMDIntrinsicUpperSave) &&
            (simdNode->gtSIMDIntrinsicID != SIMDIntrinsicUpperRestore))
        {
            noway_assert(!"SIMD intrinsic with unsupported base type.");
        }
    }

    switch (simdNode->gtSIMDIntrinsicID)
    {
        case SIMDIntrinsicInit:
            genSIMDIntrinsicInit(simdNode);
            break;

        case SIMDIntrinsicInitN:
            genSIMDIntrinsicInitN(simdNode);
            break;

        case SIMDIntrinsicSqrt:
        case SIMDIntrinsicAbs:
        case SIMDIntrinsicCast:
        case SIMDIntrinsicConvertToSingle:
        case SIMDIntrinsicConvertToInt32:
        case SIMDIntrinsicConvertToDouble:
        case SIMDIntrinsicConvertToInt64:
            genSIMDIntrinsicUnOp(simdNode);
            break;

        case SIMDIntrinsicWidenLo:
        case SIMDIntrinsicWidenHi:
            genSIMDIntrinsicWiden(simdNode);
            break;

        case SIMDIntrinsicNarrow:
            genSIMDIntrinsicNarrow(simdNode);
            break;

        case SIMDIntrinsicAdd:
        case SIMDIntrinsicSub:
        case SIMDIntrinsicMul:
        case SIMDIntrinsicDiv:
        case SIMDIntrinsicBitwiseAnd:
        case SIMDIntrinsicBitwiseAndNot:
        case SIMDIntrinsicBitwiseOr:
        case SIMDIntrinsicBitwiseXor:
        case SIMDIntrinsicMin:
        case SIMDIntrinsicMax:
        case SIMDIntrinsicEqual:
        case SIMDIntrinsicLessThan:
        case SIMDIntrinsicGreaterThan:
        case SIMDIntrinsicLessThanOrEqual:
        case SIMDIntrinsicGreaterThanOrEqual:
            genSIMDIntrinsicBinOp(simdNode);
            break;

        case SIMDIntrinsicOpEquality:
        case SIMDIntrinsicOpInEquality:
            genSIMDIntrinsicRelOp(simdNode);
            break;

        case SIMDIntrinsicDotProduct:
            genSIMDIntrinsicDotProduct(simdNode);
            break;

        case SIMDIntrinsicGetItem:
            genSIMDIntrinsicGetItem(simdNode);
            break;

        case SIMDIntrinsicSetX:
        case SIMDIntrinsicSetY:
        case SIMDIntrinsicSetZ:
        case SIMDIntrinsicSetW:
            genSIMDIntrinsicSetItem(simdNode);
            break;

        case SIMDIntrinsicUpperSave:
            genSIMDIntrinsicUpperSave(simdNode);
            break;

        case SIMDIntrinsicUpperRestore:
            genSIMDIntrinsicUpperRestore(simdNode);
            break;

        case SIMDIntrinsicSelect:
            NYI("SIMDIntrinsicSelect lowered during import to (a & sel) | (b & ~sel)");
            break;

        default:
            noway_assert(!"Unimplemented SIMD intrinsic.");
            unreached();
    }
#endif
}

insOpts CodeGen::genGetSimdInsOpt(emitAttr size, var_types elementType)
{
assert(!"unimplemented on MIPS yet");
    return INS_OPTS_NONE;
#if 0
    assert((size == EA_16BYTE) || (size == EA_8BYTE));
    insOpts result = INS_OPTS_NONE;

    switch (elementType)
    {
        case TYP_DOUBLE:
        case TYP_ULONG:
        case TYP_LONG:
            result = (size == EA_16BYTE) ? INS_OPTS_2D : INS_OPTS_1D;
            break;
        case TYP_FLOAT:
        case TYP_UINT:
        case TYP_INT:
            result = (size == EA_16BYTE) ? INS_OPTS_4S : INS_OPTS_2S;
            break;
        case TYP_USHORT:
        case TYP_SHORT:
            result = (size == EA_16BYTE) ? INS_OPTS_8H : INS_OPTS_4H;
            break;
        case TYP_UBYTE:
        case TYP_BYTE:
            result = (size == EA_16BYTE) ? INS_OPTS_16B : INS_OPTS_8B;
            break;
        default:
            assert(!"Unsupported element type");
            unreached();
    }

    return result;
#endif
}

// getOpForSIMDIntrinsic: return the opcode for the given SIMD Intrinsic
//
// Arguments:
//   intrinsicId    -   SIMD intrinsic Id
//   baseType       -   Base type of the SIMD vector
//   immed          -   Out param. Any immediate byte operand that needs to be passed to SSE2 opcode
//
//
// Return Value:
//   Instruction (op) to be used, and immed is set if instruction requires an immediate operand.
//
instruction CodeGen::getOpForSIMDIntrinsic(SIMDIntrinsicID intrinsicId, var_types baseType, unsigned* ival /*=nullptr*/)
{
assert(!"unimplemented on MIPS yet");
    return INS_invalid;
#if 0
    instruction result = INS_invalid;
    if (varTypeIsFloating(baseType))
    {
        switch (intrinsicId)
        {
            case SIMDIntrinsicAbs:
                result = INS_fabs;
                break;
            case SIMDIntrinsicAdd:
                result = INS_fadd;
                break;
            case SIMDIntrinsicBitwiseAnd:
                result = INS_and;
                break;
            case SIMDIntrinsicBitwiseAndNot:
                result = INS_bic;
                break;
            case SIMDIntrinsicBitwiseOr:
                result = INS_orr;
                break;
            case SIMDIntrinsicBitwiseXor:
                result = INS_eor;
                break;
            case SIMDIntrinsicCast:
                result = INS_mov;
                break;
            case SIMDIntrinsicConvertToInt32:
            case SIMDIntrinsicConvertToInt64:
                result = INS_fcvtns;
                break;
            case SIMDIntrinsicDiv:
                result = INS_fdiv;
                break;
            case SIMDIntrinsicEqual:
                result = INS_fcmeq;
                break;
            case SIMDIntrinsicGreaterThan:
                result = INS_fcmgt;
                break;
            case SIMDIntrinsicGreaterThanOrEqual:
                result = INS_fcmge;
                break;
            case SIMDIntrinsicLessThan:
                result = INS_fcmlt;
                break;
            case SIMDIntrinsicLessThanOrEqual:
                result = INS_fcmle;
                break;
            case SIMDIntrinsicMax:
                result = INS_fmax;
                break;
            case SIMDIntrinsicMin:
                result = INS_fmin;
                break;
            case SIMDIntrinsicMul:
                result = INS_fmul;
                break;
            case SIMDIntrinsicNarrow:
                // Use INS_fcvtn lower bytes of result followed by INS_fcvtn2 for upper bytes
                // Return lower bytes instruction here
                result = INS_fcvtn;
                break;
            case SIMDIntrinsicSelect:
                result = INS_bsl;
                break;
            case SIMDIntrinsicSqrt:
                result = INS_fsqrt;
                break;
            case SIMDIntrinsicSub:
                result = INS_fsub;
                break;
            case SIMDIntrinsicWidenLo:
                result = INS_fcvtl;
                break;
            case SIMDIntrinsicWidenHi:
                result = INS_fcvtl2;
                break;
            default:
                assert(!"Unsupported SIMD intrinsic");
                unreached();
        }
    }
    else
    {
        bool isUnsigned = varTypeIsUnsigned(baseType);

        switch (intrinsicId)
        {
            case SIMDIntrinsicAbs:
                assert(!isUnsigned);
                result = INS_abs;
                break;
            case SIMDIntrinsicAdd:
                result = INS_add;
                break;
            case SIMDIntrinsicBitwiseAnd:
                result = INS_and;
                break;
            case SIMDIntrinsicBitwiseAndNot:
                result = INS_bic;
                break;
            case SIMDIntrinsicBitwiseOr:
                result = INS_orr;
                break;
            case SIMDIntrinsicBitwiseXor:
                result = INS_eor;
                break;
            case SIMDIntrinsicCast:
                result = INS_mov;
                break;
            case SIMDIntrinsicConvertToDouble:
            case SIMDIntrinsicConvertToSingle:
                result = isUnsigned ? INS_ucvtf : INS_scvtf;
                break;
            case SIMDIntrinsicEqual:
                result = INS_cmeq;
                break;
            case SIMDIntrinsicGreaterThan:
                result = isUnsigned ? INS_cmhi : INS_cmgt;
                break;
            case SIMDIntrinsicGreaterThanOrEqual:
                result = isUnsigned ? INS_cmhs : INS_cmge;
                break;
            case SIMDIntrinsicLessThan:
                assert(!isUnsigned);
                result = INS_cmlt;
                break;
            case SIMDIntrinsicLessThanOrEqual:
                assert(!isUnsigned);
                result = INS_cmle;
                break;
            case SIMDIntrinsicMax:
                result = isUnsigned ? INS_umax : INS_smax;
                break;
            case SIMDIntrinsicMin:
                result = isUnsigned ? INS_umin : INS_smin;
                break;
            case SIMDIntrinsicMul:
                result = INS_mul;
                break;
            case SIMDIntrinsicNarrow:
                // Use INS_xtn lower bytes of result followed by INS_xtn2 for upper bytes
                // Return lower bytes instruction here
                result = INS_xtn;
                break;
            case SIMDIntrinsicSelect:
                result = INS_bsl;
                break;
            case SIMDIntrinsicSub:
                result = INS_sub;
                break;
            case SIMDIntrinsicWidenLo:
                result = isUnsigned ? INS_uxtl : INS_sxtl;
                break;
            case SIMDIntrinsicWidenHi:
                result = isUnsigned ? INS_uxtl2 : INS_sxtl2;
                break;
            default:
                assert(!"Unsupported SIMD intrinsic");
                unreached();
        }
    }

    noway_assert(result != INS_invalid);
    return result;
#endif
}

//------------------------------------------------------------------------
// genSIMDIntrinsicInit: Generate code for SIMD Intrinsic Initialize.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicInit(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicInit);

    GenTree*  op1       = simdNode->gtGetOp1();
    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types targetType = simdNode->TypeGet();

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->IsIntegralConst(0) ? REG_R0 : op1->gtRegNum;

    // TODO-MIPS64-CQ Add LD1R to allow SIMDIntrinsicInit from contained memory
    // TODO-MIPS64-CQ Add MOVI to allow SIMDIntrinsicInit from contained immediate small constants

    assert(op1->isContained() == op1->IsIntegralConst(0));
    assert(!op1->isUsedFromMemory());

    assert(genIsValidFloatReg(targetReg));
    assert(genIsValidIntReg(op1Reg) || genIsValidFloatReg(op1Reg));

    emitAttr attr = (simdNode->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts  opt  = genGetSimdInsOpt(attr, baseType);

    if (genIsValidIntReg(op1Reg))
    {
        getEmitter()->emitIns_R_R(INS_dup, attr, targetReg, op1Reg, opt);
    }
    else
    {
        getEmitter()->emitIns_R_R_I(INS_dup, attr, targetReg, op1Reg, 0, opt);
    }

    genProduceReg(simdNode);
#endif
}

//-------------------------------------------------------------------------------------------
// genSIMDIntrinsicInitN: Generate code for SIMD Intrinsic Initialize for the form that takes
//                        a number of arguments equal to the length of the Vector.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicInitN(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicInitN);

    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);

    var_types targetType = simdNode->TypeGet();

    var_types baseType = simdNode->gtSIMDBaseType;

    regNumber vectorReg = targetReg;

    if (varTypeIsFloating(baseType))
    {
        // Note that we cannot use targetReg before consuming all float source operands.
        // Therefore use an internal temp register
        vectorReg = simdNode->GetSingleTempReg(RBM_ALLFLOAT);
    }

    emitAttr baseTypeSize = emitTypeSize(baseType);

    // We will first consume the list items in execution (left to right) order,
    // and record the registers.
    regNumber operandRegs[FP_REGSIZE_BYTES];
    unsigned  initCount = 0;
    for (GenTree* list = simdNode->gtGetOp1(); list != nullptr; list = list->gtGetOp2())
    {
        assert(list->OperGet() == GT_LIST);
        GenTree* listItem = list->gtGetOp1();
        assert(listItem->TypeGet() == baseType);
        assert(!listItem->isContained());
        regNumber operandReg   = genConsumeReg(listItem);
        operandRegs[initCount] = operandReg;
        initCount++;
    }

    assert((initCount * baseTypeSize) <= simdNode->gtSIMDSize);

    if (initCount * baseTypeSize < EA_16BYTE)
    {
        getEmitter()->emitIns_R_I(INS_movi, EA_16BYTE, vectorReg, 0x00, INS_OPTS_16B);
    }

    if (varTypeIsIntegral(baseType))
    {
        for (unsigned i = 0; i < initCount; i++)
        {
            getEmitter()->emitIns_R_R_I(INS_ins, baseTypeSize, vectorReg, operandRegs[i], i);
        }
    }
    else
    {
        for (unsigned i = 0; i < initCount; i++)
        {
            getEmitter()->emitIns_R_R_I_I(INS_ins, baseTypeSize, vectorReg, operandRegs[i], i, 0);
        }
    }

    // Load the initialized value.
    if (targetReg != vectorReg)
    {
        getEmitter()->emitIns_R_R(INS_mov, EA_16BYTE, targetReg, vectorReg);
    }

    genProduceReg(simdNode);
#endif
}

//----------------------------------------------------------------------------------
// genSIMDIntrinsicUnOp: Generate code for SIMD Intrinsic unary operations like sqrt.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicUnOp(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicSqrt || simdNode->gtSIMDIntrinsicID == SIMDIntrinsicCast ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicAbs ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicConvertToSingle ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicConvertToInt32 ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicConvertToDouble ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicConvertToInt64);

    GenTree*  op1       = simdNode->gtGetOp1();
    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types targetType = simdNode->TypeGet();

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(targetReg));

    instruction ins  = getOpForSIMDIntrinsic(simdNode->gtSIMDIntrinsicID, baseType);
    emitAttr    attr = (simdNode->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = (ins == INS_mov) ? INS_OPTS_NONE : genGetSimdInsOpt(attr, baseType);

    getEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg, opt);

    genProduceReg(simdNode);
#endif
}

//--------------------------------------------------------------------------------
// genSIMDIntrinsicWiden: Generate code for SIMD Intrinsic Widen operations
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Notes:
//    The Widen intrinsics are broken into separate intrinsics for the two results.
//
void CodeGen::genSIMDIntrinsicWiden(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert((simdNode->gtSIMDIntrinsicID == SIMDIntrinsicWidenLo) ||
           (simdNode->gtSIMDIntrinsicID == SIMDIntrinsicWidenHi));

    GenTree*  op1       = simdNode->gtGetOp1();
    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types simdType = simdNode->TypeGet();

    genConsumeOperands(simdNode);
    regNumber op1Reg   = op1->gtRegNum;
    regNumber srcReg   = op1Reg;
    emitAttr  emitSize = emitActualTypeSize(simdType);

    instruction ins = getOpForSIMDIntrinsic(simdNode->gtSIMDIntrinsicID, baseType);

    if (varTypeIsFloating(baseType))
    {
        getEmitter()->emitIns_R_R(ins, EA_8BYTE, targetReg, op1Reg);
    }
    else
    {
        emitAttr attr = (simdNode->gtSIMDIntrinsicID == SIMDIntrinsicWidenHi) ? EA_16BYTE : EA_8BYTE;
        insOpts  opt  = genGetSimdInsOpt(attr, baseType);

        getEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg, opt);
    }

    genProduceReg(simdNode);
#endif
}

//--------------------------------------------------------------------------------
// genSIMDIntrinsicNarrow: Generate code for SIMD Intrinsic Narrow operations
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Notes:
//    This intrinsic takes two arguments. The first operand is narrowed to produce the
//    lower elements of the results, and the second operand produces the high elements.
//
void CodeGen::genSIMDIntrinsicNarrow(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicNarrow);

    GenTree*  op1       = simdNode->gtGetOp1();
    GenTree*  op2       = simdNode->gtGetOp2();
    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types simdType = simdNode->TypeGet();
    emitAttr  emitSize = emitTypeSize(simdType);

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(targetReg));
    assert(op2Reg != targetReg);
    assert(simdNode->gtSIMDSize == 16);

    instruction ins = getOpForSIMDIntrinsic(simdNode->gtSIMDIntrinsicID, baseType);
    assert((ins == INS_fcvtn) || (ins == INS_xtn));

    if (ins == INS_fcvtn)
    {
        getEmitter()->emitIns_R_R(INS_fcvtn, EA_8BYTE, targetReg, op1Reg);
        getEmitter()->emitIns_R_R(INS_fcvtn2, EA_8BYTE, targetReg, op2Reg);
    }
    else
    {
        insOpts opt  = INS_OPTS_NONE;
        insOpts opt2 = INS_OPTS_NONE;

        // This is not the same as genGetSimdInsOpt()
        // Basetype is the soure operand type
        // However encoding is based on the destination operand type which is 1/2 the basetype.
        switch (baseType)
        {
            case TYP_ULONG:
            case TYP_LONG:
                opt  = INS_OPTS_2S;
                opt2 = INS_OPTS_4S;
                break;
            case TYP_UINT:
            case TYP_INT:
                opt  = INS_OPTS_4H;
                opt2 = INS_OPTS_8H;
                break;
            case TYP_USHORT:
            case TYP_SHORT:
                opt  = INS_OPTS_8B;
                opt2 = INS_OPTS_16B;
                break;
            default:
                assert(!"Unsupported narrowing element type");
                unreached();
        }
        getEmitter()->emitIns_R_R(INS_xtn, EA_8BYTE, targetReg, op1Reg, opt);
        getEmitter()->emitIns_R_R(INS_xtn2, EA_16BYTE, targetReg, op2Reg, opt2);
    }

    genProduceReg(simdNode);
#endif
}

//--------------------------------------------------------------------------------
// genSIMDIntrinsicBinOp: Generate code for SIMD Intrinsic binary operations
// add, sub, mul, bit-wise And, AndNot and Or.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicBinOp(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicAdd || simdNode->gtSIMDIntrinsicID == SIMDIntrinsicSub ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicMul || simdNode->gtSIMDIntrinsicID == SIMDIntrinsicDiv ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicBitwiseAnd ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicBitwiseAndNot ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicBitwiseOr ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicBitwiseXor || simdNode->gtSIMDIntrinsicID == SIMDIntrinsicMin ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicMax || simdNode->gtSIMDIntrinsicID == SIMDIntrinsicEqual ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicLessThan ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicGreaterThan ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicLessThanOrEqual ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicGreaterThanOrEqual);

    GenTree*  op1       = simdNode->gtGetOp1();
    GenTree*  op2       = simdNode->gtGetOp2();
    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types targetType = simdNode->TypeGet();

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(targetReg));

    // TODO-MIPS64-CQ Contain integer constants where posible

    instruction ins  = getOpForSIMDIntrinsic(simdNode->gtSIMDIntrinsicID, baseType);
    emitAttr    attr = (simdNode->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    getEmitter()->emitIns_R_R_R(ins, attr, targetReg, op1Reg, op2Reg, opt);

    genProduceReg(simdNode);
#endif
}

//--------------------------------------------------------------------------------
// genSIMDIntrinsicRelOp: Generate code for a SIMD Intrinsic relational operater
// == and !=
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicRelOp(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicOpEquality ||
           simdNode->gtSIMDIntrinsicID == SIMDIntrinsicOpInEquality);

    GenTree*  op1        = simdNode->gtGetOp1();
    GenTree*  op2        = simdNode->gtGetOp2();
    var_types baseType   = simdNode->gtSIMDBaseType;
    regNumber targetReg  = simdNode->gtRegNum;
    var_types targetType = simdNode->TypeGet();

    genConsumeOperands(simdNode);
    regNumber op1Reg   = op1->gtRegNum;
    regNumber op2Reg   = op2->gtRegNum;
    regNumber otherReg = op2Reg;

    instruction ins  = getOpForSIMDIntrinsic(SIMDIntrinsicEqual, baseType);
    emitAttr    attr = (simdNode->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    // TODO-MIPS64-CQ Contain integer constants where possible

    regNumber tmpFloatReg = simdNode->GetSingleTempReg(RBM_ALLFLOAT);

    getEmitter()->emitIns_R_R_R(ins, attr, tmpFloatReg, op1Reg, op2Reg, opt);

    if ((simdNode->gtFlags & GTF_SIMD12_OP) != 0)
    {
        // For 12Byte vectors we must set upper bits to get correct comparison
        // We do not assume upper bits are zero.
        instGen_Set_Reg_To_Imm(EA_4BYTE, targetReg, -1);
        getEmitter()->emitIns_R_R_I(INS_ins, EA_4BYTE, tmpFloatReg, targetReg, 3);
    }

    getEmitter()->emitIns_R_R(INS_uminv, attr, tmpFloatReg, tmpFloatReg,
                              (simdNode->gtSIMDSize > 8) ? INS_OPTS_16B : INS_OPTS_8B);

    getEmitter()->emitIns_R_R_I(INS_mov, EA_1BYTE, targetReg, tmpFloatReg, 0);

    if (simdNode->gtSIMDIntrinsicID == SIMDIntrinsicOpInEquality)
    {
        getEmitter()->emitIns_R_R_I(INS_eor, EA_4BYTE, targetReg, targetReg, 0x1);
    }

    getEmitter()->emitIns_R_R_I(INS_and, EA_4BYTE, targetReg, targetReg, 0x1);

    genProduceReg(simdNode);
#endif
}

//--------------------------------------------------------------------------------
// genSIMDIntrinsicDotProduct: Generate code for SIMD Intrinsic Dot Product.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicDotProduct(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicDotProduct);

    GenTree*  op1      = simdNode->gtGetOp1();
    GenTree*  op2      = simdNode->gtGetOp2();
    var_types baseType = simdNode->gtSIMDBaseType;
    var_types simdType = op1->TypeGet();

    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);

    var_types targetType = simdNode->TypeGet();
    assert(targetType == baseType);

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;
    regNumber tmpReg = targetReg;

    if (!varTypeIsFloating(baseType))
    {
        tmpReg = simdNode->GetSingleTempReg(RBM_ALLFLOAT);
    }

    instruction ins  = getOpForSIMDIntrinsic(SIMDIntrinsicMul, baseType);
    emitAttr    attr = (simdNode->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    // Vector multiply
    getEmitter()->emitIns_R_R_R(ins, attr, tmpReg, op1Reg, op2Reg, opt);

    if ((simdNode->gtFlags & GTF_SIMD12_OP) != 0)
    {
        // For 12Byte vectors we must zero upper bits to get correct dot product
        // We do not assume upper bits are zero.
        getEmitter()->emitIns_R_R_I(INS_ins, EA_4BYTE, tmpReg, REG_R0, 3);
    }

    // Vector add horizontal
    if (varTypeIsFloating(baseType))
    {
        if (baseType == TYP_FLOAT)
        {
            if (opt == INS_OPTS_4S)
            {
                getEmitter()->emitIns_R_R_R(INS_faddp, attr, tmpReg, tmpReg, tmpReg, INS_OPTS_4S);
            }
            getEmitter()->emitIns_R_R(INS_faddp, EA_4BYTE, targetReg, tmpReg);
        }
        else
        {
            getEmitter()->emitIns_R_R(INS_faddp, EA_8BYTE, targetReg, tmpReg);
        }
    }
    else
    {
        ins = varTypeIsUnsigned(baseType) ? INS_uaddlv : INS_saddlv;

        getEmitter()->emitIns_R_R(ins, attr, tmpReg, tmpReg, opt);

        // Mov to integer register
        if (varTypeIsUnsigned(baseType) || (genTypeSize(baseType) < 4))
        {
            getEmitter()->emitIns_R_R_I(INS_mov, emitTypeSize(baseType), targetReg, tmpReg, 0);
        }
        else
        {
            getEmitter()->emitIns_R_R_I(INS_smov, emitActualTypeSize(baseType), targetReg, tmpReg, 0);
        }
    }

    genProduceReg(simdNode);
#endif
}

//------------------------------------------------------------------------------------
// genSIMDIntrinsicGetItem: Generate code for SIMD Intrinsic get element at index i.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicGetItem(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicGetItem);

    GenTree*  op1      = simdNode->gtGetOp1();
    GenTree*  op2      = simdNode->gtGetOp2();
    var_types simdType = op1->TypeGet();
    assert(varTypeIsSIMD(simdType));

    // op1 of TYP_SIMD12 should be considered as TYP_SIMD16
    if (simdType == TYP_SIMD12)
    {
        simdType = TYP_SIMD16;
    }

    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types targetType = simdNode->TypeGet();
    assert(targetType == genActualType(baseType));

    // GetItem has 2 operands:
    // - the source of SIMD type (op1)
    // - the index of the value to be returned.
    genConsumeOperands(simdNode);

    emitAttr baseTypeSize  = emitTypeSize(baseType);
    unsigned baseTypeScale = genLog2(EA_SIZE_IN_BYTES(baseTypeSize));

    if (op2->IsCnsIntOrI())
    {
        assert(op2->isContained());

        ssize_t index = op2->gtIntCon.gtIconVal;

        // We only need to generate code for the get if the index is valid
        // If the index is invalid, previously generated for the range check will throw
        if (getEmitter()->isValidVectorIndex(emitTypeSize(simdType), baseTypeSize, index))
        {
            if (op1->isContained())
            {
                int         offset = (int)index * genTypeSize(baseType);
                instruction ins    = ins_Load(baseType);

                assert(!op1->isUsedFromReg());

                if (op1->OperIsLocal())
                {
                    unsigned varNum = op1->gtLclVarCommon.gtLclNum;

                    getEmitter()->emitIns_R_S(ins, emitActualTypeSize(baseType), targetReg, varNum, offset);
                }
                else
                {
                    assert(op1->OperGet() == GT_IND);

                    GenTree* addr = op1->AsIndir()->Addr();
                    assert(!addr->isContained());
                    regNumber baseReg = addr->gtRegNum;

                    // ldr targetReg, [baseReg, #offset]
                    getEmitter()->emitIns_R_R_I(ins, emitActualTypeSize(baseType), targetReg, baseReg, offset);
                }
            }
            else
            {
                assert(op1->isUsedFromReg());
                regNumber srcReg = op1->gtRegNum;

                instruction ins;
                if (varTypeIsFloating(baseType))
                {
                    assert(genIsValidFloatReg(targetReg));
                    // dup targetReg, srcReg[#index]
                    ins = INS_dup;
                }
                else
                {
                    assert(genIsValidIntReg(targetReg));
                    if (varTypeIsUnsigned(baseType) || (baseTypeSize == EA_8BYTE))
                    {
                        // umov targetReg, srcReg[#index]
                        ins = INS_umov;
                    }
                    else
                    {
                        // smov targetReg, srcReg[#index]
                        ins = INS_smov;
                    }
                }
                getEmitter()->emitIns_R_R_I(ins, baseTypeSize, targetReg, srcReg, index);
            }
        }
    }
    else
    {
        assert(!op2->isContained());

        regNumber baseReg  = REG_NA;
        regNumber indexReg = op2->gtRegNum;

        if (op1->isContained())
        {
            // Optimize the case of op1 is in memory and trying to access ith element.
            assert(!op1->isUsedFromReg());
            if (op1->OperIsLocal())
            {
                unsigned varNum = op1->gtLclVarCommon.gtLclNum;

                baseReg = simdNode->ExtractTempReg();

                // Load the address of varNum
                getEmitter()->emitIns_R_S(INS_lea, EA_PTRSIZE, baseReg, varNum, 0);
            }
            else
            {
                // Require GT_IND addr to be not contained.
                assert(op1->OperGet() == GT_IND);

                GenTree* addr = op1->AsIndir()->Addr();
                assert(!addr->isContained());

                baseReg = addr->gtRegNum;
            }
        }
        else
        {
            assert(op1->isUsedFromReg());
            regNumber srcReg = op1->gtRegNum;

            unsigned simdInitTempVarNum = compiler->lvaSIMDInitTempVarNum;
            noway_assert(compiler->lvaSIMDInitTempVarNum != BAD_VAR_NUM);

            baseReg = simdNode->ExtractTempReg();

            // Load the address of simdInitTempVarNum
            getEmitter()->emitIns_R_S(INS_lea, EA_PTRSIZE, baseReg, simdInitTempVarNum, 0);

            // Store the vector to simdInitTempVarNum
            getEmitter()->emitIns_R_R(INS_str, emitTypeSize(simdType), srcReg, baseReg);
        }

        assert(genIsValidIntReg(indexReg));
        assert(genIsValidIntReg(baseReg));
        assert(baseReg != indexReg);

        // Load item at baseReg[index]
        getEmitter()->emitIns_R_R_R_Ext(ins_Load(baseType), baseTypeSize, targetReg, baseReg, indexReg, INS_OPTS_LSL,
                                        baseTypeScale);
    }

    genProduceReg(simdNode);
#endif
}

//------------------------------------------------------------------------------------
// genSIMDIntrinsicSetItem: Generate code for SIMD Intrinsic set element at index i.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
void CodeGen::genSIMDIntrinsicSetItem(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    // Determine index based on intrinsic ID
    int index = -1;
    switch (simdNode->gtSIMDIntrinsicID)
    {
        case SIMDIntrinsicSetX:
            index = 0;
            break;
        case SIMDIntrinsicSetY:
            index = 1;
            break;
        case SIMDIntrinsicSetZ:
            index = 2;
            break;
        case SIMDIntrinsicSetW:
            index = 3;
            break;

        default:
            unreached();
    }
    assert(index != -1);

    // op1 is the SIMD vector
    // op2 is the value to be set
    GenTree* op1 = simdNode->gtGetOp1();
    GenTree* op2 = simdNode->gtGetOp2();

    var_types baseType  = simdNode->gtSIMDBaseType;
    regNumber targetReg = simdNode->gtRegNum;
    assert(targetReg != REG_NA);
    var_types targetType = simdNode->TypeGet();
    assert(varTypeIsSIMD(targetType));

    assert(op2->TypeGet() == baseType);
    assert(simdNode->gtSIMDSize >= ((index + 1) * genTypeSize(baseType)));

    genConsumeOperands(simdNode);
    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;

    assert(genIsValidFloatReg(targetReg));
    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidIntReg(op2Reg) || genIsValidFloatReg(op2Reg));
    assert(targetReg != op2Reg);

    emitAttr attr = emitTypeSize(baseType);

    // Insert mov if register assignment requires it
    getEmitter()->emitIns_R_R(INS_mov, EA_16BYTE, targetReg, op1Reg);

    if (genIsValidIntReg(op2Reg))
    {
        getEmitter()->emitIns_R_R_I(INS_ins, attr, targetReg, op2Reg, index);
    }
    else
    {
        getEmitter()->emitIns_R_R_I_I(INS_ins, attr, targetReg, op2Reg, index, 0);
    }

    genProduceReg(simdNode);
#endif
}

//-----------------------------------------------------------------------------
// genSIMDIntrinsicUpperSave: save the upper half of a TYP_SIMD16 vector to
//                            the given register, if any, or to memory.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
// Notes:
//    The upper half of all SIMD registers are volatile, even the callee-save registers.
//    When a 16-byte SIMD value is live across a call, the register allocator will use this intrinsic
//    to cause the upper half to be saved.  It will first attempt to find another, unused, callee-save
//    register.  If such a register cannot be found, it will save it to an available caller-save register.
//    In that case, this node will be marked GTF_SPILL, which will cause this method to save
//    the upper half to the lclVar's home location.
//
void CodeGen::genSIMDIntrinsicUpperSave(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicUpperSave);

    GenTree* op1 = simdNode->gtGetOp1();
    assert(op1->IsLocal());
    assert(emitTypeSize(op1->TypeGet()) == 16);
    regNumber targetReg = simdNode->gtRegNum;
    regNumber op1Reg    = genConsumeReg(op1);
    assert(op1Reg != REG_NA);
    assert(targetReg != REG_NA);
    getEmitter()->emitIns_R_R_I_I(INS_mov, EA_8BYTE, targetReg, op1Reg, 0, 1);

    if ((simdNode->gtFlags & GTF_SPILL) != 0)
    {
        // This is not a normal spill; we'll spill it to the lclVar location.
        // The localVar must have a stack home.
        unsigned   varNum = op1->AsLclVarCommon()->gtLclNum;
        LclVarDsc* varDsc = compiler->lvaGetDesc(varNum);
        assert(varDsc->lvOnFrame);
        // We want to store this to the upper 8 bytes of this localVar's home.
        int offset = 8;

        emitAttr attr = emitTypeSize(TYP_SIMD8);
        getEmitter()->emitIns_S_R(INS_str, attr, targetReg, varNum, offset);
    }
    else
    {
        genProduceReg(simdNode);
    }
#endif
}

//-----------------------------------------------------------------------------
// genSIMDIntrinsicUpperRestore: Restore the upper half of a TYP_SIMD16 vector to
//                               the given register, if any, or to memory.
//
// Arguments:
//    simdNode - The GT_SIMD node
//
// Return Value:
//    None.
//
// Notes:
//    For consistency with genSIMDIntrinsicUpperSave, and to ensure that lclVar nodes always
//    have their home register, this node has its targetReg on the lclVar child, and its source
//    on the simdNode.
//    Regarding spill, please see the note above on genSIMDIntrinsicUpperSave.  If we have spilled
//    an upper-half to the lclVar's home location, this node will be marked GTF_SPILLED.
//
void CodeGen::genSIMDIntrinsicUpperRestore(GenTreeSIMD* simdNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(simdNode->gtSIMDIntrinsicID == SIMDIntrinsicUpperRestore);

    GenTree* op1 = simdNode->gtGetOp1();
    assert(op1->IsLocal());
    assert(emitTypeSize(op1->TypeGet()) == 16);
    regNumber srcReg    = simdNode->gtRegNum;
    regNumber lclVarReg = genConsumeReg(op1);
    unsigned  varNum    = op1->AsLclVarCommon()->gtLclNum;
    assert(lclVarReg != REG_NA);
    assert(srcReg != REG_NA);
    if (simdNode->gtFlags & GTF_SPILLED)
    {
        // The localVar must have a stack home.
        LclVarDsc* varDsc = compiler->lvaGetDesc(varNum);
        assert(varDsc->lvOnFrame);
        // We will load this from the upper 8 bytes of this localVar's home.
        int offset = 8;

        emitAttr attr = emitTypeSize(TYP_SIMD8);
        getEmitter()->emitIns_R_S(INS_ldr, attr, srcReg, varNum, offset);
    }
    getEmitter()->emitIns_R_R_I_I(INS_mov, EA_8BYTE, lclVarReg, srcReg, 1, 0);
#endif
}

//-----------------------------------------------------------------------------
// genStoreIndTypeSIMD12: store indirect a TYP_SIMD12 (i.e. Vector3) to memory.
// Since Vector3 is not a hardware supported write size, it is performed
// as two writes: 8 byte followed by 4-byte.
//
// Arguments:
//    treeNode - tree node that is attempting to store indirect
//
//
// Return Value:
//    None.
//
void CodeGen::genStoreIndTypeSIMD12(GenTree* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(treeNode->OperGet() == GT_STOREIND);

    GenTree* addr = treeNode->gtOp.gtOp1;
    GenTree* data = treeNode->gtOp.gtOp2;

    // addr and data should not be contained.
    assert(!data->isContained());
    assert(!addr->isContained());

#ifdef DEBUG
    // Should not require a write barrier
    GCInfo::WriteBarrierForm writeBarrierForm = gcInfo.gcIsWriteBarrierCandidate(treeNode, data);
    assert(writeBarrierForm == GCInfo::WBF_NoBarrier);
#endif

    genConsumeOperands(treeNode->AsOp());

    // Need an addtional integer register to extract upper 4 bytes from data.
    regNumber tmpReg = treeNode->GetSingleTempReg();
    assert(tmpReg != addr->gtRegNum);

    // 8-byte write
    getEmitter()->emitIns_R_R(INS_str, EA_8BYTE, data->gtRegNum, addr->gtRegNum);

    // Extract upper 4-bytes from data
    getEmitter()->emitIns_R_R_I(INS_mov, EA_4BYTE, tmpReg, data->gtRegNum, 2);

    // 4-byte write
    getEmitter()->emitIns_R_R_I(INS_str, EA_4BYTE, tmpReg, addr->gtRegNum, 8);
#endif
}

//-----------------------------------------------------------------------------
// genLoadIndTypeSIMD12: load indirect a TYP_SIMD12 (i.e. Vector3) value.
// Since Vector3 is not a hardware supported write size, it is performed
// as two loads: 8 byte followed by 4-byte.
//
// Arguments:
//    treeNode - tree node of GT_IND
//
//
// Return Value:
//    None.
//
void CodeGen::genLoadIndTypeSIMD12(GenTree* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(treeNode->OperGet() == GT_IND);

    GenTree*  addr      = treeNode->gtOp.gtOp1;
    regNumber targetReg = treeNode->gtRegNum;

    assert(!addr->isContained());

    regNumber operandReg = genConsumeReg(addr);

    // Need an addtional int register to read upper 4 bytes, which is different from targetReg
    regNumber tmpReg = treeNode->GetSingleTempReg();

    // 8-byte read
    getEmitter()->emitIns_R_R(INS_ldr, EA_8BYTE, targetReg, addr->gtRegNum);

    // 4-byte read
    getEmitter()->emitIns_R_R_I(INS_ldr, EA_4BYTE, tmpReg, addr->gtRegNum, 8);

    // Insert upper 4-bytes into data
    getEmitter()->emitIns_R_R_I(INS_mov, EA_4BYTE, targetReg, tmpReg, 2);

    genProduceReg(treeNode);
#endif
}

//-----------------------------------------------------------------------------
// genStoreLclTypeSIMD12: store a TYP_SIMD12 (i.e. Vector3) type field.
// Since Vector3 is not a hardware supported write size, it is performed
// as two stores: 8 byte followed by 4-byte.
//
// Arguments:
//    treeNode - tree node that is attempting to store TYP_SIMD12 field
//
// Return Value:
//    None.
//
void CodeGen::genStoreLclTypeSIMD12(GenTree* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert((treeNode->OperGet() == GT_STORE_LCL_FLD) || (treeNode->OperGet() == GT_STORE_LCL_VAR));

    unsigned offs   = 0;
    unsigned varNum = treeNode->gtLclVarCommon.gtLclNum;
    assert(varNum < compiler->lvaCount);

    if (treeNode->OperGet() == GT_STORE_LCL_FLD)
    {
        offs = treeNode->gtLclFld.gtLclOffs;
    }

    GenTree* op1 = treeNode->gtOp.gtOp1;
    assert(!op1->isContained());
    regNumber operandReg = genConsumeReg(op1);

    // Need an addtional integer register to extract upper 4 bytes from data.
    regNumber tmpReg = treeNode->GetSingleTempReg();

    // store lower 8 bytes
    getEmitter()->emitIns_S_R(INS_str, EA_8BYTE, operandReg, varNum, offs);

    // Extract upper 4-bytes from data
    getEmitter()->emitIns_R_R_I(INS_mov, EA_4BYTE, tmpReg, operandReg, 2);

    // 4-byte write
    getEmitter()->emitIns_S_R(INS_str, EA_4BYTE, tmpReg, varNum, offs + 8);
#endif
}

#endif // FEATURE_SIMD

#ifdef FEATURE_HW_INTRINSICS
#include "hwintrinsic.h"

instruction CodeGen::getOpForHWIntrinsic(GenTreeHWIntrinsic* node, var_types instrType)
{
assert(!"unimplemented on MIPS yet");
    return INS_invalid;
#if 0
    NamedIntrinsic intrinsicID = node->gtHWIntrinsicId;

    unsigned int instrTypeIndex = varTypeIsFloating(instrType) ? 0 : varTypeIsUnsigned(instrType) ? 2 : 1;

    instruction ins = HWIntrinsicInfo::lookup(intrinsicID).instrs[instrTypeIndex];
    assert(ins != INS_invalid);

    return ins;
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsic: Produce code for a GT_HWIntrinsic node.
//
// This is the main routine which in turn calls the genHWIntrinsicXXX() routines.
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsic(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NamedIntrinsic intrinsicID = node->gtHWIntrinsicId;

    switch (HWIntrinsicInfo::lookup(intrinsicID).form)
    {
        case HWIntrinsicInfo::UnaryOp:
            genHWIntrinsicUnaryOp(node);
            break;
        case HWIntrinsicInfo::CrcOp:
            genHWIntrinsicCrcOp(node);
            break;
        case HWIntrinsicInfo::SimdBinaryOp:
            genHWIntrinsicSimdBinaryOp(node);
            break;
        case HWIntrinsicInfo::SimdExtractOp:
            genHWIntrinsicSimdExtractOp(node);
            break;
        case HWIntrinsicInfo::SimdInsertOp:
            genHWIntrinsicSimdInsertOp(node);
            break;
        case HWIntrinsicInfo::SimdSelectOp:
            genHWIntrinsicSimdSelectOp(node);
            break;
        case HWIntrinsicInfo::SimdSetAllOp:
            genHWIntrinsicSimdSetAllOp(node);
            break;
        case HWIntrinsicInfo::SimdUnaryOp:
            genHWIntrinsicSimdUnaryOp(node);
            break;
        case HWIntrinsicInfo::SimdBinaryRMWOp:
            genHWIntrinsicSimdBinaryRMWOp(node);
            break;
        case HWIntrinsicInfo::SimdTernaryRMWOp:
            genHWIntrinsicSimdTernaryRMWOp(node);
            break;
        case HWIntrinsicInfo::Sha1HashOp:
            genHWIntrinsicShaHashOp(node);
            break;
        case HWIntrinsicInfo::Sha1RotateOp:
            genHWIntrinsicShaRotateOp(node);
            break;

        default:
            NYI("HWIntrinsic form not implemented");
    }
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicUnaryOp:
//
// Produce code for a GT_HWIntrinsic node with form UnaryOp.
//
// Consumes one scalar operand produces a scalar
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicUnaryOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    regNumber targetReg = node->gtRegNum;
    emitAttr  attr      = emitActualTypeSize(op1->TypeGet());

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;

    instruction ins = getOpForHWIntrinsic(node, node->TypeGet());

    getEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicCrcOp:
//
// Produce code for a GT_HWIntrinsic node with form CrcOp.
//
// Consumes two scalar operands and produces a scalar result
//
// This form differs from BinaryOp because the attr depends on the size of op2
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicCrcOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("genHWIntrinsicCrcOp not implemented");
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdBinaryOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdBinaryOp.
//
// Consumes two SIMD operands and produces a SIMD result
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdBinaryOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    GenTree*  op2       = node->gtGetOp2();
    var_types baseType  = node->gtSIMDBaseType;
    regNumber targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(targetReg));

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    getEmitter()->emitIns_R_R_R(ins, attr, targetReg, op1Reg, op2Reg, opt);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSwitchTable: generate the jump-table for imm-intrinsics
//    with non-constant argument
//
// Arguments:
//    swReg      - register containing the switch case to execute
//    tmpReg     - temporary integer register for calculating the switch indirect branch target
//    swMax      - the number of switch cases.
//    emitSwCase - lambda to generate an individual switch case
//
// Notes:
//    Used for cases where an instruction only supports immediate operands,
//    but at jit time the operand is not a constant.
//
//    The importer is responsible for inserting an upstream range check
//    (GT_HW_INTRINSIC_CHK) for swReg, so no range check is needed here.
//
template <typename HWIntrinsicSwitchCaseBody>
void CodeGen::genHWIntrinsicSwitchTable(regNumber                 swReg,
                                        regNumber                 tmpReg,
                                        int                       swMax,
                                        HWIntrinsicSwitchCaseBody emitSwCase)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(swMax > 0);
    assert(swMax <= 256);

    assert(genIsValidIntReg(tmpReg));
    assert(genIsValidIntReg(swReg));

    BasicBlock* switchTableBeg = genCreateTempLabel();
    BasicBlock* switchTableEnd = genCreateTempLabel();

    // Calculate switch target
    //
    // Each switch table case needs exactly 8 bytes of code.
    switchTableBeg->bbFlags |= BBF_JMP_TARGET;

    // tmpReg = switchTableBeg
    getEmitter()->emitIns_R_L(INS_adr, EA_PTRSIZE, switchTableBeg, tmpReg);

    // tmpReg = switchTableBeg + swReg * 8
    getEmitter()->emitIns_R_R_R_I(INS_add, EA_PTRSIZE, tmpReg, tmpReg, swReg, 3, INS_OPTS_LSL);

    // br tmpReg
    getEmitter()->emitIns_R(INS_br, EA_PTRSIZE, tmpReg);

    genDefineTempLabel(switchTableBeg);
    for (int i = 0; i < swMax; ++i)
    {
        unsigned prevInsCount = getEmitter()->emitInsCount;

        emitSwCase(i);

        assert(getEmitter()->emitInsCount == prevInsCount + 1);

        inst_JMP(EJ_jmp, switchTableEnd);

        assert(getEmitter()->emitInsCount == prevInsCount + 2);
    }
    genDefineTempLabel(switchTableEnd);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdExtractOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdExtractOp.
//
// Consumes one SIMD operand and one scalar
//
// The element index operand is typically a const immediate
// When it is not, a switch table is generated
//
// See genHWIntrinsicSwitchTable comments
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdExtractOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1        = node->gtGetOp1();
    GenTree*  op2        = node->gtGetOp2();
    var_types simdType   = op1->TypeGet();
    var_types targetType = node->TypeGet();
    regNumber targetReg  = node->gtRegNum;

    assert(targetReg != REG_NA);

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));

    emitAttr baseTypeSize = emitTypeSize(targetType);

    int elements = emitTypeSize(simdType) / baseTypeSize;

    auto emitSwCase = [&](int element) {
        assert(element >= 0);
        assert(element < elements);

        if (varTypeIsFloating(targetType))
        {
            assert(genIsValidFloatReg(targetReg));
            getEmitter()->emitIns_R_R_I_I(INS_mov, baseTypeSize, targetReg, op1Reg, 0, element);
        }
        else if (varTypeIsUnsigned(targetType) || (baseTypeSize == EA_8BYTE))
        {
            assert(genIsValidIntReg(targetReg));
            getEmitter()->emitIns_R_R_I(INS_umov, baseTypeSize, targetReg, op1Reg, element);
        }
        else
        {
            assert(genIsValidIntReg(targetReg));
            getEmitter()->emitIns_R_R_I(INS_smov, baseTypeSize, targetReg, op1Reg, element);
        }
    };

    if (op2->isContainedIntOrIImmed())
    {
        int element = (int)op2->AsIntConCommon()->IconValue();

        emitSwCase(element);
    }
    else
    {
        regNumber elementReg = op2->gtRegNum;
        regNumber tmpReg     = node->GetSingleTempReg();

        genHWIntrinsicSwitchTable(elementReg, tmpReg, elements, emitSwCase);
    }

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdInsertOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdInsertOp.
//
// Consumes one SIMD operand and two scalars
//
// The element index operand is typically a const immediate
// When it is not, a switch table is generated
//
// See genHWIntrinsicSwitchTable comments
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdInsertOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTreeArgList* argList   = node->gtGetOp1()->AsArgList();
    GenTree*        op1       = argList->Current();
    GenTree*        op2       = argList->Rest()->Current();
    GenTree*        op3       = argList->Rest()->Rest()->Current();
    var_types       simdType  = op1->TypeGet();
    var_types       baseType  = node->gtSIMDBaseType;
    regNumber       targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);

    genConsumeRegs(op1);
    genConsumeRegs(op2);
    genConsumeRegs(op3);

    regNumber op1Reg = op1->gtRegNum;

    assert(genIsValidFloatReg(targetReg));
    assert(genIsValidFloatReg(op1Reg));

    emitAttr baseTypeSize = emitTypeSize(baseType);

    int elements = emitTypeSize(simdType) / baseTypeSize;

    if (targetReg != op1Reg)
    {
        getEmitter()->emitIns_R_R(INS_mov, baseTypeSize, targetReg, op1Reg);
    }

    if (op3->isContained())
    {
        // Handle vector element to vector element case
        //
        // If op3 is contained this is because lowering found an opportunity to contain a Simd.Extract in a Simd.Insert
        //
        regNumber op3Reg = op3->gtGetOp1()->gtRegNum;

        assert(genIsValidFloatReg(op3Reg));

        // op3 containment currently only occurs when
        //   + op3 is a Simd.Extract() (gtHWIntrinsicId == NI_MIPS64_SIMD_GetItem)
        //   + element & srcLane are immediate constants
        assert(op2->isContainedIntOrIImmed());
        assert(op3->OperIs(GT_HWIntrinsic));
        assert(op3->AsHWIntrinsic()->gtHWIntrinsicId == NI_MIPS64_SIMD_GetItem);
        assert(op3->gtGetOp2()->isContainedIntOrIImmed());

        int element = (int)op2->AsIntConCommon()->IconValue();
        int srcLane = (int)op3->gtGetOp2()->AsIntConCommon()->IconValue();

        // Emit mov targetReg[element], op3Reg[srcLane]
        getEmitter()->emitIns_R_R_I_I(INS_mov, baseTypeSize, targetReg, op3Reg, element, srcLane);
    }
    else
    {
        // Handle scalar to vector element case
        // TODO-MIPS64-CQ handle containing op3 scalar const where possible
        regNumber op3Reg = op3->gtRegNum;

        auto emitSwCase = [&](int element) {
            assert(element >= 0);
            assert(element < elements);

            if (varTypeIsFloating(baseType))
            {
                assert(genIsValidFloatReg(op3Reg));
                getEmitter()->emitIns_R_R_I_I(INS_mov, baseTypeSize, targetReg, op3Reg, element, 0);
            }
            else
            {
                assert(genIsValidIntReg(op3Reg));
                getEmitter()->emitIns_R_R_I(INS_mov, baseTypeSize, targetReg, op3Reg, element);
            }
        };

        if (op2->isContainedIntOrIImmed())
        {
            int element = (int)op2->AsIntConCommon()->IconValue();

            emitSwCase(element);
        }
        else
        {
            regNumber elementReg = op2->gtRegNum;
            regNumber tmpReg     = node->GetSingleTempReg();

            genHWIntrinsicSwitchTable(elementReg, tmpReg, elements, emitSwCase);
        }
    }

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdSelectOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdSelectOp.
//
// Consumes three SIMD operands and produces a SIMD result
//
// This intrinsic form requires one of the source registers to be the
// destination register.  Inserts a INS_mov if this requirement is not met.
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdSelectOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTreeArgList* argList   = node->gtGetOp1()->AsArgList();
    GenTree*        op1       = argList->Current();
    GenTree*        op2       = argList->Rest()->Current();
    GenTree*        op3       = argList->Rest()->Rest()->Current();
    var_types       baseType  = node->gtSIMDBaseType;
    regNumber       targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeRegs(op1);
    genConsumeRegs(op2);
    genConsumeRegs(op3);

    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;
    regNumber op3Reg = op3->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(op3Reg));
    assert(genIsValidFloatReg(targetReg));

    emitAttr attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;

    // Arm64 has three bit select forms; each uses three source registers
    // One of the sources is also the destination
    if (targetReg == op3Reg)
    {
        // op3 is target use bit insert if true
        // op3 = op3 ^ (op1 & (op2 ^ op3))
        getEmitter()->emitIns_R_R_R(INS_bit, attr, op3Reg, op2Reg, op1Reg);
    }
    else if (targetReg == op2Reg)
    {
        // op2 is target use bit insert if false
        // op2 = op2 ^ (~op1 & (op2 ^ op3))
        getEmitter()->emitIns_R_R_R(INS_bif, attr, op2Reg, op3Reg, op1Reg);
    }
    else
    {
        if (targetReg != op1Reg)
        {
            // target is not one of the sources, copy op1 to use bit select form
            getEmitter()->emitIns_R_R(INS_mov, attr, targetReg, op1Reg);
        }
        // use bit select
        // targetReg = op3 ^ (targetReg & (op2 ^ op3))
        getEmitter()->emitIns_R_R_R(INS_bsl, attr, targetReg, op2Reg, op3Reg);
    }

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdSetAllOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdSetAllOp.
//
// Consumes single scalar operand and produces a SIMD result
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdSetAllOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    var_types baseType  = node->gtSIMDBaseType;
    regNumber targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;

    assert(genIsValidFloatReg(targetReg));
    assert(genIsValidIntReg(op1Reg) || genIsValidFloatReg(op1Reg));

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    // TODO-MIPS64-CQ Support contained immediate cases

    if (genIsValidIntReg(op1Reg))
    {
        getEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg, opt);
    }
    else
    {
        getEmitter()->emitIns_R_R_I(ins, attr, targetReg, op1Reg, 0, opt);
    }

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdUnaryOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdUnaryOp.
//
// Consumes single SIMD operand and produces a SIMD result
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdUnaryOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    var_types baseType  = node->gtSIMDBaseType;
    regNumber targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(targetReg));

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    getEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg, opt);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdBinaryRMWOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdBinaryRMWOp.
//
// Consumes two SIMD operands and produces a SIMD result.
// First operand is both source and destination.
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdBinaryRMWOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    GenTree*  op2       = node->gtGetOp2();
    var_types baseType  = node->gtSIMDBaseType;
    regNumber targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);

    genConsumeOperands(node);

    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(targetReg));

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;
    insOpts     opt  = genGetSimdInsOpt(attr, baseType);

    if (targetReg != op1Reg)
    {
        getEmitter()->emitIns_R_R(INS_mov, attr, targetReg, op1Reg);
    }
    getEmitter()->emitIns_R_R(ins, attr, targetReg, op2Reg, opt);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicSimdTernaryRMWOp:
//
// Produce code for a GT_HWIntrinsic node with form SimdTernaryRMWOp
//
// Consumes three SIMD operands and produces a SIMD result.
// First operand is both source and destination.
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicSimdTernaryRMWOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTreeArgList* argList   = node->gtGetOp1()->AsArgList();
    GenTree*        op1       = argList->Current();
    GenTree*        op2       = argList->Rest()->Current();
    GenTree*        op3       = argList->Rest()->Rest()->Current();
    var_types       baseType  = node->gtSIMDBaseType;
    regNumber       targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeRegs(op1);
    genConsumeRegs(op2);
    genConsumeRegs(op3);

    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;
    regNumber op3Reg = op3->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op2Reg));
    assert(genIsValidFloatReg(op3Reg));
    assert(genIsValidFloatReg(targetReg));
    assert(targetReg != op2Reg);
    assert(targetReg != op3Reg);

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;

    if (targetReg != op1Reg)
    {
        getEmitter()->emitIns_R_R(INS_mov, attr, targetReg, op1Reg);
    }

    getEmitter()->emitIns_R_R_R(ins, attr, targetReg, op2Reg, op3Reg);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicShaHashOp:
//
// Produce code for a GT_HWIntrinsic node with form Sha1HashOp.
// Used in MIPS64 SHA1 Hash operations.
//
// Consumes three operands and returns a Simd result.
// First Simd operand is both source and destination.
// Second Operand is an unsigned int.
// Third operand is a simd operand.

// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicShaHashOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTreeArgList* argList   = node->gtGetOp1()->AsArgList();
    GenTree*        op1       = argList->Current();
    GenTree*        op2       = argList->Rest()->Current();
    GenTree*        op3       = argList->Rest()->Rest()->Current();
    var_types       baseType  = node->gtSIMDBaseType;
    regNumber       targetReg = node->gtRegNum;

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeRegs(op1);
    genConsumeRegs(op2);
    genConsumeRegs(op3);

    regNumber op1Reg = op1->gtRegNum;
    regNumber op2Reg = op2->gtRegNum;
    regNumber op3Reg = op3->gtRegNum;

    assert(genIsValidFloatReg(op1Reg));
    assert(genIsValidFloatReg(op3Reg));
    assert(targetReg != op2Reg);
    assert(targetReg != op3Reg);

    instruction ins  = getOpForHWIntrinsic(node, baseType);
    emitAttr    attr = (node->gtSIMDSize > 8) ? EA_16BYTE : EA_8BYTE;

    assert(genIsValidIntReg(op2Reg));
    regNumber elementReg = op2->gtRegNum;
    regNumber tmpReg     = node->GetSingleTempReg(RBM_ALLFLOAT);

    getEmitter()->emitIns_R_R(INS_fmov, EA_4BYTE, tmpReg, elementReg);

    if (targetReg != op1Reg)
    {
        getEmitter()->emitIns_R_R(INS_mov, attr, targetReg, op1Reg);
    }

    getEmitter()->emitIns_R_R_R(ins, attr, targetReg, tmpReg, op3Reg);

    genProduceReg(node);
#endif
}

//------------------------------------------------------------------------
// genHWIntrinsicShaRotateOp:
//
// Produce code for a GT_HWIntrinsic node with form Sha1RotateOp.
// Used in MIPS64 SHA1 Rotate operations.
//
// Consumes one integer operand and returns unsigned int result.
//
// Arguments:
//    node - the GT_HWIntrinsic node
//
// Return Value:
//    None.
//
void CodeGen::genHWIntrinsicShaRotateOp(GenTreeHWIntrinsic* node)
{
assert(!"unimplemented on MIPS yet");
#if 0
    GenTree*  op1       = node->gtGetOp1();
    regNumber targetReg = node->gtRegNum;
    emitAttr  attr      = emitActualTypeSize(node);

    assert(targetReg != REG_NA);
    var_types targetType = node->TypeGet();

    genConsumeOperands(node);

    instruction ins        = getOpForHWIntrinsic(node, node->TypeGet());
    regNumber   elementReg = op1->gtRegNum;
    regNumber   tmpReg     = node->GetSingleTempReg(RBM_ALLFLOAT);

    getEmitter()->emitIns_R_R(INS_fmov, EA_4BYTE, tmpReg, elementReg);
    getEmitter()->emitIns_R_R(ins, EA_4BYTE, tmpReg, tmpReg);
    getEmitter()->emitIns_R_R(INS_fmov, attr, targetReg, tmpReg);

    genProduceReg(node);
#endif
}

#endif // FEATURE_HW_INTRINSICS

/*****************************************************************************
 * Unit testing of the MIPS64 emitter: generate a bunch of instructions into the prolog
 * (it's as good a place as any), then use COMPlus_JitLateDisasm=* to see if the late
 * disassembler thinks the instructions as the same as we do.
 */

// Uncomment "#define ALL_MIPS64_EMITTER_UNIT_TESTS" to run all the unit tests here.
// After adding a unit test, and verifying it works, put it under this #ifdef, so we don't see it run every time.
//#define ALL_MIPS64_EMITTER_UNIT_TESTS

#if defined(DEBUG)
void CodeGen::genMIPS64EmitterUnitTests()
{
    if (!verbose)
    {
        return;
    }

    if (!compiler->opts.altJit)
    {
        // No point doing this in a "real" JIT.
        return;
    }

    // Mark the "fake" instructions in the output.
    printf("*************** In genMIPS64EmitterUnitTests()\n");

    printf("*************** End of genMIPS64EmitterUnitTests()\n");
}
#endif // defined(DEBUG)

//------------------------------------------------------------------------
// genStackPointerConstantAdjustment: add a specified constant value to the stack pointer.
// No probe is done.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative or zero.
//
// Return Value:
//    None.
//
void CodeGen::genStackPointerConstantAdjustment(ssize_t spDelta)
{
    assert(spDelta < 0);

    // We assert that the SP change is less than one page. If it's greater, you should have called a
    // function that does a probe, which will in turn call this function.
    assert((target_size_t)(-spDelta) <= compiler->eeGetPageSize());

    inst_RV_IV(INS_daddiu, REG_SPBASE, spDelta, EA_PTRSIZE);
}

//------------------------------------------------------------------------
// genStackPointerConstantAdjustmentWithProbe: add a specified constant value to the stack pointer,
// and probe the stack as appropriate. Should only be called as a helper for
// genStackPointerConstantAdjustmentLoopWithProbe.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative or zero. If zero, the probe happens,
//                              but the stack pointer doesn't move.
//    regTmp                  - temporary register to use as target for probe load instruction
//
// Return Value:
//    None.
//
void CodeGen::genStackPointerConstantAdjustmentWithProbe(ssize_t spDelta, regNumber regTmp)
{
    getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, regTmp, REG_SP, 0);
    genStackPointerConstantAdjustment(spDelta);
}

//------------------------------------------------------------------------
// genStackPointerConstantAdjustmentLoopWithProbe: Add a specified constant value to the stack pointer,
// and probe the stack as appropriate. Generates one probe per page, up to the total amount required.
// This will generate a sequence of probes in-line.
//
// Arguments:
//    spDelta                 - the value to add to SP. Must be negative.
//    regTmp                  - temporary register to use as target for probe load instruction
//
// Return Value:
//    Offset in bytes from SP to last probed address.
//
target_ssize_t CodeGen::genStackPointerConstantAdjustmentLoopWithProbe(ssize_t spDelta, regNumber regTmp)
{
    assert(spDelta < 0);

    const target_size_t pageSize = compiler->eeGetPageSize();

    ssize_t spRemainingDelta = spDelta;
    do
    {
        ssize_t spOneDelta = -(ssize_t)min((target_size_t)-spRemainingDelta, pageSize);
        genStackPointerConstantAdjustmentWithProbe(spOneDelta, regTmp);
        spRemainingDelta -= spOneDelta;
    } while (spRemainingDelta < 0);

    // What offset from the final SP was the last probe? This depends on the fact that
    // genStackPointerConstantAdjustmentWithProbe() probes first, then does "SUB SP".
    target_size_t lastTouchDelta = (target_size_t)(-spDelta) % pageSize;
    if ((lastTouchDelta == 0) || (lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES > pageSize))
    {
        // We haven't probed almost a complete page. If lastTouchDelta==0, then spDelta was an exact
        // multiple of pageSize, which means we last probed exactly one page back. Otherwise, we probed
        // the page, but very far from the end. If the next action on the stack might subtract from SP
        // first, before touching the current SP, then we do one more probe at the very bottom. This can
        // happen on x86, for example, when we copy an argument to the stack using a "SUB ESP; REP MOV"
        // strategy.

        getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, regTmp, REG_SP, 0);
        lastTouchDelta = 0;
    }

    return lastTouchDelta;
}

//------------------------------------------------------------------------
// genCodeForTreeNode Generate code for a single node in the tree.
//
// Preconditions:
//    All operands have been evaluated.
//
void CodeGen::genCodeForTreeNode(GenTree* treeNode)
{
    regNumber targetReg  = treeNode->gtRegNum;
    var_types targetType = treeNode->TypeGet();
    emitter*  emit       = getEmitter();

#ifdef DEBUG
    // Validate that all the operands for the current node are consumed in order.
    // This is important because LSRA ensures that any necessary copies will be
    // handled correctly.
    lastConsumedNode = nullptr;
    if (compiler->verbose)
    {
        unsigned seqNum = treeNode->gtSeqNum; // Useful for setting a conditional break in Visual Studio
        compiler->gtDispLIRNode(treeNode, "Generating: ");
    }
#endif // DEBUG

    // Is this a node whose value is already in a register?  LSRA denotes this by
    // setting the GTF_REUSE_REG_VAL flag.
    if (treeNode->IsReuseRegVal())
    {
        // For now, this is only used for constant nodes.
        assert((treeNode->OperGet() == GT_CNS_INT) || (treeNode->OperGet() == GT_CNS_DBL));
        JITDUMP("  TreeNode is marked ReuseReg\n");
        return;
    }

    // contained nodes are part of their parents for codegen purposes
    // ex : immediates, most LEAs
    if (treeNode->isContained())
    {
        return;
    }

    switch (treeNode->gtOper)
    {
        case GT_START_NONGC:
            getEmitter()->emitDisableGC();
            break;

        case GT_START_PREEMPTGC:
            // Kill callee saves GC registers, and create a label
            // so that information gets propagated to the emitter.
            gcInfo.gcMarkRegSetNpt(RBM_INT_CALLEE_SAVED);
            genDefineTempLabel(genCreateTempLabel());
            break;

        case GT_PROF_HOOK:
            // We should be seeing this only if profiler hook is needed
            noway_assert(compiler->compIsProfilerHookNeeded());

#ifdef PROFILING_SUPPORTED
            // Right now this node is used only for tail calls. In future if
            // we intend to use it for Enter or Leave hooks, add a data member
            // to this node indicating the kind of profiler hook. For example,
            // helper number can be used.
            genProfilingLeaveCallback(CORINFO_HELP_PROF_FCN_TAILCALL);
#endif // PROFILING_SUPPORTED
            break;

        case GT_LCLHEAP:
            genLclHeap(treeNode);
            break;

        case GT_CNS_INT:
        case GT_CNS_DBL:
            genSetRegToConst(targetReg, targetType, treeNode);
            genProduceReg(treeNode);
            break;

        case GT_NOT:
        case GT_NEG:
            genCodeForNegNot(treeNode);
            break;

        case GT_MOD:
        case GT_UMOD:
        case GT_DIV:
        case GT_UDIV:
            genCodeForDivMod(treeNode->AsOp());
            break;

        case GT_OR:
        case GT_XOR:
        case GT_AND:
            assert(varTypeIsIntegralOrI(treeNode));

            __fallthrough;

        case GT_ADD:
        case GT_SUB:
        case GT_MUL:
            genConsumeOperands(treeNode->AsOp());
            genCodeForBinary(treeNode->AsOp());
            break;

        case GT_LSH:
        case GT_RSH:
        case GT_RSZ:
        case GT_ROR:
            genCodeForShift(treeNode);
            break;

        case GT_CAST:
            genCodeForCast(treeNode->AsOp());
            break;

        case GT_BITCAST:
        {
            GenTree* op1 = treeNode->gtOp.gtOp1;
            if (varTypeIsFloating(treeNode) != varTypeIsFloating(op1))
            {
             /* FIXME for MIPS: */
             assert(!"unimplemented on MIPS yet");
                //inst_RV_RV(INS_fmov, targetReg, genConsumeReg(op1), targetType);
            }
            else
            {
                inst_RV_RV(ins_Copy(targetType), targetReg, genConsumeReg(op1), targetType);
            }
        }
        break;

        case GT_LCL_FLD_ADDR:
        case GT_LCL_VAR_ADDR:
            genCodeForLclAddr(treeNode);
            break;

        case GT_LCL_FLD:
            genCodeForLclFld(treeNode->AsLclFld());
            break;

        case GT_LCL_VAR:
            genCodeForLclVar(treeNode->AsLclVar());
            break;

        case GT_STORE_LCL_FLD:
            genCodeForStoreLclFld(treeNode->AsLclFld());
            break;

        case GT_STORE_LCL_VAR:
            genCodeForStoreLclVar(treeNode->AsLclVar());
            break;

        case GT_RETFILT:
        case GT_RETURN:
            genReturn(treeNode);
            break;

        case GT_LEA:
            // If we are here, it is the case where there is an LEA that cannot be folded into a parent instruction.
            genLeaInstruction(treeNode->AsAddrMode());
            break;

        case GT_INDEX_ADDR:
            genCodeForIndexAddr(treeNode->AsIndexAddr());
            break;

        case GT_IND:
            genCodeForIndir(treeNode->AsIndir());
            break;

        case GT_MULHI:
            genCodeForMulHi(treeNode->AsOp());
            break;

        case GT_SWAP:
            genCodeForSwap(treeNode->AsOp());
            break;

        case GT_JMP:
            genJmpMethod(treeNode);
            break;

        case GT_CKFINITE:
            genCkfinite(treeNode);
            break;

        case GT_INTRINSIC:
            genIntrinsic(treeNode);
            break;

#ifdef FEATURE_SIMD
        case GT_SIMD:
            genSIMDIntrinsic(treeNode->AsSIMD());
            break;
#endif // FEATURE_SIMD

#ifdef FEATURE_HW_INTRINSICS
        case GT_HWIntrinsic:
            genHWIntrinsic(treeNode->AsHWIntrinsic());
            break;
#endif // FEATURE_HW_INTRINSICS

        case GT_EQ:
        case GT_NE:
        case GT_LT:
        case GT_LE:
        case GT_GE:
        case GT_GT:
        case GT_CMP:
            genCodeForCompare(treeNode->AsOp());
            break;

        case GT_JTRUE:
            genCodeForJumpTrue(treeNode->AsOp());
            break;

        case GT_JCMP:
            genCodeForJumpCompare(treeNode->AsOp());
            break;

        case GT_JCC:
            genCodeForJcc(treeNode->AsCC());
            break;

        case GT_SETCC:
            genCodeForSetcc(treeNode->AsCC());
            break;

        case GT_RETURNTRAP:
            genCodeForReturnTrap(treeNode->AsOp());
            break;

        case GT_STOREIND:
            genCodeForStoreInd(treeNode->AsStoreInd());
            break;

        case GT_COPY:
            // This is handled at the time we call genConsumeReg() on the GT_COPY
            break;

        case GT_LIST:
        case GT_FIELD_LIST:
            // Should always be marked contained.
            assert(!"LIST, FIELD_LIST nodes should always be marked contained.");
            break;

        case GT_PUTARG_STK:
            genPutArgStk(treeNode->AsPutArgStk());
            break;

        case GT_PUTARG_REG:
            genPutArgReg(treeNode->AsOp());
            break;

#if FEATURE_ARG_SPLIT
        case GT_PUTARG_SPLIT:
            genPutArgSplit(treeNode->AsPutArgSplit());
            break;
#endif // FEATURE_ARG_SPLIT

        case GT_CALL:
            genCallInstruction(treeNode->AsCall());
            break;

        case GT_MEMORYBARRIER:
            instGen_MemoryBarrier();
            break;

        case GT_XCHG:
        case GT_XADD:
            genLockedInstructions(treeNode->AsOp());
            break;

        case GT_CMPXCHG:
            genCodeForCmpXchg(treeNode->AsCmpXchg());
            break;

        case GT_RELOAD:
            // do nothing - reload is just a marker.
            // The parent node will call genConsumeReg on this which will trigger the unspill of this node's child
            // into the register specified in this node.
            break;

        case GT_NOP:
            break;

        case GT_NO_OP:
            instGen(INS_nop);
            break;

        case GT_ARR_BOUNDS_CHECK:
#ifdef FEATURE_SIMD
        case GT_SIMD_CHK:
#endif // FEATURE_SIMD
#ifdef FEATURE_HW_INTRINSICS
        case GT_HW_INTRINSIC_CHK:
#endif // FEATURE_HW_INTRINSICS
            genRangeCheck(treeNode);
            break;

        case GT_PHYSREG:
            genCodeForPhysReg(treeNode->AsPhysReg());
            break;

        case GT_NULLCHECK:
            genCodeForNullCheck(treeNode->AsOp());
            break;

        case GT_CATCH_ARG:

            noway_assert(handlerGetsXcptnObj(compiler->compCurBB->bbCatchTyp));

            /* Catch arguments get passed in a register. genCodeForBBlist()
               would have marked it as holding a GC object, but not used. */

            noway_assert(gcInfo.gcRegGCrefSetCur & RBM_EXCEPTION_OBJECT);
            genConsumeReg(treeNode);
            break;

        case GT_PINVOKE_PROLOG:
            noway_assert(((gcInfo.gcRegGCrefSetCur | gcInfo.gcRegByrefSetCur) & ~fullIntArgRegMask()) == 0);

            // the runtime side requires the codegen here to be consistent
            emit->emitDisableRandomNops();
            break;

        case GT_LABEL:
            genPendingCallLabel = genCreateTempLabel();
            emit->emitIns_R_L(INS_ld, EA_PTRSIZE, genPendingCallLabel, targetReg);
            break;

        case GT_STORE_OBJ:
        case GT_STORE_DYN_BLK:
        case GT_STORE_BLK:
            genCodeForStoreBlk(treeNode->AsBlk());
            break;

        case GT_JMPTABLE:
            genJumpTable(treeNode);
            break;

        case GT_SWITCH_TABLE:
            genTableBasedSwitch(treeNode);
            break;

        case GT_ARR_INDEX:
            genCodeForArrIndex(treeNode->AsArrIndex());
            break;

        case GT_ARR_OFFSET:
            genCodeForArrOffset(treeNode->AsArrOffs());
            break;

        case GT_IL_OFFSET:
            // Do nothing; these nodes are simply markers for debug info.
            break;

        default:
        {
#ifdef DEBUG
            char message[256];
            _snprintf_s(message, _countof(message), _TRUNCATE, "NYI: Unimplemented node type %s",
                        GenTree::OpName(treeNode->OperGet()));
            NYIRAW(message);
#else
            NYI("unimplemented node");
#endif
        }
        break;
    }
}

//------------------------------------------------------------------------
// genSetRegToIcon: Generate code that will set the given register to the integer constant.
//
void CodeGen::genSetRegToIcon(regNumber reg, ssize_t val, var_types type, insFlags flags)
{
    // Reg cannot be a FP reg
    assert(!genIsValidFloatReg(reg));

    // The only TYP_REF constant that can come this path is a managed 'null' since it is not
    // relocatable.  Other ref type constants (e.g. string objects) go through a different
    // code path.
    noway_assert(type != TYP_REF || val == 0);

    instGen_Set_Reg_To_Imm(emitActualTypeSize(type), reg, val, flags);
}

//---------------------------------------------------------------------
// genSetGSSecurityCookie: Set the "GS" security cookie in the prolog.
//
// Arguments:
//     initReg        - register to use as a scratch register
//     pInitRegZeroed - OUT parameter. *pInitRegZeroed is set to 'false' if and only if
//                      this call sets 'initReg' to a non-zero value.
//
// Return Value:
//     None
//
void CodeGen::genSetGSSecurityCookie(regNumber initReg, bool* pInitRegZeroed)
{
    assert(compiler->compGeneratingProlog);

    if (!compiler->getNeedsGSSecurityCookie())
    {
        return;
    }

    if (compiler->gsGlobalSecurityCookieAddr == nullptr)
    {
        noway_assert(compiler->gsGlobalSecurityCookieVal != 0);
        // initReg = #GlobalSecurityCookieVal; [frame.GSSecurityCookie] = initReg
        genSetRegToIcon(initReg, compiler->gsGlobalSecurityCookieVal, TYP_I_IMPL);
        getEmitter()->emitIns_S_R(INS_sd, EA_PTRSIZE, initReg, compiler->lvaGSSecurityCookie, 0);
    }
    else
    {
        instGen_Set_Reg_To_Imm(EA_PTR_DSP_RELOC, initReg, (ssize_t)compiler->gsGlobalSecurityCookieAddr);
        getEmitter()->emitIns_R_R_I(INS_ld, EA_PTRSIZE, initReg, initReg, 0);
        regSet.verifyRegUsed(initReg);
        getEmitter()->emitIns_S_R(INS_sd, EA_PTRSIZE, initReg, compiler->lvaGSSecurityCookie, 0);
    }

    *pInitRegZeroed = false;
}

//---------------------------------------------------------------------
// genIntrinsic - generate code for a given intrinsic
//
// Arguments
//    treeNode - the GT_INTRINSIC node
//
// Return value:
//    None
//
void CodeGen::genIntrinsic(GenTree* treeNode)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(treeNode->OperIs(GT_INTRINSIC));

    // Both operand and its result must be of the same floating point type.
    GenTree* srcNode = treeNode->gtOp.gtOp1;
    assert(varTypeIsFloating(srcNode));
    assert(srcNode->TypeGet() == treeNode->TypeGet());

    // Right now only Abs/Ceiling/Floor/Round/Sqrt are treated as math intrinsics.
    //
    switch (treeNode->gtIntrinsic.gtIntrinsicId)
    {
        case CORINFO_INTRINSIC_Abs:
            genConsumeOperands(treeNode->AsOp());
            getEmitter()->emitInsBinary(INS_ABS, emitActualTypeSize(treeNode), treeNode, srcNode);
            break;

#ifdef _TARGET_MIPS64_
        case CORINFO_INTRINSIC_Ceiling:
            genConsumeOperands(treeNode->AsOp());
            getEmitter()->emitInsBinary(INS_frintp, emitActualTypeSize(treeNode), treeNode, srcNode);
            break;

        case CORINFO_INTRINSIC_Floor:
            genConsumeOperands(treeNode->AsOp());
            getEmitter()->emitInsBinary(INS_frintm, emitActualTypeSize(treeNode), treeNode, srcNode);
            break;

        case CORINFO_INTRINSIC_Round:
            genConsumeOperands(treeNode->AsOp());
            getEmitter()->emitInsBinary(INS_frintn, emitActualTypeSize(treeNode), treeNode, srcNode);
            break;
#endif // _TARGET_MIPS64_

        case CORINFO_INTRINSIC_Sqrt:
            genConsumeOperands(treeNode->AsOp());
            getEmitter()->emitInsBinary(INS_SQRT, emitActualTypeSize(treeNode), treeNode, srcNode);
            break;

        default:
            assert(!"genIntrinsic: Unsupported intrinsic");
            unreached();
    }

    genProduceReg(treeNode);
#endif
}

//---------------------------------------------------------------------
// genPutArgStk - generate code for a GT_PUTARG_STK node
//
// Arguments
//    treeNode - the GT_PUTARG_STK node
//
// Return value:
//    None
//
void CodeGen::genPutArgStk(GenTreePutArgStk* treeNode)
{
    assert(treeNode->OperIs(GT_PUTARG_STK));
    GenTree*  source     = treeNode->gtOp1;
    var_types targetType = genActualType(source->TypeGet());
    emitter*  emit       = getEmitter();

    // This is the varNum for our store operations,
    // typically this is the varNum for the Outgoing arg space
    // When we are generating a tail call it will be the varNum for arg0
    unsigned varNumOut    = (unsigned)-1;
    unsigned argOffsetMax = (unsigned)-1; // Records the maximum size of this area for assert checks

    // Get argument offset to use with 'varNumOut'
    // Here we cross check that argument offset hasn't changed from lowering to codegen since
    // we are storing arg slot number in GT_PUTARG_STK node in lowering phase.
    unsigned argOffsetOut = treeNode->gtSlotNum * TARGET_POINTER_SIZE;

#ifdef DEBUG
    fgArgTabEntry* curArgTabEntry = compiler->gtArgEntryByNode(treeNode->gtCall, treeNode);
    assert(curArgTabEntry);
    assert(argOffsetOut == (curArgTabEntry->slotNum * TARGET_POINTER_SIZE));
#endif // DEBUG

    // Whether to setup stk arg in incoming or out-going arg area?
    // Fast tail calls implemented as epilog+jmp = stk arg is setup in incoming arg area.
    // All other calls - stk arg is setup in out-going arg area.
    if (treeNode->putInIncomingArgArea())
    {
        varNumOut    = getFirstArgWithStackSlot();
        argOffsetMax = compiler->compArgSize;
#if FEATURE_FASTTAILCALL
        // This must be a fast tail call.
        assert(treeNode->gtCall->IsFastTailCall());

        // Since it is a fast tail call, the existence of first incoming arg is guaranteed
        // because fast tail call requires that in-coming arg area of caller is >= out-going
        // arg area required for tail call.
        LclVarDsc* varDsc = &(compiler->lvaTable[varNumOut]);
        assert(varDsc != nullptr);
#endif // FEATURE_FASTTAILCALL
    }
    else
    {
        varNumOut    = compiler->lvaOutgoingArgSpaceVar;
        argOffsetMax = compiler->lvaOutgoingArgSpaceSize;
    }

    bool isStruct = (targetType == TYP_STRUCT) || (source->OperGet() == GT_FIELD_LIST);

    if (!isStruct) // a normal non-Struct argument
    {
        if (varTypeIsSIMD(targetType))
        {
            assert(!"unimplemented on MIPS yet");
        }

        instruction storeIns  = ins_Store(targetType);
        emitAttr    storeAttr = emitTypeSize(targetType);

        // If it is contained then source must be the integer constant zero
        if (source->isContained())
        {
            assert(source->OperGet() == GT_CNS_INT);
            assert(source->AsIntConCommon()->IconValue() == 0);

            emit->emitIns_S_R(storeIns, storeAttr, REG_R0, varNumOut, argOffsetOut);
        }
        else
        {
            genConsumeReg(source);
            if (storeIns == INS_sw)
            {
                emit->emitIns_R_R_R(INS_addu, EA_4BYTE, source->gtRegNum, source->gtRegNum, REG_R0);
                storeIns = INS_sd;
                storeAttr = EA_8BYTE;
            }
            emit->emitIns_S_R(storeIns, storeAttr, source->gtRegNum, varNumOut, argOffsetOut);
        }
        argOffsetOut += EA_SIZE_IN_BYTES(storeAttr);
        assert(argOffsetOut <= argOffsetMax); // We can't write beyound the outgoing area area
    }
    else // We have some kind of a struct argument
    {
        assert(source->isContained()); // We expect that this node was marked as contained in Lower

        if (source->OperGet() == GT_FIELD_LIST)
        {
            genPutArgStkFieldList(treeNode, varNumOut);
        }
        else // We must have a GT_OBJ or a GT_LCL_VAR
        {
            noway_assert((source->OperGet() == GT_LCL_VAR) || (source->OperGet() == GT_OBJ));

            var_types targetType = source->TypeGet();
            noway_assert(varTypeIsStruct(targetType));

            // Setup loReg from the internal registers that we reserved in lower.
            //
            regNumber loReg = treeNode->ExtractTempReg();
            regNumber addrReg = REG_NA;

            GenTreeLclVarCommon* varNode  = nullptr;
            GenTree*             addrNode = nullptr;

            if (source->OperGet() == GT_LCL_VAR)
            {
                varNode = source->AsLclVarCommon();
            }
            else // we must have a GT_OBJ
            {
                assert(source->OperGet() == GT_OBJ);

                addrNode = source->gtOp.gtOp1;

                // addrNode can either be a GT_LCL_VAR_ADDR or an address expression
                //
                if (addrNode->OperGet() == GT_LCL_VAR_ADDR)
                {
                    // We have a GT_OBJ(GT_LCL_VAR_ADDR)
                    //
                    // We will treat this case the same as above
                    // (i.e if we just had this GT_LCL_VAR directly as the source)
                    // so update 'source' to point this GT_LCL_VAR_ADDR node
                    // and continue to the codegen for the LCL_VAR node below
                    //
                    varNode  = addrNode->AsLclVarCommon();
                    addrNode = nullptr;
                }
                else // addrNode is used
                {
                    // Generate code to load the address that we need into a register
                    genConsumeAddress(addrNode);
                    addrReg = addrNode->gtRegNum;
                }
            }

            // Either varNode or addrNOde must have been setup above,
            // the xor ensures that only one of the two is setup, not both
            assert((varNode != nullptr) ^ (addrNode != nullptr));

            BYTE* gcPtrs = treeNode->gtGcPtrs;

            unsigned gcPtrCount; // The count of GC pointers in the struct
            unsigned structSize;
            bool     isHfa;

            gcPtrCount = treeNode->gtNumSlots;
            // Setup the structSize, isHFa, and gcPtrCount
            if (source->OperGet() == GT_LCL_VAR)
            {
                assert(varNode != nullptr);
                LclVarDsc* varDsc = compiler->lvaGetDesc(varNode);

                // This struct also must live in the stack frame
                // And it can't live in a register (SIMD)
                assert(varDsc->lvType == TYP_STRUCT);
                assert(varDsc->lvOnFrame && !varDsc->lvRegister);

                structSize = varDsc->lvSize(); // This yields the roundUp size, but that is fine
                                               // as that is how much stack is allocated for this LclVar
            }
            else // we must have a GT_OBJ
            {
                assert(source->OperGet() == GT_OBJ);

                // If the source is an OBJ node then we need to use the type information
                // it provides (size and GC layout) even if the node wraps a lclvar. Due
                // to struct reinterpretation (e.g. Unsafe.As<X, Y>) it is possible that
                // the OBJ node has a different type than the lclvar.
                CORINFO_CLASS_HANDLE objClass = source->gtObj.gtClass;

                structSize = compiler->info.compCompHnd->getClassSize(objClass);

                // The codegen code below doesn't have proper support for struct sizes
                // that are not multiple of the slot size. Call arg morphing handles this
                // case by copying non-local values to temporary local variables.
                // More generally, we can always round up the struct size when the OBJ node
                // wraps a local variable because the local variable stack allocation size
                // is also rounded up to be a multiple of the slot size.
                if (varNode != nullptr)
                {
                    structSize = roundUp(structSize, TARGET_POINTER_SIZE);
                }
                else
                {
                    assert((structSize % TARGET_POINTER_SIZE) == 0);
                }
            }

            int      remainingSize = structSize;
            unsigned structOffset  = 0;
            unsigned nextIndex     = 0;

            while (remainingSize > 0)
            {
                if (remainingSize >= TARGET_POINTER_SIZE)
                {
                    var_types nextType = compiler->getJitGCType(gcPtrs[nextIndex]);
                    emitAttr  nextAttr = emitTypeSize(nextType);
                    remainingSize -= TARGET_POINTER_SIZE;

                    if (varNode != nullptr)
                    {
                        // Load from our varNumImp source
                        emit->emitIns_R_S(ins_Load(nextType), nextAttr, loReg, varNode->GetLclNum(), structOffset);
                    }
                    else
                    {
                        assert(loReg != addrReg);

                        // Load from our address expression source
                        emit->emitIns_R_R_I(ins_Load(nextType), nextAttr, loReg, addrReg, structOffset);
                    }
                    // Emit a store instruction to store the register into the outgoing argument area
                    emit->emitIns_S_R(ins_Store(nextType), nextAttr, loReg, varNumOut, argOffsetOut);
                    argOffsetOut += EA_SIZE_IN_BYTES(nextAttr);
                    assert(argOffsetOut <= argOffsetMax); // We can't write beyound the outgoing area area
                    assert(nextIndex < gcPtrCount);

                    structOffset += TARGET_POINTER_SIZE;
                    nextIndex++;
                }
                else // (remainingSize < TARGET_POINTER_SIZE)
                {
                    int loadSize  = remainingSize;
                    remainingSize = 0;

                    // We should never have to do a non-pointer sized load when we have a LclVar source
                    assert(varNode == nullptr);

                    // the left over size is smaller than a pointer and thus can never be a GC type
                    assert(varTypeIsGC(compiler->getJitGCType(gcPtrs[nextIndex])) == false);

                    var_types loadType = TYP_UINT;
                    if (loadSize == 1)
                    {
                        loadType = TYP_UBYTE;
                    }
                    else if (loadSize == 2)
                    {
                        loadType = TYP_USHORT;
                    }
                    else
                    {
                        // Need to handle additional loadSize cases here
                        noway_assert(loadSize == 4);
                    }

                    instruction loadIns  = ins_Load(loadType);
                    emitAttr    loadAttr = emitAttr(loadSize);

                    assert(loReg != addrReg);

                    emit->emitIns_R_R_I(loadIns, loadAttr, loReg, addrReg, structOffset);

                    // Emit a store instruction to store the register into the outgoing argument area
                    emit->emitIns_S_R(ins_Store(loadType), loadAttr, loReg, varNumOut, argOffsetOut);
                    argOffsetOut += EA_SIZE_IN_BYTES(loadAttr);
                    assert(argOffsetOut <= argOffsetMax); // We can't write beyound the outgoing area area
                    assert(nextIndex < gcPtrCount);
                }
            }
        }
    }
}

//---------------------------------------------------------------------
// genPutArgReg - generate code for a GT_PUTARG_REG node
//
// Arguments
//    tree - the GT_PUTARG_REG node
//
// Return value:
//    None
//
void CodeGen::genPutArgReg(GenTreeOp* tree)
{
    assert(tree->OperIs(GT_PUTARG_REG));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;

    assert(targetType != TYP_STRUCT);

    GenTree* op1 = tree->gtOp1;
    genConsumeReg(op1);

    // If child node is not already in the register we need, move it
    if (targetReg != op1->gtRegNum)
    {
        inst_RV_RV(ins_Copy(targetType), targetReg, op1->gtRegNum, targetType);
    }

    genProduceReg(tree);
}

#if FEATURE_ARG_SPLIT
//---------------------------------------------------------------------
// genPutArgSplit - generate code for a GT_PUTARG_SPLIT node
//
// Arguments
//    tree - the GT_PUTARG_SPLIT node
//
// Return value:
//    None
//
void CodeGen::genPutArgSplit(GenTreePutArgSplit* treeNode)
{
    assert(treeNode->OperIs(GT_PUTARG_SPLIT));

    GenTree* source       = treeNode->gtOp1;
    emitter* emit         = getEmitter();
    unsigned varNumOut    = compiler->lvaOutgoingArgSpaceVar;
    unsigned argOffsetMax = compiler->lvaOutgoingArgSpaceSize;
    unsigned argOffsetOut = treeNode->gtSlotNum * TARGET_POINTER_SIZE;

    if (source->OperGet() == GT_FIELD_LIST)
    {
        // Evaluate each of the GT_FIELD_LIST items into their register
        // and store their register into the outgoing argument area
        unsigned regIndex = 0;
        for (GenTreeFieldList* fieldListPtr = source->AsFieldList(); fieldListPtr != nullptr;
             fieldListPtr                   = fieldListPtr->Rest())
        {
            GenTree*  nextArgNode = fieldListPtr->gtGetOp1();
            regNumber fieldReg    = nextArgNode->gtRegNum;
            genConsumeReg(nextArgNode);

            if (regIndex >= treeNode->gtNumRegs)
            {
                var_types type = nextArgNode->TypeGet();
                emitAttr  attr = emitTypeSize(type);

                // Emit store instructions to store the registers produced by the GT_FIELD_LIST into the outgoing
                // argument area
                emit->emitIns_S_R(ins_Store(type), attr, fieldReg, varNumOut, argOffsetOut);
                argOffsetOut += EA_SIZE_IN_BYTES(attr);
                assert(argOffsetOut <= argOffsetMax); // We can't write beyound the outgoing area area
            }
            else
            {
                var_types type   = treeNode->GetRegType(regIndex);
                regNumber argReg = treeNode->GetRegNumByIdx(regIndex);

                // If child node is not already in the register we need, move it
                if (argReg != fieldReg)
                {
                    inst_RV_RV(ins_Copy(type), argReg, fieldReg, type);
                }
                regIndex++;
            }
        }
    }
    else
    {
        var_types targetType = source->TypeGet();
        assert(source->OperGet() == GT_OBJ);
        assert(varTypeIsStruct(targetType));

        regNumber baseReg = treeNode->ExtractTempReg();
        regNumber addrReg = REG_NA;

        GenTreeLclVarCommon* varNode  = nullptr;
        GenTree*             addrNode = nullptr;

        addrNode = source->gtOp.gtOp1;

        // addrNode can either be a GT_LCL_VAR_ADDR or an address expression
        //
        if (addrNode->OperGet() == GT_LCL_VAR_ADDR)
        {
            // We have a GT_OBJ(GT_LCL_VAR_ADDR)
            //
            // We will treat this case the same as above
            // (i.e if we just had this GT_LCL_VAR directly as the source)
            // so update 'source' to point this GT_LCL_VAR_ADDR node
            // and continue to the codegen for the LCL_VAR node below
            //
            varNode  = addrNode->AsLclVarCommon();
            addrNode = nullptr;
        }

        // Either varNode or addrNOde must have been setup above,
        // the xor ensures that only one of the two is setup, not both
        assert((varNode != nullptr) ^ (addrNode != nullptr));

        // Setup the structSize, isHFa, and gcPtrCount
        BYTE*    gcPtrs     = treeNode->gtGcPtrs;
        unsigned gcPtrCount = treeNode->gtNumberReferenceSlots; // The count of GC pointers in the struct
        int      structSize = treeNode->getArgSize();

        // This is the varNum for our load operations,
        // only used when we have a struct with a LclVar source
        unsigned srcVarNum = BAD_VAR_NUM;

        if (varNode != nullptr)
        {
            srcVarNum = varNode->gtLclNum;
            assert(srcVarNum < compiler->lvaCount);

            // handle promote situation
            LclVarDsc* varDsc = compiler->lvaTable + srcVarNum;

            // This struct also must live in the stack frame
            // And it can't live in a register (SIMD)
            assert(varDsc->lvType == TYP_STRUCT);
            assert(varDsc->lvOnFrame && !varDsc->lvRegister);

            // We don't split HFA struct
            assert(!varDsc->lvIsHfa());
        }
        else // addrNode is used
        {
            assert(addrNode != nullptr);

            // Generate code to load the address that we need into a register
            genConsumeAddress(addrNode);
            addrReg = addrNode->gtRegNum;

            // If addrReg equal to baseReg, we use the last target register as alternative baseReg.
            // Because the candidate mask for the internal baseReg does not include any of the target register,
            // we can ensure that baseReg, addrReg, and the last target register are not all same.
            assert(baseReg != addrReg);

            // We don't split HFA struct
            assert(!compiler->IsHfa(source->gtObj.gtClass));
        }

        // Put on stack first
        unsigned nextIndex     = treeNode->gtNumRegs;
        unsigned structOffset  = nextIndex * TARGET_POINTER_SIZE;
        int      remainingSize = structSize - structOffset;

        // remainingSize is always multiple of TARGET_POINTER_SIZE
        assert(remainingSize % TARGET_POINTER_SIZE == 0);
        while (remainingSize > 0)
        {
            var_types type = compiler->getJitGCType(gcPtrs[nextIndex]);

            if (varNode != nullptr)
            {
                // Load from our varNumImp source
                emit->emitIns_R_S(INS_ld, emitTypeSize(type), baseReg, srcVarNum, structOffset);
            }
            else
            {
                // check for case of destroying the addrRegister while we still need it
                assert(baseReg != addrReg);

                // Load from our address expression source
                emit->emitIns_R_R_I(INS_ld, emitTypeSize(type), baseReg, addrReg, structOffset);
            }

            // Emit str instruction to store the register into the outgoing argument area
            emit->emitIns_S_R(INS_sd, emitTypeSize(type), baseReg, varNumOut, argOffsetOut);

            argOffsetOut += TARGET_POINTER_SIZE;  // We stored 4-bytes of the struct
            assert(argOffsetOut <= argOffsetMax); // We can't write beyound the outgoing area area
            remainingSize -= TARGET_POINTER_SIZE; // We loaded 4-bytes of the struct
            structOffset += TARGET_POINTER_SIZE;
            nextIndex += 1;
        }

        // We set up the registers in order, so that we assign the last target register `baseReg` is no longer in use,
        // in case we had to reuse the last target register for it.
        structOffset = 0;
        for (unsigned idx = 0; idx < treeNode->gtNumRegs; idx++)
        {
            regNumber targetReg = treeNode->GetRegNumByIdx(idx);
            var_types type      = treeNode->GetRegType(idx);

            if (varNode != nullptr)
            {
                // Load from our varNumImp source
                emit->emitIns_R_S(ins_Load(type), emitTypeSize(type), targetReg, srcVarNum, structOffset);
            }
            else
            {
                // check for case of destroying the addrRegister while we still need it
                if (targetReg == addrReg && idx != treeNode->gtNumRegs - 1)
                {
                    assert(targetReg != baseReg);
                    emit->emitIns_R_R(INS_mov, emitActualTypeSize(type), baseReg, addrReg);
                    addrReg = baseReg;
                }

                // Load from our address expression source
                emit->emitIns_R_R_I(ins_Load(type), emitTypeSize(type), targetReg, addrReg, structOffset);
            }
            structOffset += TARGET_POINTER_SIZE;
        }
    }
    genProduceReg(treeNode);
}
#endif // FEATURE_ARG_SPLIT

//----------------------------------------------------------------------------------
// genMultiRegCallStoreToLocal: store multi-reg return value of a call node to a local
//
// Arguments:
//    treeNode  -  Gentree of GT_STORE_LCL_VAR
//
// Return Value:
//    None
//
// Assumption:
//    The child of store is a multi-reg call node.
//    genProduceReg() on treeNode is made by caller of this routine.
//
void CodeGen::genMultiRegCallStoreToLocal(GenTree* treeNode)
{
    /* FIXME for MIPS: should confirm. */

    assert(treeNode->OperGet() == GT_STORE_LCL_VAR);

    // Structs of size >=9 and <=16 are returned in two return registers on MIPS64 and HFAs.
    assert(varTypeIsStruct(treeNode));

    // Assumption: current implementation requires that a multi-reg
    // var in 'var = call' is flagged as lvIsMultiRegRet to prevent it from
    // being promoted.
    unsigned   lclNum = treeNode->AsLclVarCommon()->gtLclNum;
    LclVarDsc* varDsc = &(compiler->lvaTable[lclNum]);
    noway_assert(varDsc->lvIsMultiRegRet);

    GenTree*     op1       = treeNode->gtGetOp1();
    GenTree*     actualOp1 = op1->gtSkipReloadOrCopy();
    GenTreeCall* call      = actualOp1->AsCall();
    assert(call->HasMultiRegRetVal());

    genConsumeRegs(op1);

    ReturnTypeDesc* pRetTypeDesc = call->GetReturnTypeDesc();
    unsigned        regCount     = pRetTypeDesc->GetReturnRegCount();

    if (treeNode->gtRegNum != REG_NA)
    {
        assert(!"unimplemented on MIPS yet");
        // Right now the only enregistrable multi-reg return types supported are SIMD types.
        assert(varTypeIsSIMD(treeNode));
        assert(regCount != 0);

        regNumber dst = treeNode->gtRegNum;

        // Treat dst register as a homogenous vector with element size equal to the src size
        // Insert pieces in reverse order
        for (int i = regCount - 1; i >= 0; --i)
        {
            var_types type = pRetTypeDesc->GetReturnRegType(i);
            regNumber reg  = call->GetRegNumByIdx(i);
            if (op1->IsCopyOrReload())
            {
                // GT_COPY/GT_RELOAD will have valid reg for those positions
                // that need to be copied or reloaded.
                regNumber reloadReg = op1->AsCopyOrReload()->GetRegNumByIdx(i);
                if (reloadReg != REG_NA)
                {
                    reg = reloadReg;
                }
            }

            assert(reg != REG_NA);
            if (varTypeIsFloating(type))
            {
                // If the register piece was passed in a floating point register
                // Use a vector mov element instruction
                // src is not a vector, so it is in the first element reg[0]
                // mov dst[i], reg[0]
                // This effectively moves from `reg[0]` to `dst[i]`, leaving other dst bits unchanged till further
                // iterations
                // For the case where reg == dst, if we iterate so that we write dst[0] last, we eliminate the need for
                // a temporary
                getEmitter()->emitIns_R_R_I_I(INS_mov, emitTypeSize(type), dst, reg, i, 0);
            }
            else
            {
                // If the register piece was passed in an integer register
                // Use a vector mov from general purpose register instruction
                // mov dst[i], reg
                // This effectively moves from `reg` to `dst[i]`
                getEmitter()->emitIns_R_R_I(INS_mov, emitTypeSize(type), dst, reg, i);
            }
        }

        genProduceReg(treeNode);
    }
    else
    {
        // Stack store
        int offset = 0;
        for (unsigned i = 0; i < regCount; ++i)
        {
            var_types type = pRetTypeDesc->GetReturnRegType(i);
            regNumber reg  = call->GetRegNumByIdx(i);
            if (op1->IsCopyOrReload())
            {
                // GT_COPY/GT_RELOAD will have valid reg for those positions
                // that need to be copied or reloaded.
                regNumber reloadReg = op1->AsCopyOrReload()->GetRegNumByIdx(i);
                if (reloadReg != REG_NA)
                {
                    reg = reloadReg;
                }
            }

            assert(reg != REG_NA);
            getEmitter()->emitIns_S_R(ins_Store(type), emitTypeSize(type), reg, lclNum, offset);
            offset += genTypeSize(type);
        }

        genUpdateLife(treeNode);
        varDsc->lvRegNum = REG_STK;
    }
}

//------------------------------------------------------------------------
// genRangeCheck: generate code for GT_ARR_BOUNDS_CHECK node.
//
void CodeGen::genRangeCheck(GenTree* oper)
{
    noway_assert(oper->OperIsBoundsCheck());
    GenTreeBoundsChk* bndsChk = oper->AsBoundsChk();

    GenTree* arrLen    = bndsChk->gtArrLen;
    GenTree* arrIndex  = bndsChk->gtIndex;
    GenTree* arrRef    = NULL;
    int      lenOffset = 0;

    GenTree*     src1;
    GenTree*     src2;
    emitJumpKind jmpKind = EJ_jmp;

    genConsumeRegs(arrIndex);
    genConsumeRegs(arrLen);

    emitter* emit = getEmitter();
    GenTreeIntConCommon* intConst = nullptr;
    ssize_t imm = 0;
    if (arrIndex->isContainedIntOrIImmed())
    {
        src1    = arrLen;
        src2    = arrIndex;

        intConst = src2->AsIntConCommon();
        imm = intConst->IconValue();

        ssize_t imm2 = splitLow(imm >> 48);

        emit->emitIns_R_I(INS_lui, EA_PTRSIZE, REG_AT, imm2);
        imm2 = splitLow(imm >> 32);
        emit->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm2);

        // dsll(reg, reg, 16);
        emit->emitIns_R_R_I(INS_dsll, EA_8BYTE, REG_AT, REG_AT, 16);
        imm2 = splitLow(imm >> 16);
        emit->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm2);

        // dsll(reg, reg, 16);
        emit->emitIns_R_R_I(INS_dsll, EA_8BYTE, REG_AT, REG_AT, 16);
        imm2 = splitLow(imm);
        emit->emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm2);

        emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, REG_AT, REG_AT, src1->gtRegNum);

        imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
        emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);
        emit->emitIns(INS_nop);
    }
    else
    {
        src1    = arrIndex;
        src2    = arrLen;

        emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, REG_AT, src1->gtRegNum, src2->gtRegNum);

        imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
        emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);
        emit->emitIns(INS_nop);
    }

    var_types bndsChkType = genActualType(src2->TypeGet());
#if DEBUG
    // Bounds checks can only be 32 or 64 bit sized comparisons.
    assert(bndsChkType == TYP_INT || bndsChkType == TYP_LONG);

    // The type of the bounds check should always wide enough to compare against the index.
    assert(emitTypeSize(bndsChkType) >= emitActualTypeSize(src1->TypeGet()));
#endif // DEBUG

    genJumpToThrowHlpBlk(jmpKind, bndsChk->gtThrowKind, bndsChk->gtIndRngFailBB); //EJ_jmp is 6/8-ins.
    //emit->emitIns(INS_nop);
}

//---------------------------------------------------------------------
// genCodeForPhysReg - generate code for a GT_PHYSREG node
//
// Arguments
//    tree - the GT_PHYSREG node
//
// Return value:
//    None
//
void CodeGen::genCodeForPhysReg(GenTreePhysReg* tree)
{
    assert(tree->OperIs(GT_PHYSREG));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;

    if (targetReg != tree->gtSrcReg)
    {
        inst_RV_RV(ins_Copy(targetType), targetReg, tree->gtSrcReg, targetType);
        genTransferRegGCState(targetReg, tree->gtSrcReg);
    }

    genProduceReg(tree);
}

//---------------------------------------------------------------------
// genCodeForNullCheck - generate code for a GT_NULLCHECK node
//
// Arguments
//    tree - the GT_NULLCHECK node
//
// Return value:
//    None
//
void CodeGen::genCodeForNullCheck(GenTreeOp* tree)
{
    assert(tree->OperIs(GT_NULLCHECK));
    assert(!tree->gtOp1->isContained());
    regNumber addrReg = genConsumeReg(tree->gtOp1);

    /* FIXME for MIPS */
    // NOTE: This is high depended on Your CPU's implementation !!!
    // For loongson's CPUs, liking lw $0,offset(base) has specially meaning,
    // which liking an prefetch but ignoring the Vir-addr-translation-exception.
    // So here using non-user-space register K0.
    regNumber targetReg = REG_K0;

    getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, targetReg, addrReg, 0);
}

//------------------------------------------------------------------------
// genOffsetOfMDArrayLowerBound: Returns the offset from the Array object to the
//   lower bound for the given dimension.
//
// Arguments:
//    elemType  - the element type of the array
//    rank      - the rank of the array
//    dimension - the dimension for which the lower bound offset will be returned.
//
// Return Value:
//    The offset.
// TODO-Cleanup: move to CodeGenCommon.cpp

// static
unsigned CodeGen::genOffsetOfMDArrayLowerBound(var_types elemType, unsigned rank, unsigned dimension)
{
    // Note that the lower bound and length fields of the Array object are always TYP_INT
    return compiler->eeGetArrayDataOffset(elemType) + genTypeSize(TYP_INT) * (dimension + rank);
}

//------------------------------------------------------------------------
// genOffsetOfMDArrayLength: Returns the offset from the Array object to the
//   size for the given dimension.
//
// Arguments:
//    elemType  - the element type of the array
//    rank      - the rank of the array
//    dimension - the dimension for which the lower bound offset will be returned.
//
// Return Value:
//    The offset.
// TODO-Cleanup: move to CodeGenCommon.cpp

// static
unsigned CodeGen::genOffsetOfMDArrayDimensionSize(var_types elemType, unsigned rank, unsigned dimension)
{
    // Note that the lower bound and length fields of the Array object are always TYP_INT
    return compiler->eeGetArrayDataOffset(elemType) + genTypeSize(TYP_INT) * dimension;
}

//------------------------------------------------------------------------
// genCodeForArrIndex: Generates code to bounds check the index for one dimension of an array reference,
//                     producing the effective index by subtracting the lower bound.
//
// Arguments:
//    arrIndex - the node for which we're generating code
//
// Return Value:
//    None.
//
void CodeGen::genCodeForArrIndex(GenTreeArrIndex* arrIndex)
{
    emitter*  emit      = getEmitter();
    GenTree*  arrObj    = arrIndex->ArrObj();
    GenTree*  indexNode = arrIndex->IndexExpr();
    regNumber arrReg    = genConsumeReg(arrObj);
    regNumber indexReg  = genConsumeReg(indexNode);
    regNumber tgtReg    = arrIndex->gtRegNum;
    noway_assert(tgtReg != REG_NA);

    // We will use a temp register to load the lower bound and dimension size values.

    regNumber tmpReg = arrIndex->GetSingleTempReg();
    assert(tgtReg != tmpReg);

    unsigned  dim      = arrIndex->gtCurrDim;
    unsigned  rank     = arrIndex->gtArrRank;
    var_types elemType = arrIndex->gtArrElemType;
    unsigned  offset;

    offset = genOffsetOfMDArrayLowerBound(elemType, rank, dim);
    emit->emitIns_R_R_I(INS_lw, EA_4BYTE, tmpReg, arrReg, offset);
    emit->emitIns_R_R_R(INS_subu, EA_4BYTE, tgtReg, indexReg, tmpReg);

    offset = genOffsetOfMDArrayDimensionSize(elemType, rank, dim);
    emit->emitIns_R_R_I(INS_lw, EA_4BYTE, tmpReg, arrReg, offset);
    emit->emitIns_R_R_R(INS_sltu, EA_PTRSIZE, REG_AT, tgtReg, tmpReg);

    ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
    emit->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);
    emit->emitIns(INS_nop);

    genJumpToThrowHlpBlk(EJ_jmp, SCK_RNGCHK_FAIL); //EJ_jmp is 6/8-ins.
    //emit->emitIns(INS_nop);

    genProduceReg(arrIndex);
}

//------------------------------------------------------------------------
// genCodeForArrOffset: Generates code to compute the flattened array offset for
//    one dimension of an array reference:
//        result = (prevDimOffset * dimSize) + effectiveIndex
//    where dimSize is obtained from the arrObj operand
//
// Arguments:
//    arrOffset - the node for which we're generating code
//
// Return Value:
//    None.
//
// Notes:
//    dimSize and effectiveIndex are always non-negative, the former by design,
//    and the latter because it has been normalized to be zero-based.

void CodeGen::genCodeForArrOffset(GenTreeArrOffs* arrOffset)
{
    GenTree*  offsetNode = arrOffset->gtOffset;
    GenTree*  indexNode  = arrOffset->gtIndex;
    regNumber tgtReg     = arrOffset->gtRegNum;

    noway_assert(tgtReg != REG_NA);

    if (!offsetNode->IsIntegralConst(0))
    {
        emitter*  emit      = getEmitter();
        regNumber offsetReg = genConsumeReg(offsetNode);
        regNumber indexReg  = genConsumeReg(indexNode);
        regNumber arrReg    = genConsumeReg(arrOffset->gtArrObj);
        noway_assert(offsetReg != REG_NA);
        noway_assert(indexReg != REG_NA);
        noway_assert(arrReg != REG_NA);

        regNumber tmpReg = arrOffset->GetSingleTempReg();

        unsigned  dim      = arrOffset->gtCurrDim;
        unsigned  rank     = arrOffset->gtArrRank;
        var_types elemType = arrOffset->gtArrElemType;
        unsigned  offset   = genOffsetOfMDArrayDimensionSize(elemType, rank, dim);

        // Load tmpReg with the dimension size and evaluate
        // tgtReg = offsetReg*tmpReg + indexReg.
        emit->emitIns_R_R_I(INS_lw, EA_4BYTE, tmpReg, arrReg, offset);
        emit->emitIns_R_R(INS_dmultu, EA_PTRSIZE, tmpReg, offsetReg);
        emit->emitIns_R(INS_mflo, EA_PTRSIZE, REG_AT);
        emit->emitIns_R_R_R(INS_daddu, EA_PTRSIZE, tgtReg, REG_AT, indexReg);
    }
    else
    {
        regNumber indexReg = genConsumeReg(indexNode);
        if (indexReg != tgtReg)
        {
            inst_RV_RV(INS_mov, tgtReg, indexReg, TYP_INT);
        }
    }
    genProduceReg(arrOffset);
}

//------------------------------------------------------------------------
// genCodeForShift: Generates the code sequence for a GenTree node that
// represents a bit shift or rotate operation (<<, >>, >>>, rol, ror).
//
// Arguments:
//    tree - the bit shift node (that specifies the type of bit shift to perform).
//
// Assumptions:
//    a) All GenTrees are register allocated.
//
void CodeGen::genCodeForShift(GenTree* tree)
{
    //var_types   targetType = tree->TypeGet();
    //genTreeOps  oper       = tree->OperGet();
    instruction ins        = genGetInsForOper(tree);
    emitAttr    size       = emitActualTypeSize(tree);

    assert(tree->gtRegNum != REG_NA);

    genConsumeOperands(tree->AsOp());

    GenTree* operand = tree->gtGetOp1();
    GenTree* shiftBy = tree->gtGetOp2();
    if (!shiftBy->IsCnsIntOrI())
    {
        getEmitter()->emitIns_R_R_R(ins, size, tree->gtRegNum, operand->gtRegNum, shiftBy->gtRegNum);
    }
    else
    {
        unsigned shiftByImm = (unsigned)shiftBy->gtIntCon.gtIconVal;
        //should check shiftByImm for mips32-ins.
        unsigned immWidth = emitter::getBitWidth(size); // For MIPS64, immWidth will be set to 32 or 64
        shiftByImm &= (immWidth - 1);

        if (ins == INS_dsll && shiftByImm >= 32 && shiftByImm < 64)
        {
            ins = INS_dsll32;
            shiftByImm -= 32;
        }
        else if (ins == INS_dsra && shiftByImm >= 32 && shiftByImm < 64)
        {
            ins = INS_dsra32;
            shiftByImm -= 32;
        }
        else if (ins == INS_dsrl && shiftByImm >= 32 && shiftByImm < 64)
        {
            ins = INS_dsrl32;
            shiftByImm -= 32;
        }
        else if (ins == INS_drotr && shiftByImm >= 32 && shiftByImm < 64)
        {
            ins = INS_drotr32;
            shiftByImm -= 32;
        }

        getEmitter()->emitIns_R_R_I(ins, size, tree->gtRegNum, operand->gtRegNum, shiftByImm);
    }

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForLclAddr: Generates the code for GT_LCL_FLD_ADDR/GT_LCL_VAR_ADDR.
//
// Arguments:
//    tree - the node.
//
void CodeGen::genCodeForLclAddr(GenTree* tree)
{
    assert(tree->OperIs(GT_LCL_FLD_ADDR, GT_LCL_VAR_ADDR));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;

    // Address of a local var.
    noway_assert((targetType == TYP_BYREF) || (targetType == TYP_I_IMPL));

    emitAttr size = emitTypeSize(targetType);

    inst_RV_TT(INS_lea, targetReg, tree, 0, size);
    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForLclFld: Produce code for a GT_LCL_FLD node.
//
// Arguments:
//    tree - the GT_LCL_FLD node
//
void CodeGen::genCodeForLclFld(GenTreeLclFld* tree)
{
    assert(tree->OperIs(GT_LCL_FLD));

    var_types targetType = tree->TypeGet();
    regNumber targetReg  = tree->gtRegNum;
    emitter*  emit       = getEmitter();

    NYI_IF(targetType == TYP_STRUCT, "GT_LCL_FLD: struct load local field not supported");
    assert(targetReg != REG_NA);

    emitAttr size   = emitTypeSize(targetType);
    unsigned offs   = tree->gtLclOffs;
    unsigned varNum = tree->gtLclNum;
    assert(varNum < compiler->lvaCount);

    emit->emitIns_R_S(ins_Load(targetType), size, targetReg, varNum, offs);

    genProduceReg(tree);
}

//------------------------------------------------------------------------
// genCodeForIndexAddr: Produce code for a GT_INDEX_ADDR node.
//
// Arguments:
//    tree - the GT_INDEX_ADDR node
//
void CodeGen::genCodeForIndexAddr(GenTreeIndexAddr* node)
{
    GenTree* const base  = node->Arr();
    GenTree* const index = node->Index();

    genConsumeReg(base);
    genConsumeReg(index);

    // NOTE: `genConsumeReg` marks the consumed register as not a GC pointer, as it assumes that the input registers
    // die at the first instruction generated by the node. This is not the case for `INDEX_ADDR`, however, as the
    // base register is multiply-used. As such, we need to mark the base register as containing a GC pointer until
    // we are finished generating the code for this node.

    gcInfo.gcMarkRegPtrVal(base->gtRegNum, base->TypeGet());
    assert(!varTypeIsGC(index->TypeGet()));

    // The index is never contained, even if it is a constant.
    assert(index->isUsedFromReg());

    const regNumber tmpReg = node->GetSingleTempReg();

    // Generate the bounds check if necessary.
    if ((node->gtFlags & GTF_INX_RNGCHK) != 0)
    {
        getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, tmpReg, base->gtRegNum, node->gtLenOffset);
        //   if (index >= tmpReg)
        //   {
        //     JumpToThrowHlpBlk;
        //   }
        //
        //   sltu  AT, index, tmpReg
        //   bne  AT, zero, RngChkExit
        //   nop
        // IndRngFail:
        //   ...
        //   nop
        // RngChkExit:
        //   nop
        getEmitter()->emitIns_R_R_R(INS_sltu, emitActualTypeSize(index->TypeGet()), REG_AT, index->gtRegNum, tmpReg);
        ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
        getEmitter()->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);
        getEmitter()->emitIns(INS_nop);

        genJumpToThrowHlpBlk(EJ_jmp, SCK_RNGCHK_FAIL, node->gtIndRngFailBB);//NOTE:this is 8-ins.
        //getEmitter()->emitIns(INS_nop);
    }

    emitAttr attr = emitActualTypeSize(node);
    // Can we use a ScaledAdd instruction?
    //
    if (isPow2(node->gtElemSize) && (node->gtElemSize <= 32768))
    {
        DWORD scale;
        BitScanForward(&scale, node->gtElemSize);

        // dest = base + index * scale
        genScaledAdd(emitActualTypeSize(node), node->gtRegNum, base->gtRegNum, index->gtRegNum, scale);
    }
    else // we have to load the element size and use a MADD (multiply-add) instruction
    {
        // tmpReg = element size
        CodeGen::genSetRegToIcon(tmpReg, (ssize_t)node->gtElemSize, TYP_INT);

        // dest = index * tmpReg + base
        if (attr == EA_4BYTE)
        {
            getEmitter()->emitIns_R_R(INS_multu, EA_4BYTE, index->gtRegNum, tmpReg);
            getEmitter()->emitIns_R(INS_mflo, attr, tmpReg);
            getEmitter()->emitIns_R_R_R(INS_addu, attr, node->gtRegNum, tmpReg, base->gtRegNum);
        }
        else
        {
            getEmitter()->emitIns_R_R(INS_dmultu, EA_PTRSIZE, index->gtRegNum, tmpReg);
            getEmitter()->emitIns_R(INS_mflo, attr, tmpReg);
            getEmitter()->emitIns_R_R_R(INS_daddu, attr, node->gtRegNum, tmpReg, base->gtRegNum);
        }
    }

    // dest = dest + elemOffs
    getEmitter()->emitIns_R_R_I(INS_daddiu, attr, node->gtRegNum, node->gtRegNum, node->gtElemOffset);

    gcInfo.gcMarkRegSetNpt(base->gtGetRegMask());

    genProduceReg(node);
}

//------------------------------------------------------------------------
// genCodeForIndir: Produce code for a GT_IND node.
//
// Arguments:
//    tree - the GT_IND node
//
void CodeGen::genCodeForIndir(GenTreeIndir* tree)
{
    assert(tree->OperIs(GT_IND));

#ifdef FEATURE_SIMD
    // Handling of Vector3 type values loaded through indirection.
    if (tree->TypeGet() == TYP_SIMD12)
    {
        genLoadIndTypeSIMD12(tree);
        return;
    }
#endif // FEATURE_SIMD

    var_types   type      = tree->TypeGet();
    instruction ins       = ins_Load(type);
    instruction ins2      = INS_none;
    regNumber   targetReg = tree->gtRegNum;
    regNumber   tmpReg = targetReg;
    emitAttr    attr = emitActualTypeSize(type);
    int offset = 0;

    genConsumeAddress(tree->Addr());

    bool emitBarrier = false;

    {
        bool addrIsInReg   = tree->Addr()->isUsedFromReg();
        bool addrIsUnaligned = ((tree->gtFlags & GTF_IND_UNALIGNED) != 0);

        if ((ins == INS_lb) && addrIsInReg && addrIsUnaligned)
        {
            //assert(!"-------Confirm for MIPS64--lb-----\n");//ins = INS_lb;
        }
        else if ((ins == INS_lh) && addrIsInReg && addrIsUnaligned)
        {
            ins = INS_lb;
            ins2 = INS_lb;
            attr = EA_1BYTE;
            assert(REG_AT != targetReg);

            getEmitter()->emitIns_R_R_I(ins2, attr, REG_AT, tree->Addr()->gtRegNum, 1);
            getEmitter()->emitIns_R_R_I(INS_dsll, EA_8BYTE, REG_AT, REG_AT, 8);
            assert(!tree->Addr()->isContained());
        }
        else if ((ins == INS_lw) && addrIsInReg && addrIsUnaligned)
        {
            ins = INS_lwr;
            ins2 = INS_lwl;
            tmpReg = (tmpReg == tree->Addr()->gtRegNum) ? REG_AT : tmpReg;
            offset = 3;
            assert(REG_AT != targetReg);
            assert(!tree->Addr()->isContained());
        }
        else if ((ins == INS_ld) && addrIsInReg && addrIsUnaligned && genIsValidIntReg(targetReg))
        {
            ins = INS_ldr;
            ins2 = INS_ldl;
            tmpReg = (tmpReg == tree->Addr()->gtRegNum) ? REG_AT : tmpReg;
            offset = 7;
            assert(REG_AT != targetReg);
            assert(!tree->Addr()->isContained());
        }
        else
        {
            if ((tree->gtFlags & GTF_IND_VOLATILE) != 0)
                emitBarrier = true;
        }
    }

    getEmitter()->emitInsLoadStoreOp(ins, attr, tmpReg, tree);

    if (emitBarrier)
    {
        instGen_MemoryBarrier(INS_BARRIER_RMB);
    }
    else if (ins2 == INS_lb)
    {
        getEmitter()->emitIns_R_R_I_I(INS_dins, EA_8BYTE, REG_AT, targetReg, 0, 8);
        getEmitter()->emitIns_R_R_I(INS_ori, EA_8BYTE, targetReg, REG_AT, 0);
    }
    else if (ins2 != INS_none)
    {
        getEmitter()->emitIns_R_R_I(ins2, attr, tmpReg, tree->Addr()->gtRegNum, offset);
        if (tmpReg != targetReg)
            getEmitter()->emitIns_R_R_I(INS_ori, EA_8BYTE, targetReg, tmpReg, 0);
    }

    genProduceReg(tree);
}

//----------------------------------------------------------------------------------
// genCodeForCpBlkHelper - Generate code for a CpBlk node by the means of the VM memcpy helper call
//
// Arguments:
//    cpBlkNode - the GT_STORE_[BLK|OBJ|DYN_BLK]
//
// Preconditions:
//   The register assignments have been set appropriately.
//   This is validated by genConsumeBlockOp().
//
void CodeGen::genCodeForCpBlkHelper(GenTreeBlk* cpBlkNode)
{
    // Destination address goes in arg0, source address goes in arg1, and size goes in arg2.
    // genConsumeBlockOp takes care of this for us.
    genConsumeBlockOp(cpBlkNode, REG_ARG_0, REG_ARG_1, REG_ARG_2);

    if (cpBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a full memory barrier before a volatile CpBlk operation
        instGen_MemoryBarrier();
    }

    genEmitHelperCall(CORINFO_HELP_MEMCPY, 0, EA_UNKNOWN);

    if (cpBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a INS_BARRIER_RMB after a volatile CpBlk operation
        instGen_MemoryBarrier(INS_BARRIER_RMB);
    }
}

//----------------------------------------------------------------------------------
// genCodeForCpBlkUnroll: Generates CpBlk code by performing a loop unroll
//
// Arguments:
//    cpBlkNode  -  Copy block node
//
// Return Value:
//    None
//
// Assumption:
//  The size argument of the CpBlk node is a constant and <= CPBLK_UNROLL_LIMIT bytes.
//
void CodeGen::genCodeForCpBlkUnroll(GenTreeBlk* cpBlkNode)
{
    // Make sure we got the arguments of the cpblk operation in the right registers
    unsigned size    = cpBlkNode->Size();
    GenTree* dstAddr = cpBlkNode->Addr();
    GenTree* source  = cpBlkNode->Data();
    GenTree* srcAddr = nullptr;

    assert((size != 0) && (size <= CPBLK_UNROLL_LIMIT));

    emitter* emit = getEmitter();

    if (dstAddr->isUsedFromReg())
    {
        genConsumeReg(dstAddr);
    }

    if (cpBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a full memory barrier before a volatile CpBlkUnroll operation
        instGen_MemoryBarrier();
    }

    if (source->gtOper == GT_IND)
    {
        srcAddr = source->gtGetOp1();
        if (srcAddr->isUsedFromReg())
        {
            genConsumeReg(srcAddr);
        }
    }
    else
    {
        noway_assert(source->IsLocal());
        // TODO-Cleanup: Consider making the addrForm() method in Rationalize public, e.g. in GenTree.
        // OR: transform source to GT_IND(GT_LCL_VAR_ADDR)
        if (source->OperGet() == GT_LCL_VAR)
        {
            source->SetOper(GT_LCL_VAR_ADDR);
        }
        else
        {
            assert(source->OperGet() == GT_LCL_FLD);
            source->SetOper(GT_LCL_FLD_ADDR);
        }
        srcAddr = source;
    }

    unsigned offset = 0;

    // Grab the integer temp register to emit the loads and stores.
    regNumber tmpReg = cpBlkNode->ExtractTempReg(RBM_ALLINT);

    if (size >= 2 * REGSIZE_BYTES)
    {
        regNumber tmp2Reg = cpBlkNode->ExtractTempReg(RBM_ALLINT);

        size_t slots = size / (2 * REGSIZE_BYTES);

        while (slots-- > 0)
        {
            // Load
            if (srcAddr->OperIsLocalAddr())
            {
                unsigned lclOffs = 0;

                if (srcAddr->gtOper == GT_LCL_FLD_ADDR)
                    lclOffs += srcAddr->gtLclFld.gtLclOffs;

                emit->emitIns_R_S(INS_ld, EA_8BYTE, tmpReg, srcAddr->gtLclVarCommon.gtLclNum, offset + lclOffs);
                emit->emitIns_R_S(INS_ld, EA_8BYTE, tmp2Reg, srcAddr->gtLclVarCommon.gtLclNum, offset + lclOffs + 8);
            }
            else
            {
                emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmpReg, srcAddr->gtRegNum, offset);
                emit->emitIns_R_R_I(INS_ld, EA_8BYTE, tmp2Reg, srcAddr->gtRegNum, offset+8);
            }

            // Store
            if (dstAddr->OperIsLocalAddr())
            {
                unsigned lclOffs = 0;

                if (dstAddr->gtOper == GT_LCL_FLD_ADDR)
                    lclOffs += dstAddr->gtLclFld.gtLclOffs;

                emit->emitIns_S_R(INS_sd, EA_8BYTE, tmpReg, dstAddr->gtLclVarCommon.gtLclNum, offset + lclOffs);
                emit->emitIns_S_R(INS_sd, EA_8BYTE, tmp2Reg, dstAddr->gtLclVarCommon.gtLclNum, offset+ lclOffs + 8);
            }
            else
            {
                emit->emitIns_R_R_I(INS_sd, EA_8BYTE, tmpReg, dstAddr->gtRegNum, offset);
                emit->emitIns_R_R_I(INS_sd, EA_8BYTE, tmp2Reg, dstAddr->gtRegNum, offset+8);
            }

            offset += 2 * REGSIZE_BYTES;
        }
    }

    // Fill the remainder (15 bytes or less) if there's one.
    if ((size & 0xf) != 0)
    {
        if ((size & 8) != 0)
        {
            genCodeForLoadOffset(INS_ld, EA_8BYTE, tmpReg, srcAddr, offset);
            genCodeForStoreOffset(INS_sd, EA_8BYTE, tmpReg, dstAddr, offset);
            offset += 8;
        }
        if ((size & 4) != 0)
        {
            genCodeForLoadOffset(INS_lw, EA_4BYTE, tmpReg, srcAddr, offset);
            genCodeForStoreOffset(INS_sw, EA_4BYTE, tmpReg, dstAddr, offset);
            offset += 4;
        }
        if ((size & 2) != 0)
        {
            genCodeForLoadOffset(INS_lh, EA_2BYTE, tmpReg, srcAddr, offset);
            genCodeForStoreOffset(INS_sh, EA_2BYTE, tmpReg, dstAddr, offset);
            offset += 2;
        }
        if ((size & 1) != 0)
        {
            genCodeForLoadOffset(INS_lb, EA_1BYTE, tmpReg, srcAddr, offset);
            genCodeForStoreOffset(INS_sb, EA_1BYTE, tmpReg, dstAddr, offset);
        }
    }

    if (cpBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a INS_BARRIER_RMB after a volatile CpBlkUnroll operation
        instGen_MemoryBarrier(INS_BARRIER_RMB);
    }
}

//------------------------------------------------------------------------
// genCodeForInitBlkHelper - Generate code for an InitBlk node by the means of the VM memcpy helper call
//
// Arguments:
//    initBlkNode - the GT_STORE_[BLK|OBJ|DYN_BLK]
//
// Preconditions:
//   The register assignments have been set appropriately.
//   This is validated by genConsumeBlockOp().
//
void CodeGen::genCodeForInitBlkHelper(GenTreeBlk* initBlkNode)
{
    // Size goes in arg2, source address goes in arg1, and size goes in arg2.
    // genConsumeBlockOp takes care of this for us.
    genConsumeBlockOp(initBlkNode, REG_ARG_0, REG_ARG_1, REG_ARG_2);

    if (initBlkNode->gtFlags & GTF_BLK_VOLATILE)
    {
        // issue a full memory barrier before a volatile initBlock Operation
        instGen_MemoryBarrier();
    }

    genEmitHelperCall(CORINFO_HELP_MEMSET, 0, EA_UNKNOWN);
}

// Generate code for a load from some address + offset
//   base: tree node which can be either a local address or arbitrary node
//   offset: distance from the base from which to load
void CodeGen::genCodeForLoadOffset(instruction ins, emitAttr size, regNumber dst, GenTree* base, unsigned offset)
{
    emitter* emit = getEmitter();

    if (base->OperIsLocalAddr())
    {
        if (base->gtOper == GT_LCL_FLD_ADDR)
            offset += base->gtLclFld.gtLclOffs;
        emit->emitIns_R_S(ins, size, dst, base->gtLclVarCommon.gtLclNum, offset);
    }
    else
    {
        emit->emitIns_R_R_I(ins, size, dst, base->gtRegNum, offset);
    }
}

// Generate code for a store to some address + offset
//   base: tree node which can be either a local address or arbitrary node
//   offset: distance from the base from which to load
void CodeGen::genCodeForStoreOffset(instruction ins, emitAttr size, regNumber src, GenTree* base, unsigned offset)
{
    emitter* emit = getEmitter();

    if (base->OperIsLocalAddr())
    {
        if (base->gtOper == GT_LCL_FLD_ADDR)
            offset += base->gtLclFld.gtLclOffs;
        emit->emitIns_S_R(ins, size, src, base->gtLclVarCommon.gtLclNum, offset);
    }
    else
    {
        emit->emitIns_R_R_I(ins, size, src, base->gtRegNum, offset);
    }
}

//------------------------------------------------------------------------
// genRegCopy: Produce code for a GT_COPY node.
//
// Arguments:
//    tree - the GT_COPY node
//
// Notes:
//    This will copy the register(s) produced by this node's source, to
//    the register(s) allocated to this GT_COPY node.
//    It has some special handling for these cases:
//    - when the source and target registers are in different register files
//      (note that this is *not* a conversion).
//    - when the source is a lclVar whose home location is being moved to a new
//      register (rather than just being copied for temporary use).
//
void CodeGen::genRegCopy(GenTree* treeNode)
{
    assert(treeNode->OperGet() == GT_COPY);
    GenTree* op1 = treeNode->gtOp.gtOp1;

    regNumber sourceReg = genConsumeReg(op1);

    if (op1->IsMultiRegNode())
    {
        noway_assert(!op1->IsCopyOrReload());
        unsigned regCount = op1->GetMultiRegCount();
        for (unsigned i = 0; i < regCount; i++)
        {
            regNumber srcReg  = op1->GetRegByIndex(i);
            regNumber tgtReg  = treeNode->AsCopyOrReload()->GetRegNumByIdx(i);
            var_types regType = op1->GetRegTypeByIndex(i);
            inst_RV_RV(ins_Copy(regType), tgtReg, srcReg, regType);
        }
    }
    else
    {
        var_types targetType = treeNode->TypeGet();
        regNumber targetReg  = treeNode->gtRegNum;
        assert(targetReg != REG_NA);
        assert(targetType != TYP_STRUCT);

        // Check whether this node and the node from which we're copying the value have the same
        // register type.
        // This can happen if (currently iff) we have a SIMD vector type that fits in an integer
        // register, in which case it is passed as an argument, or returned from a call,
        // in an integer register and must be copied if it's in a floating point register.

        bool srcFltReg = (varTypeIsFloating(op1) || varTypeIsSIMD(op1));
        bool tgtFltReg = (varTypeIsFloating(treeNode) || varTypeIsSIMD(treeNode));
        if (srcFltReg != tgtFltReg)
        {
            inst_RV_RV(op1->TypeGet() == TYP_FLOAT ? INS_mov_s : INS_mov_d, targetReg, sourceReg, targetType);
        }
        else
        {
            inst_RV_RV(ins_Copy(targetType), targetReg, sourceReg, targetType);
        }
    }

    if (op1->IsLocal())
    {
        // The lclVar will never be a def.
        // If it is a last use, the lclVar will be killed by genConsumeReg(), as usual, and genProduceReg will
        // appropriately set the gcInfo for the copied value.
        // If not, there are two cases we need to handle:
        // - If this is a TEMPORARY copy (indicated by the GTF_VAR_DEATH flag) the variable
        //   will remain live in its original register.
        //   genProduceReg() will appropriately set the gcInfo for the copied value,
        //   and genConsumeReg will reset it.
        // - Otherwise, we need to update register info for the lclVar.

        GenTreeLclVarCommon* lcl = op1->AsLclVarCommon();
        assert((lcl->gtFlags & GTF_VAR_DEF) == 0);

        if ((lcl->gtFlags & GTF_VAR_DEATH) == 0 && (treeNode->gtFlags & GTF_VAR_DEATH) == 0)
        {
            LclVarDsc* varDsc = &compiler->lvaTable[lcl->gtLclNum];

            // If we didn't just spill it (in genConsumeReg, above), then update the register info
            if (varDsc->lvRegNum != REG_STK)
            {
                // The old location is dying
                genUpdateRegLife(varDsc, /*isBorn*/ false, /*isDying*/ true DEBUGARG(op1));

                gcInfo.gcMarkRegSetNpt(genRegMask(op1->gtRegNum));

                genUpdateVarReg(varDsc, treeNode);

#ifdef USING_VARIABLE_LIVE_RANGE
                // Report the home change for this variable
                varLiveKeeper->siUpdateVariableLiveRange(varDsc, lcl->gtLclNum);
#endif // USING_VARIABLE_LIVE_RANGE

                // The new location is going live
                genUpdateRegLife(varDsc, /*isBorn*/ true, /*isDying*/ false DEBUGARG(treeNode));
            }
        }
    }

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCallInstruction: Produce code for a GT_CALL node
//
void CodeGen::genCallInstruction(GenTreeCall* call)
{
    gtCallTypes callType = (gtCallTypes)call->gtCallType;

    IL_OFFSETX ilOffset = BAD_IL_OFFSET;

    // all virtuals should have been expanded into a control expression
    assert(!call->IsVirtual() || call->gtControlExpr || call->gtCallAddr);

    // Consume all the arg regs
    for (GenTree* list = call->gtCallLateArgs; list; list = list->MoveNext())
    {
        assert(list->OperIsList());

        GenTree* argNode = list->Current();

        fgArgTabEntry* curArgTabEntry = compiler->gtArgEntryByNode(call, argNode);
        assert(curArgTabEntry);

        // GT_RELOAD/GT_COPY use the child node
        argNode = argNode->gtSkipReloadOrCopy();

        if (curArgTabEntry->regNum == REG_STK)
            continue;

        // Deal with multi register passed struct args.
        if (argNode->OperGet() == GT_FIELD_LIST)
        {
            GenTreeArgList* argListPtr   = argNode->AsArgList();
            unsigned        iterationNum = 0;
            regNumber       argReg       = curArgTabEntry->regNum;
            for (; argListPtr != nullptr; argListPtr = argListPtr->Rest(), iterationNum++)
            {
                GenTree* putArgRegNode = argListPtr->gtOp.gtOp1;
                assert(putArgRegNode->gtOper == GT_PUTARG_REG);

                genConsumeReg(putArgRegNode);
            }
        }
#if FEATURE_ARG_SPLIT
        else if (curArgTabEntry->isSplit)
        {
            assert(curArgTabEntry->numRegs >= 1);
            genConsumeArgSplitStruct(argNode->AsPutArgSplit());
        }
#endif // FEATURE_ARG_SPLIT
        else
        {
            regNumber argReg = curArgTabEntry->regNum;
            genConsumeReg(argNode);
            if (argNode->gtRegNum != argReg)
            {
                inst_RV_RV(ins_Move_Extend(argNode->TypeGet(), true), argReg, argNode->gtRegNum);
            }
        }
    }

    // Insert a null check on "this" pointer if asked.
    if (call->NeedsNullCheck())
    {
        const regNumber regThis = genGetThisArgReg(call);

        // Ditto as genCodeForNullCheck
        getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, REG_K0, regThis, 0);
    }

    // Either gtControlExpr != null or gtCallAddr != null or it is a direct non-virtual call to a user or helper
    // method.
    CORINFO_METHOD_HANDLE methHnd;
    GenTree*              target = call->gtControlExpr;
    if (callType == CT_INDIRECT)
    {
        assert(target == nullptr);
        target  = call->gtCallAddr;
        methHnd = nullptr;
    }
    else
    {
        methHnd = call->gtCallMethHnd;
    }

    CORINFO_SIG_INFO* sigInfo = nullptr;
#ifdef DEBUG
    // Pass the call signature information down into the emitter so the emitter can associate
    // native call sites with the signatures they were generated from.
    if (callType != CT_HELPER)
    {
        sigInfo = call->callSig;
    }
#endif // DEBUG

    // If fast tail call, then we are done.  In this case we setup the args (both reg args
    // and stack args in incoming arg area) and call target.  Epilog sequence would
    // generate "br <reg>".
    if (call->IsFastTailCall())
    {
        // Don't support fast tail calling JIT helpers
        assert(callType != CT_HELPER);

        // Fast tail calls materialize call target either in gtControlExpr or in gtCallAddr.
        assert(target != nullptr);

        genConsumeReg(target);

        // Use T9 on MIPS64 as the call target register.
        if (target->gtRegNum != REG_FASTTAILCALL_TARGET)
        {
            inst_RV_RV(INS_mov, REG_FASTTAILCALL_TARGET, target->gtRegNum);
        }

        return;
    }

    // For a pinvoke to unmanaged code we emit a label to clear
    // the GC pointer state before the callsite.
    // We can't utilize the typical lazy killing of GC pointers
    // at (or inside) the callsite.
    if (compiler->killGCRefs(call))
    {
        genDefineTempLabel(genCreateTempLabel());
    }

    // Determine return value size(s).
    ReturnTypeDesc* pRetTypeDesc  = call->GetReturnTypeDesc();
    emitAttr        retSize       = EA_PTRSIZE;
    emitAttr        secondRetSize = EA_UNKNOWN;

    if (call->HasMultiRegRetVal())
    {
        retSize       = emitTypeSize(pRetTypeDesc->GetReturnRegType(0));
        secondRetSize = emitTypeSize(pRetTypeDesc->GetReturnRegType(1));
    }
    else
    {
        assert(call->gtType != TYP_STRUCT);

        if (call->gtType == TYP_REF)
        {
            retSize = EA_GCREF;
        }
        else if (call->gtType == TYP_BYREF)
        {
            retSize = EA_BYREF;
        }
    }

    // We need to propagate the IL offset information to the call instruction, so we can emit
    // an IL to native mapping record for the call, to support managed return value debugging.
    // We don't want tail call helper calls that were converted from normal calls to get a record,
    // so we skip this hash table lookup logic in that case.
    if (compiler->opts.compDbgInfo && compiler->genCallSite2ILOffsetMap != nullptr && !call->IsTailCall())
    {
        (void)compiler->genCallSite2ILOffsetMap->Lookup(call, &ilOffset);
    }

    if (target != nullptr)
    {
        // A call target can not be a contained indirection
        assert(!target->isContainedIndir());

        genConsumeReg(target);

        // We have already generated code for gtControlExpr evaluating it into a register.
        // We just need to emit "call reg" in this case.
        //
        assert(genIsValidIntReg(target->gtRegNum));

        genEmitCall(emitter::EC_INDIR_R, methHnd,
                    INDEBUG_LDISASM_COMMA(sigInfo) nullptr, // addr
                    retSize MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(secondRetSize), ilOffset, target->gtRegNum);
    }
    else
    {
        // Generate a direct call to a non-virtual user defined or helper method
        assert(callType == CT_HELPER || callType == CT_USER_FUNC);

        void* addr = nullptr;
#ifdef FEATURE_READYTORUN_COMPILER
        if (call->gtEntryPoint.addr != NULL)
        {
            assert(call->gtEntryPoint.accessType == IAT_VALUE);
            addr = call->gtEntryPoint.addr;
        }
        else
#endif // FEATURE_READYTORUN_COMPILER
            if (callType == CT_HELPER)
        {
            CorInfoHelpFunc helperNum = compiler->eeGetHelperNum(methHnd);
            noway_assert(helperNum != CORINFO_HELP_UNDEF);

            void* pAddr = nullptr;
            addr        = compiler->compGetHelperFtn(helperNum, (void**)&pAddr);
            assert(pAddr == nullptr);
        }
        else
        {
            // Direct call to a non-virtual user function.
            addr = call->gtDirectCallAddress;
        }

        assert(addr != nullptr);

// Non-virtual direct call to known addresses
        {
            genEmitCall(emitter::EC_FUNC_TOKEN, methHnd, INDEBUG_LDISASM_COMMA(sigInfo) addr,
                        retSize MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(secondRetSize), ilOffset);
        }

#if 0
        // Use this path if you want to load an absolute call target using
        //  a sequence of movs followed by an indirect call (blr instruction)
        // If this path is enabled, we need to ensure that REG_IP0 is assigned during Lowering.

        // Load the call target address in x16
        instGen_Set_Reg_To_Imm(EA_8BYTE, REG_IP0, (ssize_t) addr);

        // indirect call to constant address in IP0
        genEmitCall(emitter::EC_INDIR_R,
                    methHnd,
                    INDEBUG_LDISASM_COMMA(sigInfo)
                    nullptr, //addr
                    retSize,
                    secondRetSize,
                    ilOffset,
                    REG_IP0);
#endif
    }

    // if it was a pinvoke we may have needed to get the address of a label
    if (genPendingCallLabel)
    {
        assert(call->IsUnmanaged());
        genDefineTempLabel(genPendingCallLabel);
        genPendingCallLabel = nullptr;
    }

    // Update GC info:
    // All Callee arg registers are trashed and no longer contain any GC pointers.
    // TODO-Bug?: As a matter of fact shouldn't we be killing all of callee trashed regs here?
    // For now we will assert that other than arg regs gc ref/byref set doesn't contain any other
    // registers from RBM_CALLEE_TRASH
    assert((gcInfo.gcRegGCrefSetCur & (RBM_CALLEE_TRASH & ~RBM_ARG_REGS)) == 0);
    assert((gcInfo.gcRegByrefSetCur & (RBM_CALLEE_TRASH & ~RBM_ARG_REGS)) == 0);
    gcInfo.gcRegGCrefSetCur &= ~RBM_ARG_REGS;
    gcInfo.gcRegByrefSetCur &= ~RBM_ARG_REGS;

    var_types returnType = call->TypeGet();
    if (returnType != TYP_VOID)
    {
        regNumber returnReg;

        if (call->HasMultiRegRetVal())
        {
            assert(pRetTypeDesc != nullptr);
            unsigned regCount = pRetTypeDesc->GetReturnRegCount();

            // If regs allocated to call node are different from ABI return
            // regs in which the call has returned its result, move the result
            // to regs allocated to call node.
            for (unsigned i = 0; i < regCount; ++i)
            {
                var_types regType      = pRetTypeDesc->GetReturnRegType(i);
                returnReg              = pRetTypeDesc->GetABIReturnReg(i);
                regNumber allocatedReg = call->GetRegNumByIdx(i);
                if (returnReg != allocatedReg)
                {
                    inst_RV_RV(ins_Copy(regType), allocatedReg, returnReg, regType);
                }
            }
        }
        else
        {
            if (varTypeUsesFloatArgReg(returnType))
            {
                returnReg = REG_FLOATRET;
            }
            else
            {
                returnReg = REG_INTRET;
            }

            if (call->gtRegNum != returnReg)
            {
                {
                    inst_RV_RV(ins_Copy(returnType), call->gtRegNum, returnReg, returnType);
                }
            }
        }

        genProduceReg(call);
    }

    // If there is nothing next, that means the result is thrown away, so this value is not live.
    // However, for minopts or debuggable code, we keep it live to support managed return value debugging.
    if ((call->gtNext == nullptr) && !compiler->opts.MinOpts() && !compiler->opts.compDbgCode)
    {
        gcInfo.gcMarkRegSetNpt(RBM_INTRET);
    }
}

// Produce code for a GT_JMP node.
// The arguments of the caller needs to be transferred to the callee before exiting caller.
// The actual jump to callee is generated as part of caller epilog sequence.
// Therefore the codegen of GT_JMP is to ensure that the callee arguments are correctly setup.
void CodeGen::genJmpMethod(GenTree* jmp)
{
    assert(jmp->OperGet() == GT_JMP);
    assert(compiler->compJmpOpUsed);

    // If no arguments, nothing to do
    if (compiler->info.compArgsCount == 0)
    {
        return;
    }

    // Make sure register arguments are in their initial registers
    // and stack arguments are put back as well.
    unsigned   varNum;
    LclVarDsc* varDsc;

    // First move any en-registered stack arguments back to the stack.
    // At the same time any reg arg not in correct reg is moved back to its stack location.
    //
    // We are not strictly required to spill reg args that are not in the desired reg for a jmp call
    // But that would require us to deal with circularity while moving values around.  Spilling
    // to stack makes the implementation simple, which is not a bad trade off given Jmp calls
    // are not frequent.
    for (varNum = 0; (varNum < compiler->info.compArgsCount); varNum++)
    {
        varDsc = compiler->lvaTable + varNum;

        if (varDsc->lvPromoted)
        {
            noway_assert(varDsc->lvFieldCnt == 1); // We only handle one field here

            unsigned fieldVarNum = varDsc->lvFieldLclStart;
            varDsc               = compiler->lvaTable + fieldVarNum;
        }
        noway_assert(varDsc->lvIsParam);

        if (varDsc->lvIsRegArg && (varDsc->lvRegNum != REG_STK))
        {
            // Skip reg args which are already in its right register for jmp call.
            // If not, we will spill such args to their stack locations.
            //
            // If we need to generate a tail call profiler hook, then spill all
            // arg regs to free them up for the callback.
            if (!compiler->compIsProfilerHookNeeded() && (varDsc->lvRegNum == varDsc->lvArgReg))
                continue;
        }
        else if (varDsc->lvRegNum == REG_STK)
        {
            // Skip args which are currently living in stack.
            continue;
        }

        // If we came here it means either a reg argument not in the right register or
        // a stack argument currently living in a register.  In either case the following
        // assert should hold.
        assert(varDsc->lvRegNum != REG_STK);
        assert(varDsc->TypeGet() != TYP_STRUCT);
        var_types storeType = genActualType(varDsc->TypeGet());
        emitAttr  storeSize = emitActualTypeSize(storeType);

        getEmitter()->emitIns_S_R(ins_Store(storeType), storeSize, varDsc->lvRegNum, varNum, 0);
        // Update lvRegNum life and GC info to indicate lvRegNum is dead and varDsc stack slot is going live.
        // Note that we cannot modify varDsc->lvRegNum here because another basic block may not be expecting it.
        // Therefore manually update life of varDsc->lvRegNum.
        regMaskTP tempMask = genRegMask(varDsc->lvRegNum);
        regSet.RemoveMaskVars(tempMask);
        gcInfo.gcMarkRegSetNpt(tempMask);
        if (compiler->lvaIsGCTracked(varDsc))
        {
            VarSetOps::AddElemD(compiler, gcInfo.gcVarPtrSetCur, varNum);
        }
    }

#ifdef PROFILING_SUPPORTED
    // At this point all arg regs are free.
    // Emit tail call profiler callback.
    genProfilingLeaveCallback(CORINFO_HELP_PROF_FCN_TAILCALL);
#endif

    // Next move any un-enregistered register arguments back to their register.
    regMaskTP fixedIntArgMask = RBM_NONE;    // tracks the int arg regs occupying fixed args in case of a vararg method.
    unsigned  firstArgVarNum  = BAD_VAR_NUM; // varNum of the first argument in case of a vararg method.
    for (varNum = 0; (varNum < compiler->info.compArgsCount); varNum++)
    {
        varDsc = compiler->lvaTable + varNum;
        if (varDsc->lvPromoted)
        {
            noway_assert(varDsc->lvFieldCnt == 1); // We only handle one field here

            unsigned fieldVarNum = varDsc->lvFieldLclStart;
            varDsc               = compiler->lvaTable + fieldVarNum;
        }
        noway_assert(varDsc->lvIsParam);

        // Skip if arg not passed in a register.
        if (!varDsc->lvIsRegArg)
            continue;

        // Register argument
        noway_assert(isRegParamType(genActualType(varDsc->TypeGet())));

        // Is register argument already in the right register?
        // If not load it from its stack location.
        regNumber argReg     = varDsc->lvArgReg; // incoming arg register
        regNumber argRegNext = REG_NA;

        if (varTypeIsStruct(varDsc))
        {
            CORINFO_CLASS_HANDLE typeHnd = varDsc->lvVerTypeInfo.GetClassHandle();
            assert(typeHnd != nullptr);

            MIPS64_CORINFO_STRUCT_REG_PASSING_DESCRIPTOR structDesc;
            compiler->eeGetMIPS64PassStructInRegisterDescriptor(typeHnd, &structDesc);

            var_types regType = structDesc.eightByteClassifications[0] == MIPS64ClassificationTypeDouble ? TYP_DOUBLE : TYP_I_IMPL;
            unsigned regArgNum = genMapRegNumToRegArgNum(varDsc->lvArgReg, regType);
            unsigned slots = structDesc.eightByteCount;
            if (slots > 1)
            {
                if (regArgNum + slots > MAX_REG_ARG)
                {
                    slots = MAX_REG_ARG - regArgNum;
                }
            }

            for (unsigned i = 0; i < slots; i++)
            {
                regNumber reg = REG_NA;

                CorInfoGCType currentGcLayoutType = (CorInfoGCType)varDsc->lvGcLayout[i];
                if (currentGcLayoutType == TYPE_GC_NONE)
                {
                    if (structDesc.IsDoubleSlot(i))
                    {
                        reg = genMapFloatRegArgNumToRegNum(regArgNum + i);
                        regType = TYP_DOUBLE;
                    }
                    else
                    {
                        //assert(structDesc.IsValidSlot(i));
                        reg = genMapIntRegArgNumToRegNum(regArgNum + i);
                        regType = TYP_I_IMPL;
                    }
                }
                else
                {
                    reg = genMapIntRegArgNumToRegNum(regArgNum + i);
                    regType = compiler->getJitGCType(currentGcLayoutType);
                }

                if (i == 0)
                    assert(varDsc->lvArgReg == reg);

                getEmitter()->emitIns_R_S(ins_Load(regType), emitTypeSize(regType), reg, varNum, i * TARGET_POINTER_SIZE);
                regSet.rsMaskVars |= genRegMask(reg);
                gcInfo.gcMarkRegPtrVal(reg, regType);
            }
        }
        else
        {
            var_types loadType = varDsc->TypeGet();
            emitAttr loadSize = emitTypeSize(loadType);
            if (varDsc->lvRegNum != argReg)
            {
                assert(genIsValidReg(argReg));
                getEmitter()->emitIns_R_S(ins_Load(loadType), loadSize, argReg, varNum, 0);

                // Update argReg life and GC Info to indicate varDsc stack slot is dead and argReg is going live.
                // Note that we cannot modify varDsc->lvRegNum here because another basic block may not be expecting it.
                // Therefore manually update life of argReg.  Note that GT_JMP marks the end of the basic block
                // and after which reg life and gc info will be recomputed for the new block in genCodeForBBList().
                regSet.AddMaskVars(genRegMask(argReg));
                gcInfo.gcMarkRegPtrVal(argReg, loadType);
            }
        }

        if (compiler->lvaIsGCTracked(varDsc))
        {
            VarSetOps::RemoveElemD(compiler, gcInfo.gcVarPtrSetCur, varDsc->lvVarIndex);
        }

        if (compiler->info.compIsVarArgs)
        {
            assert(!"unimplemented on MIPS yet!");
            // In case of a jmp call to a vararg method ensure only integer registers are passed.
            assert((genRegMask(argReg) & (RBM_ARG_REGS)) != RBM_NONE);
            assert(!varDsc->lvIsHfaRegArg());

            fixedIntArgMask |= genRegMask(argReg);

            if (compiler->lvaIsMultiregStruct(varDsc, compiler->info.compIsVarArgs))
            {
                assert(argRegNext != REG_NA);
                fixedIntArgMask |= genRegMask(argRegNext);
            }

            if (argReg == REG_ARG_0)
            {
                assert(firstArgVarNum == BAD_VAR_NUM);
                firstArgVarNum = varNum;
            }
        }

    }

    // Jmp call to a vararg method - if the method has fewer than fixed arguments that can be max size of reg,
    // load the remaining integer arg registers from the corresponding
    // shadow stack slots.  This is for the reason that we don't know the number and type
    // of non-fixed params passed by the caller, therefore we have to assume the worst case
    // of caller passing all integer arg regs that can be max size of reg.
    //
    // The caller could have passed gc-ref/byref type var args.  Since these are var args
    // the callee no way of knowing their gc-ness.  Therefore, mark the region that loads
    // remaining arg registers from shadow stack slots as non-gc interruptible.
    if (fixedIntArgMask != RBM_NONE)
    {
        assert(compiler->info.compIsVarArgs);
        assert(firstArgVarNum != BAD_VAR_NUM);

        regMaskTP remainingIntArgMask = RBM_ARG_REGS & ~fixedIntArgMask;
        if (remainingIntArgMask != RBM_NONE)
        {
            getEmitter()->emitDisableGC();
            for (int argNum = 0, argOffset = 0; argNum < MAX_REG_ARG; ++argNum)
            {
                regNumber argReg     = intArgRegs[argNum];
                regMaskTP argRegMask = genRegMask(argReg);

                if ((remainingIntArgMask & argRegMask) != 0)
                {
                    remainingIntArgMask &= ~argRegMask;
                    getEmitter()->emitIns_R_S(INS_ld, EA_PTRSIZE, argReg, firstArgVarNum, argOffset);
                }

                argOffset += REGSIZE_BYTES;
            }
            getEmitter()->emitEnableGC();
        }
    }
}

//------------------------------------------------------------------------
// genIntCastOverflowCheck: Generate overflow checking code for an integer cast.
//
// Arguments:
//    cast - The GT_CAST node
//    desc - The cast description
//    reg  - The register containing the value to check
//
void CodeGen::genIntCastOverflowCheck(GenTreeCast* cast, const GenIntCastDesc& desc, regNumber reg)
{
    switch (desc.CheckKind())
    {
        case GenIntCastDesc::CHECK_POSITIVE:
        {
            getEmitter()->emitIns_R_R_R(INS_slt, EA_ATTR(desc.CheckSrcSize()), REG_AT, reg, REG_R0);
            ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
            getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
            getEmitter()->emitIns(INS_nop);

            genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
            //getEmitter()->emitIns(INS_nop);
        }
        break;

        case GenIntCastDesc::CHECK_UINT_RANGE:
        {
            // We need to check if the value is not greater than 0xFFFFFFFF
            // if the upper 32 bits are zero.
            ssize_t imm = -1;
            getEmitter()->emitIns_R_R_I(INS_daddiu, EA_8BYTE, REG_AT, REG_R0, imm);

            getEmitter()->emitIns_R_R_I(INS_dsll32, EA_8BYTE, REG_AT, REG_AT, 0);
            getEmitter()->emitIns_R_R_R(INS_and, EA_8BYTE, REG_AT, reg, REG_AT);

            imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
            getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
            getEmitter()->emitIns(INS_nop);

            genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
            //getEmitter()->emitIns(INS_nop);
        }
        break;

        case GenIntCastDesc::CHECK_POSITIVE_INT_RANGE:
        {
            // We need to check if the value is not greater than 0x7FFFFFFF
            // if the upper 33 bits are zero.
            //instGen_Set_Reg_To_Imm(EA_8BYTE, REG_AT, 0xFFFFFFFF80000000LL);
            ssize_t imm = -1;
            getEmitter()->emitIns_R_R_I(INS_daddiu, EA_8BYTE, REG_AT, REG_R0, imm);

            getEmitter()->emitIns_R_R_I(INS_dsll, EA_8BYTE, REG_AT, REG_AT, 31);

            getEmitter()->emitIns_R_R_R(INS_and, EA_8BYTE, REG_AT, reg, REG_AT);

            imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
            getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
            getEmitter()->emitIns(INS_nop);

            genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
            //getEmitter()->emitIns(INS_nop);
        }
        break;

        case GenIntCastDesc::CHECK_INT_RANGE:
        {
            const regNumber tempReg = cast->GetSingleTempReg();
            assert(tempReg != reg);
            instGen_Set_Reg_To_Imm(EA_8BYTE, tempReg, INT32_MAX);
            getEmitter()->emitIns_R_R_R(INS_slt, EA_8BYTE, REG_AT, tempReg, reg);

            ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
            getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
            getEmitter()->emitIns(INS_nop);

            genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
            //getEmitter()->emitIns(INS_nop);

            instGen_Set_Reg_To_Imm(EA_8BYTE, tempReg, INT32_MIN);
            getEmitter()->emitIns_R_R_R(INS_slt, EA_8BYTE, REG_AT, reg, tempReg);

            //imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
            getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
            getEmitter()->emitIns(INS_nop);

            genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
            //getEmitter()->emitIns(INS_nop);
        }
        break;

        default:
        {
            assert(desc.CheckKind() == GenIntCastDesc::CHECK_SMALL_INT_RANGE);
            const int castMaxValue = desc.CheckSmallIntMax();
            const int castMinValue = desc.CheckSmallIntMin();

            if (castMaxValue > 255)
            {
                assert((castMaxValue == 32767) || (castMaxValue == 65535));
                instGen_Set_Reg_To_Imm(EA_ATTR(desc.CheckSrcSize()), REG_AT, castMaxValue + 1);
                if (castMinValue == 0)
                    getEmitter()->emitIns_R_R_R(INS_sltu, EA_ATTR(desc.CheckSrcSize()), REG_AT, reg, REG_AT);
                else
                    getEmitter()->emitIns_R_R_R(INS_slt, EA_ATTR(desc.CheckSrcSize()), REG_AT, reg, REG_AT);

                ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                getEmitter()->emitIns_R_R_I(INS_bne, EA_PTRSIZE, REG_AT, REG_R0, imm);
                getEmitter()->emitIns(INS_nop);

                genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
                //getEmitter()->emitIns(INS_nop);
            }
            else
            {
                getEmitter()->emitIns_R_R_I(INS_addi, EA_ATTR(desc.CheckSrcSize()), REG_AT, REG_R0, castMaxValue);
                if (castMinValue == 0)
                {
                    getEmitter()->emitIns_R_R_R(INS_sltu, EA_ATTR(desc.CheckSrcSize()), REG_AT, REG_AT, reg);
                }
                else
                {
                    getEmitter()->emitIns_R_R_R(INS_slt, EA_ATTR(desc.CheckSrcSize()), REG_AT, REG_AT, reg);
                }

                ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
                getEmitter()->emitIns(INS_nop);

                genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
                //getEmitter()->emitIns(INS_nop);
            }

            if (castMinValue != 0)
            {
                getEmitter()->emitIns_R_R_I(INS_slti, EA_ATTR(desc.CheckSrcSize()), REG_AT, reg, castMinValue);
                ssize_t imm = compiler->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
                getEmitter()->emitIns(INS_nop);

                genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW);
                //getEmitter()->emitIns(INS_nop);
            }
        }
        break;
    }
}

//------------------------------------------------------------------------
// genIntToIntCast: Generate code for an integer cast, with or without overflow check.
//
// Arguments:
//    cast - The GT_CAST node
//
// Assumptions:
//    The cast node is not a contained node and must have an assigned register.
//    Neither the source nor target type can be a floating point type.
//
// TODO-MIPS64-CQ: Allow castOp to be a contained node without an assigned register.
//
void CodeGen::genIntToIntCast(GenTreeCast* cast)
{
    genConsumeRegs(cast->gtGetOp1());

    emitter* emit = getEmitter();
    var_types dstType = cast->CastToType();
    var_types srcType = genActualType(cast->gtGetOp1()->TypeGet());
    const regNumber srcReg = cast->gtGetOp1()->gtRegNum;
    const regNumber dstReg = cast->gtRegNum;
    const unsigned char pos = 0;
    const unsigned char size = 32;

    assert(genIsValidIntReg(srcReg));
    assert(genIsValidIntReg(dstReg));

    GenIntCastDesc desc(cast);

    if (desc.CheckKind() != GenIntCastDesc::CHECK_NONE)
    {
        genIntCastOverflowCheck(cast, desc, srcReg);
    }

    /* FIXME for MIPS: It is not needed for the time being. */
    //if ((EA_ATTR(genTypeSize(srcType)) == EA_8BYTE) && (EA_ATTR(genTypeSize(dstType)) == EA_4BYTE))
    //{
    //    if (dstType == TYP_INT)
    //    {
    //        // convert t0 int32
    //        emit->emitIns_R_R_I(INS_sll, EA_4BYTE, dstReg, srcReg, 0);
    //    }
    //    else
    //    {
    //        // convert t0 uint32
    //        emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, dstReg, srcReg, pos, 32);
    //    }
    //}
    //else if ((desc.ExtendKind() != GenIntCastDesc::COPY) || (srcReg != dstReg))
    if ((desc.ExtendKind() != GenIntCastDesc::COPY) || (srcReg != dstReg))
    {
        instruction ins;

        switch (desc.ExtendKind())
        {
            case GenIntCastDesc::ZERO_EXTEND_SMALL_INT:
                /* FIXME for MIPS */
                if (desc.ExtendSrcSize() == 1)
                {
                    emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, dstReg, srcReg, pos, 8);
                }
                else
                {
                    emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, dstReg, srcReg, pos, 16);
                }
                break;
            case GenIntCastDesc::SIGN_EXTEND_SMALL_INT:
                ins = (desc.ExtendSrcSize() == 1) ? INS_seb : INS_seh;
                emit->emitIns_R_R(ins, EA_PTRSIZE, dstReg, srcReg);
                break;
#ifdef _TARGET_64BIT_
            case GenIntCastDesc::ZERO_EXTEND_INT:
                emit->emitIns_R_R_I_I(INS_dext, EA_PTRSIZE, dstReg, srcReg, pos, 32);
                break;
            case GenIntCastDesc::SIGN_EXTEND_INT:
                emit->emitIns_R_R_I(INS_sll, EA_4BYTE, dstReg, srcReg, 0);
                break;
#endif
            default:
                assert(desc.ExtendKind() == GenIntCastDesc::COPY);
                emit->emitIns_R_R(INS_mov, EA_PTRSIZE, dstReg, srcReg);
                break;
        }
    }

    genProduceReg(cast);
}

//------------------------------------------------------------------------
// genFloatToFloatCast: Generate code for a cast between float and double
//
// Arguments:
//    treeNode - The GT_CAST node
//
// Return Value:
//    None.
//
// Assumptions:
//    Cast is a non-overflow conversion.
//    The treeNode must have an assigned register.
//    The cast is between float and double.
//
void CodeGen::genFloatToFloatCast(GenTree* treeNode)
{
    // float <--> double conversions are always non-overflow ones
    assert(treeNode->OperGet() == GT_CAST);
    assert(!treeNode->gtOverflow());

    regNumber targetReg = treeNode->gtRegNum;
    assert(genIsValidFloatReg(targetReg));

    GenTree* op1 = treeNode->gtOp.gtOp1;
    assert(!op1->isContained());               // Cannot be contained
    assert(genIsValidFloatReg(op1->gtRegNum)); // Must be a valid float reg.

    var_types dstType = treeNode->CastToType();
    var_types srcType = op1->TypeGet();
    assert(varTypeIsFloating(srcType) && varTypeIsFloating(dstType));

    genConsumeOperands(treeNode->AsOp());

    // treeNode must be a reg
    assert(!treeNode->isContained());

    if (srcType != dstType)
    {
        instruction ins = (srcType == TYP_FLOAT) ? INS_cvt_d_s  // convert Single to Double
                                                 : INS_cvt_s_d; // convert Double to Single

        getEmitter()->emitIns_R_R(ins, emitActualTypeSize(treeNode), treeNode->gtRegNum, op1->gtRegNum);
    }
    else if (treeNode->gtRegNum != op1->gtRegNum)
    {
        // If double to double cast or float to float cast. Emit a move instruction.
        instruction ins = (srcType == TYP_FLOAT) ? INS_mov_s : INS_mov_d;
        getEmitter()->emitIns_R_R(ins, emitActualTypeSize(treeNode), treeNode->gtRegNum, op1->gtRegNum);
    }

    genProduceReg(treeNode);
}

//------------------------------------------------------------------------
// genCreateAndStoreGCInfo: Create and record GC Info for the function.
//
void CodeGen::genCreateAndStoreGCInfo(unsigned codeSize,
                                      unsigned prologSize,
                                      unsigned epilogSize DEBUGARG(void* codePtr))
{
    IAllocator*    allowZeroAlloc = new (compiler, CMK_GC) CompIAllocator(compiler->getAllocatorGC());
    GcInfoEncoder* gcInfoEncoder  = new (compiler, CMK_GC)
        GcInfoEncoder(compiler->info.compCompHnd, compiler->info.compMethodInfo, allowZeroAlloc, NOMEM);
    assert(gcInfoEncoder != nullptr);

    // Follow the code pattern of the x86 gc info encoder (genCreateAndStoreGCInfoJIT32).
    gcInfo.gcInfoBlockHdrSave(gcInfoEncoder, codeSize, prologSize);

    // We keep the call count for the second call to gcMakeRegPtrTable() below.
    unsigned callCnt = 0;

    // First we figure out the encoder ID's for the stack slots and registers.
    gcInfo.gcMakeRegPtrTable(gcInfoEncoder, codeSize, prologSize, GCInfo::MAKE_REG_PTR_MODE_ASSIGN_SLOTS, &callCnt);

    // Now we've requested all the slots we'll need; "finalize" these (make more compact data structures for them).
    gcInfoEncoder->FinalizeSlotIds();

    // Now we can actually use those slot ID's to declare live ranges.
    gcInfo.gcMakeRegPtrTable(gcInfoEncoder, codeSize, prologSize, GCInfo::MAKE_REG_PTR_MODE_DO_WORK, &callCnt);

    if (compiler->opts.compDbgEnC)
    {
        // what we have to preserve is called the "frame header" (see comments in VM\eetwain.cpp)
        // which is:
        //  -return address
        //  -saved off RBP
        //  -saved 'this' pointer and bool for synchronized methods

        // 4 slots for RBP + return address + RSI + RDI
        int preservedAreaSize = 4 * REGSIZE_BYTES;

        if (compiler->info.compFlags & CORINFO_FLG_SYNCH)
        {
            if (!(compiler->info.compFlags & CORINFO_FLG_STATIC))
                preservedAreaSize += REGSIZE_BYTES;

            preservedAreaSize += 1; // bool for synchronized methods
        }

        // Used to signal both that the method is compiled for EnC, and also the size of the block at the top of the
        // frame
        gcInfoEncoder->SetSizeOfEditAndContinuePreservedArea(preservedAreaSize);
    }

    if (compiler->opts.IsReversePInvoke())
    {
        unsigned reversePInvokeFrameVarNumber = compiler->lvaReversePInvokeFrameVar;
        assert(reversePInvokeFrameVarNumber != BAD_VAR_NUM && reversePInvokeFrameVarNumber < compiler->lvaRefCount);
        LclVarDsc& reversePInvokeFrameVar = compiler->lvaTable[reversePInvokeFrameVarNumber];
        gcInfoEncoder->SetReversePInvokeFrameSlot(reversePInvokeFrameVar.lvStkOffs);
    }

    gcInfoEncoder->Build();

    // GC Encoder automatically puts the GC info in the right spot using ICorJitInfo::allocGCInfo(size_t)
    // let's save the values anyway for debugging purposes
    compiler->compInfoBlkAddr = gcInfoEncoder->Emit();
    compiler->compInfoBlkSize = 0; // not exposed by the GCEncoder interface
}

/* FIXME for MIPS: not used for mips */
// clang-format off
const CodeGen::GenConditionDesc CodeGen::GenConditionDesc::map[32]
{
    //{ },       // NONE
    //{ },       // 1
    //{ EJ_lt }, // SLT
    //{ EJ_le }, // SLE
    //{ EJ_ge }, // SGE
    //{ EJ_gt }, // SGT
    //{ EJ_mi }, // S
    //{ EJ_pl }, // NS

    //{ EJ_eq }, // EQ
    //{ EJ_ne }, // NE
    //{ EJ_lo }, // ULT
    //{ EJ_ls }, // ULE
    //{ EJ_hs }, // UGE
    //{ EJ_hi }, // UGT
    //{ EJ_hs }, // C
    //{ EJ_lo }, // NC

    //{ EJ_eq },                // FEQ
    //{ EJ_gt, GT_AND, EJ_lo }, // FNE
    //{ EJ_lo },                // FLT
    //{ EJ_ls },                // FLE
    //{ EJ_ge },                // FGE
    //{ EJ_gt },                // FGT
    //{ EJ_vs },                // O
    //{ EJ_vc },                // NO

    //{ EJ_eq, GT_OR, EJ_vs },  // FEQU
    //{ EJ_ne },                // FNEU
    //{ EJ_lt },                // FLTU
    //{ EJ_le },                // FLEU
    //{ EJ_hs },                // FGEU
    //{ EJ_hi },                // FGTU
    //{ },                      // P
    //{ },                      // NP
};
// clang-format on

//------------------------------------------------------------------------
// inst_SETCC: Generate code to set a register to 0 or 1 based on a condition.
//
// Arguments:
//   condition - The condition
//   type      - The type of the value to be produced
//   dstReg    - The destination register to be set to 1 or 0
//
void CodeGen::inst_SETCC(GenCondition condition, var_types type, regNumber dstReg)
{
/* FIXME for MIPS: not used for mips. */
	assert(!"unimplemented on MIPS yet");
#if 0
    assert(varTypeIsIntegral(type));
    assert(genIsValidIntReg(dstReg));

    // Emit code like that:
    //   ...
    //   bgt True
    //   daddiu rD, zero, 0
    //   b Next
    // True:
    //   daddiu rD, zero, 1
    // Next:
    //   ...

    BasicBlock* labelTrue = genCreateTempLabel();
    inst_JCC(condition, labelTrue);

    getEmitter()->emitIns_R_R_I(INS_daddiu, emitActualTypeSize(type), dstReg, REG_R0, 0);

    BasicBlock* labelNext = genCreateTempLabel();
    getEmitter()->emitIns_J(INS_b, labelNext);

    genDefineTempLabel(labelTrue);
    getEmitter()->emitIns_R_R_I(INS_daddiu, emitActualTypeSize(type), dstReg, REG_R0, 1);
    genDefineTempLabel(labelNext);

#endif
}

//------------------------------------------------------------------------
// genCodeForStoreBlk: Produce code for a GT_STORE_OBJ/GT_STORE_DYN_BLK/GT_STORE_BLK node.
//
// Arguments:
//    tree - the node
//
void CodeGen::genCodeForStoreBlk(GenTreeBlk* blkOp)
{
    assert(blkOp->OperIs(GT_STORE_OBJ, GT_STORE_DYN_BLK, GT_STORE_BLK));

    if (blkOp->OperIs(GT_STORE_OBJ) && blkOp->OperIsCopyBlkOp())
    {
        assert(blkOp->AsObj()->gtGcPtrCount != 0);
        genCodeForCpObj(blkOp->AsObj());
        return;
    }

    if (blkOp->gtBlkOpGcUnsafe)
    {
        getEmitter()->emitDisableGC();
    }
    bool isCopyBlk = blkOp->OperIsCopyBlkOp();

    switch (blkOp->gtBlkOpKind)
    {
        case GenTreeBlk::BlkOpKindHelper:
            if (isCopyBlk)
            {
                genCodeForCpBlkHelper(blkOp);
            }
            else
            {
                genCodeForInitBlkHelper(blkOp);
            }
            break;

        case GenTreeBlk::BlkOpKindUnroll:
            if (isCopyBlk)
            {
                genCodeForCpBlkUnroll(blkOp);
            }
            else
            {
                genCodeForInitBlkUnroll(blkOp);
            }
            break;

        default:
            unreached();
    }

    if (blkOp->gtBlkOpGcUnsafe)
    {
        getEmitter()->emitEnableGC();
    }
}

//------------------------------------------------------------------------
// genScaledAdd: A helper for genLeaInstruction.
//
void CodeGen::genScaledAdd(emitAttr attr, regNumber targetReg, regNumber baseReg, regNumber indexReg, int scale)
{
    emitter* emit = getEmitter();
    if (scale == 0)
    {
        // target = base + index
        emit->emitIns_R_R_R(INS_daddu, attr, targetReg, baseReg, indexReg);
    }
    else
    {
        // target = base + index<<scale
        emit->emitIns_R_R_I(INS_dsll, EA_PTRSIZE/*attr*/, REG_AT, indexReg, scale);
        emit->emitIns_R_R_R(INS_daddu, attr, targetReg, baseReg, REG_AT);
    }
}

//------------------------------------------------------------------------
// genLeaInstruction: Produce code for a GT_LEA node.
//
// Arguments:
//    lea - the node
//
void CodeGen::genLeaInstruction(GenTreeAddrMode* lea)
{
    genConsumeOperands(lea);
    emitter* emit   = getEmitter();
    emitAttr size   = emitTypeSize(lea);
    int      offset = lea->Offset();

    // In MIPS we can only load addresses of the form:
    //
    // [Base + index*scale]
    // [Base + Offset]
    // [Literal] (PC-Relative)
    //
    // So for the case of a LEA node of the form [Base + Index*Scale + Offset] we will generate:
    // destReg = baseReg + indexReg * scale;
    // destReg = destReg + offset;
    //
    // TODO-MIPS64-CQ: The purpose of the GT_LEA node is to directly reflect a single target architecture
    //             addressing mode instruction.  Currently we're 'cheating' by producing one or more
    //             instructions to generate the addressing mode so we need to modify lowering to
    //             produce LEAs that are a 1:1 relationship to the MIPS64 architecture.
    if (lea->Base() && lea->Index())
    {
        GenTree* memBase = lea->Base();
        GenTree* index   = lea->Index();

        DWORD scale;

        assert(isPow2(lea->gtScale));
        BitScanForward(&scale, lea->gtScale);

        assert(scale <= 4);

        if (offset != 0)
        {
            regNumber tmpReg = lea->GetSingleTempReg();

            // When generating fully interruptible code we have to use the "large offset" sequence
            // when calculating a EA_BYREF as we can't report a byref that points outside of the object
            //
            bool useLargeOffsetSeq = compiler->genInterruptible && (size == EA_BYREF);

            if (!useLargeOffsetSeq && emitter::emitIns_valid_imm_for_add(offset))
            {
                // Generate code to set tmpReg = base + index*scale
                genScaledAdd(size, tmpReg, memBase->gtRegNum, index->gtRegNum, scale);

                // Then compute target reg from [tmpReg + offset]
                emit->emitIns_R_R_I(INS_daddiu, size, lea->gtRegNum, tmpReg, offset);
            }
            else // large offset sequence
            {
                noway_assert(tmpReg != index->gtRegNum);
                noway_assert(tmpReg != memBase->gtRegNum);

                // First load/store tmpReg with the offset constant
                //      rTmp = imm
                instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, offset);

                // Then add the scaled index register
                //      rTmp = rTmp + index*scale
                genScaledAdd(EA_PTRSIZE, tmpReg, tmpReg, index->gtRegNum, scale);

                // Then compute target reg from [base + tmpReg ]
                //      rDst = base + rTmp
                emit->emitIns_R_R_R(INS_daddu, size, lea->gtRegNum, memBase->gtRegNum, tmpReg);
            }
        }
        else
        {
            // Then compute target reg from [base + index*scale]
            genScaledAdd(size, lea->gtRegNum, memBase->gtRegNum, index->gtRegNum, scale);
        }
    }
    else if (lea->Base())
    {
        GenTree* memBase = lea->Base();

        if (emitter::emitIns_valid_imm_for_add(offset))
        {
            if (offset != 0)
            {
                // Then compute target reg from [memBase + offset]
                emit->emitIns_R_R_I(INS_daddiu, size, lea->gtRegNum, memBase->gtRegNum, offset);
            }
            else // offset is zero
            {
                if (lea->gtRegNum != memBase->gtRegNum)
                {
                    emit->emitIns_R_R(INS_mov, size, lea->gtRegNum, memBase->gtRegNum);
                }
            }
        }
        else
        {
            // We require a tmpReg to hold the offset
            regNumber tmpReg = lea->GetSingleTempReg();

            // First load tmpReg with the large offset constant
            instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, offset);

            // Then compute target reg from [memBase + tmpReg]
            emit->emitIns_R_R_R(INS_daddu, size, lea->gtRegNum, memBase->gtRegNum, tmpReg);
        }
    }
    else if (lea->Index())
    {
        // If we encounter a GT_LEA node without a base it means it came out
        // when attempting to optimize an arbitrary arithmetic expression during lower.
        // This is currently disabled in MIPS64 since we need to adjust lower to account
        // for the simpler instructions MIPS64 supports.
        // TODO-MIPS64-CQ:  Fix this and let LEA optimize arithmetic trees too.
        assert(!"We shouldn't see a baseless address computation during CodeGen for MIPS64");
    }

    genProduceReg(lea);
}

//------------------------------------------------------------------------
// isStructReturn: Returns whether the 'treeNode' is returning a struct.
//
// Arguments:
//    treeNode - The tree node to evaluate whether is a struct return.
//
// Return Value:
//    Returns true if the 'treeNode" is a GT_RETURN node of type struct.
//    Otherwise returns false.
//
bool CodeGen::isStructReturn(GenTree* treeNode)
{
    // This method could be called for 'treeNode' of GT_RET_FILT or GT_RETURN.
    // For the GT_RET_FILT, the return is always
    // a bool or a void, for the end of a finally block.
    noway_assert(treeNode->OperGet() == GT_RETURN || treeNode->OperGet() == GT_RETFILT);
    var_types returnType = treeNode->TypeGet();

    return varTypeIsStruct(returnType) && (compiler->info.compRetNativeType == TYP_STRUCT);
}

//------------------------------------------------------------------------
// genStructReturn: Generates code for returning a struct.
//
// Arguments:
//    treeNode - The GT_RETURN tree node.
//
// Return Value:
//    None
//
// Assumption:
//    op1 of GT_RETURN node is either GT_LCL_VAR or multi-reg GT_CALL
void CodeGen::genStructReturn(GenTree* treeNode)
{
    assert(treeNode->OperGet() == GT_RETURN);
    assert(isStructReturn(treeNode));
    GenTree* op1 = treeNode->gtGetOp1();

    if (op1->OperGet() == GT_LCL_VAR)
    {
        GenTreeLclVarCommon* lclVar  = op1->AsLclVarCommon();
        LclVarDsc*           varDsc  = &(compiler->lvaTable[lclVar->gtLclNum]);
        var_types            lclType = genActualType(varDsc->TypeGet());

        assert(varTypeIsStruct(lclType));
        assert(varDsc->lvIsMultiRegRet);

        ReturnTypeDesc retTypeDesc;
        unsigned       regCount;

        retTypeDesc.InitializeStructReturnType(compiler, varDsc->lvVerTypeInfo.GetClassHandle());
        regCount = retTypeDesc.GetReturnRegCount();

        assert(regCount >= 2);

        assert(varTypeIsSIMD(lclType) || op1->isContained());

        if (op1->isContained())
        {
            // Copy var on stack into ABI return registers
            // TODO: It could be optimized by reducing two float loading to one double
            int offset = 0;
            for (unsigned i = 0; i < regCount; ++i)
            {
                var_types type = retTypeDesc.GetReturnRegType(i);
                regNumber reg  = retTypeDesc.GetABIReturnReg(i);
                getEmitter()->emitIns_R_S(ins_Load(type), emitTypeSize(type), reg, lclVar->gtLclNum, offset);
                offset += genTypeSize(type);
            }
        }
        else
        {
            // Handle SIMD genStructReturn case
            assert(!"unimplemented on MIPS yet");
#if 0
            genConsumeRegs(op1);
            regNumber src = op1->gtRegNum;

            // Treat src register as a homogenous vector with element size equal to the reg size
            // Insert pieces in order
            for (unsigned i = 0; i < regCount; ++i)
            {
                var_types type = retTypeDesc.GetReturnRegType(i);
                regNumber reg  = retTypeDesc.GetABIReturnReg(i);
                if (varTypeIsFloating(type))
                {
                    // If the register piece is to be passed in a floating point register
                    // Use a vector mov element instruction
                    // reg is not a vector, so it is in the first element reg[0]
                    // mov reg[0], src[i]
                    // This effectively moves from `src[i]` to `reg[0]`, upper bits of reg remain unchanged
                    // For the case where src == reg, since we are only writing reg[0], as long as we iterate
                    // so that src[0] is consumed before writing reg[0], we do not need a temporary.
                    getEmitter()->emitIns_R_R_I_I(INS_mov, emitTypeSize(type), reg, src, 0, i);
                }
                else
                {
                    // If the register piece is to be passed in an integer register
                    // Use a vector mov to general purpose register instruction
                    // mov reg, src[i]
                    // This effectively moves from `src[i]` to `reg`
                    getEmitter()->emitIns_R_R_I(INS_mov, emitTypeSize(type), reg, src, i);
                }
            }
#endif
        }
    }
    else // op1 must be multi-reg GT_CALL
    {
        assert(op1->IsMultiRegCall() || op1->IsCopyOrReloadOfMultiRegCall());

        genConsumeRegs(op1);

        GenTree*     actualOp1 = op1->gtSkipReloadOrCopy();
        GenTreeCall* call      = actualOp1->AsCall();

        ReturnTypeDesc* pRetTypeDesc;
        unsigned        regCount;
        unsigned        matchingCount = 0;

        pRetTypeDesc = call->GetReturnTypeDesc();
        regCount     = pRetTypeDesc->GetReturnRegCount();

        var_types regType[MAX_RET_REG_COUNT];
        regNumber returnReg[MAX_RET_REG_COUNT];
        regNumber allocatedReg[MAX_RET_REG_COUNT];
        regMaskTP srcRegsMask       = 0;
        regMaskTP dstRegsMask       = 0;
        bool      needToShuffleRegs = false; // Set to true if we have to move any registers

        for (unsigned i = 0; i < regCount; ++i)
        {
            regType[i]   = pRetTypeDesc->GetReturnRegType(i);
            returnReg[i] = pRetTypeDesc->GetABIReturnReg(i);

            regNumber reloadReg = REG_NA;
            if (op1->IsCopyOrReload())
            {
                // GT_COPY/GT_RELOAD will have valid reg for those positions
                // that need to be copied or reloaded.
                reloadReg = op1->AsCopyOrReload()->GetRegNumByIdx(i);
            }

            if (reloadReg != REG_NA)
            {
                allocatedReg[i] = reloadReg;
            }
            else
            {
                allocatedReg[i] = call->GetRegNumByIdx(i);
            }

            if (returnReg[i] == allocatedReg[i])
            {
                matchingCount++;
            }
            else // We need to move this value
            {
                // We want to move the value from allocatedReg[i] into returnReg[i]
                // so record these two registers in the src and dst masks
                //
                srcRegsMask |= genRegMask(allocatedReg[i]);
                dstRegsMask |= genRegMask(returnReg[i]);

                needToShuffleRegs = true;
            }
        }

        if (needToShuffleRegs)
        {
            assert(matchingCount < regCount);

            unsigned  remainingRegCount = regCount - matchingCount;
            regMaskTP extraRegMask      = treeNode->gtRsvdRegs;

            while (remainingRegCount > 0)
            {
                // set 'available' to the 'dst' registers that are not currently holding 'src' registers
                //
                regMaskTP availableMask = dstRegsMask & ~srcRegsMask;

                regMaskTP dstMask;
                regNumber srcReg;
                regNumber dstReg;
                var_types curType   = TYP_UNKNOWN;
                regNumber freeUpReg = REG_NA;

                if (availableMask == 0)
                {
                    // Circular register dependencies
                    // So just free up the lowest register in dstRegsMask by moving it to the 'extra' register

                    assert(dstRegsMask == srcRegsMask);         // this has to be true for us to reach here
                    assert(extraRegMask != 0);                  // we require an 'extra' register
                    assert((extraRegMask & ~dstRegsMask) != 0); // it can't be part of dstRegsMask

                    availableMask = extraRegMask & ~dstRegsMask;

                    regMaskTP srcMask = genFindLowestBit(srcRegsMask);
                    freeUpReg         = genRegNumFromMask(srcMask);
                }

                dstMask = genFindLowestBit(availableMask);
                dstReg  = genRegNumFromMask(dstMask);
                srcReg  = REG_NA;

                if (freeUpReg != REG_NA)
                {
                    // We will free up the srcReg by moving it to dstReg which is an extra register
                    //
                    srcReg = freeUpReg;

                    // Find the 'srcReg' and set 'curType', change allocatedReg[] to dstReg
                    // and add the new register mask bit to srcRegsMask
                    //
                    for (unsigned i = 0; i < regCount; ++i)
                    {
                        if (allocatedReg[i] == srcReg)
                        {
                            curType         = regType[i];
                            allocatedReg[i] = dstReg;
                            srcRegsMask |= genRegMask(dstReg);
                        }
                    }
                }
                else // The normal case
                {
                    // Find the 'srcReg' and set 'curType'
                    //
                    for (unsigned i = 0; i < regCount; ++i)
                    {
                        if (returnReg[i] == dstReg)
                        {
                            srcReg  = allocatedReg[i];
                            curType = regType[i];
                        }
                    }
                    // After we perform this move we will have one less registers to setup
                    remainingRegCount--;
                }
                assert(curType != TYP_UNKNOWN);

                inst_RV_RV(ins_Copy(curType), dstReg, srcReg, curType);

                // Clear the appropriate bits in srcRegsMask and dstRegsMask
                srcRegsMask &= ~genRegMask(srcReg);
                dstRegsMask &= ~genRegMask(dstReg);

            } // while (remainingRegCount > 0)

        } // (needToShuffleRegs)

    } // op1 must be multi-reg GT_CALL
}

//------------------------------------------------------------------------
// genAllocLclFrame: Probe the stack and allocate the local stack frame: subtract from SP.
//
// Notes:
//      On MIPS64, this only does the probing; allocating the frame is done when callee-saved registers are saved.
//      This is done before anything has been pushed. The previous frame might have a large outgoing argument
//      space that has been allocated, but the lowest addresses have not been touched. Our frame setup might
//      not touch up to the first 504 bytes. This means we could miss a guard page. On Windows, however,
//      there are always three guard pages, so we will not miss them all. On Linux, there is only one guard
//      page by default, so we need to be more careful. We do an extra probe if we might not have probed
//      recently enough. That is, if a call and prolog establishment might lead to missing a page. We do this
//      on Windows as well just to be consistent, even though it should not be necessary.
//
void CodeGen::genAllocLclFrame(unsigned frameSize, regNumber initReg, bool* pInitRegZeroed, regMaskTP maskArgRegsLiveIn)
{
    assert(compiler->compGeneratingProlog);

    if (frameSize == 0)
    {
        return;
    }

    const target_size_t pageSize = compiler->eeGetPageSize();

    // What offset from the final SP was the last probe? If we haven't probed almost a complete page, and
    // if the next action on the stack might subtract from SP first, before touching the current SP, then
    // we do one more probe at the very bottom. This can happen if we call a function on arm64 that does
    // a "STP fp, lr, [sp-504]!", that is, pre-decrement SP then store. Note that we probe here for arm64,
    // but we don't alter SP.
    target_size_t lastTouchDelta = 0;

    assert(!compiler->info.compPublishStubParam || (REG_SECRET_STUB_PARAM != initReg));

    if (frameSize < pageSize)
    {
        lastTouchDelta = frameSize;
    }
    else if (frameSize < compiler->getVeryLargeFrameSize())
    {
        // We don't need a register for the target of the dummy load
        // NOTE: Here not using REG_R0, for some-mips-cpu liking loongson,
        // lw $0,offset(base) will ignor the addr-exception.
        regNumber rTemp = REG_K0;
        lastTouchDelta  = frameSize;

        for (target_size_t probeOffset = pageSize; probeOffset <= frameSize; probeOffset += pageSize)
        {
            // Generate:
            //    lw rTemp, -probeOffset(SP)  // load into initReg
            getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, rTemp, REG_SPBASE, -(ssize_t)probeOffset);
            regSet.verifyRegUsed(initReg);
            *pInitRegZeroed = false; // The initReg does not contain zero

            lastTouchDelta -= pageSize;
        }

        assert(lastTouchDelta == frameSize % pageSize);
        compiler->unwindPadding();
    }
    else
    {
        assert(frameSize >= compiler->getVeryLargeFrameSize());

        // Emit the following sequence to 'tickle' the pages. Note it is important that stack pointer not change
        // until this is complete since the tickles could cause a stack overflow, and we need to be able to crawl
        // the stack afterward (which means the stack pointer needs to be known).
        //
        // MIPS64 needs 2 registers. See VERY_LARGE_FRAME_SIZE_REG_MASK for how these
        // are reserved.

        regMaskTP availMask = RBM_ALLINT & (regSet.rsGetModifiedRegsMask() | ~RBM_INT_CALLEE_SAVED);
        availMask &= ~maskArgRegsLiveIn;   // Remove all of the incoming argument registers as they are currently live
        availMask &= ~genRegMask(initReg); // Remove the pre-calculated initReg

        regNumber rOffset = initReg;
        regNumber rLimit;
        regMaskTP tempMask;

        // We don't need a register for the target of the dummy load
        // NOTE: Here not using REG_R0, for some-mips-cpu liking loongson,
        // lw $0,offset(base) will ignor the addr-exception.
        regNumber rTemp = REG_K0;

        // We pick the next lowest register number for rLimit
        noway_assert(availMask != RBM_NONE);
        tempMask = genFindLowestBit(availMask);
        rLimit   = genRegNumFromMask(tempMask);
        availMask &= ~tempMask;

        // Generate:
        //
        //      daddiu rOffset, R0, -pageSize
        //      daddiu rLimit, R0, -frameSize
        //
        // loop:
        //      daddu  AT, sp, rOffset,
        //      lw rTemp, 0(AT)
        //      daddiu rOffset, rOffset, -pageSize
        //      slt AT, rOffset, rLimit
        //      beq AT,zero, loop                 // If rLimit is less or equal rOffset, we need to probe this rOffset.
        //      nop

        noway_assert((ssize_t)(int)frameSize == (ssize_t)frameSize); // make sure framesize safely fits within an int

        instGen_Set_Reg_To_Imm(EA_PTRSIZE, rOffset, -(ssize_t)pageSize);
        instGen_Set_Reg_To_Imm(EA_PTRSIZE, rLimit, -(ssize_t)frameSize);

        //
        // Can't have a label inside the ReJIT padding area
        //
        genPrologPadForReJit();

        // There's a "virtual" label here. But we can't create a label in the prolog, so we use the magic
        // `emitIns_J` with a negative `instrCount` to branch back a specific number of instructions.

        getEmitter()->emitIns_R_R_R(INS_daddu, EA_PTRSIZE, REG_AT, REG_SPBASE, rOffset);
        getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, rTemp, REG_AT, 0);
        getEmitter()->emitIns_R_R_I(INS_daddiu, EA_PTRSIZE, rOffset, rOffset, -(ssize_t)pageSize);
        getEmitter()->emitIns_R_R_R(INS_slt, EA_PTRSIZE, REG_AT, rOffset, rLimit);

        assert(REG_AT != rLimit);
        assert(REG_AT != rOffset);
        ssize_t imm = -5<<2;
        getEmitter()->emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
        getEmitter()->emitIns(INS_nop);

        *pInitRegZeroed = false; // The initReg does not contain zero

        compiler->unwindPadding();

        lastTouchDelta = frameSize % pageSize;
    }

    if (lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES > pageSize)
    {

        assert(lastTouchDelta + STACK_PROBE_BOUNDARY_THRESHOLD_BYTES < 2 * pageSize);
        getEmitter()->emitIns_R_R_I(INS_lw, EA_4BYTE, REG_K0, REG_SPBASE, -(ssize_t)frameSize);
        compiler->unwindPadding();

        regSet.verifyRegUsed(initReg);
        *pInitRegZeroed = false; // The initReg does not contain zero
    }
}

#endif // _TARGET_MIPS64_
