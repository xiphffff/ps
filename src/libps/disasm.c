// Copyright 2019 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

// XXX: Complete pseudoinstruction support is a must, along with the ability to
// customize the result, whether it's uppercase, whether there are prefixes on
// the registers, etc.

#ifdef LIBPS_DEBUG

#include <math.h>
#include <stdio.h>
#include "cpu_defs.h"
#include "disasm.h"

// General purpose registers (GPR) as defined by MIPS conventions
static const char* const gpr[32] =
{
    "zero", // 0
    "at",   // 1
    "v0",   // 2
    "v1",   // 3
    "a0",   // 4
    "a1",   // 5
    "a2",   // 6
    "a3",   // 7
    "t0",   // 8
    "t1",   // 9
    "t2",   // 10
    "t3",   // 11
    "t4",   // 12
    "t5",   // 13
    "t6",   // 14
    "t7",   // 15
    "s0",   // 16
    "s1",   // 17
    "s2",   // 18
    "s3",   // 19
    "s4",   // 20
    "s5",   // 21
    "s6",   // 22
    "s7",   // 23
    "t8",   // 24
    "t9",   // 25
    "k0",   // 26
    "k1",   // 27
    "gp",   // 28
    "sp",   // 29
    "fp",   // 30
    "ra"    // 31
};

// System control co-processor (COP0) registers
static const char* const cop0_cpr[32] =
{
    "C0_UNUSED0",  // 0
    "C0_UNUSED1",  // 1
    "C0_UNUSED2",  // 2
    "BPC",         // 3
    "C0_UNUSED4",  // 4
    "BDA",         // 5
    "TAR",         // 6
    "DCIC",        // 7
    "BadA",        // 8
    "BDAM",        // 9
    "C0_UNUSED10", // 10
    "BPCM",        // 11
    "SR",          // 12
    "Cause",       // 13
    "EPC",         // 14
    "PRId",        // 15
    "C0_UNUSED16", // 16
    "C0_UNUSED17", // 17
    "C0_UNUSED18", // 18
    "C0_UNUSED19", // 19
    "C0_UNUSED20", // 20
    "C0_UNUSED21", // 21
    "C0_UNUSED22", // 22
    "C0_UNUSED23", // 23
    "C0_UNUSED24", // 24
    "C0_UNUSED25", // 25
    "C0_UNUSED26", // 26
    "C0_UNUSED27", // 27
    "C0_UNUSED28", // 28
    "C0_UNUSED29", // 29
    "C0_UNUSED30", // 30
    "C0_UNUSED31", // 31
};

// Geometry Transformation Engine (COP2, GTE) data registers
static const char* const cop2_cpr[32] =
{
    "C2_VXY0", // 0
    "C2_VZ0",  // 1
    "C2_VXY1", // 2
    "C2_VZ1",  // 3
    "C2_VXY2", // 4
    "C2_VZ2",  // 5
    "C2_RGB",  // 6
    "C2_OTZ",  // 7
    "C2_IR0",  // 8
    "C2_IR1",  // 9
    "C2_IR2",  // 10
    "C2_IR3",  // 11
    "C2_SXY0", // 12
    "C2_SXY1", // 13
    "C2_SXY2", // 14
    "C2_SXYP", // 15
    "C2_SZ0",  // 16
    "C2_SZ1",  // 17
    "C2_SZ2",  // 18
    "C2_SZ3",  // 19
    "C2_RGB0", // 20
    "C2_RGB1", // 21
    "C2_RGB2", // 22
    "C2_MAC0", // 23
    "C2_MAC1", // 24
    "C2_MAC2", // 25
    "C2_MAC3", // 26
    "C2_IRGB", // 27
    "C2_ORGB", // 28
    "C2_LZCS", // 29
    "C2_LZCR"  // 30
};

// Geometry Transformation Engine (COP2, GTE) control registers
static const char* const cop2_ccr[32] =
{
    "C2_R11R12", // 0
    "C2_R13R21", // 1
    "C2_R22R23", // 2
    "C2_R31R32", // 3
    "C2_R33",    // 4
    "C2_TRX",    // 5
    "C2_TRY",    // 6
    "C2_TRZ",    // 7
    "C2_L11L12", // 8
    "C2_L13L21", // 9
    "C2_L22L23", // 10
    "C2_L31L32", // 11
    "C2_L33",    // 12
    "C2_RBK",    // 13
    "C2_GBK",    // 14
    "C2_BBK",    // 15
    "C2_LR1LR2", // 16
    "C2_LR3LG1", // 17
    "C2_LG2LG3", // 18
    "C2_LB1LB2", // 19
    "C2_LB3",    // 20
    "C2_RFC",    // 21
    "C2_GFC",    // 22
    "C2_BFC",    // 23
    "C2_OFX",    // 24
    "C2_OFY",    // 25
    "C2_H",      // 26
    "C2_DQA",    // 27
    "C2_DQB",    // 28
    "C2_ZSF3",   // 29
    "C2_ZSF4",   // 30
    "C2_FLAG"    // 31
};

// Converts `instruction` to MIPS-I assembly language, and stores this result
// in `result`. `pc` is required so as to compute a proper branch target in the
// event `instruction` is a branch instruction.
void libps_disassemble_instruction(const uint32_t instruction,
                                   const uint32_t pc,
                                   char* result)
{
    // We handle `nop` here so as to not clutter processing of the `sll`
    // opcode.
    if (instruction == 0x00000000)
    {
        sprintf(result, "nop");
        return;
    }

    // XXX: There might be a more compact way to do this, but this is likely
    // the most "efficient" way for however much one needs to care about
    // disassembler efficiency...
    //
    // Also, let's not forget that strings are awful to work with C.
    switch (LIBPS_CPU_DECODE_OP(instruction))
    {
        case LIBPS_CPU_OP_GROUP_SPECIAL:
            switch (LIBPS_CPU_DECODE_FUNCT(instruction))
            {
                case LIBPS_CPU_OP_SLL:
                    sprintf(result,
                            "sll %s,%s,0x%05X",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            LIBPS_CPU_DECODE_SHAMT(instruction));
                    break;

                case LIBPS_CPU_OP_SRL:
                    sprintf(result,
                            "srl %s,%s,0x%05X",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            LIBPS_CPU_DECODE_SHAMT(instruction));
                    break;

                case LIBPS_CPU_OP_SRA:
                    sprintf(result,
                            "sra %s,%s,0x%05X",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            LIBPS_CPU_DECODE_SHAMT(instruction));
                    break;

                case LIBPS_CPU_OP_SLLV:
                    sprintf(result,
                            "sllv %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_SRLV:
                    sprintf(result,
                            "srlv %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_SRAV:
                    sprintf(result,
                            "srav %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_JR:
                    sprintf(result,
                            "jr %s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_JALR:
                    sprintf(result,
                            "jalr %s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_SYSCALL:
                    sprintf(result, "syscall");
                    break;

                case LIBPS_CPU_OP_BREAK:
                    sprintf(result, "break");
                    break;

                case LIBPS_CPU_OP_MFHI:
                    sprintf(result,
                            "mfhi %s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_MTHI:
                    sprintf(result,
                            "mthi %s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_MFLO:
                    sprintf(result,
                            "mflo %s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_MTLO:
                    sprintf(result,
                            "mtlo %s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)]);
                    break;

                case LIBPS_CPU_OP_MULT:
                    sprintf(result,
                            "mult %s,%s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_MULTU:
                    sprintf(result,
                            "multu %s,%s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_DIV:
                    sprintf(result,
                            "div %s,%s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_DIVU:
                    sprintf(result,
                            "divu %s,%s",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_ADD:
                    sprintf(result,
                            "add %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_ADDU:
                    sprintf(result,
                            "addu %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_SUB:
                    sprintf(result,
                            "sub %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_SUBU:
                    sprintf(result,
                            "subu %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_AND:
                    sprintf(result,
                            "and %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_OR:
                    sprintf(result,
                            "or %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_XOR:
                    sprintf(result,
                            "xor %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_NOR:
                    sprintf(result,
                            "nor %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_SLT:
                    sprintf(result,
                            "slt %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                case LIBPS_CPU_OP_SLTU:
                    sprintf(result,
                            "sltu %s,%s,%s",
                            gpr[LIBPS_CPU_DECODE_RD(instruction)],
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            gpr[LIBPS_CPU_DECODE_RT(instruction)]);
                    break;

                default:
                    sprintf(result, "illegal 0x%08X", instruction);
                    break;
            }
            break;

        case LIBPS_CPU_OP_GROUP_BCOND:
            switch (LIBPS_CPU_DECODE_RT(instruction))
            {
                case LIBPS_CPU_OP_BLTZ:
                {
                    const int16_t offset =
                    ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

                    sprintf(result,
                            "bltz %s,0x%08X",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            offset + pc);
                    break;
                }

                case LIBPS_CPU_OP_BGEZ:
                {
                    const int16_t offset =
                    ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

                    sprintf(result,
                            "bgez %s,0x%08X",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            offset + pc);
                    break;
                }

                case LIBPS_CPU_OP_BLTZAL:
                {
                    const int16_t offset =
                    ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

                    sprintf(result,
                            "bltzal %s,0x%08X",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            offset + pc);
                    break;
                }

                case LIBPS_CPU_OP_BGEZAL:
                {
                    const int16_t offset =
                    ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

                    sprintf(result,
                            "bgezal %s,0x%08X",
                            gpr[LIBPS_CPU_DECODE_RS(instruction)],
                            offset + pc);
                    break;
                }

                default:
                    sprintf(result, "illegal 0x%08X", instruction);
                    break;
            }
            break;

        case LIBPS_CPU_OP_J:
            sprintf(result,
                    "j 0x%08X",
                    (LIBPS_CPU_DECODE_TARGET(instruction) << 2) |
                    (pc & 0xF0000000));
            break;

        case LIBPS_CPU_OP_JAL:
            sprintf(result,
                    "jal 0x%08X",
                    (LIBPS_CPU_DECODE_TARGET(instruction) << 2) |
                    (pc & 0xF0000000));
            break;

        case LIBPS_CPU_OP_BEQ:
        {
            const int16_t offset =
            ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

            sprintf(result,
                    "beq %s,%s,0x%08X",
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset + pc + 4);
            break;
        }

        case LIBPS_CPU_OP_BNE:
        {
            const int16_t offset =
            ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

            sprintf(result,
                    "bne %s,%s,0x%08X",
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset + pc + 4);
            break;
        }

        case LIBPS_CPU_OP_BLEZ:
        {
            const int16_t offset =
            ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

            sprintf(result,
                    "blez %s,0x%08X",
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    offset + pc + 4);
            break;
        }

        case LIBPS_CPU_OP_BGTZ:
        {
            const int16_t offset =
            ((int16_t)LIBPS_CPU_DECODE_OFFSET(instruction) << 2);

            sprintf(result,
                    "bgtz %s,0x%08X",
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    offset + pc + 4);
            break;
        }

        case LIBPS_CPU_OP_ADDI:
        {
            const int16_t imm =
            (int16_t)LIBPS_CPU_DECODE_IMMEDIATE(instruction);

            sprintf(result,
                    "addi %s,%s,%s0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    imm < 0 ? "-" : "",
                    abs(imm));
            break;
        }

        case LIBPS_CPU_OP_ADDIU:
            sprintf(result,
                    "addiu %s,%s,0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    LIBPS_CPU_DECODE_IMMEDIATE(instruction));
            break;

        case LIBPS_CPU_OP_SLTI:
        {
            const int16_t imm =
            (int16_t)LIBPS_CPU_DECODE_IMMEDIATE(instruction);

            sprintf(result,
                    "slti %s,%s,%s0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    imm < 0 ? "-" : "",
                    abs(imm));
            break;
        }

        case LIBPS_CPU_OP_SLTIU:
        {
            const int16_t imm =
            (int16_t)LIBPS_CPU_DECODE_IMMEDIATE(instruction);

            sprintf(result,
                    "sltiu %s,%s,%s0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    imm < 0 ? "-" : "",
                    abs(imm));
            break;
        }

        case LIBPS_CPU_OP_ANDI:
            sprintf(result,
                    "andi %s,%s,0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    LIBPS_CPU_DECODE_IMMEDIATE(instruction));
            break;

        case LIBPS_CPU_OP_ORI:
            sprintf(result,
                    "ori %s,%s,0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    LIBPS_CPU_DECODE_IMMEDIATE(instruction));
            break;

        case LIBPS_CPU_OP_XORI:
            sprintf(result,
                    "xori %s,%s,0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    gpr[LIBPS_CPU_DECODE_RS(instruction)],
                    LIBPS_CPU_DECODE_IMMEDIATE(instruction));
            break;

        case LIBPS_CPU_OP_LUI:
            sprintf(result,
                    "lui %s,0x%04X",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    LIBPS_CPU_DECODE_IMMEDIATE(instruction));
            break;

        case LIBPS_CPU_OP_GROUP_COP0:
            switch (LIBPS_CPU_DECODE_RS(instruction))
            {
                case LIBPS_CPU_OP_MF:
                    sprintf(result,
                            "mfc0 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop0_cpr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_MT:
                    sprintf(result,
                            "mtc0 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop0_cpr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                default:
                    switch (LIBPS_CPU_DECODE_FUNCT(instruction))
                    {
                        case LIBPS_CPU_OP_RFE:
                            sprintf(result, "rfe");
                            break;
                    }
                    break;
            }
            break;

        case LIBPS_CPU_OP_GROUP_COP2:
            switch (LIBPS_CPU_DECODE_RS(instruction))
            {
                case LIBPS_CPU_OP_MF:
                    sprintf(result,
                            "mfc2 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop2_cpr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_CF:
                    sprintf(result,
                            "cfc2 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop2_ccr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_MT:
                    sprintf(result,
                            "mtc2 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop2_ccr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                case LIBPS_CPU_OP_CT:
                    sprintf(result,
                            "ctc2 %s,%s",
                            gpr[LIBPS_CPU_DECODE_RT(instruction)],
                            cop2_ccr[LIBPS_CPU_DECODE_RD(instruction)]);
                    break;

                default:
                    switch (LIBPS_CPU_DECODE_FUNCT(instruction))
                    {
                        case LIBPS_CPU_OP_RTPS:
                            sprintf(result, "rtps");
                            break;

                        case LIBPS_CPU_OP_NCLIP:
                            sprintf(result, "nclip");
                            break;

                        case LIBPS_CPU_OP_OP:
                            sprintf(result,
                                    "op %d",
                                    (instruction & (1 << 19)) != 0);
                            break;

                        case LIBPS_CPU_OP_DPCS:
                            sprintf(result, "dpcs");
                            break;

                        case LIBPS_CPU_OP_INTPL:
                            sprintf(result, "intpl");
                            break;

                        case LIBPS_CPU_OP_MVMVA:
                        {
                            const unsigned int sf =
                            (instruction & (1 << 19)) != 0;

                            const unsigned int mx = 0;
                            const unsigned int v = 0;
                            const unsigned int cv = 0;
                            const unsigned int lm = 0;

                            sprintf(result,
                                    "mvmva %d,%d,%d,%d,%d",
                                    sf, mx, v, cv, lm);
                            break;
                        }

                        case LIBPS_CPU_OP_NCDS:
                            sprintf(result, "ncds");
                            break;

                        case LIBPS_CPU_OP_CDP:
                            sprintf(result, "cdp");
                            break;

                        case LIBPS_CPU_OP_NCDT:
                            sprintf(result, "ncdt");
                            break;

                        case LIBPS_CPU_OP_NCCS:
                            sprintf(result, "nccs");
                            break;

                        case LIBPS_CPU_OP_NCS:
                            sprintf(result, "ncs");
                            break;

                        case LIBPS_CPU_OP_NCT:
                            sprintf(result, "nct");
                            break;

                        case LIBPS_CPU_OP_SQR:
                            sprintf(result,
                                    "sqr %d",
                                    (instruction & (1 << 19)) != 0);
                            break;

                        case LIBPS_CPU_OP_DCPL:
                            sprintf(result, "dcpl");
                            break;

                        case LIBPS_CPU_OP_DPCT:
                            sprintf(result, "dpct");
                            break;

                        case LIBPS_CPU_OP_AVSZ3:
                            sprintf(result, "avsz3");
                            break;

                        case LIBPS_CPU_OP_AVSZ4:
                            sprintf(result, "avsz4");
                            break;

                        case LIBPS_CPU_OP_RTPT:
                            sprintf(result, "rtpt");
                            break;

                        case LIBPS_CPU_OP_GPF:
                            sprintf(result,
                                    "gpf %d",
                                    (instruction & (1 << 19)) != 0);
                            break;

                        case LIBPS_CPU_OP_GPL:
                            sprintf(result,
                                "gpl %d",
                                (instruction & (1 << 19)) != 0);
                            break;

                        case LIBPS_CPU_OP_NCCT:
                            sprintf(result, "ncct");
                            break;

                        default:
                            sprintf(result, "illegal 0x%08X", instruction);
                            break;
                    }
                    break;
            }
            break;

        case LIBPS_CPU_OP_LB:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lb %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LH:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lh %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LWL:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lwl %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LW:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lw %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LBU:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lbu %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LHU:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lhu %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LWR:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lwr %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SB:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "sb %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SH:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "sh %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SWL:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "swl %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SW:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "sw %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SWR:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "swr %s,%s0x%04X(%s)",
                    gpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_LWC2:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "lwc2 %s,%s0x%04X(%s)",
                    cop2_cpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        case LIBPS_CPU_OP_SWC2:
        {
            const int16_t offset =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(instruction);

            sprintf(result,
                    "swc2 %s,%s0x%04X(%s)",
                    cop2_cpr[LIBPS_CPU_DECODE_RT(instruction)],
                    offset < 0 ? "-" : "",
                    abs(offset),
                    gpr[LIBPS_CPU_DECODE_BASE(instruction)]);
            break;
        }

        default:
            sprintf(result, "illegal 0x%08X", instruction);
            break;
    }
}

#endif // LIBPS_DEBUG