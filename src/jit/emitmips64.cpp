// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                             emitmips64.cpp                                XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#if defined(_TARGET_MIPS64_)

/*****************************************************************************/
/*****************************************************************************/

#include "instr.h"
#include "emit.h"
#include "codegen.h"

////These are used for mips64 instrs's dump and ::emitInsMayWriteToGCReg.
#define  MIPS_OP_SPECIAL       0x0
#define  MIPS_OP_REGIMM        0x1

#define  MIPS_OP_J             0x2
#define  MIPS_OP_JAL           0x3
#define  MIPS_OP_BEQ           0x4
#define  MIPS_OP_BNE           0x5
#define  MIPS_OP_BLEZ          0x6
#define  MIPS_OP_BGTZ          0x7
#define  MIPS_OP_ADDI          0x8
#define  MIPS_OP_ADDIU         0x9
#define  MIPS_OP_SLTI          0xa
#define  MIPS_OP_SLTIU         0xb
#define  MIPS_OP_ANDI          0xc
#define  MIPS_OP_ORI           0xd
#define  MIPS_OP_XORI          0xe
#define  MIPS_OP_LUI           0xf
#define  MIPS_OP_BEQL          0x14
#define  MIPS_OP_BNEL          0x15
#define  MIPS_OP_BLEZL         0x16
#define  MIPS_OP_BGTZL         0x17
#define  MIPS_OP_DADDI         0x18
#define  MIPS_OP_DADDIU        0x19
#define  MIPS_OP_PREF          0x33


//#define  MIPS_OP_COP0          0x10
#define  MIPS_OP_COP1          0x11
#define  MIPS_OP_COP2          0x12

#define  MIPS_OP_COP1X         0x13

#define  MIPS_OP_SPECIAL2      0x1c
#define  MIPS_OP_SPECIAL3      0x1f


#define  MIPS_OP_LB            0x20
#define  MIPS_OP_LH            0x21

#define  MIPS_OP_LBU           0x24
#define  MIPS_OP_LHU           0x25
#define  MIPS_OP_LWU           0x27
#define  MIPS_OP_LWL           0x22
#define  MIPS_OP_LWR           0x26
#define  MIPS_OP_LW            0x23
#define  MIPS_OP_LD            0x37
#define  MIPS_OP_LDL           0x1a
#define  MIPS_OP_LDR           0x1b
#define  MIPS_OP_LL            0x30
#define  MIPS_OP_LLD           0x34
#define  MIPS_OP_LWC1          0x31
#define  MIPS_OP_LDC1          0x35


#define  MIPS_OP_SB            0x28
#define  MIPS_OP_SH            0x29
#define  MIPS_OP_SW            0x2b

#define  MIPS_OP_SD            0x3f
#define  MIPS_OP_SWL           0x2a
#define  MIPS_OP_SWR           0x2e
#define  MIPS_OP_SDL           0x2c
#define  MIPS_OP_SDR           0x2d
#define  MIPS_OP_SWC1          0x39
#define  MIPS_OP_SDC1          0x3d
#define  MIPS_OP_SC            0x38
#define  MIPS_OP_SCD           0x3c

////MIPS_OP_COP1X         0x13
#define  MIPS_COP1X_LWXC1      0x0
#define  MIPS_COP1X_LDXC1      0x1
#define  MIPS_COP1X_LUXC1      0x5
#define  MIPS_COP1X_SWXC1      0x8
#define  MIPS_COP1X_SDXC1      0x9
#define  MIPS_COP1X_SUXC1      0xd

#define  MIPS_COP1X_PREFX      0x0f
#define  MIPS_COP1X_MADD_S     0x20
#define  MIPS_COP1X_MADD_D     0x21
#define  MIPS_COP1X_MSUB_S     0x28
#define  MIPS_COP1X_MSUB_D     0x29
#define  MIPS_COP1X_NMADD_S    0x30
#define  MIPS_COP1X_NMADD_D    0x31
#define  MIPS_COP1X_NMSUB_S    0x38
#define  MIPS_COP1X_NMSUB_D    0x39

////MIPS_OP_COP1         0x11
#define  MIPS_COP1_MFC1      0x0
#define  MIPS_COP1_DMFC1     0x1
#define  MIPS_COP1_CFC1      0x2
#define  MIPS_COP1_MFHC1     0x3
#define  MIPS_COP1_MTC1      0x4
#define  MIPS_COP1_DMTC1     0x5
#define  MIPS_COP1_CTC1      0x6
#define  MIPS_COP1_MTHC1     0x7
#define  MIPS_COP1_BC        0x8
#define  MIPS_COP1_FMTS      0x10
#define  MIPS_COP1_FMTD      0x11
#define  MIPS_COP1_FMTW      0x14
#define  MIPS_COP1_FMTL      0x15


////MIPS_OP_SPECIAL
//#define  MIPS_SPEC_SYSCALL        0xc
//#define  MIPS_SPEC_LSA         0x5
#define  MIPS_SPEC_SLL         0x0
#define  MIPS_SPEC_MOVCI       0x1
#define  MIPS_SPEC_SRL         0x2
#define  MIPS_SPEC_SRA         0x3
#define  MIPS_SPEC_SLLV        0x4
#define  MIPS_SPEC_SRLV        0x6
#define  MIPS_SPEC_SRAV        0x7
#define  MIPS_SPEC_JR          0x8
#define  MIPS_SPEC_JALR        0x9
#define  MIPS_SPEC_MOVZ        0xa
#define  MIPS_SPEC_MOVN        0xb
#define  MIPS_SPEC_BREAK       0xd
#define  MIPS_SPEC_SYNC        0xf
#define  MIPS_SPEC_MFHI        0x10
#define  MIPS_SPEC_MTHI        0x11
#define  MIPS_SPEC_MFLO        0x12
#define  MIPS_SPEC_MTLO        0x13
#define  MIPS_SPEC_DSLLV       0x14
#define  MIPS_SPEC_DLSA        0x15
#define  MIPS_SPEC_DSRLV       0x16
#define  MIPS_SPEC_DSRAV       0x17
#define  MIPS_SPEC_MULT        0x18
#define  MIPS_SPEC_MULTU       0x19
#define  MIPS_SPEC_DIV         0x1a
#define  MIPS_SPEC_DIVU        0x1b
#define  MIPS_SPEC_DMULT       0x1c
#define  MIPS_SPEC_DMULTU      0x1d
#define  MIPS_SPEC_DDIV        0x1e
#define  MIPS_SPEC_DDIVU       0x1f
#define  MIPS_SPEC_ADD         0x20
#define  MIPS_SPEC_ADDU        0x21
#define  MIPS_SPEC_SUB         0x22
#define  MIPS_SPEC_SUBU        0x23
#define  MIPS_SPEC_AND         0x24
#define  MIPS_SPEC_OR          0x25
#define  MIPS_SPEC_XOR         0x26
#define  MIPS_SPEC_NOR         0x27
#define  MIPS_SPEC_SLT         0x2a
#define  MIPS_SPEC_SLTU        0x2b
#define  MIPS_SPEC_DADD        0x2c
#define  MIPS_SPEC_DADDU       0x2d
#define  MIPS_SPEC_DSUB        0x2e
#define  MIPS_SPEC_DSUBU       0x2f
#define  MIPS_SPEC_DSLL        0x38
#define  MIPS_SPEC_DSRL        0x3a
#define  MIPS_SPEC_DSRA        0x3b
#define  MIPS_SPEC_DSLL32      0x3c
#define  MIPS_SPEC_DSRL32      0x3e
#define  MIPS_SPEC_DSRA32      0x3f

////MIPS_OP_SPECIAL2
#define  MIPS_SPEC2_MADD       0x0
#define  MIPS_SPEC2_MADDU      0x1
#define  MIPS_SPEC2_MUL        0x2
#define  MIPS_SPEC2_MSUB       0x4
#define  MIPS_SPEC2_MSUBU      0x5
#define  MIPS_SPEC2_CLZ        0x20
#define  MIPS_SPEC2_CLO        0x21
#define  MIPS_SPEC2_DCLZ       0x24
#define  MIPS_SPEC2_DCLO       0x25

////MIPS_OP_SPECIAL3
#define  MIPS_SPEC3_EXT       0x0
#define  MIPS_SPEC3_DEXTM     0x1
#define  MIPS_SPEC3_DEXTU     0x2
#define  MIPS_SPEC3_DEXT      0x3
#define  MIPS_SPEC3_INS       0x4
#define  MIPS_SPEC3_DINSM     0x5
#define  MIPS_SPEC3_DINSU     0x6
#define  MIPS_SPEC3_DINS      0x7
#define  MIPS_SPEC3_BSHFL     0x20
#define  MIPS_SPEC3_DBSHFL    0x24
#define  MIPS_SPEC3_RDHWR     0x3b

////MIPS_OP_REGIMM
#define  MIPS_REGIMM_BLTZ        0x0
#define  MIPS_REGIMM_BGEZ        0x1
#define  MIPS_REGIMM_BLTZAL      0x10
#define  MIPS_REGIMM_BGEZAL      0x11
//#define  MIPS_REGIMM_BLTZL       0x2
//#define  MIPS_REGIMM_BGEZL       0x3
//#define  MIPS_REGIMM_BLTZALL     0x12
//#define  MIPS_REGIMM_BGEZALL     0x13

//// add other define-macro here.


/* static */ bool emitter::strictMIPSAsm = true;

/*****************************************************************************/

const instruction emitJumpKindInstructions[] = {
    INS_nop,

#define JMP_SMALL(en, rev, ins) INS_##ins,
#include "emitjmps.h"
};

const emitJumpKind emitReverseJumpKinds[] = {
    EJ_NONE,

#define JMP_SMALL(en, rev, ins) EJ_##rev,
#include "emitjmps.h"
};

/*****************************************************************************
 * Look up the instruction for a jump kind
 */

/*static*/ instruction emitter::emitJumpKindToIns(emitJumpKind jumpKind)
{
    assert((unsigned)jumpKind < ArrLen(emitJumpKindInstructions));
    return emitJumpKindInstructions[jumpKind];
}

/*****************************************************************************
* Look up the jump kind for an instruction. It better be a conditional
* branch instruction with a jump kind!
*/

/*static*/ emitJumpKind emitter::emitInsToJumpKind(instruction ins)
{
assert(!"unimplemented on MIPS yet");
    return EJ_NONE;
#if 0
    for (unsigned i = 0; i < ArrLen(emitJumpKindInstructions); i++)
    {
        if (ins == emitJumpKindInstructions[i])
        {
            emitJumpKind ret = (emitJumpKind)i;
            assert(EJ_NONE < ret && ret < EJ_COUNT);
            return ret;
        }
    }
    unreached();
#endif
}

/*****************************************************************************
 * Reverse the conditional jump
 */

/*static*/ emitJumpKind emitter::emitReverseJumpKind(emitJumpKind jumpKind)
{
    assert(jumpKind < EJ_COUNT);
    return emitReverseJumpKinds[jumpKind];
}

/*****************************************************************************
 *
 *  Return the allocated size (in bytes) of the given instruction descriptor.
 */

size_t emitter::emitSizeOfInsDsc(instrDesc* id)
{
    if (emitIsScnsInsDsc(id))
        return SMALL_IDSC_SIZE;

    insOpts insOp = id->idInsOpt();

    switch (insOp)
    {
        case INS_OPTS_RELOC:
        case INS_OPTS_RC:
            return sizeof(instrDescJmp);
            //break;

        case INS_OPTS_RL:
        case INS_OPTS_JR:
        case INS_OPTS_J:
            return sizeof(instrDescJmp);

        case INS_OPTS_C:
            if (id->idIsLargeCall())
            {
                /* Must be a "fat" call descriptor */
                return sizeof(instrDescCGCA);
            }
            else
            {
                assert(!id->idIsLargeDsp());
                assert(!id->idIsLargeCns());
                return sizeof(instrDesc);
            }
            //break;

        case INS_OPTS_NONE:
            return sizeof(instrDesc);
        default:
            NO_WAY("unexpected instruction descriptor format");
            break;
    }
}

#ifdef DEBUG
/*****************************************************************************
 *
 *  The following called for each recorded instruction -- use for debugging.
 */
void emitter::emitInsSanityCheck(instrDesc* id)
{
    /* What instruction format have we got? */

    switch (id->idInsFmt())
    {
        case IF_OPCODE:
        case IF_OPCODES_16:
        case IF_OP_FMT:
        case IF_OP_FMT_16:
        case IF_OP_FMTS_16:
        case IF_FMT_FUNC:
        case IF_FMT_FUNC_6:
        case IF_FMT_FUNC_16:
        case IF_FMT_FUNCS_6:
        case IF_FMT_FUNCS_16:
        case IF_FMT_FUNCS_6A:
        case IF_FMT_FUNCS_11A:
        case IF_FUNC:
        case IF_FUNC_6:
        case IF_FUNC_16:
        case IF_FUNC_21:
        case IF_FUNCS_6:
        case IF_FUNCS_6A:
        case IF_FUNCS_6B:
        case IF_FUNCS_6C:
        case IF_FUNCS_6D:
        case IF_FUNCS_11:
            break;

        default:
            printf("unexpected format %s\n", emitIfName(id->idInsFmt()));
            assert(!"Unexpected format");
            break;
    }

}
#endif // DEBUG

bool emitter::emitInsMayWriteToGCReg(instrDesc* id)
{
    unsigned int instr = id->idAddr()->iiaGetInstrEncode();

    // These are the formats with "destination" registers:

    /* FIXME for MIPS: maybe add other on future. */
    switch ((instr>>26) & 0x3f)
    {
        case 0x0://special:
            switch (instr & 0x3f)
            {
                case MIPS_SPEC_SLL:
                case MIPS_SPEC_SRL:
                case MIPS_SPEC_SRA:
                case MIPS_SPEC_SLLV:
                case MIPS_SPEC_SRLV:
                case MIPS_SPEC_SRAV:
                //case MIPS_SPEC_MOVZ:
                //case MIPS_SPEC_MOVN:
                case MIPS_SPEC_MFHI:
                //case MIPS_SPEC_MTHI:
                case MIPS_SPEC_MFLO:
                //case MIPS_SPEC_MTLO:
                case MIPS_SPEC_DSLLV:
                case MIPS_SPEC_DSRLV:
                case MIPS_SPEC_DSRAV:
                case MIPS_SPEC_ADD:
                case MIPS_SPEC_ADDU:
                case MIPS_SPEC_SUB:
                case MIPS_SPEC_SUBU:
                case MIPS_SPEC_AND:
                case MIPS_SPEC_OR:
                case MIPS_SPEC_XOR:
                case MIPS_SPEC_NOR:
                case MIPS_SPEC_SLT:
                case MIPS_SPEC_SLTU:
                case MIPS_SPEC_DADD:
                case MIPS_SPEC_DADDU:
                case MIPS_SPEC_DSUB:
                case MIPS_SPEC_DSUBU:
                case MIPS_SPEC_DSLL:
                case MIPS_SPEC_DSRL:
                case MIPS_SPEC_DSRA:
                case MIPS_SPEC_DSLL32:
                case MIPS_SPEC_DSRL32:
                case MIPS_SPEC_DSRA32:
                    //reg = (regNumber)((instr>>11) & 0x1f);
                    return true;
            }
            return false;
        case 0x1c://special2:
            switch (instr & 0x3f)
            {
                case MIPS_SPEC2_MUL:
                case MIPS_SPEC2_CLZ:
                case MIPS_SPEC2_CLO:
                case MIPS_SPEC2_DCLZ:
                case MIPS_SPEC2_DCLO:
                    //reg = (regNumber)((instr>>11) & 0x1f);
                    return true;
            }
            return false;
        case 0x1f://special3:
            switch (instr & 0x3f)
            {
                case MIPS_SPEC3_EXT:
                case MIPS_SPEC3_DEXTM:
                case MIPS_SPEC3_DEXTU:
                case MIPS_SPEC3_DEXT:
                case MIPS_SPEC3_INS:
                case MIPS_SPEC3_DINS:
                case MIPS_SPEC3_DINSM:
                case MIPS_SPEC3_DINSU:
                    //reg = (regNumber)((instr>>16) & 0x1f);
                    return true;
                case MIPS_SPEC3_DBSHFL:
                case MIPS_SPEC3_BSHFL:
                case MIPS_SPEC3_RDHWR:
                    //reg = (regNumber)((instr>>11) & 0x1f);
                    return true;
            }
            return false;

        case 0x11://cop1:
            switch ((instr>>21) & 0x1f)
            {
                case MIPS_COP1_MFC1:
                case MIPS_COP1_DMFC1:
                //maybe others instrs.
                    //reg = (regNumber)((instr>>11) & 0x1f);
                    return true;
            }
            return false;

        //alu
        case MIPS_OP_SLTI:
        case MIPS_OP_SLTIU:
        case MIPS_OP_ANDI:
        case MIPS_OP_ORI:
        case MIPS_OP_XORI:
        case MIPS_OP_LUI:
        case MIPS_OP_ADDI:
        case MIPS_OP_ADDIU:
        case MIPS_OP_DADDI:
        case MIPS_OP_DADDIU:
        //load
        case 0x20://INS_lb:
        case 0x24://INS_lbu:
        case 0x21://INS_lh:
        case 0x25://INS_lhu:
        case 0x23://INS_lw:
        case 0x27://INS_lwu:
        case 0x37://INS_ld:
            //reg = (regNumber)((instr>>16) & 0x1f);
            return true;
        //default:
        //    return false;
    }
    return false;
}

bool emitter::emitInsWritesToLclVarStackLoc(instrDesc* id)
{
    if (!id->idIsLclVar())
        return false;

    instruction ins = id->idIns();

    // This list is related to the list of instructions used to store local vars in emitIns_S_R().
    // We don't accept writing to float local vars.

    switch (ins)
    {
        case INS_sb:
        case INS_sh:
        case INS_sw:
        case INS_sd:
        //case INS_sdc1:
        //case INS_swc1:
            return true;
        default:
            return false;
    }
}

// Takes an instrDesc 'id' and uses the instruction 'ins' to determine the
// size of the target register that is written or read by the instruction.
// Note that even if EA_4BYTE is returned a load instruction will still
// always zero the upper 4 bytes of the target register.
// This method is required so that we can distinguish between loads that are
// sign-extending as they can have two different sizes for their target register.
// Additionally for instructions like 'lw' and 'sw', 'ld' and 'sd' these can load/store
// either 4 byte or 8 bytes to/from the target register.
// By convention the small unsigned load instructions are considered to write
// a 4 byte sized target register, though since these also zero the upper 4 bytes
// they could equally be considered to write the unsigned value to full 8 byte register.
//
emitAttr emitter::emitInsTargetRegSize(instrDesc* id)
{
/* FIXME for MIPS. */
    assert(!"unimplemented on MIPS yet");

    instruction ins    = id->idIns();
    emitAttr    result = EA_UNKNOWN;

    // This is used to determine the size of the target registers for a load/store instruction

    switch (ins)
    {
        case INS_lb:
        case INS_lbu:
        case INS_sb:
            result = EA_4BYTE;
            break;

        case INS_lh:
        case INS_lhu:
        case INS_sh:
            result = EA_4BYTE;
            break;

        case INS_lw:
        case INS_lwu:
        case INS_lwc1:
        case INS_sw:
        case INS_swc1:
            result = EA_4BYTE;
            break;

        case INS_ld:
        case INS_ldc1:
        case INS_sd:
        case INS_sdc1:
            result = EA_8BYTE;
            break;

        default:
            NO_WAY("unexpected instruction");
            break;
    }
    return result;
}

// Takes an instrDesc and uses the instruction to determine the 'size' of the
// data that is loaded from memory.
//
emitAttr emitter::emitInsLoadStoreSize(instrDesc* id)
{
/* FIXME for MIPS. */
    instruction ins    = id->idIns();
    emitAttr    result = EA_UNKNOWN;

    // The 'result' returned is the 'size' of the data that is loaded from memory.

    switch (ins)
    {
        case INS_lb:
        case INS_lbu:
        case INS_sb:
            result = EA_1BYTE;
            break;

        case INS_lh:
        case INS_lhu:
        case INS_sh:
            result = EA_2BYTE;
            break;

        case INS_lw:
        case INS_lwu:
            result = EA_4BYTE;
            break;

        case INS_ld:
        case INS_sd:
            result = id->idOpSize();
            break;

        default:
            NO_WAY("unexpected instruction");
            break;
    }
    return result;
}

/*****************************************************************************/
#ifdef DEBUG

// clang-format off
/* FIXME for MIPS */
static const char * const  xRegNames[] =
{
    #define REGDEF(name, rnum, mask, xname, wname) xname,
    #include "register.h"
};

/* FIXME for MIPS */
static const char * const  wRegNames[] =
{
    #define REGDEF(name, rnum, mask, xname, wname) wname,
    #include "register.h"
};

/* FIXME for MIPS */
static const char * const  fRegNames[] =
{
    "f0",  "f1",  "f2",  "f3",  "f4",
    "f5",  "f6",  "f7",  "f8",  "f9",
    "f10", "f11", "f12", "f13", "f14",
    "f15", "f16", "f17", "f18", "f19",
    "f20", "f21", "f22", "f23", "f24",
    "f25", "f26", "f27", "f28", "f29",
    "f30", "f31"
};

/* FIXME for MIPS */
static const char * const  qRegNames[] =
{
    "q0",  "q1",  "q2",  "q3",  "q4",
    "q5",  "q6",  "q7",  "q8",  "q9",
    "q10", "q11", "q12", "q13", "q14",
    "q15", "q16", "q17", "q18", "q19",
    "q20", "q21", "q22", "q23", "q24",
    "q25", "q26", "q27", "q28", "q29",
    "q30", "q31"
};

/* FIXME for MIPS */
static const char * const  hRegNames[] =
{
    "h0",  "h1",  "h2",  "h3",  "h4",
    "h5",  "h6",  "h7",  "h8",  "h9",
    "h10", "h11", "h12", "h13", "h14",
    "h15", "h16", "h17", "h18", "h19",
    "h20", "h21", "h22", "h23", "h24",
    "h25", "h26", "h27", "h28", "h29",
    "h30", "h31"
};
/* FIXME for MIPS */
static const char * const  bRegNames[] =
{
    "b0",  "b1",  "b2",  "b3",  "b4",
    "b5",  "b6",  "b7",  "b8",  "b9",
    "b10", "b11", "b12", "b13", "b14",
    "b15", "b16", "b17", "b18", "b19",
    "b20", "b21", "b22", "b23", "b24",
    "b25", "b26", "b27", "b28", "b29",
    "b30", "b31"
};
// clang-format on

/*****************************************************************************
 *
 *  Return a string that represents the given register.
 */

const char* emitter::emitRegName(regNumber reg, emitAttr size, bool varName)
{
/* FIXME for MIPS. */
    assert(reg < REG_COUNT);

    const char* rn = nullptr;

    if (size == EA_16BYTE || size == EA_8BYTE)
    {
        rn = xRegNames[reg];
    }
    else if (size == EA_4BYTE || size == EA_1BYTE)
    {
        rn = wRegNames[reg];
    }
    else if (isVectorRegister(reg))
    {
        if (size == EA_16BYTE)
        {
            rn = qRegNames[reg - REG_F0];
        }
        else if (size == EA_2BYTE)
        {
            rn = hRegNames[reg - REG_F0];
        }
        else if (size == EA_1BYTE)
        {
            rn = bRegNames[reg - REG_F0];
        }
    }

    assert(rn != nullptr);

    return rn;
}

/*****************************************************************************
 *
 *  Return a string that represents the given register.
 */

const char* emitter::emitVectorRegName(regNumber reg)
{
/* FIXME for MIPS. */
    assert((reg >= REG_F0) && (reg <= REG_F31));

    int index = (int)reg - (int)REG_F0;

    return fRegNames[index];
}
#endif // DEBUG

/*****************************************************************************
 *
 *  Returns the base encoding of the given CPU instruction.
 */

emitter::insFormat emitter::emitInsFormat(instruction ins)
{
    // clang-format off
    const static insFormat insFormats[] =
    {
        #define INSTS(id, nm, fp, ldst, fmt, e1) fmt,
        #include "instrs.h"
    };
    // clang-format on

    assert(ins < ArrLen(insFormats));
    assert((insFormats[ins] != IF_NONE));

    return insFormats[ins];
}

/* FIXME for MIPS: */
// INST_FP is 1
#define LD 2
#define ST 4
#define CMP 8

// clang-format off
/*static*/ const BYTE CodeGenInterface::instInfo[] =
{
    #define INSTS(id, nm, fp, ldst, fmt, e1) ldst | INST_FP*fp,
    #include "instrs.h"
};
// clang-format on

/*****************************************************************************
 *
 *  Returns true if the instruction is some kind of compare or test instruction
 */

bool emitter::emitInsIsCompare(instruction ins)
{
    assert(!"unimplemented on MIPS yet");
    return false;
}

/*****************************************************************************
 *
 *  Returns true if the instruction is some kind of load instruction
 */

bool emitter::emitInsIsLoad(instruction ins)
{
    // We have pseudo ins like lea which are not included in emitInsLdStTab.
    if (ins < ArrLen(CodeGenInterface::instInfo))
        return (CodeGenInterface::instInfo[ins] & LD) ? true : false;
    else
        return false;
}
/*****************************************************************************
 *
 *  Returns true if the instruction is some kind of store instruction
 */

bool emitter::emitInsIsStore(instruction ins)
{
    // We have pseudo ins like lea which are not included in emitInsLdStTab.
    if (ins < ArrLen(CodeGenInterface::instInfo))
        return (CodeGenInterface::instInfo[ins] & ST) ? true : false;
    else
        return false;
}

/*****************************************************************************
 *
 *  Returns true if the instruction is some kind of load/store instruction
 */

bool emitter::emitInsIsLoadOrStore(instruction ins)
{
assert(!"unimplemented on MIPS yet");
return false;
}

#undef LD
#undef ST
#undef CMP

/*****************************************************************************
 *
 *  Returns the specific encoding of the given CPU instruction and format
 */

emitter::code_t emitter::emitInsCode(instruction ins /*, insFormat fmt*/)
{
    code_t    code           = BAD_CODE;

    // clang-format off
    const static code_t insCode[] =
    {
        #define INSTS(id, nm, fp, ldst, fmt, e1) e1,
        #include "instrs.h"
    };
    // clang-format on

    code = insCode[ins];

    assert((code != BAD_CODE));

    return code;
}

// true if this 'imm' can be encoded as a input operand to daddiu instruction
/*static*/ bool emitter::emitIns_valid_imm_for_mov(INT64 imm, emitAttr size)
{
    return isValidSimm16(imm);
}

// true if this 'imm' can be encoded as a input operand to MOV.fmt instruction
/*static*/ bool emitter::emitIns_valid_imm_for_fmov(double immDbl)
{
    return false;
}

// true if this 'imm' can be encoded as a input operand to an add instruction
/*static*/ bool emitter::emitIns_valid_imm_for_add(INT64 imm, emitAttr size)
{
    return isValidSimm16(imm);
}

// true if this 'imm' can be encoded as a input operand to an non-add/sub alu instruction
/*static*/ bool emitter::emitIns_valid_imm_for_cmp(INT64 imm, emitAttr size)
{
    return isValidSimm16(imm);
}

// true if this 'imm' can be encoded as a input operand to an non-add/sub alu instruction
/*static*/ bool emitter::emitIns_valid_imm_for_alu(INT64 imm, emitAttr size)
{
    return (0 <= imm) && (imm <= 0xffff);
    //assert(!"unimplemented on MIPS yet");
    //return isValidSimm16(imm);
}

// true if this 'imm' can be encoded as the offset in a ld/sd instruction
/*static*/ bool emitter::emitIns_valid_imm_for_ldst_offset(INT64 imm, emitAttr attr)
{
    return isValidSimm16(imm);
}

/************************************************************************
 *
 *   A helper method to return the natural scale for an EA 'size'
 */

/*static*/ unsigned emitter::NaturalScale_helper(emitAttr size)
{
    assert(size == EA_1BYTE || size == EA_2BYTE || size == EA_4BYTE || size == EA_8BYTE || size == EA_16BYTE);

    unsigned result = 0;
    unsigned utemp  = (unsigned)size;

    // Compute log base 2 of utemp (aka 'size')
    while (utemp > 1)
    {
        result++;
        utemp >>= 1;
    }

    return result;
}

/************************************************************************
 *
 *  A helper method to perform a Rotate-Right shift operation
 *  the source is 'value' and it is rotated right by 'sh' bits
 *  'value' is considered to be a fixed size 'width' set of bits.
 *
 *  Example
 *      value is '00001111', sh is 2 and width is 8
 *     result is '11000011'
 */

/*static*/ UINT64 emitter::ROR_helper(UINT64 value, unsigned sh, unsigned width)
{
assert(!"unimplemented on MIPS yet");
return NULL;
}
/************************************************************************
 *
 *  A helper method to perform a 'NOT' bitwise complement operation.
 *  'value' is considered to be a fixed size 'width' set of bits.
 *
 *  Example
 *      value is '01001011', and width is 8
 *     result is '10110100'
 */

/*static*/ UINT64 emitter::NOT_helper(UINT64 value, unsigned width)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(width <= 64);

    UINT64 result = ~value;

    if (width < 64)
    {
        // Check that 'value' fits in 'width' bits. Don't consider "sign" bits above width.
        UINT64 maxVal       = 1ULL << width;
        UINT64 lowBitsMask  = maxVal - 1;
        UINT64 signBitsMask = ~lowBitsMask | (1ULL << (width - 1)); // The high bits must be set, and the top bit
                                                                    // (sign bit) must be set.
        assert((value < maxVal) || ((value & signBitsMask) == signBitsMask));

        // mask off any extra bits that we got from the complement operation
        result &= lowBitsMask;
    }

    return result;
#endif
}

/************************************************************************
 *
 *  A helper method to perform a bit Replicate operation
 *  the source is 'value' with a fixed size 'width' set of bits.
 *  value is replicated to fill out 32 or 64 bits as determined by 'size'.
 *
 *  Example
 *      value is '11000011' (0xE3), width is 8 and size is EA_8BYTE
 *     result is '11000011 11000011 11000011 11000011 11000011 11000011 11000011 11000011'
 *               0xE3E3E3E3E3E3E3E3
 */

/*static*/ UINT64 emitter::Replicate_helper(UINT64 value, unsigned width, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
}

/************************************************************************
 *
 *  Convert an imm(N,r,s) into a 64-bit immediate
 *  inputs 'bmImm' a bitMaskImm struct
 *         'size' specifies the size of the result (64 or 32 bits)
 */

/*static*/ INT64 emitter::emitDecodeBitMaskImm(const emitter::bitMaskImm bmImm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
}

/*****************************************************************************
 *
 *  Check if an immediate can use the left shifted by 12 bits encoding
 */

/*static*/ bool emitter::canEncodeWithShiftImmBy12(INT64 imm)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    if (imm < 0)
    {
        imm = -imm; // convert to unsigned
    }

    if (imm < 0)
    {
        return false; // Must be MIN_INT64
    }

    if ((imm & 0xfff) != 0) // Now the low 12 bits all have to be zero
    {
        return false;
    }

    imm >>= 12; // shift right by 12 bits

    return (imm <= 0x0fff); // Does it fit in 12 bits
#endif
}

/*****************************************************************************
 *
 *  Normalize the 'imm' so that the upper bits, as defined by 'size' are zero
 */

/*static*/ INT64 emitter::normalizeImm64(INT64 imm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    unsigned immWidth = getBitWidth(size);
    INT64    result   = imm;

    if (immWidth < 64)
    {
        // Check that 'imm' fits in 'immWidth' bits. Don't consider "sign" bits above width.
        INT64 maxVal      = 1LL << immWidth;
        INT64 lowBitsMask = maxVal - 1;
        INT64 hiBitsMask  = ~lowBitsMask;
        INT64 signBitsMask =
            hiBitsMask | (1LL << (immWidth - 1)); // The high bits must be set, and the top bit (sign bit) must be set.
        assert((imm < maxVal) || ((imm & signBitsMask) == signBitsMask));

        // mask off the hiBits
        result &= lowBitsMask;
    }
    return result;
#endif
}

/*****************************************************************************
 *
 *  Normalize the 'imm' so that the upper bits, as defined by 'size' are zero
 */

/*static*/ INT32 emitter::normalizeImm32(INT32 imm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    unsigned immWidth = getBitWidth(size);
    INT32    result   = imm;

    if (immWidth < 32)
    {
        // Check that 'imm' fits in 'immWidth' bits. Don't consider "sign" bits above width.
        INT32 maxVal       = 1 << immWidth;
        INT32 lowBitsMask  = maxVal - 1;
        INT32 hiBitsMask   = ~lowBitsMask;
        INT32 signBitsMask = hiBitsMask | (1 << (immWidth - 1)); // The high bits must be set, and the top bit
                                                                 // (sign bit) must be set.
        assert((imm < maxVal) || ((imm & signBitsMask) == signBitsMask));

        // mask off the hiBits
        result &= lowBitsMask;
    }
    return result;
#endif
}

/************************************************************************
 *
 *  returns true if 'imm' of 'size bits (32/64) can be encoded
 *  using the MIPS64 'bitmask immediate' form.
 *  When a non-null value is passed for 'wbBMI' then this method
 *  writes back the 'N','S' and 'R' values use to encode this immediate
 *
 */

/*static*/ bool emitter::canEncodeBitMaskImm(INT64 imm, emitAttr size, emitter::bitMaskImm* wbBMI)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(isValidGeneralDatasize(size)); // Only EA_4BYTE or EA_8BYTE forms

    unsigned immWidth = (size == EA_8BYTE) ? 64 : 32;
    unsigned maxLen   = (size == EA_8BYTE) ? 6 : 5;

    imm = normalizeImm64(imm, size);

    // Starting with len=1, elemWidth is 2 bits
    //               len=2, elemWidth is 4 bits
    //               len=3, elemWidth is 8 bits
    //               len=4, elemWidth is 16 bits
    //               len=5, elemWidth is 32 bits
    // (optionally)  len=6, elemWidth is 64 bits
    //
    for (unsigned len = 1; (len <= maxLen); len++)
    {
        unsigned elemWidth = 1 << len;
        UINT64   elemMask  = ((UINT64)-1) >> (64 - elemWidth);
        UINT64   tempImm   = (UINT64)imm;        // A working copy of 'imm' that we can mutate
        UINT64   elemVal   = tempImm & elemMask; // The low 'elemWidth' bits of 'imm'

        // Check for all 1's or 0's as these can't be encoded
        if ((elemVal == 0) || (elemVal == elemMask))
            continue;

        // 'checkedBits' is the count of bits that are known to match 'elemVal' when replicated
        unsigned checkedBits = elemWidth; // by definition the first 'elemWidth' bits match

        // Now check to see if each of the next bits match...
        //
        while (checkedBits < immWidth)
        {
            tempImm >>= elemWidth;

            UINT64 nextElem = tempImm & elemMask;
            if (nextElem != elemVal)
            {
                // Not matching, exit this loop and checkedBits will not be equal to immWidth
                break;
            }

            // The 'nextElem' is matching, so increment 'checkedBits'
            checkedBits += elemWidth;
        }

        // Did the full immediate contain bits that can be formed by repeating 'elemVal'?
        if (checkedBits == immWidth)
        {
            // We are not quite done, since the only values that we can encode as a
            // 'bitmask immediate' are those that can be formed by starting with a
            // bit string of 0*1* that is rotated by some number of bits.
            //
            // We check to see if 'elemVal' can be formed using these restrictions.
            //
            // Observation:
            // Rotating by one bit any value that passes these restrictions
            // can be xor-ed with the original value and will result it a string
            // of bits that have exactly two 1 bits: 'elemRorXor'
            // Further the distance between the two one bits tells us the value
            // of S and the location of the 1 bits tells us the value of R
            //
            // Some examples:   (immWidth is 8)
            //
            // S=4,R=0   S=5,R=3   S=3,R=6
            // elemVal:        00001111  11100011  00011100
            // elemRor:        10000111  11110001  00001110
            // elemRorXor:     10001000  00010010  00010010
            //      compute S  45678---  ---5678-  ---3210-
            //      compute R  01234567  ---34567  ------67

            UINT64 elemRor    = ROR_helper(elemVal, 1, elemWidth); // Rotate 'elemVal' Right by one bit
            UINT64 elemRorXor = elemVal ^ elemRor;                 // Xor elemVal and elemRor

            // If we only have a two-bit change in elemROR then we can form a mask for this value
            unsigned bitCount = 0;
            UINT64   oneBit   = 0x1;
            unsigned R        = elemWidth; // R is shift count for ROR (rotate right shift)
            unsigned S        = 0;         // S is number of consecutive one bits
            int      incr     = -1;

            // Loop over the 'elemWidth' bits in 'elemRorXor'
            //
            for (unsigned bitNum = 0; bitNum < elemWidth; bitNum++)
            {
                if (incr == -1)
                {
                    R--; // We decrement R by one whenever incr is -1
                }
                if (bitCount == 1)
                {
                    S += incr; // We incr/decr S, after we find the first one bit in 'elemRorXor'
                }

                // Is this bit position a 1 bit in 'elemRorXor'?
                //
                if (oneBit & elemRorXor)
                {
                    bitCount++;
                    // Is this the first 1 bit that we found in 'elemRorXor'?
                    if (bitCount == 1)
                    {
                        // Does this 1 bit represent a transition to zero bits?
                        bool toZeros = ((oneBit & elemVal) != 0);
                        if (toZeros)
                        {
                            // S :: Count down from elemWidth
                            S    = elemWidth;
                            incr = -1;
                        }
                        else // this 1 bit represent a transition to one bits.
                        {
                            // S :: Count up from zero
                            S    = 0;
                            incr = +1;
                        }
                    }
                    else // bitCount > 1
                    {
                        // We found the second (or third...) 1 bit in 'elemRorXor'
                        incr = 0; // stop decrementing 'R'

                        if (bitCount > 2)
                        {
                            // More than 2 transitions from 0/1 in 'elemVal'
                            // This means that 'elemVal' can't be encoded
                            // using a 'bitmask immediate'.
                            //
                            // Furthermore, it will continue to fail
                            // with any larger 'len' that we try.
                            // so just return false.
                            //
                            return false;
                        }
                    }
                }

                // shift oneBit left by one bit to test the next position
                oneBit <<= 1;
            }

            // We expect that bitCount will always be two at this point
            // but just in case return false for any bad cases.
            //
            assert(bitCount == 2);
            if (bitCount != 2)
                return false;

            // Perform some sanity checks on the values of 'S' and 'R'
            assert(S > 0);
            assert(S < elemWidth);
            assert(R < elemWidth);

            // Does the caller want us to return the N,R,S encoding values?
            //
            if (wbBMI != nullptr)
            {

                // The encoding used for S is one less than the
                //  number of consecutive one bits
                S--;

                if (len == 6)
                {
                    wbBMI->immN = 1;
                }
                else
                {
                    wbBMI->immN = 0;
                    // The encoding used for 'S' here is a bit peculiar.
                    //
                    // The upper bits need to be complemented, followed by a zero bit
                    // then the value of 'S-1'
                    //
                    unsigned upperBitsOfS = 64 - (1 << (len + 1));
                    S |= upperBitsOfS;
                }
                wbBMI->immR = R;
                wbBMI->immS = S;

                // Verify that what we are returning is correct.
                assert(imm == emitDecodeBitMaskImm(*wbBMI, size));
            }
            // Tell the caller that we can successfully encode this immediate
            // using a 'bitmask immediate'.
            //
            return true;
        }
    }
    return false;
#endif
}

/************************************************************************
 *
 *  Convert a 64-bit immediate into its 'bitmask immediate' representation imm(N,r,s)
 */

/*static*/ emitter::bitMaskImm emitter::emitEncodeBitMaskImm(INT64 imm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
    emitter::bitMaskImm result;
    result.immNRS = 0;

    return result;
#if 0
    bool canEncode = canEncodeBitMaskImm(imm, size, &result);
    assert(canEncode);

    return result;
#endif
}

/************************************************************************
 *
 *  Convert an imm(i16,hw) into a 32/64-bit immediate
 *  inputs 'hwImm' a halfwordImm struct
 *         'size' specifies the size of the result (64 or 32 bits)
 */

/*static*/ INT64 emitter::emitDecodeHalfwordImm(const emitter::halfwordImm hwImm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(isValidGeneralDatasize(size)); // Only EA_4BYTE or EA_8BYTE forms

    unsigned hw  = hwImm.immHW;
    INT64    val = (INT64)hwImm.immVal;

    assert((hw <= 1) || (size == EA_8BYTE));

    INT64 result = val << (16 * hw);
    return result;
#endif
}

/************************************************************************
 *
 *  returns true if 'imm' of 'size' bits (32/64) can be encoded
 *  using the MIPS64 'halfword immediate' form.
 *  When a non-null value is passed for 'wbHWI' then this method
 *  writes back the 'immHW' and 'immVal' values use to encode this immediate
 *
 */

/*static*/ bool emitter::canEncodeHalfwordImm(INT64 imm, emitAttr size, emitter::halfwordImm* wbHWI)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(isValidGeneralDatasize(size)); // Only EA_4BYTE or EA_8BYTE forms

    unsigned immWidth = (size == EA_8BYTE) ? 64 : 32;
    unsigned maxHW    = (size == EA_8BYTE) ? 4 : 2;

    // setup immMask to a (EA_4BYTE) 0x00000000_FFFFFFFF or (EA_8BYTE) 0xFFFFFFFF_FFFFFFFF
    const UINT64 immMask = ((UINT64)-1) >> (64 - immWidth);
    const INT64  mask16  = (INT64)0xFFFF;

    imm = normalizeImm64(imm, size);

    // Try each of the valid hw shift sizes
    for (unsigned hw = 0; (hw < maxHW); hw++)
    {
        INT64 curMask   = mask16 << (hw * 16); // Represents the mask of the bits in the current halfword
        INT64 checkBits = immMask & ~curMask;

        // Excluding the current halfword (using ~curMask)
        //  does the immediate have zero bits in every other bit that we care about?
        //  note we care about all 64-bits for EA_8BYTE
        //  and we care about the lowest 32 bits for EA_4BYTE
        //
        if ((imm & checkBits) == 0)
        {
            // Does the caller want us to return the imm(i16,hw) encoding values?
            //
            if (wbHWI != nullptr)
            {
                INT64 val     = ((imm & curMask) >> (hw * 16)) & mask16;
                wbHWI->immHW  = hw;
                wbHWI->immVal = val;

                // Verify that what we are returning is correct.
                assert(imm == emitDecodeHalfwordImm(*wbHWI, size));
            }
            // Tell the caller that we can successfully encode this immediate
            // using a 'halfword immediate'.
            //
            return true;
        }
    }
    return false;
#endif
}

/************************************************************************
 *
 *  Convert a 64-bit immediate into its 'halfword immediate' representation imm(i16,hw)
 */

/*static*/ emitter::halfwordImm emitter::emitEncodeHalfwordImm(INT64 imm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
    emitter::halfwordImm result;
    result.immHWVal = 0;

#if 0
    bool canEncode = canEncodeHalfwordImm(imm, size, &result);
    assert(canEncode);
#endif

    return result;
}

/************************************************************************
 *
 *  Convert an imm(i8,sh) into a 16/32-bit immediate
 *  inputs 'bsImm' a byteShiftedImm struct
 *         'size' specifies the size of the result (16 or 32 bits)
 */

/*static*/ INT32 emitter::emitDecodeByteShiftedImm(const emitter::byteShiftedImm bsImm, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    bool     onesShift = (bsImm.immOnes == 1);
    unsigned bySh      = bsImm.immBY;         // Num Bytes to shift 0,1,2,3
    INT32    val       = (INT32)bsImm.immVal; // 8-bit immediate
    INT32    result    = val;

    if (bySh > 0)
    {
        assert((size == EA_2BYTE) || (size == EA_4BYTE)); // Only EA_2BYTE or EA_4BYTE forms
        if (size == EA_2BYTE)
        {
            assert(bySh < 2);
        }
        else
        {
            assert(bySh < 4);
        }

        result <<= (8 * bySh);

        if (onesShift)
        {
            result |= ((1 << (8 * bySh)) - 1);
        }
    }
    return result;
#endif
}

/************************************************************************
 *
 *  returns true if 'imm' of 'size' bits (16/32) can be encoded
 *  using the MIPS64 'byteShifted immediate' form.
 *  When a non-null value is passed for 'wbBSI' then this method
 *  writes back the 'immBY' and 'immVal' values use to encode this immediate
 *
 */

/*static*/ bool emitter::canEncodeByteShiftedImm(INT64                    imm,
                                                 emitAttr                 size,
                                                 bool                     allow_MSL,
                                                 emitter::byteShiftedImm* wbBSI)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    bool     canEncode = false;
    bool     onesShift = false; // true if we use the shifting ones variant
    unsigned bySh      = 0;     // number of bytes to shift: 0, 1, 2, 3
    unsigned imm8      = 0;     // immediate to use in the encoding

    imm = normalizeImm64(imm, size);

    if (size == EA_1BYTE)
    {
        imm8 = (unsigned)imm;
        assert(imm8 < 0x100);
        canEncode = true;
    }
    else if (size == EA_8BYTE)
    {
        imm8 = (unsigned)imm;
        assert(imm8 < 0x100);
        canEncode = true;
    }
    else
    {
        assert((size == EA_2BYTE) || (size == EA_4BYTE)); // Only EA_2BYTE or EA_4BYTE forms

        unsigned immWidth = (size == EA_4BYTE) ? 32 : 16;
        unsigned maxBY    = (size == EA_4BYTE) ? 4 : 2;

        // setup immMask to a (EA_2BYTE) 0x0000FFFF or (EA_4BYTE) 0xFFFFFFFF
        const UINT32 immMask = ((UINT32)-1) >> (32 - immWidth);
        const INT32  mask8   = (INT32)0xFF;

        // Try each of the valid by shift sizes
        for (bySh = 0; (bySh < maxBY); bySh++)
        {
            INT32 curMask   = mask8 << (bySh * 8); // Represents the mask of the bits in the current byteShifted
            INT32 checkBits = immMask & ~curMask;
            INT32 immCheck  = (imm & checkBits);

            // Excluding the current byte (using ~curMask)
            //  does the immediate have zero bits in every other bit that we care about?
            //  or can be use the shifted one variant?
            //  note we care about all 32-bits for EA_4BYTE
            //  and we care about the lowest 16 bits for EA_2BYTE
            //
            if (immCheck == 0)
            {
                canEncode = true;
            }
            if (allow_MSL)
            {
                if ((bySh == 1) && (immCheck == 0xFF))
                {
                    canEncode = true;
                    onesShift = true;
                }
                else if ((bySh == 2) && (immCheck == 0xFFFF))
                {
                    canEncode = true;
                    onesShift = true;
                }
            }
            if (canEncode)
            {
                imm8 = (unsigned)(((imm & curMask) >> (bySh * 8)) & mask8);
                break;
            }
        }
    }

    if (canEncode)
    {
        // Does the caller want us to return the imm(i8,bySh) encoding values?
        //
        if (wbBSI != nullptr)
        {
            wbBSI->immOnes = onesShift;
            wbBSI->immBY   = bySh;
            wbBSI->immVal  = imm8;

            // Verify that what we are returning is correct.
            assert(imm == emitDecodeByteShiftedImm(*wbBSI, size));
        }
        // Tell the caller that we can successfully encode this immediate
        // using a 'byteShifted immediate'.
        //
        return true;
    }
    return false;
#endif
}

/************************************************************************
 *
 *  Convert a 32-bit immediate into its 'byteShifted immediate' representation imm(i8,by)
 */

/*static*/ emitter::byteShiftedImm emitter::emitEncodeByteShiftedImm(INT64 imm, emitAttr size, bool allow_MSL)
{
assert(!"unimplemented on MIPS yet");
    emitter::byteShiftedImm result;
    result.immBSVal = 0;

    return result;
#if 0
    bool canEncode = canEncodeByteShiftedImm(imm, size, allow_MSL, &result);
    assert(canEncode);

    return result;
#endif
}

/************************************************************************
 *
 *  Convert a 'float 8-bit immediate' into a double.
 *  inputs 'fpImm' a floatImm8 struct
 */

/*static*/ double emitter::emitDecodeFloatImm8(const emitter::floatImm8 fpImm)
{
assert(!"unimplemented on MIPS yet");
return 0.0;
#if 0
    unsigned sign  = fpImm.immSign;
    unsigned exp   = fpImm.immExp ^ 0x4;
    unsigned mant  = fpImm.immMant + 16;
    unsigned scale = 16 * 8;

    while (exp > 0)
    {
        scale /= 2;
        exp--;
    }

    double result = ((double)mant) / ((double)scale);
    if (sign == 1)
    {
        result = -result;
    }

    return result;
#endif
}

/************************************************************************
 *
 *  returns true if the 'immDbl' can be encoded using the 'float 8-bit immediate' form.
 *  also returns the encoding if wbFPI is non-null
 *
 */

/*static*/ bool emitter::canEncodeFloatImm8(double immDbl, emitter::floatImm8* wbFPI)
{
assert(!"unimplemented on MIPS yet");
    return false;
}

/************************************************************************
 *
 *  Convert a double into its 'float 8-bit immediate' representation
 */

/*static*/ emitter::floatImm8 emitter::emitEncodeFloatImm8(double immDbl)
{
assert(!"unimplemented on MIPS yet");
    emitter::floatImm8 result;
    result.immFPIVal = 0;

    return result;
#if 0
    bool canEncode = canEncodeFloatImm8(immDbl, &result);
    assert(canEncode);

    return result;
#endif
}

/*****************************************************************************
 *
 *  For the given 'ins' returns the reverse instruction
 *  if one exists, otherwise returns INS_INVALID
 */

/*static*/ instruction emitter::insReverse(instruction ins)
{
assert(!"unimplemented on MIPS yet");
return INS_invalid;
}

/*****************************************************************************
 *
 *  For the given 'datasize' and 'elemsize', make the proper arrangement option
 *  returns the insOpts that specifies the vector register arrangement
 *  if one does not exist returns INS_OPTS_NONE
 */
#if 0
/*static*/ insOpts emitter::optMakeArrangement(emitAttr datasize, emitAttr elemsize)
{
    insOpts result = INS_OPTS_NONE;

    if (datasize == EA_8BYTE)
    {
        switch (elemsize)
        {
            case EA_1BYTE:
                result = INS_OPTS_8B;
                break;
            case EA_2BYTE:
                result = INS_OPTS_4H;
                break;
            case EA_4BYTE:
                result = INS_OPTS_2S;
                break;
            case EA_8BYTE:
                result = INS_OPTS_1D;
                break;
            default:
                unreached();
                break;
        }
    }
    else if (datasize == EA_16BYTE)
    {
        switch (elemsize)
        {
            case EA_1BYTE:
                result = INS_OPTS_16B;
                break;
            case EA_2BYTE:
                result = INS_OPTS_8H;
                break;
            case EA_4BYTE:
                result = INS_OPTS_4S;
                break;
            case EA_8BYTE:
                result = INS_OPTS_2D;
                break;
            default:
                unreached();
                break;
        }
    }
    return result;
}
#endif

/*****************************************************************************
 *
 *  For the given 'datasize' and arrangement 'opts'
 *  returns true is the pair spcifies a valid arrangement
 */
/*static*/ bool emitter::isValidArrangement(emitAttr datasize, insOpts opt)
{
assert(!"unimplemented on MIPS yet");
        return false;
#if 0
    if (datasize == EA_8BYTE)
    {
        if ((opt == INS_OPTS_8B) || (opt == INS_OPTS_4H) || (opt == INS_OPTS_2S) || (opt == INS_OPTS_1D))
        {
            return true;
        }
    }
    else if (datasize == EA_16BYTE)
    {
        if ((opt == INS_OPTS_16B) || (opt == INS_OPTS_8H) || (opt == INS_OPTS_4S) || (opt == INS_OPTS_2D))
        {
            return true;
        }
    }
    return false;
#endif
}

//  For the given 'arrangement' returns the 'datasize' specified by the vector register arrangement
//  asserts and returns EA_UNKNOWN if an invalid 'arrangement' value is passed
//
/*static*/ emitAttr emitter::optGetDatasize(insOpts arrangement)
{
assert(!"unimplemented on MIPS yet");
        return EA_UNKNOWN;
}

//  For the given 'arrangement' returns the 'elemsize' specified by the vector register arrangement
//  asserts and returns EA_UNKNOWN if an invalid 'arrangement' value is passed
//
/*static*/ emitAttr emitter::optGetElemsize(insOpts arrangement)
{
assert(!"unimplemented on MIPS yet");
        return EA_UNKNOWN;
}

//  For the given 'arrangement' returns the 'widen-arrangement' specified by the vector register arrangement
//  asserts and returns INS_OPTS_NONE if an invalid 'arrangement' value is passed
//
/*static*/ insOpts emitter::optWidenElemsize(insOpts arrangement)
{
assert(!"unimplemented on MIPS yet");
    return INS_OPTS_NONE;
#if 0
    if ((arrangement == INS_OPTS_8B) || (arrangement == INS_OPTS_16B))
    {
        return INS_OPTS_8H;
    }
    else if ((arrangement == INS_OPTS_4H) || (arrangement == INS_OPTS_8H))
    {
        return INS_OPTS_4S;
    }
    else if ((arrangement == INS_OPTS_2S) || (arrangement == INS_OPTS_4S))
    {
        return INS_OPTS_2D;
    }
    else
    {
        assert(!" invalid 'arrangement' value");
        return INS_OPTS_NONE;
    }
#endif
}

//  For the given 'conversion' returns the 'dstsize' specified by the conversion option
/*static*/ emitAttr emitter::optGetDstsize(insOpts conversion)
{
assert(!"unimplemented on MIPS yet");
    return EA_UNKNOWN;
#if 0
    switch (conversion)
    {
        case INS_OPTS_S_TO_8BYTE:
        case INS_OPTS_D_TO_8BYTE:
        case INS_OPTS_4BYTE_TO_D:
        case INS_OPTS_8BYTE_TO_D:
        case INS_OPTS_S_TO_D:
        case INS_OPTS_H_TO_D:

            return EA_8BYTE;

        case INS_OPTS_S_TO_4BYTE:
        case INS_OPTS_D_TO_4BYTE:
        case INS_OPTS_4BYTE_TO_S:
        case INS_OPTS_8BYTE_TO_S:
        case INS_OPTS_D_TO_S:
        case INS_OPTS_H_TO_S:

            return EA_4BYTE;

        case INS_OPTS_S_TO_H:
        case INS_OPTS_D_TO_H:

            return EA_2BYTE;

        default:
            assert(!" invalid 'conversion' value");
            return EA_UNKNOWN;
    }
#endif
}

//  For the given 'conversion' returns the 'srcsize' specified by the conversion option
/*static*/ emitAttr emitter::optGetSrcsize(insOpts conversion)
{
assert(!"unimplemented on MIPS yet");
    return EA_UNKNOWN;
}

//    For the given 'size' and 'index' returns true if it specifies a valid index for a vector register of 'size'
/*static*/ bool emitter::isValidVectorIndex(emitAttr datasize, emitAttr elemsize, ssize_t index)
{
assert(!"unimplemented on MIPS yet");
    return false;
#if 0
    assert(isValidVectorDatasize(datasize));
    assert(isValidVectorElemsize(elemsize));

    bool result = false;
    if (index >= 0)
    {
        if (datasize == EA_8BYTE)
        {
            switch (elemsize)
            {
                case EA_1BYTE:
                    result = (index < 8);
                    break;
                case EA_2BYTE:
                    result = (index < 4);
                    break;
                case EA_4BYTE:
                    result = (index < 2);
                    break;
                case EA_8BYTE:
                    result = (index < 1);
                    break;
                default:
                    unreached();
                    break;
            }
        }
        else if (datasize == EA_16BYTE)
        {
            switch (elemsize)
            {
                case EA_1BYTE:
                    result = (index < 16);
                    break;
                case EA_2BYTE:
                    result = (index < 8);
                    break;
                case EA_4BYTE:
                    result = (index < 4);
                    break;
                case EA_8BYTE:
                    result = (index < 2);
                    break;
                default:
                    unreached();
                    break;
            }
        }
    }
    return result;
#endif
}

/*****************************************************************************
 *
 *  Add an instruction with no operands.
 */

void emitter::emitIns(instruction ins)
{
    //instrDesc* id  = emitNewInstrSmall(EA_8BYTE);
    instrDesc* id = emitNewInstr(EA_8BYTE);

    id->idIns(ins);
    id->idAddr()->iiaSetInstrEncode(emitInsCode(ins));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an Load/Store instruction: base+offset.
 */
void emitter::emitIns_S_R(instruction ins, emitAttr attr, regNumber reg1, int varx, int offs)
{
    assert(offs >= 0);
    ssize_t imm;

    emitAttr  size  = EA_SIZE(attr);//it's better confirm attr with ins.

#ifdef DEBUG
    switch (ins)
    {
        case INS_sb:
            assert(size == EA_1BYTE);
            break;
        case INS_sh:
            assert(size == EA_2BYTE);
            break;
        case INS_sw:
        case INS_swc1:
        //case INS_swl:
        //case INS_swr:
            assert(size == EA_4BYTE);
            break;
        case INS_sd:
        case INS_sdc1:
            assert(size == EA_8BYTE);
            break;

        default:
            NYI("emitIns_S_R"); // FP locals?
            return;

    } // end switch (ins)
#endif

    /* Figure out the variable's frame position */
    int  base;
    bool FPbased;

    base = emitComp->lvaFrameAddress(varx, &FPbased);
    imm = base + offs;

    regNumber reg2 = FPbased ? REG_FPBASE : REG_SPBASE;

    if ((-32768 <= imm) && (imm <= 32767))
    {
        emitIns_R_R_I(ins, attr, reg1, reg2, imm);
    }
    else
    {
        ssize_t imm2 = (imm>>16) & 0xffff;
        assert(isValidSimm16(imm >> 16));
        emitIns_R_I(INS_lui, EA_PTRSIZE, REG_AT, imm2);
        imm2 = imm & 0xffff;
        emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm2);
        emitIns_R_R_R(INS_daddu, EA_8BYTE, REG_AT, REG_AT, reg2);
        emitIns_R_R_I(ins, attr, reg1, REG_AT, 0);
    }

}

void emitter::emitIns_R_S(instruction ins, emitAttr attr, regNumber reg1, int varx, int offs)
{
    assert(offs >= 0);
    ssize_t imm;

    emitAttr  size  = EA_SIZE(attr);//it's better confirm attr with ins.

#ifdef DEBUG
    switch (ins)
    {
        case INS_sb:
        case INS_lb:
        case INS_lbu:
            assert(size == EA_1BYTE);
            break;

        case INS_sh:
        case INS_lh:
        case INS_lhu:
            assert(size == EA_2BYTE);
            break;

        case INS_sw:
        case INS_lw:
        case INS_lwu:
        case INS_lwc1:
            assert(size == EA_4BYTE);
            break;

        case INS_sd:
        case INS_ld:
        case INS_ldc1:
            assert(size == EA_8BYTE);
            //assert(isValidGeneralDatasize(size) || isValidVectorDatasize(size));
            break;

        case INS_lea:
            assert(size == EA_8BYTE);
            break;

        default:
            NYI("emitIns_R_S"); // FP locals?
            return;

    } // end switch (ins)
#endif

    /* Figure out the variable's frame position */
    int  base;
    bool FPbased;

    base = emitComp->lvaFrameAddress(varx, &FPbased);
    int disp = base + offs;
    imm = base + offs;

    regNumber reg2 = FPbased ? REG_FPBASE : REG_SPBASE;

    if ((-32768 <= imm) && (imm < 32768))
    {
        if (ins == INS_lea)
        {
            ins = INS_daddiu;
        }

        emitIns_R_R_I(ins, attr, reg1, reg2, imm);
    }
    else
    {
        ssize_t imm2 = (imm>>16) & 0xffff;
        assert(isValidSimm16(imm >> 16));
        emitIns_R_I(INS_lui, EA_PTRSIZE, REG_AT, imm2);
        imm2 = imm & 0xffff;
        emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm2);
        if (ins == INS_lea)
        {
            emitIns_R_R_R(INS_daddu, EA_8BYTE, reg1, REG_AT, reg2);
        }
        else
        {
            emitIns_R_R_R(INS_daddu, EA_8BYTE, REG_AT, REG_AT, reg2);
            emitIns_R_R_I(ins, attr, reg1, REG_AT, 0);
        }
    }
}

/*****************************************************************************
 *
 *  Add an instruction with a single immediate value.
 */

void emitter::emitIns_I(instruction ins, emitAttr attr, ssize_t imm)
{
#ifdef DEBUG
    switch (ins)
    {
        case INS_b:
        case INS_bal:
        case INS_sync:
        //case INS_j:
        //case INS_jal:
        //case INS_syscall:
        //case INS_wait:
            break;
        default:
            unreached();
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, nullptr, &imm));

    //dispIns(id);
    appendToCurIG(id);
}

void emitter::emitIns_I_I(instruction ins, emitAttr attr, ssize_t imm1, ssize_t imm2)
{
    ssize_t imms[] = {imm1, imm2};
#ifdef DEBUG
    switch (ins)
    {
        case INS_bc1t:
        case INS_bc1f:
        //case INS_:
        //case INS_:
            break;

        default:
            unreached();
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, nullptr, imms));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing a single register.
 */

void emitter::emitIns_R(instruction ins, emitAttr attr, regNumber reg)
{
#ifdef DEBUG
    switch (ins)
    {
        case INS_jr:
        case INS_jr_hb:
        case INS_mfhi:
        case INS_mthi:
        case INS_mflo:
        case INS_mtlo:
            assert(isGeneralRegister(reg));
            break;

        default:
            unreached();
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, &reg, nullptr));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing a register and a constant.
 */

void emitter::emitIns_R_I(instruction ins, emitAttr attr, regNumber reg, ssize_t imm, insOpts opt /* = INS_OPTS_NONE */)
{
#ifdef DEBUG
    switch (ins)
    {
        case INS_lui:
        case INS_bgez:
        case INS_bgezal:
        case INS_bgtz:
        case INS_blez:
        case INS_bltz:
        case INS_bltzal:
        case INS_synci:
            assert(isGeneralRegisterOrR0(reg));
            break;

        default:
            unreached();
            break;

    } // end switch (ins)
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, &reg, &imm));

    //dispIns(id);
    appendToCurIG(id);
}

#if 0
/*****************************************************************************
 *
 *  Add an instruction referencing a register and a floating point constant.
 */

void emitter::emitIns_R_F(
    instruction ins, emitAttr attr, regNumber reg, double immDbl, insOpts opt /* = INS_OPTS_NONE */)

{
assert(!"unimplemented on MIPS yet");
    emitAttr  size      = EA_SIZE(attr);
    emitAttr  elemsize  = EA_UNKNOWN;
    insFormat fmt       = IF_NONE;
    ssize_t   imm       = 0;
    bool      canEncode = false;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        floatImm8 fpi;

        case INS_fcmp:
        case INS_fcmpe:
            assert(insOptsNone(opt));
            assert(isValidVectorElemsizeFloat(size));
            assert(isVectorRegister(reg));
            if (immDbl == 0.0)
            {
                canEncode = true;
                fmt       = IF_DV_1C;
            }
            break;

        case INS_fmov:
            assert(isVectorRegister(reg));
            fpi.immFPIVal = 0;
            canEncode     = canEncodeFloatImm8(immDbl, &fpi);

            if (insOptsAnyArrangement(opt))
            {
                // Vector operation
                assert(isValidVectorDatasize(size));
                assert(isValidArrangement(size, opt));
                elemsize = optGetElemsize(opt);
                assert(isValidVectorElemsizeFloat(elemsize));
                assert(opt != INS_OPTS_1D); // Reserved encoding

                if (canEncode)
                {
                    imm = fpi.immFPIVal;
                    assert((imm >= 0) && (imm <= 0xff));
                    fmt = IF_DV_1B;
                }
            }
            else
            {
                // Scalar operation
                assert(insOptsNone(opt));
                assert(isValidVectorElemsizeFloat(size));

                if (canEncode)
                {
                    imm = fpi.immFPIVal;
                    assert((imm >= 0) && (imm <= 0xff));
                    fmt = IF_DV_1A;
                }
            }
            break;

        default:
            unreached();
            break;

    } // end switch (ins)

    assert(canEncode);
    assert(fmt != IF_NONE);

    instrDesc* id = emitNewInstrSC(attr, imm);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(opt);

    id->idReg1(reg);

    //dispIns(id);
    appendToCurIG(id);
}
#endif

/*****************************************************************************
 *
 *  Add an instruction referencing two registers
 */

void emitter::emitIns_R_R(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, insOpts opt /* = INS_OPTS_NONE */)
{
    regNumber regs[] = {reg1, reg2};

#ifdef DEBUG
    switch (ins)
    {
        case INS_mov:
        case INS_neg:
        case INS_dneg:
        case INS_not:
        case INS_jalr:
        case INS_jalr_hb:
        case INS_clo:
        case INS_clz:
        case INS_dclo:
        case INS_dclz:
        case INS_ddiv:
        case INS_ddivu:
        case INS_div:
        case INS_divu:
        case INS_dmult:
        case INS_dmultu:
        case INS_dsbh:
        case INS_dshd:
        case INS_madd:
        case INS_maddu:
        case INS_msub:
        case INS_msubu:
        case INS_mult:
        case INS_multu:
        case INS_rdhwr:
        case INS_seb:
        case INS_seh:
        case INS_wsbh:
        ////case INS_tne://if needed, please add others.
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_dmtc1:
        case INS_mtc1:
        case INS_dmfc1:
        case INS_mfc1:
            assert(isGeneralRegisterOrR0(reg1));
            assert(isFloatReg(reg2));
            break;
        case INS_ctc1:
        case INS_cfc1:
            assert(isGeneralRegisterOrR0(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_abs_d:
        case INS_abs_s:
        case INS_ceil_l_s:
        case INS_ceil_l_d:
        case INS_ceil_w_s:
        case INS_ceil_w_d:
        case INS_cvt_d_s:
        case INS_cvt_d_w:
        case INS_cvt_d_l:
        case INS_cvt_l_s:
        case INS_cvt_l_d:
        case INS_cvt_s_d:
        case INS_cvt_s_w:
        case INS_cvt_s_l:
        case INS_cvt_w_s:
        case INS_cvt_w_d:
        case INS_floor_l_s:
        case INS_floor_l_d:
        case INS_floor_w_s:
        case INS_floor_w_d:
        case INS_mov_s:
        case INS_mov_d:
        case INS_neg_s:
        case INS_neg_d:
        case INS_recip_s:
        case INS_recip_d:
        case INS_round_l_s:
        case INS_round_l_d:
        case INS_round_w_s:
        case INS_round_w_d:
        case INS_rsqrt_s:
        case INS_rsqrt_d:
        case INS_sqrt_s:
        case INS_sqrt_d:
        case INS_trunc_l_s:
        case INS_trunc_l_d:
        case INS_trunc_w_s:
        case INS_trunc_w_d:
            assert(isFloatReg(reg1));
            assert(isFloatReg(reg2));
            break;

        default:
            unreached();
            break;

    } // end switch (ins)
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, nullptr));

    //dispIns(id);
    appendToCurIG(id);
}

void emitter::emitIns_R_I_I(
    instruction ins, emitAttr attr, regNumber reg, ssize_t hint, ssize_t off, insOpts opt /* = INS_OPTS_NONE */)
{
#ifdef DEBUG
    switch (ins)
    {
        case INS_pref:
            assert(isGeneralRegister(reg));
            assert((-32769 < off) && (off < 32768));
            break;

        default:
            unreached();
    }
#endif

    ssize_t imms[] = {hint, off};
    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, &reg, imms));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing two registers and a constant.
 */

void emitter::emitIns_R_R_I(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, ssize_t imm, insOpts opt /* = INS_OPTS_NONE */)
{
    regNumber regs[] = {reg1, reg2};

#ifdef DEBUG
    switch (ins)
    {
        case INS_addi:
        case INS_addiu:
        case INS_daddiu:
        case INS_daddi:
        case INS_slti:
            assert((-32768 <= imm) && (imm <= 32767));
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_beq:
        case INS_bne:
            assert((-32768 <= imm) && (imm <= 32767));
            assert(isGeneralRegisterOrR0(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_sltiu:
        case INS_andi:
        case INS_ori:
        case INS_xori:
            assert((-32768 <= imm) && (imm <= 0xffff));
            break;

        case INS_sll:
        case INS_sra:
        case INS_srl:
        case INS_dsll:
        case INS_dsll32:
        case INS_dsra:
        case INS_dsra32:
        case INS_dsrl:
        case INS_dsrl32:
        case INS_rotr:
        case INS_drotr:
        case INS_drotr32:
        case INS_prefx:
            assert(isGeneralRegisterOrR0(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            assert((0 <= imm) && (imm <= 31));
            break;

        case INS_movf:
        case INS_movt:
            assert(isGeneralRegisterOrR0(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            assert((0 <= imm) && (imm <= 7));
            break;

        case INS_lb:
        case INS_lh:
        case INS_lbu:
        case INS_lhu:
        case INS_lwu:
        case INS_lw:
        case INS_ld:
        case INS_sb:
        case INS_sh:
        case INS_sw:
        case INS_sd:
        case INS_ll:
        case INS_sc:
        case INS_lld:
        case INS_scd:
        case INS_lwl:
        case INS_lwr:
        case INS_swl:
        case INS_swr:
        case INS_ldl:
        case INS_ldr:
        case INS_sdl:
        case INS_sdr:
            assert((-32768 <= imm) && (imm <= 32767));
            assert(isGeneralRegisterOrR0(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_ldc1:
        case INS_lwc1:
        case INS_sdc1:
        case INS_swc1:
            assert((-32768 <= imm) && (imm <= 32767));
            assert(isFloatReg(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            break;

        case INS_c_f_s:
        case INS_c_un_s:
        case INS_c_eq_s:
        case INS_c_ueq_s:
        case INS_c_olt_s:
        case INS_c_ult_s:
        case INS_c_ole_s:
        case INS_c_ule_s:
        case INS_c_sf_s:
        case INS_c_ngle_s:
        case INS_c_seq_s:
        case INS_c_ngl_s:
        case INS_c_lt_s:
        case INS_c_nge_s:
        case INS_c_le_s:
        case INS_c_ngt_s:
        case INS_c_f_d:
        case INS_c_un_d:
        case INS_c_eq_d:
        case INS_c_ueq_d:
        case INS_c_olt_d:
        case INS_c_ult_d:
        case INS_c_ole_d:
        case INS_c_ule_d:
        case INS_c_sf_d:
        case INS_c_ngle_d:
        case INS_c_seq_d:
        case INS_c_ngl_d:
        case INS_c_lt_d:
        case INS_c_nge_d:
        case INS_c_le_d:
        case INS_c_ngt_d:
        case INS_movf_s:
        case INS_movf_d:
        case INS_movt_s:
        case INS_movt_d:
            assert(isFloatReg(reg1));
            assert(isFloatReg(reg2));
            assert((0 <= imm) && (imm <= 7));
            break;

        default:
            unreached();
            break;

    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, &imm));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
*
*  Add an instruction referencing two registers and a constant.
*  Also checks for a large immediate that needs a second instruction
*  and will load it in reg1
*
*  - Supports instructions: add, adds, sub, subs, and, ands, eor and orr
*  - Requires that reg1 is a general register and not SP or ZR
*  - Requires that reg1 != reg2
*/
void emitter::emitIns_R_R_Imm(instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, ssize_t imm)
{
    assert(isGeneralRegister(reg1));
    assert(reg1 != reg2);

    bool immFits = true;

#ifdef DEBUG
    switch (ins)
    {
        case INS_addiu:
        case INS_daddiu:
        //case INS_lui:
        //case INS_lbu:
        //case INS_lhu:
        //case INS_lwu:
        //case INS_lb:
        //case INS_lh:
        //case INS_lw:
        //case INS_ld:
        //case INS_sb:
        //case INS_sh:
        //case INS_sw:
        //case INS_sd:
        ////case INS_lwc1:
        ////case INS_ldc1:
            immFits = emitter::emitIns_valid_imm_for_add(imm, attr);
            break;

        case INS_andi:
        case INS_ori:
        case INS_xori:
            // check  0<= imm <= 0xffff
            immFits = emitter::emitIns_valid_imm_for_alu(imm, attr);
            break;

        default:
            assert(!"Unsupported instruction in emitIns_R_R_Imm");
    }
#endif

    if (immFits)
    {
        emitIns_R_R_I(ins, attr, reg1, reg2, imm);
    }
    else
    {
        // Load 'imm' into the reg1 register
        // then issue:   'ins'  reg1, reg2, reg1
        //
        codeGen->instGen_Set_Reg_To_Imm(attr, reg1, imm);
        emitIns_R_R_R(ins, attr, reg1, reg2, reg1);
    }
}

/*****************************************************************************
 *
 *  Add an instruction referencing three registers.
 */

void emitter::emitIns_R_R_R(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, regNumber reg3, insOpts opt) /* = INS_OPTS_NONE */
{
    regNumber regs[] = {reg1, reg2, reg3};

#ifdef DEBUG
    switch (ins)
    {
        case INS_addu:
        case INS_and:
        case INS_daddu:
        case INS_drotrv:
        case INS_dsllv:
        case INS_dsrav:
        case INS_dsrlv:
        case INS_rotrv:
        case INS_sllv:
        case INS_srav:
        case INS_srlv:
        case INS_dsub:
        case INS_dsubu:
        case INS_sub:
        case INS_subu:
        case INS_movn:
        case INS_movz:
        case INS_mul:
        case INS_slt:
        case INS_sltu:
        case INS_or:
        case INS_nor:
        case INS_xor:
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            assert(isGeneralRegisterOrR0(reg3));
            break;

        case INS_movn_s:
        case INS_movn_d:
        case INS_movz_s:
        case INS_movz_d:
            assert(isFloatReg(reg1));
            assert(isFloatReg(reg2));
            assert(isGeneralRegisterOrR0(reg3));
            break;

        case INS_add_s:
        case INS_add_d:
        case INS_div_s:
        case INS_div_d:
        case INS_mul_s:
        case INS_mul_d:
        case INS_sub_s:
        case INS_sub_d:
            assert(isFloatReg(reg1));
            assert(isFloatReg(reg2));
            break;

        case INS_luxc1:
        case INS_lwxc1:
        case INS_ldxc1:
        case INS_suxc1:
        case INS_swxc1:
        case INS_sdxc1:
            assert(isFloatReg(reg1));
            assert(isGeneralRegisterOrR0(reg2));
            assert(isGeneralRegisterOrR0(reg3));
            break;

        default:
            assert(!"Unsupported instruction in emitIns_R_R_Imm");
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idReg3(reg3);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, nullptr));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing three registers and a constant.
 */

void emitter::emitIns_R_R_R_I(instruction ins,
                              emitAttr    attr,
                              regNumber   reg1,
                              regNumber   reg2,
                              regNumber   reg3,
                              ssize_t     imm,
                              insOpts     opt /* = INS_OPTS_NONE */,
                              emitAttr    attrReg2 /* = EA_UNKNOWN */)
{
    assert(!"unimplemented on MIPS yet");

    /* FIXME for MIPS: not used on mips. */
    regNumber regs[] = {reg1, reg2, reg3};

#ifdef DEBUG
    switch (ins)
    {
        case INS_addu:
            break;

        default:
            assert(!"Unsupported instruction in emitIns_R_R_Imm");
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idReg3(reg3);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, &imm));

    //dispIns(id);
    appendToCurIG(id);
}

#if 1
/*****************************************************************************
 *
 *  Add an instruction referencing three registers, with an extend option
 */

void emitter::emitIns_R_R_R_Ext(instruction ins,
                                emitAttr    attr,
                                regNumber   reg1,
                                regNumber   reg2,
                                regNumber   reg3,
                                insOpts     opt,         /* = INS_OPTS_NONE */
                                int         shiftAmount) /* = -1 -- unset   */
{
assert(!"unimplemented on MIPS yet");
}

/*****************************************************************************
 *
 *  Add an instruction referencing two registers and two constants.
 */

void emitter::emitIns_R_R_I_I(instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, int imm1, int imm2)
{
    regNumber regs[] = {reg1, reg2};
    ssize_t imms[] = {imm1, imm2};

#ifdef DEBUG
    switch (ins)
    {
        case INS_ext:
        case INS_dext:
        case INS_dextm:
        case INS_dextu:
        case INS_ins:
        case INS_dins:
        case INS_dinsm:
        case INS_dinsu:
            break;

        default:
            unreached();
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, imms));

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing four registers.
 */

void emitter::emitIns_R_R_R_R(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, regNumber reg3, regNumber reg4)
{
    regNumber regs[] = {reg1, reg2, reg3, reg4};

#ifdef DEBUG
    switch (ins)
    {
        case INS_madd_s:
        case INS_madd_d:
        case INS_msub_s:
        case INS_msub_d:
        case INS_nmadd_s:
        case INS_nmadd_d:
        case INS_nmsub_s:
        case INS_nmsub_d:
            assert(isFloatReg(reg1));
            assert(isFloatReg(reg2));
            assert(isFloatReg(reg3));
            assert(isFloatReg(reg4));
            break;

        default:
            unreached();
    }
#endif

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idReg3(reg3);
    id->idReg4(reg4);
    id->idAddr()->iiaSetInstrEncode(emitInsOps(ins, regs, nullptr));

    //dispIns(id);
    appendToCurIG(id);
}

#if 0
/*****************************************************************************
 *
 *  Add an instruction referencing a register and a condition code
 */

void emitter::emitIns_R_COND(instruction ins, emitAttr attr, regNumber reg, insCond cond)
{
assert(!"unimplemented on MIPS yet");
    insFormat    fmt = IF_NONE;
    condFlagsImm cfi;
    cfi.immCFVal = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_cset:
        case INS_csetm:
            assert(isGeneralRegister(reg));
            cfi.cond = cond;
            fmt      = IF_DR_1D;
            break;

        default:
            unreached();
            break;

    } // end switch (ins)

    assert(fmt != IF_NONE);
    assert(isValidImmCond(cfi.immCFVal));

    instrDesc* id = emitNewInstrSC(attr, cfi.immCFVal);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg);

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing two registers and a condition code
 */

void emitter::emitIns_R_R_COND(instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, insCond cond)
{
assert(!"unimplemented on MIPS yet");
    insFormat    fmt = IF_NONE;
    condFlagsImm cfi;
    cfi.immCFVal = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_cinc:
        case INS_cinv:
        case INS_cneg:
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegister(reg2));
            cfi.cond = cond;
            fmt      = IF_DR_2D;
            break;
        default:
            unreached();
            break;

    } // end switch (ins)

    assert(fmt != IF_NONE);
    assert(isValidImmCond(cfi.immCFVal));

    instrDesc* id = emitNewInstrSC(attr, cfi.immCFVal);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg1);
    id->idReg2(reg2);

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing two registers and a condition code
 */

void emitter::emitIns_R_R_R_COND(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, regNumber reg3, insCond cond)
{
assert(!"unimplemented on MIPS yet");
    insFormat    fmt = IF_NONE;
    condFlagsImm cfi;
    cfi.immCFVal = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_csel:
        case INS_csinc:
        case INS_csinv:
        case INS_csneg:
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegister(reg2));
            assert(isGeneralRegister(reg3));
            cfi.cond = cond;
            fmt      = IF_DR_3D;
            break;

        default:
            unreached();
            break;

    } // end switch (ins)

    assert(fmt != IF_NONE);
    assert(isValidImmCond(cfi.immCFVal));

    instrDesc* id = emitNewInstr(attr);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg1);
    id->idReg2(reg2);
    id->idReg3(reg3);
    id->idSmallCns(cfi.immCFVal);

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing two registers the flags and a condition code
 */

void emitter::emitIns_R_R_FLAGS_COND(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, insCflags flags, insCond cond)
{
assert(!"unimplemented on MIPS yet");
    insFormat    fmt = IF_NONE;
    condFlagsImm cfi;
    cfi.immCFVal = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_ccmp:
        case INS_ccmn:
            assert(isGeneralRegister(reg1));
            assert(isGeneralRegister(reg2));
            cfi.flags = flags;
            cfi.cond  = cond;
            fmt       = IF_DR_2I;
            break;
        default:
            unreached();
            break;
    } // end switch (ins)

    assert(fmt != IF_NONE);
    assert(isValidImmCondFlags(cfi.immCFVal));

    instrDesc* id = emitNewInstrSC(attr, cfi.immCFVal);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg1);
    id->idReg2(reg2);

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction referencing a register, an immediate, the flags and a condition code
 */

void emitter::emitIns_R_I_FLAGS_COND(
    instruction ins, emitAttr attr, regNumber reg, int imm, insCflags flags, insCond cond)
{
assert(!"unimplemented on MIPS yet");
    insFormat    fmt = IF_NONE;
    condFlagsImm cfi;
    cfi.immCFVal = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_ccmp:
        case INS_ccmn:
            assert(isGeneralRegister(reg));
            if (imm < 0)
            {
                ins = insReverse(ins);
                imm = -imm;
            }
            if ((imm >= 0) && (imm <= 31))
            {
                cfi.imm5  = imm;
                cfi.flags = flags;
                cfi.cond  = cond;
                fmt       = IF_DI_1F;
            }
            else
            {
                assert(!"Instruction cannot be encoded: ccmp/ccmn imm5");
            }
            break;
        default:
            unreached();
            break;
    } // end switch (ins)

    assert(fmt != IF_NONE);
    assert(isValidImmCondFlagsImm5(cfi.immCFVal));

    instrDesc* id = emitNewInstrSC(attr, cfi.immCFVal);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg);

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add a memory barrier instruction with a 'barrier' immediate
 */

void emitter::emitIns_BARR(instruction ins, insBarrier barrier)
{
assert(!"unimplemented on MIPS yet");
    insFormat fmt = IF_NONE;
    ssize_t   imm = 0;

    /* Figure out the encoding format of the instruction */
    switch (ins)
    {
        case INS_dsb:
        case INS_dmb:
        case INS_isb:

            fmt = IF_SI_0B;
            imm = (ssize_t)barrier;
            break;
        default:
            unreached();
            break;
    } // end switch (ins)

    assert(fmt != IF_NONE);

    instrDesc* id = emitNewInstrSC(EA_8BYTE, imm);

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    //dispIns(id);
    appendToCurIG(id);
}
#endif

/*****************************************************************************
 *
 *  Add an instruction with a static data member operand. If 'size' is 0, the
 *  instruction operates on the address of the static member instead of its
 *  value (e.g. "push offset clsvar", rather than "push dword ptr [clsvar]").
 */

void emitter::emitIns_C(instruction ins, emitAttr attr, CORINFO_FIELD_HANDLE fldHnd, int offs)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_C");
#endif
}

/*****************************************************************************
 *
 *  Add an instruction referencing stack-based local variable.
 */

void emitter::emitIns_S(instruction ins, emitAttr attr, int varx, int offs)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_S");
#endif
}

#if 0
/*****************************************************************************
 *
 *  Add an instruction referencing a register and a stack-based local variable.
 */

void emitter::emitIns_R_R_S(
    instruction ins, emitAttr attr, regNumber reg1, regNumber reg2, int sa)
{
    assert(!"unimplemented on MIPS yet");
#if 1
    regNumber regs[] = {reg1, reg2};
    ssize_t imm = (ssize_t)sa;
    emitAllocInstrOnly(emitInsOps(ins, regs, &imm), attr);
#else
    instrDesc* id = emitNewInstrCns(attr, sa);
    insFormat fmt = IF_FMT_FUNC;

    id->idIns(ins);
    id->idInsFmt(fmt);
    id->idInsOpt(INS_OPTS_NONE);

    id->idReg1(reg1);
    id->idReg2(reg2);

    //dispIns(id);
    appendToCurIG(id);
#endif
}
#endif

/*****************************************************************************
 *
 *  Add an instruction referencing two register and consectutive stack-based local variable slots.
 */
void emitter::emitIns_R_R_S_S(
    instruction ins, emitAttr attr1, emitAttr attr2, regNumber reg1, regNumber reg2, int varx, int offs)
{
assert(!"unimplemented on MIPS yet");
}

/*****************************************************************************
 *
 *  Add an instruction referencing consecutive stack-based local variable slots and two registers
 */
void emitter::emitIns_S_S_R_R(
    instruction ins, emitAttr attr1, emitAttr attr2, regNumber reg1, regNumber reg2, int varx, int offs)
{
assert(!"unimplemented on MIPS yet");
}

/*****************************************************************************
 *
 *  Add an instruction referencing stack-based local variable and an immediate
 */
void emitter::emitIns_S_I(instruction ins, emitAttr attr, int varx, int offs, int val)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_S_I");
#endif
}

/*****************************************************************************
 *
 *  Add an instruction with a register + static member operands.
 *  Constant is stored into JIT data which is adjacent to code.
 *  For MIPS64, maybe not the best, here just suports the func-interface.
 *
 */
void emitter::emitIns_R_C(
    instruction ins, emitAttr attr, regNumber reg, regNumber addrReg, CORINFO_FIELD_HANDLE fldHnd, int offs)
{
    assert(offs >= 0);
    assert(instrDesc::fitsInSmallCns(offs));
    assert(ins == INS_bal);//for special.
    assert(isGeneralRegister(reg));

    // INS_OPTS_RC: placeholders.  4-ins:
    //   bal 4
    //   lui at, off-hi-16bits
    //   ori at, at, off-lo-16bits
    //   daddu  reg, at, ra

    instrDescJmp* id   = emitNewInstrJmp();

    id->idIns(ins);
    assert(reg != REG_R0); //for special. reg Must not be R0.
    id->idReg1(reg); // destination register that will get the constant value.

    id->idSmallCns(offs); //usually is 0.
    id->idInsOpt(INS_OPTS_RC);

    if (EA_IS_GCREF(attr))
    {
        /* A special value indicates a GCref pointer value */
        id->idGCref(GCT_GCREF);
        id->idOpSize(EA_PTRSIZE);
    }
    else if (EA_IS_BYREF(attr))
    {
        /* A special value indicates a Byref pointer value */
        id->idGCref(GCT_BYREF);
        id->idOpSize(EA_PTRSIZE);
    }

    id->idSetIsBound(); // We won't patch address since we will know the exact distance
                        // once JIT code and data are allocated together.

    assert(addrReg == REG_NA);//NOTE: for MIPS64, not support addrReg != REG_NA.

    id->idAddr()->iiaFieldHnd = fldHnd;

    // Keep it long if it's in cold code.
    id->idjKeepLong = emitComp->fgIsBlockCold(emitComp->compCurBB);

#ifdef DEBUG
    if (emitComp->opts.compLongAddress)
        id->idjKeepLong = 1;
#endif // DEBUG

    /* Record the jump's IG and offset within it */
    id->idjIG   = emitCurIG;
    id->idjOffs = emitCurIGsize;

    /* Append this jump to this IG's jump list */
    id->idjNext = emitCurIGjmpList;
    emitCurIGjmpList = id;

#if EMITTER_STATS
    emitTotalIGjmps++;
#endif

    //dispIns(id);//mips dumping instr by other-fun.
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add an instruction with a static member + constant.
 */

void emitter::emitIns_C_I(instruction ins, emitAttr attr, CORINFO_FIELD_HANDLE fldHnd, ssize_t offs, ssize_t val)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_C_I");
#endif
}

/*****************************************************************************
 *
 *  Add an instruction with a static member + register operands.
 */

void emitter::emitIns_C_R(instruction ins, emitAttr attr, CORINFO_FIELD_HANDLE fldHnd, regNumber reg, int offs)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert(!"emitIns_C_R not supported for RyuJIT backend");
#endif
}

void emitter::emitIns_R_AR(instruction ins, emitAttr attr, regNumber ireg, regNumber reg, int offs)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_R_AR");
#endif
}

// This computes address from the immediate which is relocatable.
void emitter::emitIns_R_AI(instruction ins, emitAttr attr, regNumber reg, ssize_t addr)
{

    assert(EA_IS_RELOC(attr));//EA_PTR_DSP_RELOC
    assert(ins == INS_bal);//for special.
    assert(isGeneralRegister(reg));

    // INS_OPTS_RELOC: placeholders.  4-ins:
    //   bal 4
    //   lui at, off-hi-16bits
    //   ori at, at, off-lo-16bits
    //   daddu  reg, at, ra

    instrDescJmp* id = emitNewInstrJmp();

    id->idIns(ins);
    assert(reg != REG_R0); //for special. reg Must not be R0.
    id->idReg1(reg); // destination register that will get the constant value.

    id->idInsOpt(INS_OPTS_RELOC);

    if (EA_IS_GCREF(attr))
    {
        /* A special value indicates a GCref pointer value */
        id->idGCref(GCT_GCREF);
        id->idOpSize(EA_PTRSIZE);
    }
    else if (EA_IS_BYREF(attr))
    {
        /* A special value indicates a Byref pointer value */
        id->idGCref(GCT_BYREF);
        id->idOpSize(EA_PTRSIZE);
    }

    id->idSetIsBound(); // We won't patch address since we will know the exact distance
                        // once JIT code and data are allocated together.

    id->idAddr()->iiaAddr = (BYTE*)addr;

    //id->idjKeepLong = true;

    /* Record the jump's IG and offset within it */
    id->idjIG   = emitCurIG;
    id->idjOffs = emitCurIGsize;

    /* Append this jump to this IG's jump list */
    id->idjNext = emitCurIGjmpList;
    emitCurIGjmpList = id;

#if EMITTER_STATS
    emitTotalIGjmps++;
#endif

    //dispIns(id);//mips dumping instr by other-fun.
    appendToCurIG(id);
}

void emitter::emitIns_AR_R(instruction ins, emitAttr attr, regNumber ireg, regNumber reg, int offs)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_AR_R");
#endif
}

void emitter::emitIns_R_ARR(instruction ins, emitAttr attr, regNumber ireg, regNumber reg, regNumber rg2, int disp)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_R_ARR");
#endif
}

void emitter::emitIns_ARR_R(instruction ins, emitAttr attr, regNumber ireg, regNumber reg, regNumber rg2, int disp)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_R_ARR");
#endif
}

void emitter::emitIns_R_ARX(
    instruction ins, emitAttr attr, regNumber ireg, regNumber reg, regNumber rg2, unsigned mul, int disp)
{
assert(!"unimplemented on MIPS yet");
#if 0
    NYI("emitIns_R_ARR");
#endif
}

/*****************************************************************************
 *
 *  Add a data label instruction.
 */
void emitter::emitIns_R_D(instruction ins, emitAttr attr, unsigned offs, regNumber reg)
{
    NYI("emitIns_R_D");
}

void emitter::emitIns_J_R_I(instruction ins, emitAttr attr, BasicBlock* dst, regNumber reg, int imm)
{
    assert(!"unimplemented on MIPS yet");
}
#endif

/*****************************************************************************
 *
 *  Record that a jump instruction uses the short encoding
 *
 */
void emitter::emitSetShortJump(instrDescJmp* id)
{
/* FIXME for MIPS: maybe delete it on future. */
    if (id->idjKeepLong)
        return;

    //insFormat fmt = IF_NONE;
    //switch (id->idIns())
    //{
    //    case INS_beq:
    //    case INS_bne:
    //        fmt = IF_OPCODE;
    //        break;

    //    case INS_bltz:
    //        fmt = IF_OPCODES_16;
    //        break;

    //    default:
    //        unreached();
    //        break;
    //}

    //id->idInsFmt(fmt);
    id->idjShort = true;
}

/*****************************************************************************
 *
 *  Add a label instruction.
 */

void emitter::emitIns_R_L(instruction ins, emitAttr attr, BasicBlock* dst, regNumber reg)
{
    assert(dst->bbFlags & BBF_JMP_TARGET);

    // INS_OPTS_RL: placeholders.  4-ins:
    //   bal 4
    //   lui at, dst-hi-16bits
    //   ori at, at, dst-lo-16bits
    //   dsubu/daddu  reg, ra, at

    //instrDesc* id = emitNewInstr(attr);
    instrDescJmp* id = emitNewInstrJmp();

    id->idIns(ins);
    id->idInsOpt(INS_OPTS_RL);
    id->idjShort = false;//mips will ignore this.
    id->idAddr()->iiaBBlabel = dst;

    id->idReg1(reg);

    if (EA_IS_GCREF(attr))
    {
        /* A special value indicates a GCref pointer value */
        id->idGCref(GCT_GCREF);
        id->idOpSize(EA_PTRSIZE);
    }
    else if (EA_IS_BYREF(attr))
    {
        /* A special value indicates a Byref pointer value */
        id->idGCref(GCT_BYREF);
        id->idOpSize(EA_PTRSIZE);
    }

#ifdef DEBUG
    // Mark the catch return
    if (emitComp->compCurBB->bbJumpKind == BBJ_EHCATCHRET)
    {
        id->idDebugOnlyInfo()->idCatchRet = true;
    }
#endif // DEBUG

    id->idjKeepLong = emitComp->fgInDifferentRegions(emitComp->compCurBB, dst);

#ifdef DEBUG
    if (emitComp->opts.compLongAddress)
        id->idjKeepLong = 1;
#endif // DEBUG

    /* Record the jump's IG and offset within it */

    id->idjIG   = emitCurIG;
    id->idjOffs = emitCurIGsize;

    /* Append this jump to this IG's jump list */

    id->idjNext      = emitCurIGjmpList;
    emitCurIGjmpList = id;

#if EMITTER_STATS
    emitTotalIGjmps++;
#endif

    //dispIns(id);
    appendToCurIG(id);

}

void emitter::emitIns_J_R(instruction ins, emitAttr attr, BasicBlock* dst, regNumber reg)
{
    assert(dst != nullptr);
    assert((dst->bbFlags & BBF_JMP_TARGET) != 0);
    assert(ins == INS_b);//  || ins == INS_jr);

    // INS_OPTS_JR: placeholders.  6-ins:
    //   bal 4
    //   lui at, dst-hi-16bits
    //   ori at, at, dst-lo-16bits
    //   daddu  reg, at, ra
    //   jr  reg
    //   nop

    instrDescJmp* id = emitNewInstrJmp();

    id->idIns(ins);
    id->idInsOpt(INS_OPTS_JR);
    id->idReg1(reg);
    id->idjShort = false;
    id->idOpSize(EA_SIZE(attr));

    id->idAddr()->iiaBBlabel = dst;
    id->idjKeepLong = emitComp->fgInDifferentRegions(emitComp->compCurBB, dst);

    /* Record the jump's IG and offset within it */
    id->idjIG   = emitCurIG;
    id->idjOffs = emitCurIGsize;

    /* Append this jump to this IG's jump list */
    id->idjNext = emitCurIGjmpList;
    emitCurIGjmpList = id;

#if EMITTER_STATS
    emitTotalIGjmps++;
#endif

    //dispIns(id);
    appendToCurIG(id);
}

void emitter::emitIns_J(instruction ins, BasicBlock* dst, int instrCount)
{
    if (dst == nullptr)
    {
        assert(instrCount != 0);
        assert(ins == INS_b);//when dst==nullptr, ins is INS_b by now.

        instrCount = (instrCount - 1) << 2;
        if ((-32768 <= instrCount) && (instrCount<32768))
        {
            /* This jump is really short */
            emitIns_I(ins, EA_PTRSIZE, instrCount);
        }
        else
        {
            //NOTE: should not be here !!!
            assert(!"should not be here on MIPS64 !!!");

            emitIns_I(INS_bal, EA_PTRSIZE, 4);

            ssize_t imm = ((ssize_t)instrCount>>16);
            assert(isValidSimm16(imm));
            emitIns_R_I(INS_lui, EA_PTRSIZE, REG_AT, imm);
            imm = (instrCount & 0xffff);
            emitIns_R_R_I(INS_ori, EA_PTRSIZE, REG_AT, REG_AT, imm);

            emitIns_R_R_R(INS_daddu, EA_8BYTE, REG_AT, REG_AT, REG_RA);
            emitIns_R(INS_jr, EA_PTRSIZE, REG_AT);
            emitIns(INS_nop);
        }

        return ;
    }

    // (dst != nullptr)
    //
    // INS_OPTS_J: placeholders.  6-ins:
    //   bal 4
    //   lui at, dst-hi-16bits
    //   ori at, at, dst-lo-16bits
    //   daddu  t9, at, ra
    //   jalr/jr  t9
    //   nop           <-----here add delay-slot!

    assert(dst->bbFlags & BBF_JMP_TARGET);

    instrDescJmp* id = emitNewInstrJmp();

    assert(ins == INS_b || ins == INS_bal);
    id->idIns(ins);
    id->idReg1(REG_T9);
    id->idjShort = false;

    id->idInsOpt(INS_OPTS_J);
    id->idAddr()->iiaBBlabel = dst;

    id->idjKeepLong = emitComp->fgInDifferentRegions(emitComp->compCurBB, dst);
#ifdef DEBUG
    if (emitComp->opts.compLongAddress) // Force long branches
        id->idjKeepLong = 1;
#endif // DEBUG

    /* Record the jump's IG and offset within it */
    id->idjIG   = emitCurIG;
    id->idjOffs = emitCurIGsize;

    /* Append this jump to this IG's jump list */
    id->idjNext = emitCurIGjmpList;
    emitCurIGjmpList = id;

#if EMITTER_STATS
    emitTotalIGjmps++;
#endif

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Add a call instruction (direct or indirect).
 *      argSize<0 means that the caller will pop the arguments
 *
 * The other arguments are interpreted depending on callType as shown:
 * Unless otherwise specified, ireg,xreg,xmul,disp should have default values.
 *
 * EC_FUNC_TOKEN       : addr is the method address
 * EC_FUNC_ADDR        : addr is the absolute address of the function
 *
 * If callType is one of these emitCallTypes, addr has to be NULL.
 * EC_INDIR_R          : "call ireg".
 *
 * For MIPS xreg, xmul and disp are never used and should always be 0/REG_NA.
 *
 *  Please consult the "debugger team notification" comment in genFnProlog().
 */

void emitter::emitIns_Call(EmitCallType          callType,
                           CORINFO_METHOD_HANDLE methHnd,
                           INDEBUG_LDISASM_COMMA(CORINFO_SIG_INFO* sigInfo) // used to report call sites to the EE
                           void*            addr,
                           ssize_t          argSize,
                           emitAttr         retSize
                           MULTIREG_HAS_SECOND_GC_RET_ONLY_ARG(emitAttr secondRetSize),
                           VARSET_VALARG_TP ptrVars,
                           regMaskTP        gcrefRegs,
                           regMaskTP        byrefRegs,
                           IL_OFFSETX       ilOffset /* = BAD_IL_OFFSET */,
                           regNumber        ireg /* = REG_NA */,
                           regNumber        xreg /* = REG_NA */,
                           unsigned         xmul /* = 0     */,
                           ssize_t          disp /* = 0     */,
                           bool             isJump /* = false */)
{
    /* Sanity check the arguments depending on callType */

    assert(callType < EC_COUNT);
    assert((callType != EC_FUNC_TOKEN && callType != EC_FUNC_ADDR) ||
           (ireg == REG_NA && xreg == REG_NA && xmul == 0 && disp == 0));
    assert(callType < EC_INDIR_R || addr == NULL);
    assert(callType != EC_INDIR_R || (ireg < REG_COUNT && xreg == REG_NA && xmul == 0 && disp == 0));

    // ARM never uses these
    assert(xreg == REG_NA && xmul == 0 && disp == 0);

    // Our stack level should be always greater than the bytes of arguments we push. Just
    // a sanity test.
    assert((unsigned)abs(argSize) <= codeGen->genStackLevel);

    // Trim out any callee-trashed registers from the live set.
    regMaskTP savedSet = emitGetGCRegsSavedOrModified(methHnd);
    gcrefRegs &= savedSet;
    byrefRegs &= savedSet;

#ifdef DEBUG
    if (EMIT_GC_VERBOSE)
    {
        printf("Call: GCvars=%s ", VarSetOps::ToString(emitComp, ptrVars));
        dumpConvertedVarSet(emitComp, ptrVars);
        printf(", gcrefRegs=");
        printRegMaskInt(gcrefRegs);
        emitDispRegSet(gcrefRegs);
        printf(", byrefRegs=");
        printRegMaskInt(byrefRegs);
        emitDispRegSet(byrefRegs);
        printf("\n");
    }
#endif

    /* Managed RetVal: emit sequence point for the call */
    if (emitComp->opts.compDbgInfo && ilOffset != BAD_IL_OFFSET)
    {
        codeGen->genIPmappingAdd(ilOffset, false);
    }

    /*
        We need to allocate the appropriate instruction descriptor based
        on whether this is a direct/indirect call, and whether we need to
        record an updated set of live GC variables.
     */
    instrDesc* id;

    assert(argSize % REGSIZE_BYTES == 0);
    int argCnt = (int)(argSize / (int)REGSIZE_BYTES);

    if (callType >= EC_INDIR_R)
    {
        /* Indirect call, virtual calls */

        assert(callType == EC_INDIR_R);

        id = emitNewInstrCallInd(argCnt, disp, ptrVars, gcrefRegs, byrefRegs, retSize, secondRetSize);
    }
    else
    {
        /* Helper/static/nonvirtual/function calls (direct or through handle),
           and calls to an absolute addr. */

        assert(callType == EC_FUNC_TOKEN || callType == EC_FUNC_ADDR);

        id = emitNewInstrCallDir(argCnt, ptrVars, gcrefRegs, byrefRegs, retSize, secondRetSize);
    }

    /* Update the emitter's live GC ref sets */

    VarSetOps::Assign(emitComp, emitThisGCrefVars, ptrVars);
    emitThisGCrefRegs = gcrefRegs;
    emitThisByrefRegs = byrefRegs;

    id->idSetIsNoGC(emitNoGChelper(methHnd));

    /* Set the instruction - special case jumping a function */
    instruction ins;

    if (isJump)
    {
        ins = INS_jr; // jr t9
    }
    else
    {
        ins = INS_jalr; // jalr t9
    }
    id->idIns(ins);

    id->idInsOpt(INS_OPTS_C);
    // INS_OPTS_C: placeholders.  2/8-ins:
    //   if (callType == EC_INDIR_R) && (ireg == REG_t9)
    //      jr/jalr ireg          <---- 2-ins
    //      nop
    //   else if (callType == EC_INDIR_R)
    //      jr/jalr ireg          <---- 2-ins
    //      ori t9, ireg, 0
    //   else if (callType == EC_FUNC_TOKEN || callType == EC_FUNC_ADDR)
    //      lui at, (dst>>48)&0xffff
    //      ori at, at, (dst>>32)&0xffff
    //      dsll at, at, 16
    //      ori at, at, (dst>>16)&0xffff
    //      dsll at, at, 16
    //      ori REG_t9, at, dst&0xffff
    //      jr/jalr  t9
    //      nop

    /* Record the address: method, indirection, or funcptr */
    if (callType == EC_INDIR_R)
    {
        /* This is an indirect call (either a virtual call or func ptr call) */
        //assert(callType == EC_INDIR_R);

        id->idSetIsCallRegPtr();

        id->idReg3(ireg);//NOTE: for EC_INDIR_R, using idReg3.
        assert(xreg == REG_NA);

    }
    else
    {
        /* This is a simple direct call: "call helper/method/addr" */

        assert(callType == EC_FUNC_TOKEN || callType == EC_FUNC_ADDR);
        assert(addr != NULL);

        id->idAddr()->iiaAddr = (BYTE*)addr;

        if (callType == EC_FUNC_ADDR)
        {
            id->idSetIsCallAddr();
        }

        if (emitComp->opts.compReloc)
        {
            id->idSetIsDspReloc();
        }
    }

#ifdef DEBUG
    if (EMIT_GC_VERBOSE)
    {
        if (id->idIsLargeCall())
        {
            printf("[%02u] Rec call GC vars = %s\n", id->idDebugOnlyInfo()->idNum,
                   VarSetOps::ToString(emitComp, ((instrDescCGCA*)id)->idcGCvars));
        }
    }

    id->idDebugOnlyInfo()->idMemCookie = (size_t)methHnd; // method token
    id->idDebugOnlyInfo()->idCallSig   = sigInfo;
#endif // DEBUG

#ifdef LATE_DISASM
    if (addr != nullptr)
    {
        codeGen->getDisAssembler().disSetMethod((size_t)addr, methHnd);
    }
#endif // LATE_DISASM

    //dispIns(id);
    appendToCurIG(id);
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Rt' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Rt(regNumber reg)
{
    assert(isIntegerRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg << 16;
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Rs' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Rs(regNumber reg)
{
    assert(isIntegerRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg << 21;
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Rd' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Rd(regNumber reg)
{
    assert(isIntegerRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg << 11;
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Vd' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Vd(regNumber reg)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(emitter::isVectorRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg - (emitter::code_t)REG_V0;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg;
#endif
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Vt' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Vt(regNumber reg)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(emitter::isVectorRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg - (emitter::code_t)REG_V0;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg;
#endif
}

/*****************************************************************************
 *
 *  Returns an encoding for the specified register used in the 'Vn' position
 */

/*static*/ emitter::code_t emitter::insEncodeReg_Vn(regNumber reg)
{
assert(!"unimplemented on MIPS yet");
return NULL;
#if 0
    assert(emitter::isVectorRegister(reg));
    emitter::code_t ureg = (emitter::code_t)reg - (emitter::code_t)REG_V0;
    assert((ureg >= 0) && (ureg <= 31));
    return ureg << 5;
#endif
}

/*****************************************************************************
 *
 *  Output a call instruction.
 */

unsigned emitter::emitOutputCall(insGroup* ig, BYTE* dst, instrDesc* id, code_t code)
{
    unsigned char callInstrSize = sizeof(code_t); // 4 bytes
    regMaskTP           gcrefRegs;
    regMaskTP           byrefRegs;

    VARSET_TP GCvars(VarSetOps::UninitVal());

    // Is this a "fat" call descriptor?
    if (id->idIsLargeCall())
    {
        instrDescCGCA* idCall = (instrDescCGCA*)id;
        gcrefRegs             = idCall->idcGcrefRegs;
        byrefRegs             = idCall->idcByrefRegs;
        VarSetOps::Assign(emitComp, GCvars, idCall->idcGCvars);
    }
    else
    {
        assert(!id->idIsLargeDsp());
        assert(!id->idIsLargeCns());

        gcrefRegs = emitDecodeCallGCregs(id);
        byrefRegs = 0;
        VarSetOps::AssignNoCopy(emitComp, GCvars, VarSetOps::MakeEmpty(emitComp));
    }

    /* We update the GC info before the call as the variables cannot be
        used by the call. Killing variables before the call helps with
        boundary conditions if the call is CORINFO_HELP_THROW - see bug 50029.
        If we ever track aliased variables (which could be used by the
        call), we would have to keep them alive past the call. */

    emitUpdateLiveGCvars(GCvars, dst);

    assert(id->idIns() == INS_jr || id->idIns() == INS_jalr);
    regNumber regs[4];//NOTE: regs[3] is special when id->idIsCallRegPtr() is true.
    if (id->idIsCallRegPtr())
    {//EC_INDIR_R
        regs[0] = id->idIns()== INS_jalr ? REG_RA : id->idReg3();
        regs[1] = id->idReg3();
        regs[3] = regs[1];//temp_save id->idReg3().
        code = emitInsOps(id->idIns(), regs, nullptr);
    }
    else
    {
    //      lui at, (dst>>48)&0xffff
    //      ori at, at, (dst>>32)&0xffff
    //      dsll at, at, 16
    //      ori at, at, (dst>>16)&0xffff
    //      dsll at, at, 16
    //      ori REG_t9, at, dst&0xffff
    //      jr/jalr  t9
        uint64_t addr = (uint64_t) id->idAddr()->iiaAddr;
        regs[0] = REG_AT;
        ssize_t imm = (ssize_t) (addr >>48);
        *(code_t *)dst = emitInsOps(INS_lui, regs, &imm);
        dst += 4;

        regs[1] = REG_AT;
        imm = (ssize_t) (addr >>32);
        *(code_t *)dst = emitInsOps(INS_ori, regs, &imm);
        dst += 4;

        imm = 16;
        *(code_t *)dst = emitInsOps(INS_dsll, regs, &imm);
        dst += 4;

        imm = (ssize_t) (addr >>16);
        *(code_t *)dst = emitInsOps(INS_ori, regs, &imm);
        dst += 4;

        imm = 16;
        *(code_t *)dst = emitInsOps(INS_dsll, regs, &imm);
        dst += 4;

        regs[0] = REG_T9;
        imm = (ssize_t) addr;
        *(code_t *)dst = emitInsOps(INS_ori, regs, &imm);
        dst += 4;

        regs[0] = id->idIns()== INS_jalr ? REG_RA : REG_T9;
        regs[1] = REG_T9;
        code = emitInsOps(id->idIns(), regs, nullptr);
    }

    // Now output the call instruction and update the 'dst' pointer
    //
    unsigned outputInstrSize = emitOutput_Instr(dst, code);
    dst += outputInstrSize;

    // update volatile regs within emitThisGCrefRegs and emitThisByrefRegs.
    if (gcrefRegs != emitThisGCrefRegs)
    {
        emitUpdateLiveGCregs(GCT_GCREF, gcrefRegs, dst);
    }
    if (byrefRegs != emitThisByrefRegs)
    {
        emitUpdateLiveGCregs(GCT_BYREF, byrefRegs, dst);
    }

    // All call instructions are 4-byte in size on MIPS64
    // not including delay-slot which processed later.
    assert(outputInstrSize == callInstrSize);

    // If the method returns a GC ref, mark INTRET (V0) appropriately.
    if (id->idGCref() == GCT_GCREF)
    {
        gcrefRegs = emitThisGCrefRegs | RBM_INTRET;
    }
    else if (id->idGCref() == GCT_BYREF)
    {
        byrefRegs = emitThisByrefRegs | RBM_INTRET;
    }

    // If is a multi-register return method is called, mark INTRET_1 (V1) appropriately
    if (id->idIsLargeCall())
    {
        instrDescCGCA* idCall = (instrDescCGCA*)id;
        if (idCall->idSecondGCref() == GCT_GCREF)
        {
            gcrefRegs |= RBM_INTRET_1;
        }
        else if (idCall->idSecondGCref() == GCT_BYREF)
        {
            byrefRegs |= RBM_INTRET_1;
        }
    }

    // If the GC register set has changed, report the new set.
    if (gcrefRegs != emitThisGCrefRegs)
    {
        emitUpdateLiveGCregs(GCT_GCREF, gcrefRegs, dst + 4);
    }
    // If the Byref register set has changed, report the new set.
    if (byrefRegs != emitThisByrefRegs)
    {
        emitUpdateLiveGCregs(GCT_BYREF, byrefRegs, dst + 4);
    }

    // Some helper calls may be marked as not requiring GC info to be recorded.
    if ((!id->idIsNoGC()))
    {
        // On MIPS64, as on AMD64, we don't change the stack pointer to push/pop args.
        // So we're not really doing a "stack pop" here (note that "args" is 0), but we use this mechanism
        // to record the call for GC info purposes.  (It might be best to use an alternate call,
        // and protect "emitStackPop" under the EMIT_TRACK_STACK_DEPTH preprocessor variable.)
        emitStackPop(dst +4, /*isCall*/ true, callInstrSize +4, /*args*/ 0);

        // Do we need to record a call location for GC purposes?
        //
        if (!emitFullGCinfo)
        {
            emitRecordGCcall(dst +4, callInstrSize +4);
        }
    }
    if (id->idIsCallRegPtr())
    {
        if (regs[3] != REG_T9)
        {
            regs[0] = REG_T9;
            regs[1] = regs[3];//id->idReg3()
            *(code_t *)dst = emitInsOps(INS_mov, regs, nullptr);
        }
        else
        {
            *(code_t *)dst = 0;//nop
        }
        callInstrSize = 2<<2;
    }
    else
    {
        *(code_t *)dst = 0;//nop
        callInstrSize = 8<<2;// INS_OPTS_C: 8-ins:
    }
    id->idAddr()->iiaSetInstrEncode(code);//used for emitInsMayWriteToGCReg().
    return callInstrSize;
}

/*****************************************************************************
 *
 *  Emit a 32-bit MIPS64 instruction
 */

/*static*/ unsigned emitter::emitOutput_Instr(BYTE* dst, code_t code)
{
    assert(sizeof(code_t) == 4);
    *((code_t*)dst) = code;

    return sizeof(code_t);
}

emitter::code_t emitter::emitInsOps(instruction ins, /*registers_encode*/ regNumber *reg, ssize_t *imm)
{
    code_t code = BAD_CODE;

    if (ins == INS_dneg)
    {
        code = emitInsCode(INS_dsubu);
        //dsubu rd, zero, rt
        code |= (reg[0] & 0x1f)<<11;//rd
        code |= (reg[1] & 0x1f)<<16;//rt
        return code;
    }
    else if (ins == INS_neg)
    {
        code = emitInsCode(INS_subu);
        //subu rd, zero, rt
        code |= (reg[0] & 0x1f)<<11;//rd
        code |= (reg[1] & 0x1f)<<16;//rt
        return code;
    }
    else if (ins == INS_not)
    {
        code = emitInsCode(INS_nor);
        //nor rd, rs, zero
        code |= (reg[0] & 0x1f)<<11;//rd
        code |= (reg[1] & 0x1f)<<21;//rs
        return code;
    }

#define LABEL_INS(ins)   Label_ins_##ins :
#define LABEL_INS_END(ins)   return code;

    // clang-format off
    const static void* const insLabel[] =
    {
        #define INSTS(id, nm, fp, ldst, fmt, e1) &&Label_ins_##id,
        #include "instrs.h"
    };
    // clang-format on

    assert(ins < sizeof(insLabel)/sizeof(void*));

    code = emitInsCode(ins /*, fmt*/);

    goto * insLabel[ins];

LABEL_INS(invalid)
            printf("Error: Unexpected MIPS64 instruction id:  0x%x\n", ins);
            assert(!"Unexpected instruction");
LABEL_INS_END(invalid)

LABEL_INS(mov)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<21;//rs
LABEL_INS_END(mov)

LABEL_INS(abs_s)
LABEL_INS(abs_d)
LABEL_INS(neg_s)
LABEL_INS(neg_d)
LABEL_INS(ceil_l_s)
LABEL_INS(ceil_l_d)
LABEL_INS(ceil_w_s)
LABEL_INS(ceil_w_d)
LABEL_INS(cvt_d_s)
LABEL_INS(cvt_d_w)
LABEL_INS(cvt_d_l)
LABEL_INS(cvt_l_s)
LABEL_INS(cvt_l_d)
LABEL_INS(cvt_s_d)
LABEL_INS(cvt_s_w)
LABEL_INS(cvt_s_l)
LABEL_INS(cvt_w_s)
LABEL_INS(cvt_w_d)
LABEL_INS(floor_l_s)
LABEL_INS(floor_l_d)
LABEL_INS(floor_w_s)
LABEL_INS(floor_w_d)
LABEL_INS(mov_s)
LABEL_INS(mov_d)
LABEL_INS(recip_s)
LABEL_INS(recip_d)
LABEL_INS(round_l_s)
LABEL_INS(round_l_d)
LABEL_INS(round_w_s)
LABEL_INS(round_w_d)
LABEL_INS(rsqrt_s)
LABEL_INS(rsqrt_d)
LABEL_INS(sqrt_s)
LABEL_INS(sqrt_d)
LABEL_INS(trunc_l_s)
LABEL_INS(trunc_l_d)
LABEL_INS(trunc_w_s)
LABEL_INS(trunc_w_d)
    code |= (reg[0] & 0x1f)<<6;//fd
    code |= (reg[1] & 0x1f)<<11;//fs
LABEL_INS_END(trunc_w_d)


LABEL_INS(clo)
LABEL_INS(clz)
LABEL_INS(dclo)
LABEL_INS(dclz)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<21;//rs
    code |= (reg[1] & 0x1f)<<16;//rt
LABEL_INS_END(clo)

LABEL_INS(add)
LABEL_INS(addu)
LABEL_INS(and)
LABEL_INS(dadd)
LABEL_INS(daddu)
LABEL_INS(dsub)
LABEL_INS(dsubu)
LABEL_INS(movz)
LABEL_INS(movn)
LABEL_INS(mul)
LABEL_INS(nor)
LABEL_INS(or)
LABEL_INS(slt)
LABEL_INS(sltu)
LABEL_INS(sub)
LABEL_INS(subu)
LABEL_INS(xor)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<21;//rs
    code |= (reg[2] & 0x1f)<<16;//rt
LABEL_INS_END(add)

LABEL_INS(sdxc1)
LABEL_INS(drotrv)
LABEL_INS(dsllv)
LABEL_INS(dsrav)
LABEL_INS(dsrlv)
LABEL_INS(rotrv)
LABEL_INS(sllv)
LABEL_INS(srav)
LABEL_INS(srlv)
LABEL_INS(suxc1)
LABEL_INS(swxc1)
    code |= (reg[0] & 0x1f)<<11;//rd or hint/fs
    code |= (reg[1] & 0x1f)<<16;//rt or index
    code |= (reg[2] & 0x1f)<<21;//rs or base
LABEL_INS_END(sdxc1)

LABEL_INS(prefx)
    code |= (imm[0] & 0x1f)<<11;//hint
    code |= (reg[0] & 0x1f)<<16;//index
    code |= (reg[1] & 0x1f)<<21;//base
LABEL_INS_END(prefx)

LABEL_INS(lui)
    code |= (reg[0] & 0x1f)<<16;//rt
    code |= (imm[0] & 0xffff);//imm
LABEL_INS_END(lui)


LABEL_INS(b)
LABEL_INS(bal)
    assert(!(imm[0] & 0x3));
    code |= ((imm[0]>>2) & 0xffff);//offset
LABEL_INS_END(b)

LABEL_INS(bc1f)
LABEL_INS(bc1t)
    code |= (imm[0] & 0x7)<<18;//cc
    assert(!(imm[1] & 0x3));
    code |= ((imm[1]>>2) & 0xffff);//offset
LABEL_INS_END(bc1f)

LABEL_INS(sync)
    code |= (imm[0] & 0x1f)<<6;//stype
LABEL_INS_END(sync)

LABEL_INS(ldxc1)
LABEL_INS(luxc1)
LABEL_INS(lwxc1)
    code |= (reg[0] & 0x1f)<<6; //fd
    code |= (reg[1] & 0x1f)<<16;//index
    code |= (reg[2] & 0x1f)<<21;//base
LABEL_INS_END(ldxc1)

LABEL_INS(synci)
    code |= (reg[0] & 0x1f)<<21;   //base
    code |= ((imm[0]>>2) & 0xffff);//offset
LABEL_INS_END(synci)

LABEL_INS(bgez)
LABEL_INS(bgezal)
LABEL_INS(bgtz)
LABEL_INS(blez)
LABEL_INS(bltz)
LABEL_INS(bltzal)
    code |= (reg[0] & 0x1f)<<21; //rs
    assert(!(imm[0] & 0x3));
    code |= ((imm[0]>>2) & 0xffff);//offset
LABEL_INS_END(bgez)

LABEL_INS(break)
    code |= (imm[0] & 0xfffff)<<6;   //code
LABEL_INS_END(break)

LABEL_INS(dext)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert(imm[0]<32);
    code |= (imm[0] & 0x1f)<<6;  //pos
    assert((0<imm[1]) && (imm[1]<33));
    code |= ((imm[1]-1) & 0x1f)<<11; //size
LABEL_INS_END(dext)

LABEL_INS(dextm)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert(imm[0]<32);
    code |= (imm[0] & 0x1f)<<6;  //pos
    assert((32<imm[1]) && (imm[1]<65) && ((imm[1]+imm[2])<65));
    code |= ((imm[1]-1-32) & 0x1f)<<11; //size
LABEL_INS_END(dextm)

LABEL_INS(dextu)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert((31<imm[0]) && (imm[0]<64));
    code |= ((imm[0]-32) & 0x1f)<<6;  //pos
    assert((0<imm[1]) && (imm[1]<33) && ((imm[1]+imm[0])<65));
    code |= ((imm[1]-1) & 0x1f)<<11; //size
LABEL_INS_END(dextu)

LABEL_INS(dins)
LABEL_INS(ins)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert(imm[0]<32);
    code |= (imm[0] & 0x1f)<<6;  //pos
    assert((0<imm[1]) && (imm[1]<33) && ((imm[1]+imm[0])<33));
    code |= ((imm[0] + imm[1]-1) & 0x1f)<<11; //size
LABEL_INS_END(dins)

LABEL_INS(ext)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert(imm[0]<32);
    code |= (imm[0] & 0x1f)<<6;  //pos
    assert((0<imm[1]) && (imm[1]<33) && ((imm[1]+imm[0])<33));
    code |= ((imm[1]-1) & 0x1f)<<11; //size
LABEL_INS_END(ext)

LABEL_INS(dinsm)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert(imm[0]<32);
    code |= (imm[0] & 0x1f)<<6;  //pose
    assert((1<imm[1]) && (imm[1]<65) && (32<(imm[1]+imm[0])) && ((imm[1]+imm[0])<65));
    code |= ((imm[0] + imm[1]-33) & 0x1f)<<11; //size
LABEL_INS_END(dinsm)

LABEL_INS(dinsu)
    code |= (reg[0] & 0x1f)<<16; //rt
    code |= (reg[1] & 0x1f)<<21; //rs
    assert((31<imm[0]) && (imm[0]<64));
    code |= (imm[0] & 0x1f)<<6;  //pose
    assert((0<imm[1]) && (imm[1]<33) && (32<(imm[1]+imm[0])) && ((imm[1]+imm[0])<65));
    code |= ((imm[0] + imm[1]-33) & 0x1f)<<11; //size
LABEL_INS_END(dinsu)


LABEL_INS(madd_s)
LABEL_INS(madd_d)
LABEL_INS(msub_s)
LABEL_INS(msub_d)
LABEL_INS(nmadd_s)
LABEL_INS(nmadd_d)
LABEL_INS(nmsub_s)
LABEL_INS(nmsub_d)
    code |= (reg[0] & 0x1f)<<6;  //fd
    code |= (reg[1] & 0x1f)<<21; //fr
    code |= (reg[2] & 0x1f)<<11; //fs
    code |= (reg[3] & 0x1f)<<16; //ft
LABEL_INS_END(madd_s)

LABEL_INS(j)
LABEL_INS(jal)
    assert(!(imm[0] & 0x3));
    code |= ((imm[0]>>2) & 0x3ffffff);   //target
LABEL_INS_END(j)

LABEL_INS(beq)
LABEL_INS(bne)
    code |= (reg[0] & 0x1f)<<21; //rs
    code |= (reg[1] & 0x1f)<<16; //rt
    assert(!(imm[0] & 0x3));
    code |= ((imm[0]>>2) & 0xffff);   //offset
LABEL_INS_END(beq)


LABEL_INS(addi)
LABEL_INS(addiu)
LABEL_INS(andi)
LABEL_INS(daddi)
LABEL_INS(daddiu)
LABEL_INS(lb)
LABEL_INS(lbu)
LABEL_INS(ld)
LABEL_INS(ldc1)
LABEL_INS(ldl)
LABEL_INS(ldr)
LABEL_INS(lh)
LABEL_INS(lhu)
LABEL_INS(ll)
LABEL_INS(lld)
LABEL_INS(lw)
LABEL_INS(lwc1)
LABEL_INS(lwl)
LABEL_INS(lwr)
LABEL_INS(lwu)
LABEL_INS(ori)
LABEL_INS(sb)
LABEL_INS(sc)
LABEL_INS(scd)
LABEL_INS(sd)
LABEL_INS(sdc1)
LABEL_INS(sdl)
LABEL_INS(sdr)
LABEL_INS(sh)
LABEL_INS(slti)
LABEL_INS(sltiu)
LABEL_INS(sw)
LABEL_INS(swc1)
LABEL_INS(swl)
LABEL_INS(swr)
LABEL_INS(xori)
    code |= (reg[0] & 0x1f)<<16; //rt or ft or hint
    code |= (reg[1] & 0x1f)<<21; //rs or base
    code |= (imm[0] & 0xffff);   //imm or offset
LABEL_INS_END(addi)

LABEL_INS(pref)
    code |= (imm[0] & 0x1f)<<16; //hint
    code |= (reg[0] & 0x1f)<<21; //rs or base
    code |= (imm[1] & 0xffff);   //offset
LABEL_INS_END(pref)

LABEL_INS(c_f_s)
LABEL_INS(c_un_s)
LABEL_INS(c_eq_s)
LABEL_INS(c_ueq_s)
LABEL_INS(c_olt_s)
LABEL_INS(c_ult_s)
LABEL_INS(c_ole_s)
LABEL_INS(c_ule_s)
LABEL_INS(c_sf_s)
LABEL_INS(c_ngle_s)
LABEL_INS(c_seq_s)
LABEL_INS(c_ngl_s)
LABEL_INS(c_lt_s)
LABEL_INS(c_nge_s)
LABEL_INS(c_le_s)
LABEL_INS(c_ngt_s)
LABEL_INS(c_f_d)
LABEL_INS(c_un_d)
LABEL_INS(c_eq_d)
LABEL_INS(c_ueq_d)
LABEL_INS(c_olt_d)
LABEL_INS(c_ult_d)
LABEL_INS(c_ole_d)
LABEL_INS(c_ule_d)
LABEL_INS(c_sf_d)
LABEL_INS(c_ngle_d)
LABEL_INS(c_seq_d)
LABEL_INS(c_ngl_d)
LABEL_INS(c_lt_d)
LABEL_INS(c_nge_d)
LABEL_INS(c_le_d)
LABEL_INS(c_ngt_d)
    code |= (reg[0] & 0x1f)<<11;//fs
    code |= (reg[1] & 0x1f)<<16;//ft
    code |= (imm[0] & 0x7)<<8;  //cc
LABEL_INS_END(c_f_s)

LABEL_INS(cfc1)
LABEL_INS(ctc1)
LABEL_INS(dmfc1)
LABEL_INS(dmtc1)
LABEL_INS(mfc1)
LABEL_INS(mfhc1)
LABEL_INS(mtc1)
LABEL_INS(mthc1)
LABEL_INS(rdhwr)
    code |= (reg[0] & 0x1f)<<16;//rt
    code |= (reg[1] & 0x1f)<<11;//fs
LABEL_INS_END(cfc1)

LABEL_INS(dsbh)
LABEL_INS(dshd)
LABEL_INS(seb)
LABEL_INS(seh)
LABEL_INS(wsbh)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<16;//rt
LABEL_INS_END(dsbh)


LABEL_INS(ddiv)
LABEL_INS(ddivu)
LABEL_INS(div)
LABEL_INS(divu)
LABEL_INS(dmult)
LABEL_INS(dmultu)
LABEL_INS(madd)
LABEL_INS(maddu)
LABEL_INS(msub)
LABEL_INS(msubu)
LABEL_INS(mult)
LABEL_INS(multu)
    code |= (reg[0] & 0x1f)<<21;//rs
    code |= (reg[1] & 0x1f)<<16;//rt
LABEL_INS_END(ddiv)


LABEL_INS(jalr)
LABEL_INS(jalr_hb)
    assert(reg[0]);
    code |= (reg[0] & 0x1f)<<11;//rd, default ra. NOTE:must point rd before using!
    code |= (reg[1] & 0x1f)<<21;//rs
LABEL_INS_END(jalr)

LABEL_INS(jr)
LABEL_INS(jr_hb)
LABEL_INS(mthi)
LABEL_INS(mtlo)
    code |= (reg[0] & 0x1f)<<21;//rs
LABEL_INS_END(jr)

LABEL_INS(mfhi)
LABEL_INS(mflo)
    code |= (reg[0] & 0x1f)<<11;//rd
LABEL_INS_END(mfhi)

LABEL_INS(movf_s)
LABEL_INS(movf_d)
LABEL_INS(movt_s)
LABEL_INS(movt_d)
    code |= (reg[0] & 0x1f)<<6; //fd
    code |= (reg[1] & 0x1f)<<11;//fs
    code |= (imm[0] & 0x7)<<18; //cc
LABEL_INS_END(movf_s)

LABEL_INS(movf)
LABEL_INS(movt)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<21;//rs
    code |= (imm[0] & 0x7)<<18; //cc
LABEL_INS_END(movf)

LABEL_INS(add_s)
LABEL_INS(add_d)
LABEL_INS(div_s)
LABEL_INS(div_d)
LABEL_INS(movn_s)
LABEL_INS(movn_d)
LABEL_INS(movz_s)
LABEL_INS(movz_d)
LABEL_INS(mul_s)
LABEL_INS(mul_d)
LABEL_INS(sub_s)
LABEL_INS(sub_d)
    code |= (reg[0] & 0x1f)<<6; //fd
    code |= (reg[1] & 0x1f)<<11;//fs
    code |= (reg[2] & 0x1f)<<16;//ft or rt
LABEL_INS_END(add_s)

LABEL_INS(drotr)
LABEL_INS(drotr32)
LABEL_INS(dsll)
LABEL_INS(dsll32)
LABEL_INS(dsra)
LABEL_INS(dsra32)
LABEL_INS(dsrl)
LABEL_INS(dsrl32)
LABEL_INS(rotr)
LABEL_INS(sll)
LABEL_INS(sra)
LABEL_INS(srl)
    code |= (reg[0] & 0x1f)<<11;//rd
    code |= (reg[1] & 0x1f)<<16;//rt
    code |= (imm[0] & 0x1f)<<6; //sa
LABEL_INS_END(drotr)

LABEL_INS(nop)
LABEL_INS(ehb)
LABEL_INS(pause)

LABEL_INS_END(DONE)

}

/*****************************************************************************
*
 *  Append the machine code corresponding to the given instruction descriptor
 *  to the code block at '*dp'; the base of the code block is 'bp', and 'ig'
 *  is the instruction group that contains the instruction. Updates '*dp' to
 *  point past the generated code, and returns the size of the instruction
 *  descriptor in bytes.
 */

size_t emitter::emitOutputInstr(insGroup* ig, instrDesc* id, BYTE** dp)
{
    BYTE* dst = *dp;
    BYTE* dst2 = dst + 4;//addr for updating gc info if needed.
    code_t code = 0;
    size_t sz;// = emitSizeOfInsDsc(id);

#ifdef DEBUG
#if DUMP_GC_TABLES
    bool dspOffs = emitComp->opts.dspGCtbls;
#else
    bool dspOffs = !emitComp->opts.disDiffable;
#endif
#endif // DEBUG

    assert(REG_NA == (int)REG_NA);

    insOpts insOp = id->idInsOpt();
    regNumber regs[4];

    switch (insOp)
    {
        case INS_OPTS_RELOC:
        {
            instrDescJmp* jmp = (instrDescJmp*) id;
            //   bal 4
            //   lui at, off-hi-16bits
            //   ori at, at, off-lo-16bits
            //   daddu  reg, at, ra
            ssize_t imm = 4;
            *(code_t *)dst = emitInsOps(INS_bal, nullptr, &imm);
            dst += 4;

            BYTE* addr = jmp->idAddr()->iiaAddr;//get addr.

            emitRecordRelocation(dst+4, id->idAddr()->iiaAddr, IMAGE_REL_MIPS64_PC);
            int64_t addrOffs = *(int32_t*)(dst +4);

            assert(!(addrOffs & 3));
            assert(addrOffs < 0x7fffffff);
            assert(-((int64_t)1<<31) < addrOffs);

            regs[0] = REG_AT;

            imm = addrOffs >> 16;
            *(code_t *)dst = emitInsOps(INS_lui, regs, &imm);
            dst += 4;

            regs[1] = REG_AT;
            *(code_t *)dst = emitInsOps(INS_ori, regs, (ssize_t*) &addrOffs);
            dst += 4;

            regs[0] = jmp->idReg1();
            //regs[1] = REG_AT;
            regs[2] = REG_RA;
            *(code_t *)dst = emitInsOps(INS_daddu, regs, nullptr);

            id->idAddr()->iiaSetInstrEncode(*(code_t *)dst);//used for emitInsMayWriteToGCReg().

            dst += 4;
            dst2 = dst;

            sz  = sizeof(instrDescJmp);
        }
            break;
        case INS_OPTS_RC:
        {
            instrDescJmp* jmp = (instrDescJmp*) id;
            assert(jmp->idGCref() == GCT_NONE);
            assert(jmp->idIsBound());
            //   bal 4
            //   lui at, off-hi-16bits
            //   ori at, at, off-lo-16bits
            //   daddu  reg, at, ra
            ssize_t imm = 4;
            *(code_t *)dst = emitInsOps(INS_bal, nullptr, &imm);
            dst += 4;

            unsigned int dataOffs = jmp->idAddr()->iiaGetInstrEncode();//get data's offset.
            assert(dataOffs > 0);
            assert(!(dataOffs & 0x3));

            regs[0] = REG_AT;
            if (dataOffs <= 0xffff)
            {
                *(code_t *)dst = 0;//nop
                dst += 4;

                regs[1] = REG_R0;
                *(code_t *)dst = emitInsOps(INS_ori, regs, (ssize_t*) &dataOffs);
                dst += 4;
            }
            else
            {
                imm = dataOffs >> 16;
                assert(isValidSimm16(imm));
                *(code_t *)dst = emitInsOps(INS_lui, regs, &imm);
                dst += 4;

                regs[1] = REG_AT;
                *(code_t *)dst = emitInsOps(INS_ori, regs, (ssize_t*) &dataOffs);
                dst += 4;
            }

            regs[0] = jmp->idReg1();
            regs[1] = REG_AT;
            regs[2] = REG_RA;
            *(code_t *)dst = emitInsOps(INS_daddu, regs, nullptr);

            id->idAddr()->iiaSetInstrEncode(*(code_t *)dst);//used for emitInsMayWriteToGCReg().
            dst += 4;

            dst2 = dst;

            sz  = sizeof(instrDescJmp);
        }
            break;

        case INS_OPTS_RL:
        {
            //   bal 4
            //   lui at, dst-hi-16bits
            //   ori at, at, dst-lo-16bits
            //   dsubu/daddu  reg, ra, at
            ssize_t imm = 4;
            *(code_t *)dst = emitInsOps(INS_bal, nullptr, &imm);
            dst += 4;

            imm = (ssize_t) id->idAddr()->iiaGetInstrEncode();//get jmp's offset, temporarily saved.
            assert(imm > 0);

            //NOTE:bit0=1 is Backward jump!
            ssize_t adjust = (imm & 1) ? -9 : 8;//8=(4-2)<<2.
            imm = imm + adjust;

            regs[0] = REG_AT;
            if (imm <= 0xffff)
            {
                *(code_t *)dst = 0;//nop
                dst += 4;

                regs[1] = REG_R0;
                *(code_t *)dst = emitInsOps(INS_ori, regs, &imm);
                dst += 4;
            }
            else
            {
                *(code_t *)(dst+4) = emitInsOps(INS_ori, regs, &imm);

                regs[1] = REG_AT;
                imm = imm >> 16;
                assert(isValidSimm16(imm));
                *(code_t *)dst = emitInsOps(INS_lui, regs, &imm);
                dst += 8;
            }

            regs[0] = id->idReg1();
            regs[1] = REG_RA;
            regs[2] = REG_AT;
            if (adjust < 0)
            {//Backward
                *(code_t *)dst = emitInsOps(INS_dsubu, regs, nullptr);
            }
            else
            {//Forward
                *(code_t *)dst = emitInsOps(INS_daddu, regs, nullptr);
            }

            id->idAddr()->iiaSetInstrEncode(*(code_t *)dst);//used for emitInsMayWriteToGCReg().

            dst += 4;
            dst2 = dst;
            sz  = sizeof(instrDescJmp);
        }
            break;
        case INS_OPTS_JR:
        //   bal 4
        //   lui at, dst-hi-16bits
        //   ori at, at, dst-lo-16bits
        //   daddu  reg, at, ra
        //   jr  reg
        //   nop
        case INS_OPTS_J:
        //   bal 4
        //   lui at, dst-hi-16bits
        //   ori at, at, dst-lo-16bits
        //   daddu  t9, at, ra
        //   jalr/jr  t9
        //   nop           <-----here add delay-slot!
        {
            ssize_t imm = (ssize_t) id->idAddr()->iiaGetInstrEncode();//get jmp's offset, temporarily saved.
            assert(imm >= 0);

            //NOTE:bit0=1 is Backward jump!
            ssize_t adjust = (imm & 1) ? -21 : 20;//20=(6-1)<<2.
            imm = imm + adjust;

            ssize_t imm2 = imm>>2; ;
            if (imm2 <= 0x7fff)
            {
                if (adjust < 0)
                {//Backward jump.
                    imm2 = -imm;
                    *(code_t *)dst = emitInsOps(id->idIns(), nullptr, &imm2);
                }
                else
                {
                    *(code_t *)dst = emitInsOps(id->idIns(), nullptr, &imm);
                }
                dst += 4;

                *(code_t *)dst = 0;//nop
                dst += 4;
                *(code_t *)dst = 0;//nop
                dst += 4;
                *(code_t *)dst = 0;//nop
                dst += 4;
                *(code_t *)dst = 0;//nop
                dst += 4;
                *(code_t *)dst = 0;//nop
                dst += 4;
            }
            else
            {
                imm = imm + ((adjust < 0) ? 4 : -4);
                imm2 = 4;
                *(code_t *)dst = emitInsOps(INS_bal, nullptr, &imm2);
                dst += 4;

                regs[0] = REG_AT;
                if (imm <= 0xffff)
                {
                    *(code_t *)dst = 0;//nop
                    dst += 4;

                    regs[1] = REG_R0;
                    *(code_t *)dst = emitInsOps(INS_ori, regs, &imm);
                    dst += 4;
                }
                else
                {
                    regs[1] = REG_AT;
                    *(code_t *)(dst+4) = emitInsOps(INS_ori, regs, &imm);

                    imm = imm >> 16;
                    assert(isValidSimm16(imm));
                    *(code_t *)dst = emitInsOps(INS_lui, regs, &imm);
                    dst += 8;
                }

                regs[0] = id->idReg1();
                regs[1] = REG_RA;
                regs[2] = REG_AT;
                if (adjust < 0)
                {//Backward jump.
                    *(code_t *)dst = emitInsOps(INS_dsubu, regs, nullptr);
                }
                else
                {//Forward jump.
                    *(code_t *)dst = emitInsOps(INS_daddu, regs, nullptr);
                }
                dst += 4;

                instruction ins = id->idIns() == INS_bal ? INS_jalr : INS_jr;
                regs[0] = ins ==INS_jalr ? REG_RA : regs[0];
                regs[1] = id->idReg1();
                *(code_t *)dst = emitInsOps(ins, regs, nullptr);
                dst += 4;

                *(code_t *)dst = 0;//nop
                dst += 4;
            }

            sz  = sizeof(instrDescJmp);
        }
            break;

        case INS_OPTS_C:
            if (id->idIsLargeCall())
            {
                /* Must be a "fat" call descriptor */
                sz = sizeof(instrDescCGCA);
            }
            else
            {
                assert(!id->idIsLargeDsp());
                assert(!id->idIsLargeCns());
                sz = sizeof(instrDesc);
            }
            dst += emitOutputCall(ig, dst, id, 0);
            break;

        //case INS_OPTS_NONE:
        default:
            //assert(id->idGCref() == GCT_NONE);
            *(code_t *)dst = id->idAddr()->iiaGetInstrEncode();
            dst += 4;
            sz = emitSizeOfInsDsc(id);
            break;
    }

    // Determine if any registers now hold GC refs, or whether a register that was overwritten held a GC ref.
    // We assume here that "id->idGCref()" is not GC_NONE only if the instruction described by "id" writes a
    // GC ref to register "id->idReg1()".  (It may, apparently, also not be GC_NONE in other cases, such as
    // for stores, but we ignore those cases here.)
    if (emitInsMayWriteToGCReg(id)) // True if "id->idIns()" writes to a register than can hold GC ref.
    {
        // We assume that "idReg1" is the primary destination register for all instructions
        if (id->idGCref() != GCT_NONE)
        {
            emitGCregLiveUpd(id->idGCref(), id->idReg1(), dst2);
        }
        else
        {
            emitGCregDeadUpd(id->idReg1(), dst2);
        }

        //if (emitInsMayWriteMultipleRegs(id))
        //{
        //    // INS_gslq etc...
        //    // "idReg2" is the secondary destination register
        //    if (id->idGCrefReg2() != GCT_NONE)
        //    {
        //        emitGCregLiveUpd(id->idGCrefReg2(), id->idReg2(), *dp);
        //    }
        //    else
        //    {
        //        emitGCregDeadUpd(id->idReg2(), *dp);
        //    }
        //}
    }

    // Now we determine if the instruction has written to a (local variable) stack location, and either written a GC
    // ref or overwritten one.
    if (emitInsWritesToLclVarStackLoc(id) /*|| emitInsWritesToLclVarStackLocPair(id)*/)
    {
        int      varNum = id->idAddr()->iiaLclVar.lvaVarNum();
        unsigned ofs    = AlignDown(id->idAddr()->iiaLclVar.lvaOffset(), TARGET_POINTER_SIZE);
        bool     FPbased;
        int      adr = emitComp->lvaFrameAddress(varNum, &FPbased);
        if (id->idGCref() != GCT_NONE)
        {
            emitGCvarLiveUpd(adr + ofs, varNum, id->idGCref(), dst2);
        }
        else
        {
            // If the type of the local is a gc ref type, update the liveness.
            var_types vt;
            if (varNum >= 0)
            {
                // "Regular" (non-spill-temp) local.
                vt = var_types(emitComp->lvaTable[varNum].lvType);
            }
            else
            {
                TempDsc* tmpDsc = codeGen->regSet.tmpFindNum(varNum);
                vt              = tmpDsc->tdTempType();
            }
            if (vt == TYP_REF || vt == TYP_BYREF)
                emitGCvarDeadUpd(adr + ofs, dst2);
        }
        //if (emitInsWritesToLclVarStackLocPair(id))
        //{
        //    unsigned ofs2 = ofs + TARGET_POINTER_SIZE;
        //    if (id->idGCrefReg2() != GCT_NONE)
        //    {
        //        emitGCvarLiveUpd(adr + ofs2, varNum, id->idGCrefReg2(), *dp);
        //    }
        //    else
        //    {
        //        // If the type of the local is a gc ref type, update the liveness.
        //        var_types vt;
        //        if (varNum >= 0)
        //        {
        //            // "Regular" (non-spill-temp) local.
        //            vt = var_types(emitComp->lvaTable[varNum].lvType);
        //        }
        //        else
        //        {
        //            TempDsc* tmpDsc = codeGen->regSet.tmpFindNum(varNum);
        //            vt              = tmpDsc->tdTempType();
        //        }
        //        if (vt == TYP_REF || vt == TYP_BYREF)
        //            emitGCvarDeadUpd(adr + ofs2, *dp);
        //    }
        //}
    }

#ifdef DEBUG
    /* Make sure we set the instruction descriptor size correctly */

    //size_t expected = emitSizeOfInsDsc(id);
    //assert(sz == expected);

    if (emitComp->opts.disAsm || emitComp->opts.dspEmit || emitComp->verbose)
    {
        code_t *cp = (code_t*) *dp;
        while ((BYTE*)cp != dst)
        {
            emitDisInsName(*cp, (BYTE*)cp);
            cp++;
        }
        //emitDispIns(id, false, dspOffs, true, emitCurCodeOffs(odst), *dp, (dst - *dp), ig);
    }

    if (emitComp->compDebugBreak)
    {
        // For example, set JitBreakEmitOutputInstr=a6 will break when this method is called for
        // emitting instruction a6, (i.e. IN00a6 in jitdump).
        if ((unsigned)JitConfig.JitBreakEmitOutputInstr() == id->idDebugOnlyInfo()->idNum)
        {
            assert(!"JitBreakEmitOutputInstr reached");
        }
    }
#endif

    /* All instructions are expected to generate code */

    assert(*dp != dst);

    *dp = dst;

    return sz;
}

/*****************************************************************************/
/*****************************************************************************/

#ifdef DEBUG

//NOTE: At least 32bytes within dst.
void emitter::emitDisInsName(code_t code, const BYTE* dst)
{
    const BYTE* insstrs = dst;

    if (!code)
    {
        printf("   0x%llx   nop\n", insstrs);
        return ;
    }
    else if (code == 0xc0)
    {
        printf("   0x%llx   ehb\n", insstrs);
        return ;
    }

// clang-format off
    const char * const regName[] = {"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

    const char * const FregName[] = {"$f0", "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7", "$f8", "$f9", "$f10", "$f11", "$f12", "$f13", "$f14", "$f15", "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23", "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"};
// clang-format on


    unsigned int opcode = (code>>26) & 0x3f;

    //bits: 31-26.
    switch (opcode)
    {
        case MIPS_OP_COP1X:
            goto Lalel_cop1x;
            //break;
        case MIPS_OP_SPECIAL:
            goto Lalel_special;
            //break;
        case MIPS_OP_SPECIAL2:
            goto Lalel_special2;
            //break;
        case MIPS_OP_SPECIAL3:
            goto Lalel_special3;
            //break;
        case MIPS_OP_REGIMM:
            goto Lalel_regimm;
            //break;
        case MIPS_OP_COP1:
            goto Lalel_cop1;
            //break;
        case MIPS_OP_LB:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lb  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LBU:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lbu  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LHU:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lhu  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LWU:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lwu  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LWL:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lwl  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LWR:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            unsigned int ops2 = code & 0xffff;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lwr  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LW:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lw  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LD:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   ld  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LDL:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   ldl  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LDR:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   ldr  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LL:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   ll  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LLD:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lld  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LWC1:
        {
            const char *ops1 = FregName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   lwc1  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LDC1:
        {
            const char *ops1 = FregName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];

            printf("   0x%llx   ldc1  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);

            return ;
            //break;
        }
        case MIPS_OP_LH:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   lh  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SB:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sb  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SH:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sh  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SW:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sw  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SD:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sd  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SWL:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   swl  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SWR:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   swr  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SDL:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sdl  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SDR:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sdr  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SWC1:
        {
            const char *ops1 = FregName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   swc1  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SDC1:
        {
            const char *ops1 = FregName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sdc1  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SC:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sc  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_SCD:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            short ops2 = (short)code;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   scd  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }
        case MIPS_OP_PREF:
        {
            const char *ops1 = regName[(code>>16) & 0x1f];
            unsigned int ops2 = code & 0xffff;
            const char* ops3 = regName[(code>>21) & 0x1f];
            printf("   0x%llx   pref  %s, %d(%s)\n", insstrs, ops1, ops2, ops3);
            return ;
            //break;
        }

        case MIPS_OP_J:
        {
            printf("   0x%llx   j  0x%llx\n", insstrs, ((int64_t)insstrs & 0xfffffffff0000000) + ((code&0x3ffffff)<<2));//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_JAL:
        {
            printf("   0x%llx   jal  0x%llx\n", insstrs, ((int64_t)insstrs & 0xfffffffff0000000) + ((code&0x3ffffff)<<2));//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BEQ:
        {
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            if (!((code>>21) & 0x1f) && !((code>>16) & 0x1f))
            {
                printf("   0x%llx   b  0x%llx\n", insstrs, (int64_t)insstrs + 4 + offset);//should amend further.
                return ;
            }

            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            printf("   0x%llx   beq  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BEQL://likely not used.
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            printf("   0x%llx   beql  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BNE:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            printf("   0x%llx   bne  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BNEL://likely not used.
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            printf("   0x%llx   bnel  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BLEZ:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            printf("   0x%llx   blez  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BLEZL://likely not used.
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            printf("   0x%llx   blezl  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BGTZ:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            printf("   0x%llx   bgtz  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }
        case MIPS_OP_BGTZL://likely not used.
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;
            printf("   0x%llx   bgtzl  %s, %s, 0x%llx\n", insstrs, rs, rt, (int64_t)insstrs + 4 + offset);//should amend further.
            return ;
            //break;
        }

        case MIPS_OP_SLTI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            int imm = (int)(code & 0xffff);
            printf("   0x%llx   slti  %s, %s, %d\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_SLTIU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;
            printf("   0x%llx   sltiu  %s, %s, %d\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_ANDI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;
            printf("   0x%llx   andi  %s, %s, 0x%x\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_ORI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;
            printf("   0x%llx   ori  %s, %s, 0x%x\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_XORI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;
            printf("   0x%llx   xori  %s, %s, 0x%x\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_LUI:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;

            printf("   0x%llx   lui  %s, 0x%x\n", insstrs, rt, imm);

            return ;
            //break;
        }

        case MIPS_OP_ADDI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int imm = code & 0xffff;
            printf("   0x%llx   addi  %s, %s, 0x%x\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_ADDIU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short imm = (short)code;
            printf("   0x%llx   addiu  %s, %s, %d\n", insstrs, rt, rs, (int)imm);
            return ;
            //break;
        }
        case MIPS_OP_DADDI:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short imm = (short)code;
            printf("   0x%llx   daddi  %s, %s, 0x%d\n", insstrs, rt, rs, imm);
            return ;
            //break;
        }
        case MIPS_OP_DADDIU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            short imm = (short)code;
            printf("   0x%llx   daddiu  %s, %s, %d\n", insstrs, rt, rs, (int)imm);
            return ;
            //break;
        }

        //// add new code here!
        //case MIPS_OP_***:
        //{
        //    break;
        //}
        default :
            printf("MIPS illegal instruction: 0x%x\n", code);
            return ;
    }


Lalel_special:
    //bits: 5-0.
    opcode = code & 0x3f;

    switch (opcode)
    {
        case MIPS_SPEC_SLL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;
            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(sll): 0x%08x\n", code);
                return ;
            }
            printf("   0x%llx   sll  %s, %s, %d\n", insstrs, rd, rt, sa);
            return ;
            //break;
        }
        case MIPS_SPEC_MOVCI:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int cc = (code>>18) & 0x7;
            if ((code>>16) & 0x3)
                printf("   0x%llx   movf  %s, %s, %d\n", insstrs, rd, rs, cc);
            else if (((code>>16) & 0x3) == 1)
                printf("   0x%llx   movt  %s, %s, %d\n", insstrs, rd, rs, cc);
            else
                printf(" * MIPS illegal instruction(movci): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_SRL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if (((code>>21) & 0x1f) == 1)
                printf("   0x%llx   rotr  %s, %s, %d\n", insstrs, rd, rt, sa);
            else if (((code>>21) & 0x1f) == 0)
                printf("   0x%llx   srl  %s, %s, %d\n", insstrs, rd, rt, sa);
            else
                printf(" * MIPS illegal instruction(srl): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_SRA:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;
            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(sra): 0x%08x\n", code);
                return ;
            }
            printf("   0x%llx   sra  %s, %s, %d\n", insstrs, rd, rt, sa);
            return ;
            //break;
        }
        case MIPS_SPEC_SLLV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(sllv): 0x%08x\n", code);
                return ;
            }
            printf("   0x%llx   sllv  %s, %s, %s\n", insstrs, rd, rt, rs);
            return ;
            //break;
        }
        //case MIPS_SPEC_LSA:
        //{}
        case MIPS_SPEC_SRLV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];

            if (((code>>6) & 0x1f) == 0)
                printf("   0x%llx   srlv  %s, %s, %s\n", insstrs, rd, rt, rs);
            else if (((code>>6) & 0x1f) == 1)
                printf("   0x%llx   rotrv  %s, %s, %s\n", insstrs, rd, rt, rs);
            else
                printf(" * MIPS illegal instruction(srlv): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_SRAV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(srav): 0x%08x\n", code);
                return ;
            }
            printf("   0x%llx   srav  %s, %s, %s\n", insstrs, rd, rt, rs);
            return ;
            //break;
        }
        case MIPS_SPEC_JR:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            if (((code>>6) & 0x7fff) == 0)
                printf("   0x%llx   jr  %s\n", insstrs, rs);
            else if (((code>>6) & 0x7fff) == 0x10)
                printf("   0x%llx   jr.hb  %s\n", insstrs, rs);
            else
                printf(" * MIPS illegal instruction(jr): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_JALR:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int rd = (code>>11) & 0x1f;
            if ((code>>16) & 0x1f)
            {
                printf(" * MIPS illegal instruction(jalr-1): 0x%08x\n", code);
                return ;
            }

            if (((code>>6) & 0x1f) == 0)
            {
                if (rd != 31)
                {
                    printf("   0x%llx   jalr  %s, %s\n", insstrs, regName[rd], rs);
                }
                else
                {
                    printf("   0x%llx   jalr  %s\n", insstrs, rs);
                }
            }
            else if (((code>>6) & 0x1f) == 0x10)
            {
                if (rd != 31)
                {
                    printf("   0x%llx   jalr.hb  %s, %s\n", insstrs, regName[rd], rs);
                }
                else
                {
                    printf("   0x%llx   jalr.hb  %s\n", insstrs, rs);
                }
            }
            else
                printf(" * MIPS illegal instruction(jalr-2): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_MOVZ:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(movz): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   movz  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_MOVN:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(movn): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   movn  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_BREAK:
        {
            printf("   0x%llx   break\n", insstrs);

            return ;
            //break;
        }
        case MIPS_SPEC_SYNC:
        {
            unsigned int stype = (code>>6) & 0x1f;

            if ((code>>11) & 0x7fff)
            {
                printf(" * MIPS illegal instruction(sync): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   sync  %d\n", insstrs, stype);

            return ;
            //break;
        }
        case MIPS_SPEC_MFHI:
        {
            const char *rd = regName[(code>>11) & 0x1f];

            if (code & 0x03ff07c0)
            {
                printf(" * MIPS illegal instruction(mfhi): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mfhi  %s\n", insstrs, rd);

            return ;
            //break;
        }
        case MIPS_SPEC_MTHI:
        {
            const char *rs = regName[(code>>21) & 0x1f];

            if (code & 0x001fffc0)
            {
                printf(" * MIPS illegal instruction(mthi): 0x%08x\n", insstrs, code);
                return ;
            }

            printf("   0x%llx   mthi  %s\n", insstrs, rs);

            return ;
            //break;
        }
        case MIPS_SPEC_MFLO:
        {
            const char *rd = regName[(code>>11) & 0x1f];

            if (code & 0x03ff07c0)
            {
                printf(" * MIPS illegal instruction(mflo): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mflo  %s\n", insstrs, rd);

            return ;
            //break;
        }
        case MIPS_SPEC_MTLO:
        {
            const char *rs = regName[(code>>21) & 0x1f];

            if (code & 0x001fffc0)
            {
                printf(" * MIPS illegal instruction(mtlo): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mtlo  %s\n", insstrs, rs);

            return ;
            //break;
        }
        case MIPS_SPEC_DSLLV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsllv): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsllv  %s, %s, %s\n", insstrs, rd, rt, rs);

            return ;
            //break;
        }
        //case MIPS_SPEC_DLSA:
        case MIPS_SPEC_DSRLV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];

            if (((code>>6) & 0x1f) == 0)
                printf("   0x%llx   dsrlv  %s, %s, %s\n", insstrs, rd, rt, rs);
            else if (((code>>6) & 0x1f) == 1)
                printf("   0x%llx   drotrv  %s, %s, %s\n", insstrs, rd, rt, rs);
            else
                printf(" * MIPS illegal instruction(dsrlv): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_DSRAV:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsrav): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsrav  %s, %s, %s\n", insstrs, rd, rt, rs);

            return ;
            //break;
        }
        case MIPS_SPEC_MULT:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(mult): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mult  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_MULTU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(multu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   multu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DIV:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(div): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   div  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DIVU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(divu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   divu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DMULT:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(dmult): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dmult  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DMULTU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(dmultu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dmultu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DDIV:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(ddiv): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   ddiv  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DDIVU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x3ff)
            {
                printf(" * MIPS illegal instruction(ddivu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   ddivu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_ADD:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(add): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   add  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_ADDU:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(addu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   addu  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_SUB:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(sub): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   sub  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_SUBU:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(subu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   subu  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_AND:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(and): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   and  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_OR:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(or): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   or  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_XOR:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(xor): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   xor  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_NOR:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(nor): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   nor  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_SLT:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(slt): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   slt  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_SLTU:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(sltu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   sltu  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DADD:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dadd): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dadd  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DADDU:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(daddu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   daddu  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DSUB:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsub): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsub  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DSUBU:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsubu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsubu  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC_DSLL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsll): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsll  %s, %s, %d\n", insstrs, rd, rt, sa);

            return ;
            //break;
        }
        case MIPS_SPEC_DSRL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if (((code>>21) & 0x1f) == 0)
                printf("   0x%llx   dsrl  %s, %s, %d\n", insstrs, rd, rt, sa);
            else if (((code>>21) & 0x1f) == 1)
                printf("   0x%llx   drotr  %s, %s, %d\n", insstrs, rd, rt, sa);
            else
                printf(" * MIPS illegal instruction(dsrl): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_DSRA:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsra): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsra  %s, %s, %d\n", insstrs, rd, rt, sa);

            return ;
            //break;
        }
        case MIPS_SPEC_DSLL32:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsll32): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsll32  %s, %s, %d\n", insstrs, rd, rt, sa);

            return ;
            //break;
        }
        case MIPS_SPEC_DSRL32:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if (((code>>21) & 0x1f) == 0)
                printf("   0x%llx   dsrl32  %s, %s, %d\n", insstrs, rd, rt, sa);
            else if (((code>>21) & 0x1f) == 1)
                printf("   0x%llx   drotr32  %s, %s, %d\n", insstrs, rd, rt, sa);
            else
                printf(" * MIPS illegal instruction(dsrl32): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC_DSRA32:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int sa = (code>>6) & 0x1f;

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dsra32): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dsra32  %s, %s, %d\n", insstrs, rd, rt, sa);

            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(spec): 0x%08x\n", code);
            return ;
    }

Lalel_special2:
    //bits: 5-0.
    opcode = code & 0x3f;

    switch (opcode)
    {
        case MIPS_SPEC2_MUL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>6) & 0x1f)
            {
                printf(" * MIPS illegal instruction(mul): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mul  %s, %s, %s\n", insstrs, rd, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC2_MADD:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0xffff)
            {
                printf(" * MIPS illegal instruction(madd): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   madd  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC2_MADDU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0xffc0)
            {
                printf(" * MIPS illegal instruction(maddu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   maddu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC2_MSUB:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0xffc0)
            {
                printf(" * MIPS illegal instruction(msub): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   msub  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC2_MSUBU:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0xffc0)
            {
                printf(" * MIPS illegal instruction(msubu): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   msubu  %s, %s\n", insstrs, rs, rt);

            return ;
            //break;
        }
        case MIPS_SPEC2_CLZ:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            //const char *rt = regName[(code>>16) & 0x1f];

            if (((code>>11) & 0x1f) != ((code>>16) & 0x1f))
            {
                printf(" * MIPS illegal instruction: clz.\n");
                return ;
            }

            if (code & 0x7c0)
            {
                printf(" * MIPS illegal instruction(clz): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   clz  %s, %s\n", insstrs, rd, rs);

            return ;
            //break;
        }
        case MIPS_SPEC2_CLO:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            //const char *rt = regName[(code>>16) & 0x1f];

            if (((code>>11) & 0x1f) != ((code>>16) & 0x1f))
            {
                printf(" * MIPS illegal instruction: clo.\n");
                return ;
            }

            if (code & 0x7c0)
            {
                printf(" * MIPS illegal instruction(clo): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   clo  %s, %s\n", insstrs, rd, rs);

            return ;
            //break;
        }
        case MIPS_SPEC2_DCLZ:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            //const char *rt = regName[(code>>16) & 0x1f];

            if (((code>>11) & 0x1f) != ((code>>16) & 0x1f))
            {
                printf(" * MIPS illegal instruction: dclz.\n");
                return ;
            }

            if (code & 0x7c0)
            {
                printf(" * MIPS illegal instruction(dclz): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dclz  %s, %s\n", insstrs, rd, rs);

            return ;
            //break;
        }
        case MIPS_SPEC2_DCLO:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            //const char *rt = regName[(code>>16) & 0x1f];

            if (((code>>11) & 0x1f) != ((code>>16) & 0x1f))
            {
                printf(" * MIPS illegal instruction: dclo.\n");
                return ;
            }

            if (code & 0x7c0)
            {
                printf(" * MIPS illegal instruction(dclo): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dclo  %s, %s\n", insstrs, rd, rs);

            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(spec2): 0x%08x\n", code);
            return ;
    }

Lalel_special3:
    //bits: 5-0.
    opcode = code & 0x3f;

    switch (opcode)
    {
        case MIPS_SPEC3_EXT:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = (code>>11) & 0x1f;

            printf("   0x%llx   ext  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size+1);
            return ;
            //break;
        }
        case MIPS_SPEC3_DEXTM:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = (code>>11) & 0x1f;

            printf("   0x%llx   dextm  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size+33);
            return ;
            //break;
        }
        case MIPS_SPEC3_DEXTU:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = (code>>11) & 0x1f;

            printf("   0x%llx   dextu  %s, %s, %d, %d\n", insstrs, rt, rs, pos+32, size+1);
            return ;
            //break;
        }
        case MIPS_SPEC3_DEXT:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = (code>>11) & 0x1f;

            printf("   0x%llx   dext  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size+1);
            return ;
            //break;
        }
        case MIPS_SPEC3_INS:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = ((code>>11) & 0x1f) + 1 - pos;

            printf("   0x%llx   ins  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size);
            return ;
            //break;
        }
        case MIPS_SPEC3_DINSM:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = ((code>>11) & 0x1f) + 33 - pos;

            printf("   0x%llx   dinsm  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size);
            return ;
            //break;
        }
        case MIPS_SPEC3_DINSU:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = ((code>>6) & 0x1f) + 32;
            unsigned int size = ((code>>11) & 0x1f) + 33 - pos;

            printf("   0x%llx   dinsu  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size);
            return ;
            //break;
        }
        case MIPS_SPEC3_DINS:
        {
            const char *rt = regName[(code>>16) & 0x1f];
            const char *rs = regName[(code>>21) & 0x1f];
            unsigned int pos = (code>>6) & 0x1f;
            unsigned int size = ((code>>11) & 0x1f) + 1 - pos;

            printf("   0x%llx   dins  %s, %s, %d, %d\n", insstrs, rt, rs, pos, size);
            return ;
            //break;
        }
        case MIPS_SPEC3_DBSHFL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(dbshfl-1): 0x%08x\n", code);
                return ;
            }

            if ((code & 0x7c0) == 0x140)
                printf("   0x%llx   dshd  %s, %s\n", insstrs, rd, rt);
            else if ((code & 0x7c0) == 0x80)
                printf("   0x%llx   dsbh  %s, %s\n", insstrs, rd, rt);
            else
                printf(" * MIPS illegal instruction(dbshfl-2): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC3_BSHFL:
        {
            const char *rd = regName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if ((code>>21) & 0x1f)
            {
                printf(" * MIPS illegal instruction(bshfl-1): 0x%08x\n", code);
                return ;
            }

            if ((code & 0x7c0) == 0x080)
                printf("   0x%llx   wsbh  %s, %s\n", insstrs, rd, rt);
            else if ((code & 0x7c0) == 0x400)
                printf("   0x%llx   seb  %s, %s\n", insstrs, rd, rt);
            else if ((code & 0x7c0) == 0x600)
                printf("   0x%llx   seh  %s, %s\n", insstrs, rd, rt);
            else
                printf(" * MIPS illegal instruction(bshfl-2): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_SPEC3_RDHWR:
        {
            unsigned int rd = (code>>11) & 0x1f;
            const char *rt = regName[(code>>16) & 0x1f];

            if (((code>>21) & 0x1f) || ((code>>6) & 0x1f))
            {
                printf(" * MIPS illegal instruction(rdhwr): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   rdhwr  %s, $%d\n", insstrs, rt, rd);

            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(spec3): 0x%08x\n", code);
            return ;
    }

Lalel_regimm:
    //bits: 20-16.
    opcode = (code>>16) & 0x1f;

    switch (opcode)
    {
        case MIPS_REGIMM_BLTZAL:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            printf("   0x%llx   bltzal  %s, 0x%llx\n", insstrs, rs, (int64_t)insstrs + 4 + offset);

            return ;
            //break;
        }
        case MIPS_REGIMM_BGEZAL:
        {
            unsigned int rs = (code>>21) & 0x1f;
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            if (rs)
                printf("   0x%llx   bgezal  %s, 0x%llx\n", insstrs, regName[rs], (int64_t)insstrs + 4 + offset);
            else
                printf("   0x%llx   bal  0x%llx\n", insstrs, (int64_t)insstrs + 4 + offset);

            return ;
            //break;
        }
        case MIPS_REGIMM_BLTZ:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            printf("   0x%llx   bltz  %s, 0x%llx\n", insstrs, rs, (int64_t)insstrs + 4 + offset);

            return ;
            //break;
        }
        case MIPS_REGIMM_BGEZ:
        {
            const char *rs = regName[(code>>21) & 0x1f];
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            printf("   0x%llx   bgez  %s, 0x%llx\n", insstrs, rs, (int64_t)insstrs + 4 + offset);

            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(regimm): 0x%08x\n", code);
            return ;
    }

Lalel_cop1x:
    //bits: 5-0.
    opcode = code & 0x3f;

    switch (opcode)
    {
        case MIPS_COP1X_LWXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   lwxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_LDXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   ldxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_LUXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   luxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_SWXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   swxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_SDXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   sdxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_SUXC1:
        {
            const char *fd    = FregName[(code>>6) & 0x1f];
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   suxc1  %s,%s(%s)\n", insstrs, fd, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_PREFX:
        {
            unsigned int hint = (code>>11) & 0x1f;
            const char *index = regName[(code>>16) & 0x1f];
            const char *base  = regName[(code>>21) & 0x1f];
            printf("   0x%llx   prefx  %d,%s(%s)\n", insstrs, hint, index, base);
            return ;
            //break;
        }
        case MIPS_COP1X_MADD_S:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   madd.s  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_MADD_D:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   madd.d  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_MSUB_S:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   msub.s  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_MSUB_D:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   msub.d  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_NMADD_S:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   nmadd.s  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_NMADD_D:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   nmadd.d  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_NMSUB_S:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   nmsub.s  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }
        case MIPS_COP1X_NMSUB_D:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fr = FregName[(code>>21) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            printf("   0x%llx   nmsub.d  %s, %s, %s, %s\n", insstrs, fd, fr, fs, ft);
            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(cop1x): 0x%08x\n", code);
            return ;
    }

Lalel_cop1:
    //bits: 25-21.
    opcode = (code>>21) & 0x1f;

    switch (opcode)
    {
        case MIPS_COP1_MFC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(mfc1): 0x%08x\n", insstrs, code);
                return ;
            }

            printf("   0x%llx   mfc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_DMFC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(dmfc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dmfc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_CFC1:
        {
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(cfc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   cfc1  %s, $%d\n", insstrs, rt, (code>>11) & 0x1f);

            return ;
            //break;
        }
        case MIPS_COP1_MFHC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(mfhc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mfhc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_MTC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(mtc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mtc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_DMTC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(dmtc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   dmtc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_CTC1:
        {
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(ctc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   ctc1  %s, $%d\n", insstrs, rt, (code>>11) & 0x1f);

            return ;
            //break;
        }
        case MIPS_COP1_MTHC1:
        {
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];

            if (code & 0x7ff)
            {
                printf(" * MIPS illegal instruction(mthc1): 0x%08x\n", code);
                return ;
            }

            printf("   0x%llx   mthc1  %s, %s\n", insstrs, rt, fs);

            return ;
            //break;
        }
        case MIPS_COP1_BC:
        {
            unsigned int cc = (code>>18) & 0x7;
            short offset = (short) (code & 0xffff);
            offset <<= 2;

            if (((code>>16) & 0x3) == 0)
            {
                printf("   0x%llx   bc1f  fcc%d, 0x%llx\n", insstrs, cc, (int64_t)insstrs + 4 + offset);
                return ;
            }
            else if (((code>>16) & 0x3) == 1)
            {
                printf("   0x%llx   bc1t  fcc%d, 0x%llx\n", insstrs, cc, (int64_t)insstrs + 4 + offset);
                return ;
            }
            else
                printf(" * MIPS illegal instruction(bc1): 0x%08x\n", code);

            return ;
            //break;
        }
        case MIPS_COP1_FMTS:
        {
// clang-format off
            const char * const ccName[] = {"f", "un", "eq", "ueq", "olt", "ult",
            "ole", "ule", "sf", "ngle", "seq", "ngl", "lt", "nge", "le", "ngt"};
// clang-format on
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int cc = (code>>8) & 0x7;

            switch (code & 0x3f)
            {
                case 0://ADD
                    printf("   0x%llx   add.s  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 1://SUB
                    printf("   0x%llx   sub.s  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 2://MUL
                    printf("   0x%llx   mul.s  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 3://DIV
                    printf("   0x%llx   div.s  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 4://SQRT
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(sqrt.s): 0x%08x\n", insstrs, code);
                        return ;
                    }
                    printf("   0x%llx   sqrt.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 5://ABS
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(abs.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   abs.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 6://MOV
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(mov.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   mov.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 7://NEG
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(neg.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   neg.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 8://ROUND.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(round.l.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   round.l.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 9://TRUNC.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(trunc.l.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   trunc.l.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 10://CEIL.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(ceil.l.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   ceil.l.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 11://FLOOR.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(floor.l.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   floor.l.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 12://ROUND.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(round.w.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   round.w.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 13://TRUNC.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(trunc.w.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   trunc.w.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 14://CEIL.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(ceil.w.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   ceil.w.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 15://FLOOR.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(floor.w.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   floor.w.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x11://MOVF
                    if (((code>>16) & 0x3) == 0)
                    {
                        printf("   0x%llx   movf.s  %s, %s, fcc%d\n", insstrs, fd, fs, (code>>18) & 0x7);
                    }
                    else if (((code>>16) & 0x3) == 1)
                    {
                        printf("   0x%llx   movt.s  %s, %s, fcc%d\n", insstrs, fd, fs, (code>>18) & 0x7);
                    }
                    else
                    {
                        printf(" * MIPS illegal instruction(movf.s): 0x%08x\n", code);
                    }
                    return ;
                case 0x12://MOVZ
                    printf("   0x%llx   movz.s  %s, %s, %s\n", insstrs, fd, fs, rt);
                    return ;
                case 0x13://MOVN
                    printf("   0x%llx   movn.s  %s, %s, %s\n", insstrs, fd, fs, rt);
                    return ;
                case 0x15://RECIP
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(recip.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   recip.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x16://RSQRT
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(rsqrt.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   rsqrt.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x21://CVT.D
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.d.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.d.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x24://CVT.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.w.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.w.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x25://CVT.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.l.s): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.l.s  %s, %s\n", insstrs, fd, fs);
                    return ;
                default:
                    if (((code & 0x30) == 0x30) && !(code & 0xc0))
                    {
                        printf("   0x%llx   c.%s.s  fcc%d, %s, %s\n", insstrs, ccName[code & 0xf], cc, fs, ft);
                        return ;
                    }
                    printf(" * MIPS illegal instruction(fmts): 0x%08x\n", code);
            }

            return ;
            //break;
        }
        case MIPS_COP1_FMTD:
        {
// clang-format off
            const char * const ccName[] = {"f", "un", "eq", "ueq", "olt", "ult",
            "ole", "ule", "sf", "ngle", "seq", "ngl", "lt", "nge", "le", "ngt"};
// clang-format on
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];
            const char *ft = FregName[(code>>16) & 0x1f];
            const char *rt = regName[(code>>16) & 0x1f];
            unsigned int cc = (code>>8) & 0x7;

            switch (code & 0x3f)
            {
                case 0://ADD
                    printf("   0x%llx   add.d  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 1://SUB
                    printf("   0x%llx   sub.d  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 2://MUL
                    printf("   0x%llx   mul.d  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 3://DIV
                    printf("   0x%llx   div.d  %s, %s, %s\n", insstrs, fd, fs, ft);
                    return ;
                case 4://SQRT
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(sqrt.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   sqrt.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 5://ABS
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(abs.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   abs.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 6://MOV
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(mov.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   mov.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 7://NEG
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(neg.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   neg.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 8://ROUND.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(round.l.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   round.l.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 9://TRUNC.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(trunc.l.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   trunc.l.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 10://CEIL.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(ceil.l.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   ceil.l.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 11://FLOOR.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(floor.l.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   floor.l.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 12://ROUND.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(round.w.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   round.w.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 13://TRUNC.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(trunc.w.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   trunc.w.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 14://CEIL.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(ceil.w.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   ceil.w.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 15://FLOOR.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(floor.w.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   floor.w.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x11://MOVF
                    if (((code>>16) & 0x3) == 0)
                    {
                        printf("   0x%llx   movf.d  %s, %s, fcc%d\n", insstrs, fd, fs, (code>>18) & 0x7);
                    }
                    else if (((code>>16) & 0x3) == 1)
                    {
                        printf("   0x%llx   movt.d  %s, %s, fcc%d\n", insstrs, fd, fs, (code>>18) & 0x7);
                    }
                    else
                    {
                        printf(" * MIPS illegal instruction(movf.d): 0x%08x\n", code);
                    }
                    return ;
                case 0x12://MOVZ
                    printf("   0x%llx   movz.d  %s, %s, %s\n", insstrs, fd, fs, rt);
                    return ;
                case 0x13://MOVN
                    printf("   0x%llx   movn.d  %s, %s, %s\n", insstrs, fd, fs, rt);
                    return ;
                case 0x15://RECIP
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(recip.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   recip.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x16://RSQRT
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(rsqrt.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   rsqrt.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x20://CVT.S
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.s.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.s.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x24://CVT.W
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.w.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.w.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x25://CVT.L
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.l.d): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.l.d  %s, %s\n", insstrs, fd, fs);
                    return ;
                default:
                    if (((code & 0x30) == 0x30) && !(code & 0xc0))
                    {
                        printf("   0x%llx   c.%s.d  fcc%d, %s, %s\n", insstrs, ccName[code & 0xf], cc, fs, ft);
                        return ;
                    }
                    printf(" * MIPS illegal instruction(fmts): 0x%08x\n", code);
            }

            return ;
            //break;
        }
        case MIPS_COP1_FMTW:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];

            switch (code & 0x3f)
            {
                case 0x20://CVT.S
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.s.w): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.s.w  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x21://CVT.D
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.d.w): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.d.w  %s, %s\n", insstrs, fd, fs);
                    return ;
                default:
                    printf(" * MIPS illegal instruction(fmtsW): 0x%08x\n", code);
            }

            return ;
            //break;
        }
        case MIPS_COP1_FMTL:
        {
            const char *fd = FregName[(code>>6) & 0x1f];
            const char *fs = FregName[(code>>11) & 0x1f];

            switch (code & 0x3f)
            {
                case 0x20://CVT.S
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.s.l): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.s.l  %s, %s\n", insstrs, fd, fs);
                    return ;
                case 0x21://CVT.D
                    if ((code>>16) & 0x1f)
                    {
                        printf(" * MIPS illegal instruction(cvt.d.l): 0x%08x\n", code);
                        return ;
                    }
                    printf("   0x%llx   cvt.d.l  %s, %s\n", insstrs, fd, fs);
                    return ;
                default:
                    printf(" * MIPS illegal instruction(fmtsL): 0x%08x\n", code);
            }

            return ;
            //break;
        }

        default :
            printf(" * MIPS illegal instruction(cop1): 0x%08x\n", code);
            return ;
    }

    return ;
}

#if 0
/*****************************************************************************
 *
 *  Display the instruction name
 */
void emitter::emitDispInst(instruction ins)
{
    const char* insstr = codeGen->genInsName(ins);
    size_t      len    = strlen(insstr);

    /* Display the instruction name */

    printf("%s", insstr);

    //
    // Add at least one space after the instruction name
    // and add spaces until we have reach the normal size of 8
    do
    {
        printf(" ");
        len++;
    } while (len < 8);
}

/*****************************************************************************
 *
 *  Display an immediate value
 */
void emitter::emitDispImm(ssize_t imm, bool addComma, bool alwaysHex /* =false */)
{
    if (strictMIPSAsm)
    {
        printf("#");
    }

    // Munge any pointers if we want diff-able disassembly.
    // Since some may be emitted as partial words, print as diffable anything that has
    // significant bits beyond the lowest 8-bits.
    if (emitComp->opts.disDiffable)
    {
        ssize_t top56bits = (imm >> 8);
        if ((top56bits != 0) && (top56bits != -1))
            imm = 0xD1FFAB1E;
    }

    if (!alwaysHex && (imm > -1000) && (imm < 1000))
    {
        printf("%d", imm);
    }
    else
    {
        if ((imm < 0) && ((imm & 0xFFFFFFFF00000000LL) == 0xFFFFFFFF00000000LL))
        {
            printf("-");
            imm = -imm;
        }

        if ((imm & 0xFFFFFFFF00000000LL) != 0)
        {
            printf("0x%llx", imm);
        }
        else
        {
            printf("0x%02x", imm);
        }
    }

    if (addComma)
        printf(", ");
}

/*****************************************************************************
 *
 *  Display a float zero constant
 */
void emitter::emitDispFloatZero()
{
assert(!"unimplemented on MIPS yet");
#if 0
    if (strictMIPSAsm)
    {
        printf("#");
    }
    printf("0.0");
#endif
}

/*****************************************************************************
 *
 *  Display an encoded float constant value
 */
void emitter::emitDispFloatImm(ssize_t imm8)
{
assert(!"unimplemented on MIPS yet");
#if 0
    assert((0 <= imm8) && (imm8 <= 0x0ff));
    if (strictMIPSAsm)
    {
        printf("#");
    }

    floatImm8 fpImm;
    fpImm.immFPIVal = (unsigned)imm8;
    double result   = emitDecodeFloatImm8(fpImm);

    printf("%.4f", result);
#endif
}

/*****************************************************************************
 *
 *  Display an MIPS64 'barrier' for the memory barrier instructions
 */
void emitter::emitDispBarrier(insBarrier barrier)
{
    printf("sync");
}

/*****************************************************************************
 *
 *  Prints the encoding for the Shift Type encoding
 */

void emitter::emitDispShiftOpts(insOpts opt)
{
assert(!"unimplemented on MIPS yet");
        assert(!"Bad value");
}

/*****************************************************************************
 *
 *  Prints the encoding for the Extend Type encoding
 */

void emitter::emitDispExtendOpts(insOpts opt)
{
assert(!"unimplemented on MIPS yet");
        assert(!"Bad value");
}
#endif

#if 0
/*****************************************************************************
 *
 *  Prints the encoding for the Extend Type encoding in loads/stores
 */

void emitter::emitDispLSExtendOpts(insOpts opt)
{
assert(!"unimplemented on MIPS yet");
    if (opt == INS_OPTS_LSL)
        printf("LSL");
    else if (opt == INS_OPTS_UXTW)
        printf("UXTW");
    else if (opt == INS_OPTS_UXTX)
        printf("UXTX");
    else if (opt == INS_OPTS_SXTW)
        printf("SXTW");
    else if (opt == INS_OPTS_SXTX)
        printf("SXTX");
    else
        assert(!"Bad value");
}

/*****************************************************************************
 *
 *  Display a register
 */
void emitter::emitDispReg(regNumber reg, emitAttr attr, bool addComma)
{
    emitAttr size = EA_SIZE(attr);
    printf(emitRegName(reg, size));

    if (addComma)
        printf(", ");
}

/*****************************************************************************
 *
 *  Display a vector register with an arrangement suffix
 */
void emitter::emitDispVectorReg(regNumber reg, insOpts opt, bool addComma)
{
    assert(isVectorRegister(reg));
    printf(emitVectorRegName(reg));
    emitDispArrangement(opt);

    if (addComma)
        printf(", ");
}

/*****************************************************************************
 *
 *  Display an vector register index suffix
 */
void emitter::emitDispVectorRegIndex(regNumber reg, emitAttr elemsize, ssize_t index, bool addComma)
{
assert(!"unimplemented on MIPS yet");
    assert(isVectorRegister(reg));
    printf(emitVectorRegName(reg));

    switch (elemsize)
    {
        case EA_1BYTE:
            printf(".b");
            break;
        case EA_2BYTE:
            printf(".h");
            break;
        case EA_4BYTE:
            printf(".s");
            break;
        case EA_8BYTE:
            printf(".d");
            break;
        default:
            assert(!"invalid elemsize");
            break;
    }

    printf("[%d]", index);

    if (addComma)
        printf(", ");
}

/*****************************************************************************
 *
 *  Display an arrangement suffix
 */
void emitter::emitDispArrangement(insOpts opt)
{
    const char* str = "???";

    switch (opt)
    {
        case INS_OPTS_8B:
            str = "8b";
            break;
        case INS_OPTS_16B:
            str = "16b";
            break;
        case INS_OPTS_4H:
            str = "4h";
            break;
        case INS_OPTS_8H:
            str = "8h";
            break;
        case INS_OPTS_2S:
            str = "2s";
            break;
        case INS_OPTS_4S:
            str = "4s";
            break;
        case INS_OPTS_1D:
            str = "1d";
            break;
        case INS_OPTS_2D:
            str = "2d";
            break;

        default:
            assert(!"Invalid insOpt for vector register");
    }
    printf(".");
    printf(str);
}

/*****************************************************************************
 *
 *  Display a register with an optional shift operation
 */
void emitter::emitDispShiftedReg(regNumber reg, insOpts opt, ssize_t imm, emitAttr attr)
{
assert(!"unimplemented on MIPS yet");
    emitAttr size = EA_SIZE(attr);
    assert((imm & 0x003F) == imm);
    assert(((imm & 0x0020) == 0) || (size == EA_8BYTE));

    printf(emitRegName(reg, size));

    if (imm > 0)
    {
        if (strictMIPSAsm)
        {
            printf(",");
        }
        emitDispShiftOpts(opt);
        emitDispImm(imm, false);
    }
}
#endif

#if 0
/*****************************************************************************
 *
 *  Display a register with an optional extend and scale operations
 */
void emitter::emitDispExtendReg(regNumber reg, insOpts opt, ssize_t imm)
{
assert(!"unimplemented on MIPS yet");
    assert((imm >= 0) && (imm <= 4));
    assert(insOptsNone(opt) || insOptsAnyExtend(opt) || (opt == INS_OPTS_LSL));

    // size is based on the extend option, not the instr size.
    emitAttr size = insOpts32BitExtend(opt) ? EA_4BYTE : EA_8BYTE;

    if (strictMIPSAsm)
    {
        if (insOptsNone(opt))
        {
            emitDispReg(reg, size, false);
        }
        else
        {
            emitDispReg(reg, size, true);
            if (opt == INS_OPTS_LSL)
                printf("LSL");
            else
                emitDispExtendOpts(opt);
            if ((imm > 0) || (opt == INS_OPTS_LSL))
            {
                printf(" ");
                emitDispImm(imm, false);
            }
        }
    }
    else // !strictMIPSAsm
    {
        if (insOptsNone(opt))
        {
            emitDispReg(reg, size, false);
        }
        else
        {
            if (opt != INS_OPTS_LSL)
            {
                emitDispExtendOpts(opt);
                printf("(");
                emitDispReg(reg, size, false);
                printf(")");
            }
        }
        if (imm > 0)
        {
            printf("*");
            emitDispImm(ssize_t{1} << imm, false);
        }
    }
}
#endif

/*****************************************************************************
 *
 *  Display an addressing operand [reg + imm]
 */
#if 0
void emitter::emitDispAddrRI(regNumber reg, insOpts opt, ssize_t imm)
{
    if (strictMIPSAsm)
    {
        printf("[");

        emitDispReg(reg, EA_8BYTE, false);

        if (!insOptsPostIndex(opt) && (imm != 0))
        {
            printf(",");
            emitDispImm(imm, false);
        }
        printf("]");

        if (insOptsPreIndex(opt))
        {
            printf("!");
        }
        else if (insOptsPostIndex(opt))
        {
            printf(",");
            emitDispImm(imm, false);
        }
    }
    else // !strictMIPSAsm
    {
        printf("[");

        const char* operStr = "++";
        if (imm < 0)
        {
            operStr = "--";
            imm     = -imm;
        }

        if (insOptsPreIndex(opt))
        {
            printf(operStr);
        }

        emitDispReg(reg, EA_8BYTE, false);

        if (insOptsPostIndex(opt))
        {
            printf(operStr);
        }

        if (insOptsIndexed(opt))
        {
            printf(", ");
        }
        else
        {
            printf("%c", operStr[1]);
        }
        emitDispImm(imm, false);
        printf("]");
    }
}
#endif

#if 0
/*****************************************************************************
 *
 *  Display an addressing operand [reg + extended reg]
 */
void emitter::emitDispAddrRRExt(regNumber reg1, regNumber reg2, insOpts opt, bool isScaled, emitAttr size)
{
assert(!"unimplemented on MIPS yet");
    unsigned scale = 0;
    if (isScaled)
    {
        scale = NaturalScale_helper(size);
    }

    printf("[");

    if (strictMIPSAsm)
    {
        emitDispReg(reg1, EA_8BYTE, true);
        emitDispExtendReg(reg2, opt, scale);
    }
    else // !strictMIPSAsm
    {
        emitDispReg(reg1, EA_8BYTE, false);
        printf("+");
        emitDispExtendReg(reg2, opt, scale);
    }

    printf("]");
}
#endif

/*****************************************************************************
 *
 *  Display (optionally) the instruction encoding in hex
 */

void emitter::emitDispInsHex(instrDesc* id, BYTE* code, size_t sz)
{
    // We do not display the instruction hex if we want diff-able disassembly
    if (!emitComp->opts.disDiffable)
    {
        if (sz == 4)
        {
            printf("  %08X    ", (*((code_t*)code)));
        }
        else
        {
            assert(sz == 0);
            printf("              ");
        }
    }
}

/****************************************************************************
 *
 *  Display the given instruction.
 */

void emitter::emitDispIns(
    instrDesc* id, bool isNew, bool doffs, bool asmfm, unsigned offset, BYTE* pCode, size_t sz, insGroup* ig)
{//not used on mips64.
    printf("------------not implements emitDispIns() for mips64!!!\n");
}

/*****************************************************************************
 *
 *  Display a stack frame reference.
 */

void emitter::emitDispFrameRef(int varx, int disp, int offs, bool asmfm)
{
    printf("[");

    if (varx < 0)
        printf("TEMP_%02u", -varx);
    else
        emitComp->gtDispLclVar(+varx, false);

    if (disp < 0)
        printf("-0x%02x", -disp);
    else if (disp > 0)
        printf("+0x%02x", +disp);

    printf("]");

    if (varx >= 0 && emitComp->opts.varNames)
    {
        LclVarDsc*  varDsc;
        const char* varName;

        assert((unsigned)varx < emitComp->lvaCount);
        varDsc  = emitComp->lvaTable + varx;
        varName = emitComp->compLocalVarName(varx, offs);

        if (varName)
        {
            printf("'%s", varName);

            if (disp < 0)
                printf("-%d", -disp);
            else if (disp > 0)
                printf("+%d", +disp);

            printf("'");
        }
    }
}

#endif // DEBUG

// Generate code for a load or store operation with a potentially complex addressing mode
// This method handles the case of a GT_IND with contained GT_LEA op1 of the x86 form [base + index*sccale + offset]
// Since MIPS64 does not directly support this complex of an addressing mode
// we may generates up to three instructions for this for MIPS64
//
void emitter::emitInsLoadStoreOp(instruction ins, emitAttr attr, regNumber dataReg, GenTreeIndir* indir)
{
    GenTree* addr = indir->Addr();

    if (addr->isContained())
    {
        assert(addr->OperGet() == GT_LCL_VAR_ADDR || addr->OperGet() == GT_LEA);

        int offset = 0;
        DWORD lsl = 0;

        if (addr->OperGet() == GT_LEA)
        {
            offset = addr->AsAddrMode()->Offset();
            if (addr->AsAddrMode()->gtScale > 0)
            {
                assert(isPow2(addr->AsAddrMode()->gtScale));
                BitScanForward(&lsl, addr->AsAddrMode()->gtScale);
            }
        }

        GenTree* memBase = indir->Base();
        emitAttr addType = varTypeIsGC(memBase) ? EA_BYREF : EA_PTRSIZE;

        if (indir->HasIndex())
        {
            GenTree* index = indir->Index();

            if (offset != 0)
            {
                regNumber tmpReg = indir->GetSingleTempReg();

                if (emitIns_valid_imm_for_add(offset, EA_8BYTE))
                {
                    if (lsl > 0)
                    {
                        // Generate code to set tmpReg = base + index*scale
                        emitIns_R_R_I(INS_dsll, attr, REG_AT, index->gtRegNum, lsl);
                        emitIns_R_R_R(INS_daddu, addType, tmpReg, memBase->gtRegNum, REG_AT);
                    }
                    else // no scale
                    {
                        // Generate code to set tmpReg = base + index
                        emitIns_R_R_R(INS_daddu, addType, tmpReg, memBase->gtRegNum, index->gtRegNum);
                    }

                    noway_assert(emitInsIsLoad(ins) || (tmpReg != dataReg));

                    // Then load/store dataReg from/to [tmpReg + offset]
                    emitIns_R_R_I(ins, attr, dataReg, tmpReg, offset);
                }
                else // large offset
                {
                    // First load/store tmpReg with the large offset constant
                    codeGen->instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, offset);
                    // Then add the base register
                    //      rd = rd + base
                    emitIns_R_R_R(INS_daddu, addType, tmpReg, tmpReg, memBase->gtRegNum);

                    noway_assert(emitInsIsLoad(ins) || (tmpReg != dataReg));
                    noway_assert(tmpReg != index->gtRegNum);

                    // Then load/store dataReg from/to [tmpReg + index*scale]
                    emitIns_R_R_I(INS_dsll, attr, REG_AT, index->gtRegNum, lsl);
                    emitIns_R_R_R(INS_daddu, addType, tmpReg, tmpReg, REG_AT);
                    emitIns_R_R_I(ins, attr, dataReg, tmpReg, 0);
                }
            }
            else // (offset == 0)
            {
                if (lsl > 0)
                {
                    // Then load/store dataReg from/to [memBase + index*scale]
                    emitIns_R_R_I(INS_dsll, attr, REG_AT, index->gtRegNum, lsl);
                    emitIns_R_R_R(INS_daddu, addType, REG_AT, memBase->gtRegNum, REG_AT);
                    emitIns_R_R_I(ins, attr, dataReg, REG_AT, 0);
                }
                else // no scale
                {
                    // Then load/store dataReg from/to [memBase + index]
                    emitIns_R_R_R(INS_daddu, addType, REG_AT, memBase->gtRegNum, index->gtRegNum);
                    emitIns_R_R_I(ins, attr, dataReg, REG_AT, 0);
                }
            }
        }
        else // no Index register
        {
            if (emitIns_valid_imm_for_ldst_offset(offset, emitTypeSize(indir->TypeGet())))
            {
                // Then load/store dataReg from/to [memBase + offset]
                emitIns_R_R_I(ins, attr, dataReg, memBase->gtRegNum, offset);
            }
            else
            {
                // We require a tmpReg to hold the offset
                regNumber tmpReg = indir->GetSingleTempReg();

                // First load/store tmpReg with the large offset constant
                codeGen->instGen_Set_Reg_To_Imm(EA_PTRSIZE, tmpReg, offset);

                // Then load/store dataReg from/to [memBase + tmpReg]
                emitIns_R_R_R(INS_daddu, addType, tmpReg, memBase->gtRegNum, tmpReg);
                emitIns_R_R_I(ins, attr, dataReg, tmpReg, 0);
            }
        }
    }
    else // addr is not contained, so we evaluate it into a register
    {
        // Then load/store dataReg from/to [addrReg]
        emitIns_R_R_I(ins, attr, dataReg, addr->gtRegNum, 0);
    }
}

// The callee must call genConsumeReg() for any non-contained srcs
// and genProduceReg() for any non-contained dsts.

regNumber emitter::emitInsBinary(instruction ins, emitAttr attr, GenTree* dst, GenTree* src)
{
    assert(!"unimplemented on MIPS yet");
    return dst->gtRegNum;
}

// The callee must call genConsumeReg() for any non-contained srcs
// and genProduceReg() for any non-contained dsts.

regNumber emitter::emitInsTernary(instruction ins, emitAttr attr, GenTree* dst, GenTree* src1, GenTree* src2)
{
    /* FIXME for MIPS: should amend. */
    // dst can only be a reg
    assert(!dst->isContained());

    // find immed (if any) - it cannot be a dst
    // Only one src can be an int.
    GenTreeIntConCommon* intConst  = nullptr;
    GenTree*             nonIntReg = nullptr;

    bool needCheckOv = dst->gtOverflowEx();

    if (varTypeIsFloating(dst))
    {
        // src1 can only be a reg
        assert(!src1->isContained());
        // src2 can only be a reg
        assert(!src2->isContained());
    }
    else // not floating point
    {
        // src2 can be immed or reg
        assert(!src2->isContained() || src2->isContainedIntOrIImmed());

        // Check src2 first as we can always allow it to be a contained immediate
        if (src2->isContainedIntOrIImmed())
        {
            intConst  = src2->AsIntConCommon();
            nonIntReg = src1;
        }
        // Only for commutative operations do we check src1 and allow it to be a contained immediate
        else if (dst->OperIsCommutative())
        {
            // src1 can be immed or reg
            assert(!src1->isContained() || src1->isContainedIntOrIImmed());

            // Check src1 and allow it to be a contained immediate
            if (src1->isContainedIntOrIImmed())
            {
                assert(!src2->isContainedIntOrIImmed());
                intConst  = src1->AsIntConCommon();
                nonIntReg = src2;
            }
        }
        else
        {
            // src1 can only be a reg
            assert(!src1->isContained());
        }
    }

    if (needCheckOv)
    {
        if (ins == INS_daddu)
        {
            assert(attr == EA_8BYTE);
            //ins = INS_daddu;//condition ?
        }
        else if (ins == INS_addu)// || ins == INS_add
        {
            assert(attr == EA_4BYTE);
            //ins = INS_addu;//condition ?
        }
        else if (ins == INS_daddiu)
        {
            assert(intConst != nullptr);
        }
        else if (ins == INS_addiu)
        {
            assert(intConst != nullptr);
        }
        else if (ins == INS_dsubu)
        {
            assert(attr == EA_8BYTE);
            //ins = INS_dsubu;//condition ?
        }
        else if (ins == INS_subu)
        {
            assert(attr == EA_4BYTE);
            //ins = INS_subu;//condition ?
        }
        else if ((ins == INS_dmult) || (ins == INS_dmultu))
        {
            assert(attr == EA_8BYTE);
            //NOTE: overflow format doesn't support an int constant operand directly.
            assert(intConst == nullptr);
        }
        else if ((ins == INS_mult) || (ins == INS_multu))
        {
            assert(attr == EA_4BYTE);
            //NOTE: overflow format doesn't support an int constant operand directly.
            assert(intConst == nullptr);
        }
        else
        {
#ifdef DEBUG
            printf("Invalid ins for overflow check: %s\n", codeGen->genInsName(ins));
#endif
            assert(!"Invalid ins for overflow check");
        }
    }

    if (intConst != nullptr)
    {//should re-design this case!!! ---2020.04.11.
        ssize_t imm = intConst->IconValue();
        if (ins == INS_andi || ins == INS_ori || ins == INS_xori)
            assert((0 <= imm) && (imm <= 0xffff));
            //assert((-32768 <= imm) && (imm <= 0xffff));
        else
            assert((-32769 < imm) && (imm < 32768));

        if (ins == INS_dsub)
        {
            assert(attr == EA_8BYTE);
            assert(imm != -32768);
            ins = INS_daddiu;
            imm = -imm;
        }
        else if (ins == INS_sub)
        {
            assert(attr == EA_4BYTE);
            assert(imm != -32768);
            ins = INS_addiu;
            imm = -imm;
        }

        assert(ins == INS_daddiu || ins == INS_addiu || ins == INS_andi || ins == INS_ori || ins == INS_xori);

        if ((ins == INS_andi || ins == INS_ori) && imm < 0)
        {
            /*FIXME for MIPS:  Here will sign-extend imm when imm less than 0 ! */
#ifdef DEBUG
            if (ins == INS_andi)
                printf("[MIPS64] Here will sign-extend imm for \"andi\" when imm(0x%x) less than 0 !!!\n", imm);
            else if (ins == INS_ori)
                printf("[MIPS64] Here will sign-extend imm for \"ori\" when imm(0x%x) less than 0 !!!\n", imm);
#endif

            //assert(-32769 < imm);
            assert(attr == EA_8BYTE || attr == EA_4BYTE);
            assert(nonIntReg->gtRegNum != REG_AT);

            emitIns_R_R_I(INS_daddiu, EA_8BYTE, REG_AT, REG_R0, imm);

            if (ins == INS_andi)
                emitIns_R_R_R(INS_and, attr, dst->gtRegNum, REG_AT, nonIntReg->gtRegNum);
            else// if (ins == INS_ori)
                emitIns_R_R_R(INS_or, attr, dst->gtRegNum, REG_AT, nonIntReg->gtRegNum);

            goto L_Done;
        }
        else if (ins == INS_xori && imm < 0)
        {
            // FIXME for MIPS: Should sign-extend ?
#ifdef DEBUG
            printf("[MIPS64] Here do not sign-extend imm for \"xori\" when imm(0x%x) less than 0 !!!\n", imm);
#endif
            emitIns(INS_break);
        }

        if (needCheckOv)
        {
            emitIns_R_R_R(INS_or, attr, REG_AT, nonIntReg->gtRegNum, REG_R0);
        }

        emitIns_R_R_I(ins, attr, dst->gtRegNum, nonIntReg->gtRegNum, imm);

        if (needCheckOv)
        {
            if (ins == INS_daddiu || ins == INS_addiu)
            {
                // A = B + C
                if ((dst->gtFlags & GTF_UNSIGNED) != 0)
                {
                    //regNumber regs[] = {REG_AT, dst->gtRegNum};
                    emitIns_R_R_I(INS_sltiu, attr, REG_AT, dst->gtRegNum, imm);

                    ssize_t imm2 = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm2);
                    emitIns(INS_nop);

                    codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                    //emitIns(INS_nop);
                }
                else
                {
                    if (imm > 0)
                    {
                        // B > 0 and C > 0, if A < B, goto overflow
                        emitIns_R_R_R(INS_slt, attr, REG_AT, REG_R0, REG_AT);

                        //10/12=1+1+1+6/8 +1.
                        ssize_t imm2 = emitComp->fgUseThrowHelperBlocks() ? (10<<2) : (12<< 2);
                        emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm2);
                        emitIns(INS_nop);

                        emitIns_R_R_I(INS_slti, EA_PTRSIZE, REG_AT, dst->gtRegNum, imm);

                        imm2 = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                        emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm2);
                        emitIns(INS_nop);

                        codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                        //emitIns(INS_nop);
                    }
                    else if (imm < 0)
                    {
                        // B < 0 and C < 0, if A > B, goto overflow
                        emitIns_R_R_R(INS_slt, EA_PTRSIZE, REG_AT, REG_AT, REG_R0);

                        ///11/13=1+1+1+1+6/8 +1.
                        ssize_t imm2 = emitComp->fgUseThrowHelperBlocks() ? (11<<2) : (13<< 2);
                        emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm2);// skip overflow
                        emitIns(INS_nop);

                        emitIns_R_R_I(INS_daddiu, attr, REG_AT, REG_R0, imm);
                        emitIns_R_R_R(INS_slt, EA_PTRSIZE, REG_AT, REG_AT, dst->gtRegNum);

                        imm2 = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                        emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm2);// skip overflow
                        emitIns(INS_nop);

                        codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                        //emitIns(INS_nop);
                    }
                }
            }
            else
            {
                assert(!"unimplemented on MIPS yet");
            }
        }
    }
    else if (varTypeIsFloating(dst))
    {
        regNumber regs[] = {dst->gtRegNum, src1->gtRegNum, src2->gtRegNum};
        emitIns_R_R_R(ins, attr, dst->gtRegNum, src1->gtRegNum, src2->gtRegNum);
    }
    else if (dst->OperGet() == GT_MUL)
    {
        ssize_t imm;

        // n * n bytes will store n bytes result
        emitIns_R_R(ins, attr, src1->gtRegNum, src2->gtRegNum);
        emitIns_R(INS_mflo, attr, dst->gtRegNum);

        if (needCheckOv)
        {
            assert(REG_AT != dst->gtRegNum);
            assert(REG_AT != src1->gtRegNum);
            assert(REG_AT != src2->gtRegNum);

            if ((dst->gtFlags & GTF_UNSIGNED) != 0)
            {
                emitIns_R(INS_mfhi, attr, REG_AT);

                imm = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
                emitIns(INS_nop);

                codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                //emitIns(INS_nop);
            }
            else
            {
                emitIns_R(INS_mfhi, attr, REG_AT);
                emitIns_R_R_I(attr == EA_8BYTE ? INS_dsra32 : INS_sra, attr, REG_T0, dst->gtRegNum, 31);

                imm = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_T0, imm);
                emitIns(INS_nop);

                codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                //emitIns(INS_nop);
            }
        }
    }
    else if (dst->OperGet() == GT_DIV || dst->OperGet() == GT_UDIV || dst->OperGet() == GT_MOD || dst->OperGet() == GT_UMOD)
    {
        emitIns_R_R(ins, attr, src1->gtRegNum, src2->gtRegNum);

        if (dst->OperGet() == GT_DIV || dst->OperGet() == GT_UDIV)
            emitIns_R(INS_mflo, attr, dst->gtRegNum);
        else
            emitIns_R(INS_mfhi, attr, dst->gtRegNum);

        // There's no need to check for overflow here
    }
    else if (dst->OperGet() == GT_AND || dst->OperGet() == GT_OR || dst->OperGet() == GT_XOR)
    {
        emitIns_R_R_R(ins, attr, dst->gtRegNum, src1->gtRegNum, src2->gtRegNum);

        // MIPS needs to sign-extend dst when deal with 32bit data
        if (attr == EA_4BYTE)
             emitIns_R_R_R(INS_addu, attr, dst->gtRegNum, dst->gtRegNum, REG_R0);
    }
    else
    {
        regNumber saveOperReg1 = REG_NA;
        regNumber saveOperReg2 = REG_NA;

        if (needCheckOv)
        {
            assert(!varTypeIsFloating(dst));

            assert(REG_AT != dst->gtRegNum);
            assert(REG_AT != src1->gtRegNum);
            assert(REG_AT != src2->gtRegNum);

            if (dst->gtRegNum == src1->gtRegNum)
            {
                saveOperReg1 = REG_AT;
                saveOperReg2 = src2->gtRegNum;
                emitIns_R_R_R(INS_or, attr, REG_AT, src1->gtRegNum, REG_R0);
            }
            else if (dst->gtRegNum == src2->gtRegNum)
            {
                saveOperReg1 = src1->gtRegNum;
                saveOperReg2 = REG_AT;
                emitIns_R_R_R(INS_or, attr, REG_AT, src2->gtRegNum, REG_R0);
            }
            else
            {
                saveOperReg1 = src1->gtRegNum;
                saveOperReg2 = src2->gtRegNum;
            }
        }

        emitIns_R_R_R(ins, attr, dst->gtRegNum, src1->gtRegNum, src2->gtRegNum);

        if (needCheckOv)
        {
            if (dst->OperGet() == GT_ADD || dst->OperGet() == GT_SUB)
            {
                ssize_t imm;
                // ADD : A = B + C
                // SUB : C = A - B
                if ((dst->gtFlags & GTF_UNSIGNED) != 0)
                {
                    // if A < B, goto overflow
                    if (dst->OperGet() == GT_ADD)
                        emitIns_R_R_R(INS_sltu, attr, REG_AT, dst->gtRegNum, saveOperReg1);
                    else
                        emitIns_R_R_R(INS_sltu, attr, REG_AT, saveOperReg1, saveOperReg2);

                    imm = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);
                    emitIns(INS_nop);

                    codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                    //emitIns(INS_nop);
                }
                else
                {
                    regNumber tempReg1 = REG_RA;
                    regNumber tempReg2 = dst->GetSingleTempReg();
                    assert(tempReg1 != tempReg2);
                    assert(tempReg1 != saveOperReg1);
                    assert(tempReg2 != saveOperReg2);

                    if (dst->OperGet() == GT_ADD)
                        emitIns_R_R_I(attr == EA_4BYTE ? INS_dsrl : INS_dsrl32, attr, tempReg1, saveOperReg1, 31);
                    else
                        emitIns_R_R_I(attr == EA_4BYTE ? INS_dsrl : INS_dsrl32, attr, tempReg1, dst->gtRegNum, 31);
                    emitIns_R_R_I(attr == EA_4BYTE ? INS_dsrl : INS_dsrl32, attr, tempReg2, saveOperReg2, 31);

                    emitIns_R_R_R(INS_xor, attr, tempReg1, tempReg1, tempReg2);
                    if (attr == EA_4BYTE)
                    {
                        imm = 1;
                        emitIns_R_R_I(INS_andi, attr, tempReg1, tempReg1, imm);
                        emitIns_R_R_I(INS_andi, attr, tempReg2, tempReg2, imm);
                    }
                    // if (B > 0 && C < 0) || (B < 0  && C > 0), skip overflow
                    //17/19=1+1+1+1+1+1+1+1+1+1+6/8 +1.
                    imm = emitComp->fgUseThrowHelperBlocks() ? (17<<2) : (19<< 2);
                    emitIns_R_R_I(INS_bne, attr, tempReg1, REG_R0, imm);// skip overflow
                    emitIns(INS_nop);

                    imm = 6 << 2;
                    emitIns_R_R_I(INS_bne, attr, tempReg2, REG_R0, imm);// goto the judgement when B < 0 and C < 0
                    emitIns(INS_nop);
                    // B > 0 and C > 0, if A < B, goto overflow
                    emitIns_R_R_R(INS_slt, EA_PTRSIZE, REG_AT,
                                    dst->OperGet() == GT_ADD ? dst->gtRegNum : saveOperReg1,
                                    dst->OperGet() == GT_ADD ? saveOperReg1  : saveOperReg2);

                    //12/14=1+1+1+1+1+6/8 +1.
                    imm = emitComp->fgUseThrowHelperBlocks() ? (12<<2) : (14<< 2);
                    emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);// skip overflow
                    emitIns(INS_nop);

                    imm = 4 << 2;
                    emitIns_I(INS_b, EA_PTRSIZE, imm); // goto overflow
                    emitIns(INS_nop);

                    // B < 0 and C < 0, if A > B, goto overflow
                    emitIns_R_R_R(INS_slt, EA_PTRSIZE, REG_AT,
                                    dst->OperGet() == GT_ADD ? saveOperReg1  : saveOperReg2,
                                    dst->OperGet() == GT_ADD ? dst->gtRegNum : saveOperReg1);

                    imm = emitComp->fgUseThrowHelperBlocks() ? (7<<2) : (9 << 2);
                    emitIns_R_R_I(INS_beq, EA_PTRSIZE, REG_AT, REG_R0, imm);// skip overflow
                    emitIns(INS_nop);

                    codeGen->genJumpToThrowHlpBlk(EJ_jmp, SCK_OVERFLOW); //EJ_jmp is 6/8-ins.
                    //emitIns(INS_nop);
                }
            }
            else
            {
#ifdef DEBUG
                printf("---------[MIPS64]-NOTE: UnsignedOverflow instruction %d\n", ins);
#endif
                assert(!"unimplemented on MIPS yet");
            }
        }
    }

L_Done:

    return dst->gtRegNum;
}

#endif // defined(_TARGET_MIPS64_)
