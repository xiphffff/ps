// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <array>
#include <fmt/printf.h>
#include <assert.h>
#include "../psemu/include/psemu.h"

struct
{
    struct
    {
        std::string str;
        uint32_t* ptr;
    } post_process;

    std::string result;
    uint32_t paddr;
} static state;

// Conventional names of general-purpose registers
static const std::array<const std::string, 32> gpr =
{
    "$zero", // 0
    "$at",   // 1
    "$v0",   // 2
    "$v1",   // 3
    "$a0",   // 4
    "$a1",   // 5
    "$a2",   // 6
    "$a3",   // 7
    "$t0",   // 8
    "$t1",   // 9
    "$t2",   // 10
    "$t3",   // 11
    "$t4",   // 12
    "$t5",   // 13
    "$t6",   // 14
    "$t7",   // 15
    "$s0",   // 16
    "$s1",   // 17
    "$s2",   // 18
    "$s3",   // 19
    "$s4",   // 20
    "$s5",   // 21
    "$s6",   // 22
    "$s7",   // 23
    "$t8",   // 24
    "$t9",   // 25
    "$k0",   // 26
    "$k1",   // 27
    "$gp",   // 28
    "$sp",   // 29
    "$fp",   // 30
    "$ra"    // 31
};

// System control co-processor (COP0) registers
static const std::array<const std::string, 32> cop0 =
{
    "",      // 0
    "",      // 1
    "",      // 2
    "BPC",   // 3
    "",      // 4
    "BDA",   // 5
    "TAR",   // 6
    "DCIC",  // 7
    "BadA",  // 8
    "BDAM",  // 9
    "",      // 10
    "BPCM",  // 11
    "SR",    // 12
    "Cause", // 13
    "EPC",   // 14
    "PRId",  // 15
    "",      // 16
    "",      // 17
    "",      // 18
    "",      // 19
    "",      // 20
    "",      // 21
    "",      // 22
    "",      // 23
    "",      // 24
    "",      // 25
    "",      // 26
    "",      // 27
    "",      // 28
    "",      // 29
    "",      // 30
    ""       // 31
};

// Geometry Transformation Engine (GTE/COP2) data registers
static const std::array<const std::string, 32> cop2_cpr =
{
    "C2_VXY0",    // 0
    "C2_VZ0",     // 1
    "C2_VXY1",    // 2
    "C2_VZ1",     // 3
    "C2_VXY2",    // 4
    "C2_VZ2",     // 5
    "C2_RGB",     // 6
    "C2_OTZ",     // 7
    "C2_IR0",     // 8
    "C2_IR1",     // 9
    "C2_IR2",     // 10
    "C2_IR3",     // 11
    "C2_SXY0",    // 12
    "C2_SXY1",    // 13
    "C2_SXY2",    // 14
    "C2_SXYP",    // 15
    "C2_SZ0",     // 16
    "C2_SZ1",     // 17
    "C2_SZ2",     // 18
    "C2_SZ3",     // 19
    "C2_RGB0",    // 20
    "C2_RGB1",    // 21
    "C2_RGB2",    // 22
    "C2_ILLEGAL", // 23
    "C2_MAC0",    // 24
    "C2_MAC1",    // 25
    "C2_MAC2",    // 26
    "C2_MAC3",    // 27
    "C2_IRGB",    // 28
    "C2_ORGB",    // 29
    "C2_LZCS",    // 30
    "C2_LZCR"     // 31
};

// Geometry Transformation Engine (GTE/COP2) control registers
static const std::array<const std::string, 32> cop2_ccr =
{
    "C2_R11R12", // 0
    "C2_R13R21", // 1
    "C2_R22R23", // 2
    "C2_R31R32", // 3
    "C2_R33",    // 4
    "C2_TRX",    // 5
    "C2_TRY",    // 6
    "C2_TRZ",    // 7
    "C2_L11L12"  // 8
    "C2_L13L21"  // 9
    "C2_L22L23"  // 10
    "C2_L31L32"  // 11
    "C2_L33"     // 12
    "C2_RBK"     // 13
    "C2_GBK"     // 14
    "C2_BBK"     // 15
    "C2_LR1LR2"  // 16
    "C2_LR3LG1"  // 17
    "C2_LG2LG3"  // 18
    "C2_LB1LB2"  // 19
    "C2_LB3"     // 20
    "C2_RFC"     // 21
    "C2_GFC"     // 22
    "C2_BFC"     // 23
    "C2_OFX"     // 24
    "C2_OFY"     // 25
    "C2_H"       // 26
    "C2_DQA"     // 27
    "C2_DQB"     // 28
    "C2_ZSF3"    // 29
    "C2_ZSF4"    // 30
    "C2_FLAG"    // 31
};

// Instructions that are accessed through the operation code `op` field
static const std::array<const std::string, 63> primary_instructions =
{
    "GROUP_SPECIAL",                         // 0x00
    "GROUP_BCOND",                           // 0x01
    "j $branch_address",                     // 0x02
    "jal $branch_address",                   // 0x03
    "beq $gpr_rs, $gpr_rt, $offset_address", // 0x04
    "bne $gpr_rs, $gpr_rt, $offset_address", // 0x05
    "blez $gpr_rs, $offset_address",         // 0x06
    "bgtz $gpr_rs, $offset_address",         // 0x07
    "addi $gpr_rt, $gpr_rs, $sext_imm",      // 0x08
    "addiu $gpr_rt, $gpr_rs, $sext_imm",     // 0x09
    "slti $gpr_rt, $gpr_rs, $sext_imm",      // 0x0A
    "sltiu $gpr_rt, $gpr_rs, $sext_imm",     // 0x0B
    "andi $gpr_rt, $gpr_rs, $zext_imm",      // 0x0C
    "ori $gpr_rt, $gpr_rs, $zext_imm",       // 0x0D
    "xori $gpr_rt, $gpr_rs, $zext_imm",      // 0x0E
    "lui $gpr_rt, $zext_imm",                // 0x0F
    "GROUP_COP0",                            // 0x10
    "",                                      // 0x11
    "GROUP_COP2",                            // 0x12
    "",                                      // 0x13
    "",                                      // 0x14
    "",                                      // 0x15
    "",                                      // 0x16
    "",                                      // 0x17
    "",                                      // 0x18
    "",                                      // 0x19
    "",                                      // 0x1A
    "",                                      // 0x1B
    "",                                      // 0x1C
    "",                                      // 0x1D
    "",                                      // 0x1E
    "",                                      // 0x1F
    "lb $mem",                               // 0x20
    "lh $mem",                               // 0x21
    "lwl $mem",                              // 0x22
    "lw $mem",                               // 0x23
    "lbu $mem",                              // 0x24
    "lhu $mem",                              // 0x25
    "lwr $mem",                              // 0x26
    "",                                      // 0x27
    "sb $mem",                               // 0x28
    "sh $mem",                               // 0x29
    "swl $mem",                              // 0x2A
    "sw $mem",                               // 0x2B
    "",                                      // 0x2C
    "",                                      // 0x2D
    "swr $mem",                              // 0x2E
    "",                                      // 0x2F
    "",                                      // 0x30
    "",                                      // 0x31
    "lwc2 $cp2_mem",                         // 0x32
    "",                                      // 0x33
    "",                                      // 0x34
    "",                                      // 0x35
    "",                                      // 0x36
    "",                                      // 0x37
    "",                                      // 0x38
    "",                                      // 0x39
    "swc2 $cp2_mem",                         // 0x3A
    "",                                      // 0x3B
    "",                                      // 0x3C
    "",                                      // 0x3D
    ""                                       // 0x3E
};

// Instructions that are accessed through the function `funct` field
static const std::array<const std::string, 63> special_instructions =
{
    "sll $gpr_rd, $gpr_rt, $shamt",   // 0x00
    "",                               // 0x01
    "srl $gpr_rd, $gpr_rt, $shamt",   // 0x02
    "sra $gpr_rd, $gpr_rt, $shamt",   // 0x03
    "sllv $gpr_rd, $gpr_rt, $gpr_rs", // 0x04
    "",                               // 0x05
    "srlv $gpr_rd, $gpr_rt, $gpr_rs", // 0x06
    "srav $gpr_rd, $gpr_rt, $gpr_rs", // 0x07
    "jr $gpr_rs",                     // 0x08
    "jalr $gpr_rd, $gpr_rs",          // 0x09
    "",                               // 0x0A
    "",                               // 0x0B
    "syscall",                        // 0x0C
    "break",                          // 0x0D
    "",                               // 0x0E
    "",                               // 0x0F
    "mfhi $gpr_rd",                   // 0x10
    "mthi $gpr_rs",                   // 0x11
    "mflo $gpr_rd",                   // 0x12
    "mtlo $gpr_rs",                   // 0x13
    "",                               // 0x14
    "",                               // 0x15
    "",                               // 0x16
    "",                               // 0x17
    "mult $gpr_rs, $gpr_rt",          // 0x18
    "multu $gpr_rs, $gpr_rt",         // 0x19
    "div $gpr_rs, $gpr_rt",           // 0x1A
    "divu $gpr_rs, $gpr_rt",          // 0x1B
    "",                               // 0x1C
    "",                               // 0x1D
    "",                               // 0x1E
    "",                               // 0x1F
    "add $gpr_rd, $gpr_rs, $gpr_rt",  // 0x20
    "addu $gpr_rd, $gpr_rs, $gpr_rt", // 0x21
    "sub $gpr_rd, $gpr_rs, $gpr_rt",  // 0x22
    "subu $gpr_rd, $gpr_rs, $gpr_rt", // 0x23
    "and $gpr_rd, $gpr_rs, $gpr_rt",  // 0x24
    "or $gpr_rd, $gpr_rs, $gpr_rt",   // 0x25
    "xor $gpr_rd, $gpr_rs, $gpr_rt",  // 0x26
    "nor $gpr_rd, $gpr_rs, $gpr_rt",  // 0x27
    "",                               // 0x28
    "",                               // 0x29
    "slt $gpr_rd, $gpr_rs, $gpr_rt",  // 0x2A
    "sltu $gpr_rd, $gpr_rs, $gpr_rt", // 0x2B
    "",                               // 0x2C
    "",                               // 0x2D
    "",                               // 0x2E
    "",                               // 0x2F
    "",                               // 0x30
    "",                               // 0x31
    "",                               // 0x32
    "",                               // 0x33
    "",                               // 0x34
    "",                               // 0x35
    "",                               // 0x36
    "",                               // 0x37
    "",                               // 0x38
    "",                               // 0x39
    "",                               // 0x3A
    "",                               // 0x3B
    "",                               // 0x3C
    "",                               // 0x3D
    ""                                // 0x3E
};

// Geometry Transformation Engine (GTE/COP2) instructions
static const std::array<const std::string, 63> cop2_instructions =
{
    "",                  // 0x00
    "rtps",              // 0x01
    "",                  // 0x02
    "",                  // 0x03
    "",                  // 0x04
    "",                  // 0x05
    "nclip",             // 0x06
    "",                  // 0x07
    "",                  // 0x08
    "",                  // 0x09
    "",                  // 0x0A
    "",                  // 0x0B
    "op $sf",            // 0x0C
    "",                  // 0x0D
    "",                  // 0x0E
    "",                  // 0x0F
    "dpcs",              // 0x10
    "intpl",             // 0x11
    "mvmva $mvmva_bits", // 0x12
    "ncds",              // 0x13
    "cdp",               // 0x14
    "",                  // 0x15
    "ncdt",              // 0x16
    "",                  // 0x17
    "",                  // 0x18
    "",                  // 0x19
    "",                  // 0x1A
    "nccs",              // 0x1B
    "cc",                // 0x1C
    "",                  // 0x1D
    "ncs",               // 0x1E
    "",                  // 0x1F
    "nct",               // 0x20
    "",                  // 0x21
    "",                  // 0x22
    "",                  // 0x23
    "",                  // 0x24
    "",                  // 0x25
    "",                  // 0x26
    "",                  // 0x27
    "sqr $sf",           // 0x28
    "dcpl",              // 0x29
    "dpct",              // 0x2A
    "",                  // 0x2B
    "",                  // 0x2C
    "avsz3",             // 0x2D
    "avsz4",             // 0x2E
    "",                  // 0x2F
    "rtpt",              // 0x30
    "",                  // 0x31
    "",                  // 0x32
    "",                  // 0x33
    "",                  // 0x34
    "",                  // 0x35
    "",                  // 0x36
    "",                  // 0x37
    "",                  // 0x38
    "",                  // 0x39
    "",                  // 0x3A
    "",                  // 0x3B
    "",                  // 0x3C
    "gpf $sf",           // 0x3D
    "gpl $sf"            // 0x3E
};

// Disassembles the current instruction before execution takes place.
void disassemble_before(struct psemu_cpu* const cpu) noexcept
{
    assert(cpu != NULL);

    state = { };

    state.result = fmt::sprintf("0x%08X\t%08X\t",
                                cpu->pc,
                                cpu->instruction.word);

    auto op{ primary_instructions[cpu->instruction.op] };

    if (op == "GROUP_SPECIAL")
    {
        op = special_instructions[cpu->instruction.funct];
    }

    if (op == "GROUP_BCOND")
    {
        ;
    }

    if (op == "GROUP_COP0")
    {
        switch (cpu->instruction.rs)
        {
            case PSEMU_CPU_OP_MF:
                op = "mfc0 $gpr_rt, $cop0_reg";
                break;

            case PSEMU_CPU_OP_MT:
                op = "mtc0 $gpr_rt, $cop0_reg";
                break;

            default:
                switch (cpu->instruction.funct)
                {
                    case PSEMU_CPU_OP_RFE:
                        op = "rfe";
                        break;

                    default:
                        op = "illegal";
                        break;
                }
                break;
        }
    }

    if (op == "GROUP_COP2")
    {
        switch (cpu->instruction.rs)
        {
            case PSEMU_CPU_OP_MF:
                op = "mfc2 $gpr_rt, $cop2_cpr";
                break;

            case PSEMU_CPU_OP_CF:
                op = "cfc2 $gpr_rt, $cop2_ccr";
                break;

            case PSEMU_CPU_OP_MT:
                op = "mtc2 $gpr_rt, $cop2_cpr";
                break;

            case PSEMU_CPU_OP_CT:
                op = "ctc2 $gpr_rt, $cop2_ccr";
                break;

            default:
                op = cop2_instructions[cpu->instruction.funct];
                break;
        }
    }

    if (op.empty())
    {
        state.result += "illegal";
        return;
    }

    bool dest_found{ false };

    for (auto ptr{ 0 }; ptr < op.size();)
    {
        if (op[ptr] != '$')
        {
            state.result += op[ptr];
            continue;
        }

        if (op.compare(0, 6, "gpr_rd") == 0)
        {
            if (!dest_found)
            {
                state.post_process.str = gpr[cpu->instruction.rd];
                state.post_process.ptr = &cpu->gpr[cpu->instruction.rd];

                dest_found = true;
            }

            state.result += gpr[cpu->instruction.rd];
            ptr += 6;
        }

        if (op.compare(0, 6, "gpr_rs") == 0)
        {
            if (!dest_found)
            {
                state.post_process.str = gpr[cpu->instruction.rs];
                state.post_process.ptr = &cpu->gpr[cpu->instruction.rs];

                dest_found = true;
            }

            state.result += gpr[cpu->instruction.rs];
            ptr += 6;
        }

        if (op.compare(0, 6, "gpr_rt") == 0)
        {
            if (!dest_found)
            {
                state.post_process.str = gpr[cpu->instruction.rt];
                state.post_process.ptr = &cpu->gpr[cpu->instruction.rt];

                dest_found = true;
            }

            state.result += gpr[cpu->instruction.rt];
            ptr += 6;
        }

        if (op.compare(0, 6, "shamt") == 0)
        {
            state.result += fmt::sprintf("fuck");
            ptr += 5;
        }

        if (op.compare(0, 3, "mem") == 0)
        {
            const auto offset
            {
                static_cast<int16_t>
                (PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word))
            };

            state.result += fmt::sprintf("%s, %s0x%04X(%s)",
                                        gpr[cpu->instruction.rt],
                                        offset < 0 ? "-" : "",
                                        abs(offset),
                                        gpr[cpu->instruction.rs]);

            state.paddr =
            (offset + cpu->gpr[cpu->instruction.rs]) & 0x1FFFFFFF;

            state.post_process.str = "paddr";
            state.post_process.ptr = &state.paddr;

            ptr += 3;
        }

        if (op.compare(0, 8, "zext_imm") == 0)
        {
            state.result +=
            fmt::sprintf("0x%04X",
                        PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word));

            ptr += 8;
        }

        if (op.compare(0, 8, "sext_imm") == 0)
        {
            const auto imm
            {
                static_cast<int16_t>
                (PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word))
            };

            state.result += fmt::sprintf("%s0x%04X",
                                        imm < 0 ? "-" : "",
                                        abs(imm));
            ptr += 8;
        }

        if (op.compare(0, 14, "branch_address") == 0)
        {
            const auto address
            {
                ((PSEMU_CPU_DECODE_TARGET(cpu->instruction.word) << 2) |
                (cpu->pc & 0xF0000000))
            };

            state.result += fmt::sprintf("0x%08X", address);
            ptr += 14;
        }

        if (op.compare(0, 14, "offset_address") == 0)
        {
            const auto branch_address
            {
                static_cast<int16_t>
                (PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word) << 2) +
                cpu->pc + 4
            };

            state.result += fmt::sprintf("0x%08X", branch_address);
            ptr += 14;
        }

        if (op.compare(0, 8, "cop0_reg") == 0)
        {
            state.result += cop0[cpu->instruction.rd];

            state.post_process.str = cop0[cpu->instruction.rd];
            state.post_process.ptr = &cpu->cop0[cpu->instruction.rd];

            ptr += 8;
        }
    }
}

// Disassembles the current instruction after execution takes place.
std::string disassemble_after() noexcept
{
    if (state.post_process.ptr)
    {
        state.result += fmt::sprintf("; %s=0x%08X",
                                    state.post_process.str,
                                    *state.post_process.ptr);
    }
    return state.result;
}