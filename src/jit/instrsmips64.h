// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Copyright (c) Loongson Technology. All rights reserved.

/*****************************************************************************
 *  Mips64 instructions for JIT compiler
 *
 *          id      -- the enum name for the instruction
 *          nm      -- textual name (for assembly dipslay)
 *          fp      -- floating point instruction
 *          ld/st/cmp   -- load/store/compare instruction
 *          fmt     -- encoding format used by this instruction
 *          e1      -- encoding 1
 *          e2      -- encoding 2
 *          e3      -- encoding 3
 *          e4      -- encoding 4
 *          e5      -- encoding 5
 *
******************************************************************************/

#if !defined(_TARGET_MIPS64_)
#error Unexpected target type
#endif

#ifndef INSTS
#error INSTS must be defined before including this file.
#endif

/*****************************************************************************/
/*               The following is MIPS64-specific                               */
/*****************************************************************************/

// If you're adding a new instruction:
// You need not only to fill in one of these macros describing the instruction, but also:
//   * If the instruction writes to more than one destination register, update the function
//     emitInsMayWriteMultipleRegs in emitMips64.cpp.

// clang-format off
INSTS(invalid, "INVALID", 0, 0, IF_NONE,  BAD_CODE)

//    enum     name     FP LD/ST   FMT   ENCODE
INSTS(mov,     "mov",    0, 0, IF_FUNCS_6,      0x0000002d)
      //  mov     rt,rs
      //NOTE: On mips, usually it's name is move, but here for compatible using mov.
      //      In fact, mov is an alias commond, "daddu rt,rs,zero"

INSTS(nop,             "nop",          0, 0, IF_FMT_FUNC, 0x00000000)
INSTS(ehb,             "ehb",          0, 0, IF_FMT_FUNC, 0x000000c0)
INSTS(pause,           "pause",        0, 0, IF_FMT_FUNC, 0x00000140)

//    enum:id          name           FP LD/ST   Formate        ENCODE
INSTS(abs_s,           "abs.s",        0, 0, IF_FMT_FUNCS_16, 0x46000005)
INSTS(abs_d,           "abs.d",        0, 0, IF_FMT_FUNCS_16, 0x46200005)
INSTS(add,             "add",          0, 0, IF_FUNCS_6,      0x00000020)
INSTS(add_s,           "add.s",        0, 0, IF_FMT_FUNC,     0x46000000)
INSTS(add_d,           "add.d",        0, 0, IF_FMT_FUNC,     0x46200000)
INSTS(addi,            "addi",         0, 0, IF_OPCODE,       0x20000000)
INSTS(addiu,           "addiu",        0, 0, IF_OPCODE,       0x24000000)
INSTS(addu,            "addu",         0, 0, IF_FUNCS_6,      0x00000021)
INSTS(and,             "and",          0, 0, IF_FUNCS_6,      0x00000024)
INSTS(andi,            "andi",         0, 0, IF_OPCODE,       0x30000000)
INSTS(b,               "b",            0, 0, IF_OP_FMTS_16,   0x10000000)
INSTS(bal,             "bal",          0, 0, IF_OP_FMTS_16,   0x04110000)
INSTS(bc1f,            "bc1f",         0, 0, IF_OP_FMT_16,    0x45000000)
INSTS(bc1t,            "bc1t",         0, 0, IF_OP_FMT_16,    0x45010000)
INSTS(beq,             "beq",          0, 0, IF_OPCODE,       0x10000000)
INSTS(bgez,            "bgez",         0, 0, IF_OPCODES_16,   0x04010000)
INSTS(bgezal,          "bgezal",       0, 0, IF_OPCODES_16,   0x04110000)
INSTS(bgtz,            "bgtz",         0, 0, IF_OPCODES_16,   0x1c000000)
INSTS(blez,            "blez",         0, 0, IF_OPCODES_16,   0x18000000)
INSTS(bltz,            "bltz",         0, 0, IF_OPCODES_16,   0x04000000)
INSTS(bltzal,          "bltzal",       0, 0, IF_OPCODES_16,   0x04100000)
INSTS(bne,             "bne",          0, 0, IF_OPCODE,       0x14000000)
INSTS(break,           "break",        0, 0, IF_FUNC,         0x0000000d)
INSTS(c_f_s,           "c.f.s",        0, 0, IF_FMT_FUNC_6,   0x46000030)
INSTS(c_un_s,          "c.un.s",       0, 0, IF_FMT_FUNC_6,   0x46000031)
INSTS(c_eq_s,          "c.eq.s",       0, 0, IF_FMT_FUNC_6,   0x46000032)
INSTS(c_ueq_s,         "c.ueq.s",      0, 0, IF_FMT_FUNC_6,   0x46000033)
INSTS(c_olt_s,         "c.olt.s",      0, 0, IF_FMT_FUNC_6,   0x46000034)
INSTS(c_ult_s,         "c.ult.s",      0, 0, IF_FMT_FUNC_6,   0x46000035)
INSTS(c_ole_s,         "c.ole.s",      0, 0, IF_FMT_FUNC_6,   0x46000036)
INSTS(c_ule_s,         "c.ule.s",      0, 0, IF_FMT_FUNC_6,   0x46000037)
INSTS(c_sf_s,          "c.sf.s",       0, 0, IF_FMT_FUNC_6,   0x46000038)
INSTS(c_ngle_s,        "c.ngle.s",     0, 0, IF_FMT_FUNC_6,   0x46000039)
INSTS(c_seq_s,         "c.seq.s",      0, 0, IF_FMT_FUNC_6,   0x4600003a)
INSTS(c_ngl_s,         "c.ngl.s",      0, 0, IF_FMT_FUNC_6,   0x4600003b)
INSTS(c_lt_s,          "c.lt.s",       0, 0, IF_FMT_FUNC_6,   0x4600003c)
INSTS(c_nge_s,         "c.nge.s",      0, 0, IF_FMT_FUNC_6,   0x4600003d)
INSTS(c_le_s,          "c.le.s",       0, 0, IF_FMT_FUNC_6,   0x4600003e)
INSTS(c_ngt_s,         "c.ngt.s",      0, 0, IF_FMT_FUNC_6,   0x4600003f)
INSTS(c_f_d,           "c.f.d",        0, 0, IF_FMT_FUNC_6,   0x46200030)
INSTS(c_un_d,          "c.un.d",       0, 0, IF_FMT_FUNC_6,   0x46200031)
INSTS(c_eq_d,          "c.eq.d",       0, 0, IF_FMT_FUNC_6,   0x46200032)
INSTS(c_ueq_d,         "c.ueq.d",      0, 0, IF_FMT_FUNC_6,   0x46200033)
INSTS(c_olt_d,         "c.olt.d",      0, 0, IF_FMT_FUNC_6,   0x46200034)
INSTS(c_ult_d,         "c.ult.d",      0, 0, IF_FMT_FUNC_6,   0x46200035)
INSTS(c_ole_d,         "c.ole.d",      0, 0, IF_FMT_FUNC_6,   0x46200036)
INSTS(c_ule_d,         "c.ule.d",      0, 0, IF_FMT_FUNC_6,   0x46200037)
INSTS(c_sf_d,          "c.sf.d",       0, 0, IF_FMT_FUNC_6,   0x46200038)
INSTS(c_ngle_d,        "c.ngle.d",     0, 0, IF_FMT_FUNC_6,   0x46200039)
INSTS(c_seq_d,         "c.seq.d",      0, 0, IF_FMT_FUNC_6,   0x4620003a)
INSTS(c_ngl_d,         "c.ngl.d",      0, 0, IF_FMT_FUNC_6,   0x4620003b)
INSTS(c_lt_d,          "c.lt.d",       0, 0, IF_FMT_FUNC_6,   0x4620003c)
INSTS(c_nge_d,         "c.nge.d",      0, 0, IF_FMT_FUNC_6,   0x4620003d)
INSTS(c_le_d,          "c.le.d",       0, 0, IF_FMT_FUNC_6,   0x4620003e)
INSTS(c_ngt_d,         "c.ngt.d",      0, 0, IF_FMT_FUNC_6,   0x4620003f)
INSTS(ceil_l_s,        "ceil.l.s",     0, 0, IF_FMT_FUNCS_16, 0x4600000a)
INSTS(ceil_l_d,        "ceil.l.d",     0, 0, IF_FMT_FUNCS_16, 0x4620000a)
INSTS(ceil_w_s,        "ceil.w.s",     0, 0, IF_FMT_FUNCS_16, 0x4600000e)
INSTS(ceil_w_d,        "ceil.w.d",     0, 0, IF_FMT_FUNCS_16, 0x4620000e)
INSTS(cfc1,            "cfc1",         0, 0, IF_FMT_FUNCS_6,  0x44400000)
INSTS(ctc1,            "ctc1",         0, 0, IF_FMT_FUNCS_6,  0x44c00000)
INSTS(clo,             "clo",          0, 0, IF_FUNCS_6,      0x70000021)
INSTS(clz,             "clz",          0, 0, IF_FUNCS_6,      0x70000020)
INSTS(cvt_d_s,         "cvt.d.s",      0, 0, IF_FMT_FUNCS_16, 0x46000021)
INSTS(cvt_d_w,         "cvt.d.w",      0, 0, IF_FMT_FUNCS_16, 0x46800021)
INSTS(cvt_d_l,         "cvt.d.l",      0, 0, IF_FMT_FUNCS_16, 0x46a00021)
INSTS(cvt_l_s,         "cvt.l.s",      0, 0, IF_FMT_FUNCS_16, 0x46000025)
INSTS(cvt_l_d,         "cvt.l.d",      0, 0, IF_FMT_FUNCS_16, 0x46200025)
INSTS(cvt_s_d,         "cvt.s.d",      0, 0, IF_FMT_FUNCS_16, 0x46200020)
INSTS(cvt_s_w,         "cvt.s.w",      0, 0, IF_FMT_FUNCS_16, 0x46800020)
INSTS(cvt_s_l,         "cvt.s.l",      0, 0, IF_FMT_FUNCS_16, 0x46a00020)
INSTS(cvt_w_s,         "cvt.w.s",      0, 0, IF_FMT_FUNCS_16, 0x46000024)
INSTS(cvt_w_d,         "cvt.w.d",      0, 0, IF_FMT_FUNCS_16, 0x46200024)
INSTS(dadd,            "dadd",         0, 0, IF_FUNCS_6,      0x0000002c)
INSTS(daddi,           "daddi",        0, 0, IF_OPCODE,       0x60000000)
INSTS(daddiu,          "daddiu",       0, 0, IF_OPCODE,       0x64000000)
INSTS(daddu,           "daddu",        0, 0, IF_FUNCS_6,      0x0000002d)
INSTS(dclo,            "dclo",         0, 0, IF_FUNCS_6,      0x70000025)
INSTS(dclz,            "dclz",         0, 0, IF_FUNCS_6,      0x70000024)
INSTS(ddiv,            "ddiv",         0, 0, IF_FUNCS_6A,     0x0000001e)
INSTS(ddivu,           "ddivu",        0, 0, IF_FUNCS_6A,     0x0000001f)
INSTS(dext,            "dext",         0, 0, IF_FUNC,         0x7c000003)
INSTS(dextm,           "dextm",        0, 0, IF_FUNC,         0x7c000001)
INSTS(dextu,           "dextu",        0, 0, IF_FUNC,         0x7c000002)
INSTS(dins,            "dins",         0, 0, IF_FUNC,         0x7c000007)
INSTS(dinsm,           "dinsm",        0, 0, IF_FUNC,         0x7c000005)
INSTS(dinsu,           "dinsu",        0, 0, IF_FUNC,         0x7c000006)
INSTS(div,             "div",          0, 0, IF_FUNCS_6A,     0x0000001a)
INSTS(div_s,           "div.s",        0, 0, IF_FMT_FUNC,     0x46000003)
INSTS(div_d,           "div.d",        0, 0, IF_FMT_FUNC,     0x46200003)
INSTS(divu,            "divu",         0, 0, IF_FUNCS_6A,     0x0000001b)
INSTS(dmfc1,           "dmfc1",        0, 0, IF_FMT_FUNCS_6,  0x44200000)
INSTS(dmtc1,           "dmtc1",        0, 0, IF_FMT_FUNCS_6,  0x44a00000)
INSTS(dmult,           "dmult",        0, 0, IF_FUNCS_6A,     0x0000001c)
INSTS(dmultu,          "dmultu",       0, 0, IF_FUNCS_6A,     0x0000001d)
INSTS(drotr,           "drotr",        0, 0, IF_FMT_FUNC,     0x0020003a)
INSTS(drotr32,         "drotr32",      0, 0, IF_FMT_FUNC,     0x0020003e)
INSTS(drotrv,          "drotrv",       0, 0, IF_FUNCS_6,      0x00000056)
INSTS(dsbh,            "dsbh",         0, 0, IF_FMT_FUNCS_6,  0x7c0000a4)
INSTS(dshd,            "dshd",         0, 0, IF_FMT_FUNCS_6,  0x7c000164)
INSTS(dsll,            "dsll",         0, 0, IF_FMT_FUNC,     0x00000038)
INSTS(dsll32,          "dsll32",       0, 0, IF_FMT_FUNC,     0x0000003c)
INSTS(dsllv,           "dsllv",        0, 0, IF_FUNCS_6,      0x00000014)
INSTS(dsra,            "dsra",         0, 0, IF_FMT_FUNC,     0x0000003b)
INSTS(dsra32,          "dsra32",       0, 0, IF_FMT_FUNC,     0x0000003f)
INSTS(dsrav,           "dsrav",        0, 0, IF_FUNCS_6,      0x00000017)
INSTS(dsrl,            "dsrl",         0, 0, IF_FMT_FUNC,     0x0000003a)
INSTS(dsrl32,          "dsrl32",       0, 0, IF_FMT_FUNC,     0x0000003e)
INSTS(dsrlv,           "dsrlv",        0, 0, IF_FUNCS_6,      0x00000016)
INSTS(dsub,            "dsub",         0, 0, IF_FUNCS_6,      0x0000002e)
INSTS(dsubu,           "dsubu",        0, 0, IF_FUNCS_6,      0x0000002f)
INSTS(ext,             "ext",          0, 0, IF_FUNC,         0x7c000000)
INSTS(floor_l_s,       "floor.l.s",    0, 0, IF_FMT_FUNCS_16, 0x4600000b)
INSTS(floor_l_d,       "floor.l.d",    0, 0, IF_FMT_FUNCS_16, 0x4620000b)
INSTS(floor_w_s,       "floor.w.s",    0, 0, IF_FMT_FUNCS_16, 0x4600000f)
INSTS(floor_w_d,       "floor.w.d",    0, 0, IF_FMT_FUNCS_16, 0x4620000f)
INSTS(ins,             "ins",          0, 0, IF_FUNC,         0x7c000004)
INSTS(j,               "j",            0, 0, IF_OPCODE,       0x08000000)
INSTS(jal,             "jal",          0, 0, IF_OPCODE,       0x0c000000)
INSTS(jalr,            "jalr",         0, 0, IF_FUNCS_6B,     0x00000009)
INSTS(jalr_hb,         "jalr.hb",      0, 0, IF_FUNCS_6B,     0x00000409)
INSTS(jr,              "jr",           0, 0, IF_FUNCS_6C,     0x00000008)
INSTS(jr_hb,           "jr.hb",        0, 0, IF_FUNCS_6C,     0x00000408)
INSTS(lb,              "lb",           0, LD, IF_OPCODE,      0x80000000)
INSTS(lbu,             "lbu",          0, LD, IF_OPCODE,      0x90000000)
INSTS(ld,              "ld",           0, LD, IF_OPCODE,      0xdc000000)
INSTS(ldc1,            "ldc1",         0, LD, IF_OPCODE,      0xd4000000)
INSTS(ldl,             "ldl",          0, LD, IF_OPCODE,      0x68000000)
INSTS(ldr,             "ldr",          0, LD, IF_OPCODE,      0x6c000000)
INSTS(ldxc1,           "ldxc1",        0, LD, IF_FUNCS_11,    0x4c000001)
INSTS(lh,              "lh",           0, LD, IF_OPCODE,      0x84000000)
INSTS(lhu,             "lhu",          0, LD, IF_OPCODE,      0x94000000)
INSTS(ll,              "ll",           0, LD, IF_OPCODE,      0xc0000000)
INSTS(lld,             "lld",          0, LD, IF_OPCODE,      0xd0000000)
INSTS(lui,             "lui",          0, 0, IF_OP_FMT,       0x3c000000)
INSTS(luxc1,           "luxc1",        0, LD, IF_FUNCS_11,    0x4c000005)
INSTS(lw,              "lw",           0, LD, IF_OPCODE,      0x8c000000)
INSTS(lwc1,            "lwc1",         0, LD, IF_OPCODE,      0xc4000000)
INSTS(lwl,             "lwl",          0, LD, IF_OPCODE,      0x88000000)
INSTS(lwr,             "lwr",          0, LD, IF_OPCODE,      0x98000000)
INSTS(lwu,             "lwu",          0, LD, IF_OPCODE,      0x9c000000)
INSTS(lwxc1,           "lwxc1",        0, LD, IF_FUNCS_11,    0x4c000000)
INSTS(madd,            "madd",         0, 0, IF_FUNCS_6A,     0x70000000)
INSTS(madd_s,          "madd.s",       0, 0, IF_FUNC,         0x4c000020)
INSTS(madd_d,          "madd.d",       0, 0, IF_FUNC,         0x4c000021)
INSTS(maddu,           "maddu",        0, 0, IF_FUNCS_6A,     0x70000001)
INSTS(mfc1,            "mfc1",         0, 0, IF_FMT_FUNCS_6,  0x44000000)
INSTS(mfhc1,           "mfhc1",        0, 0, IF_FMT_FUNCS_6,  0x44600000)
INSTS(mfhi,            "mfhi",         0, 0, IF_FMT_FUNCS_6A, 0x00000010)
INSTS(mflo,            "mflo",         0, 0, IF_FMT_FUNCS_6A, 0x00000012)
INSTS(mov_s,           "mov.s",        0, 0, IF_FMT_FUNCS_16, 0x46000006)
INSTS(mov_d,           "mov.d",        0, 0, IF_FMT_FUNCS_16, 0x46200006)
INSTS(movf,            "movf",         0, 0, IF_FUNCS_6D,     0x00000001)
INSTS(movf_s,          "movf.s",       0, 0, IF_FMT_FUNC_16,  0x46000011)
INSTS(movf_d,          "movf.d",       0, 0, IF_FMT_FUNC_16,  0x46200011)
INSTS(movn,            "movn",         0, 0, IF_FUNCS_6,      0x0000000b)
INSTS(movn_s,          "movn.s",       0, 0, IF_FMT_FUNC,     0x46000013)
INSTS(movn_d,          "movn.d",       0, 0, IF_FMT_FUNC,     0x46200013)
INSTS(movt,            "movt",         0, 0, IF_FUNCS_6D,     0x00010001)
INSTS(movt_s,          "movt.s",       0, 0, IF_FMT_FUNC_16,  0x46010011)
INSTS(movt_d,          "movt.d",       0, 0, IF_FMT_FUNC_16,  0x46210011)
INSTS(movz,            "movz",         0, 0, IF_FUNCS_6,      0x0000000a)
INSTS(movz_s,          "movz.s",       0, 0, IF_FMT_FUNC,     0x46000012)
INSTS(movz_d,          "movz.d",       0, 0, IF_FMT_FUNC,     0x46200012)
INSTS(msub,            "msub",         0, 0, IF_FUNCS_6A,     0x70000004)
INSTS(msub_s,          "msub.s",       0, 0, IF_FUNC,         0x4c000028)
INSTS(msub_d,          "msub.d",       0, 0, IF_FUNC,         0x4c000029)
INSTS(msubu,           "msubu",        0, 0, IF_FUNCS_6A,     0x70000005)
INSTS(mtc1,            "mtc1",         0, 0, IF_FMT_FUNCS_6,  0x44800000)
INSTS(mthc1,           "mthc1",        0, 0, IF_FMT_FUNCS_6,  0x44e00000)
INSTS(mthi,            "mthi",         0, 0, IF_FUNCS_6C,     0x00000011)
INSTS(mtlo,            "mtlo",         0, 0, IF_FUNCS_6C,     0x00000013)
INSTS(mul,             "mul",          0, 0, IF_FUNCS_6,      0x70000002)
INSTS(mul_s,           "mul.s",        0, 0, IF_FMT_FUNC,     0x46000002)
INSTS(mul_d,           "mul.d",        0, 0, IF_FMT_FUNC,     0x46200002)
INSTS(mult,            "mult",         0, 0, IF_FUNCS_6A,     0x00000018)
INSTS(multu,           "multu",        0, 0, IF_FUNCS_6A,     0x00000019)
INSTS(neg_s,           "neg.s",        0, 0, IF_FMT_FUNCS_16, 0x46000007)
INSTS(neg_d,           "neg.d",        0, 0, IF_FMT_FUNCS_16, 0x46200007)
INSTS(nmadd_s,         "nmadd.s",      0, 0, IF_FUNC,         0x4c000030)
INSTS(nmadd_d,         "nmadd.d",      0, 0, IF_FUNC,         0x4c000031)
INSTS(nmsub_s,         "nmsub.s",      0, 0, IF_FUNC,         0x4c000038)
INSTS(nmsub_d,         "nmsub.d",      0, 0, IF_FUNC,         0x4c000039)
INSTS(nor,             "nor",          0, 0, IF_FUNCS_6,      0x00000027)
INSTS(or,              "or",           0, 0, IF_FUNCS_6,      0x00000025)
INSTS(ori,             "ori",          0, 0, IF_OPCODE,       0x34000000)
INSTS(pref,            "pref",         0, 0, IF_OPCODE,       0xcc000000)
INSTS(prefx,           "prefx",        0, 0, IF_FUNCS_6,      0x4c00000f)
INSTS(rdhwr,           "rdhwr",        0, 0, IF_FMT_FUNCS_6,  0x7c00003b)
INSTS(recip_s,         "recip.s",      0, 0, IF_FMT_FUNCS_16, 0x46000015)
INSTS(recip_d,         "recip.d",      0, 0, IF_FMT_FUNCS_16, 0x46200015)
INSTS(rotr,            "rotr",         0, 0, IF_FMT_FUNC,     0x00200002)
INSTS(rotrv,           "rotrv",        0, 0, IF_FUNCS_6,      0x00000046)
INSTS(round_l_s,       "round.l.s",    0, 0, IF_FMT_FUNCS_16, 0x46000008)
INSTS(round_l_d,       "round.l.d",    0, 0, IF_FMT_FUNCS_16, 0x46200008)
INSTS(round_w_s,       "round.w.s",    0, 0, IF_FMT_FUNCS_16, 0x4600000c)
INSTS(round_w_d,       "round.w.d",    0, 0, IF_FMT_FUNCS_16, 0x4620000c)
INSTS(rsqrt_s,         "rsqrt.s",      0, 0, IF_FMT_FUNCS_16, 0x46000016)
INSTS(rsqrt_d,         "rsqrt.d",      0, 0, IF_FMT_FUNCS_16, 0x46200016)
INSTS(sb,              "sb",           0, ST, IF_OPCODE,      0xa0000000)
INSTS(sc,              "sc",           0, ST, IF_OPCODE,      0xe0000000)
INSTS(scd,             "scd",          0, ST, IF_OPCODE,      0xf0000000)
INSTS(sd,              "sd",           0, ST, IF_OPCODE,      0xfc000000)
INSTS(sdc1,            "sdc1",         0, ST, IF_OPCODE,      0xf4000000)
INSTS(sdl,             "sdl",          0, ST, IF_OPCODE,      0xb0000000)
INSTS(sdr,             "sdr",          0, ST, IF_OPCODE,      0xb4000000)
INSTS(sdxc1,           "sdxc1",        0, ST, IF_FUNCS_6,     0x4c000009)
INSTS(seb,             "seb",          0, 0, IF_FMT_FUNCS_6,  0x7c000420)
INSTS(seh,             "seh",          0, 0, IF_FMT_FUNCS_6,  0x7c000620)
INSTS(sh,              "sh",           0, ST, IF_OPCODE,      0xa4000000)
INSTS(sll,             "sll",          0, 0, IF_FMT_FUNC,     0x00000000)
INSTS(sllv,            "sllv",         0, 0, IF_FUNCS_6,      0x00000004)
INSTS(slt,             "slt",          0, 0, IF_FUNCS_6,      0x0000002a)
INSTS(slti,            "slti",         0, 0, IF_OPCODE,       0x28000000)
INSTS(sltiu,           "sltiu",        0, 0, IF_OPCODE,       0x2c000000)
INSTS(sltu,            "sltu",         0, 0, IF_FUNCS_6,      0x0000002b)
INSTS(sqrt_s,          "sqrt.s",       0, 0, IF_FMT_FUNCS_16, 0x46000004)
INSTS(sqrt_d,          "sqrt.d",       0, 0, IF_FMT_FUNCS_16, 0x46200004)
INSTS(sra,             "sra",          0, 0, IF_FMT_FUNC,     0x00000003)
INSTS(srav,            "srav",         0, 0, IF_FUNCS_6,      0x00000007)
INSTS(srl,             "srl",          0, 0, IF_FMT_FUNC,     0x00000002)
INSTS(srlv,            "srlv",         0, 0, IF_FUNCS_6,      0x00000006)
INSTS(sub,             "sub",          0, 0, IF_FUNCS_6,      0x00000022)
INSTS(sub_s,           "sub.s",        0, 0, IF_FMT_FUNC,     0x46000001)
INSTS(sub_d,           "sub.d",        0, 0, IF_FMT_FUNC,     0x46200001)
INSTS(subu,            "subu",         0, 0, IF_FUNCS_6,      0x00000023)
INSTS(suxc1,           "suxc1",        0, ST, IF_FUNCS_6,     0x4c00000d)
INSTS(sw,              "sw",           0, ST, IF_OPCODE,      0xac000000)
INSTS(swc1,            "swc1",         0, ST, IF_OPCODE,      0xe4000000)
INSTS(swl,             "swl",          0, ST, IF_OPCODE,      0xa8000000)
INSTS(swr,             "swr",          0, ST, IF_OPCODE,      0xb8000000)
INSTS(swxc1,           "swxc1",        0, ST, IF_FUNCS_6,     0x4c000008)
INSTS(sync,            "sync",         0, 0, IF_FMT_FUNCS_11A,0x0000000f)
INSTS(synci,           "synci",        0, 0, IF_OPCODES_16,   0x041f0000)
INSTS(trunc_l_s,       "trunc.l.s",    0, 0, IF_FMT_FUNCS_16, 0x46000009)
INSTS(trunc_l_d,       "trunc.l.d",    0, 0, IF_FMT_FUNCS_16, 0x46200009)
INSTS(trunc_w_s,       "trunc.w.s",    0, 0, IF_FMT_FUNCS_16, 0x4600000d)
INSTS(trunc_w_d,       "trunc.w.d",    0, 0, IF_FMT_FUNCS_16, 0x4620000d)
INSTS(wsbh,            "wsbh",         0, 0, IF_FMT_FUNCS_6,  0x7c0000a0)
INSTS(xor,             "xor",          0, 0, IF_FUNCS_6,      0x00000026)
INSTS(xori,            "xori",         0, 0, IF_OPCODE,       0x38000000)


//bnel
//bltzl
//bltzall
//blezl
//bgtzl
//bgezl
//bgezall
//beql
//bc1tl
//bc1fl
//
//syscall
//teq
//teqi
//teqiu
//tequ
//tge
//ssnop

// clang-format on

/*****************************************************************************/
#undef INSTS
/*****************************************************************************/
