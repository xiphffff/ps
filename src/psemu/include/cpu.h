// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright noticeand this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

// Instruction decoders that cannot fit into the `instruction` union
#define PSEMU_CPU_DECODE_TARGET(instruction) (instruction & 0x03FFFFFF)
#define PSEMU_CPU_DECODE_IMMEDIATE(instruction) (instruction & 0x0000FFFF)

// Instruction groups
#define PSEMU_CPU_OP_GROUP_SPECIAL 0x00
#define PSEMU_CPU_OP_GROUP_BCOND 0x01
#define PSEMU_CPU_OP_GROUP_COP0 0x10
#define PSEMU_CPU_OP_GROUP_COP2 0x12

// Primary instructions
#define PSEMU_CPU_OP_J 0x02
#define PSEMU_CPU_OP_JAL 0x03
#define PSEMU_CPU_OP_BEQ 0x04
#define PSEMU_CPU_OP_BNE 0x05
#define PSEMU_CPU_OP_BLEZ 0x06
#define PSEMU_CPU_OP_BGTZ 0x07
#define PSEMU_CPU_OP_ADDI 0x08
#define PSEMU_CPU_OP_ADDIU 0x09
#define PSEMU_CPU_OP_SLTI 0x0A
#define PSEMU_CPU_OP_SLTIU 0x0B
#define PSEMU_CPU_OP_ANDI 0x0C
#define PSEMU_CPU_OP_ORI 0x0D
#define PSEMU_CPU_OP_XORI 0x0E
#define PSEMU_CPU_OP_LUI 0x0F
#define PSEMU_CPU_OP_LB 0x20
#define PSEMU_CPU_OP_LH 0x21
#define PSEMU_CPU_OP_LWL 0x22
#define PSEMU_CPU_OP_LW 0x23
#define PSEMU_CPU_OP_LBU 0x24
#define PSEMU_CPU_OP_LHU 0x25
#define PSEMU_CPU_OP_LWR 0x26
#define PSEMU_CPU_OP_SB 0x28
#define PSEMU_CPU_OP_SH 0x29
#define PSEMU_CPU_OP_SWL 0x2A
#define PSEMU_CPU_OP_SW 0x2B
#define PSEMU_CPU_OP_SWR 0x2E
#define PSEMU_CPU_OP_LWC2 0x32
#define PSEMU_CPU_OP_SWC2 0x3A

// SPECIAL group instructions
#define PSEMU_CPU_OP_SLL 0x00
#define PSEMU_CPU_OP_SRL 0x02
#define PSEMU_CPU_OP_SRA 0x03
#define PSEMU_CPU_OP_SLLV 0x04
#define PSEMU_CPU_OP_SRLV 0x06
#define PSEMU_CPU_OP_SRAV 0x07
#define PSEMU_CPU_OP_JR 0x08
#define PSEMU_CPU_OP_JALR 0x09
#define PSEMU_CPU_OP_SYSCALL 0x0C
#define PSEMU_CPU_OP_BREAK 0x0D
#define PSEMU_CPU_OP_MFHI 0x10
#define PSEMU_CPU_OP_MTHI 0x11
#define PSEMU_CPU_OP_MFLO 0x12
#define PSEMU_CPU_OP_MTLO 0x13
#define PSEMU_CPU_OP_MULT 0x18
#define PSEMU_CPU_OP_MULTU 0x19
#define PSEMU_CPU_OP_DIV 0x1A
#define PSEMU_CPU_OP_DIVU 0x1B
#define PSEMU_CPU_OP_ADD 0x20
#define PSEMU_CPU_OP_ADDU 0x21
#define PSEMU_CPU_OP_SUB 0x22
#define PSEMU_CPU_OP_SUBU 0x23
#define PSEMU_CPU_OP_AND 0x24
#define PSEMU_CPU_OP_OR 0x25
#define PSEMU_CPU_OP_XOR 0x26
#define PSEMU_CPU_OP_NOR 0x27
#define PSEMU_CPU_OP_SLT 0x2A
#define PSEMU_CPU_OP_SLTU 0x2B

// Inherent co-processor instructions
#define PSEMU_CPU_OP_MF 0x00
#define PSEMU_CPU_OP_CF 0x02
#define PSEMU_CPU_OP_MT 0x04
#define PSEMU_CPU_OP_CT 0x06

// System control co-processor (COP0) instruction
#define PSEMU_CPU_OP_RFE 0x10

// Geometry Transformation Engine (GTE/COP2) instructions
#define PSEMU_CPU_OP_NCLIP 0x06
#define PSEMU_CPU_OP_NCDS 0x13
#define PSEMU_CPU_OP_RTPT 0x30

// System control co-processor (COP0) registers
#define PSEMU_CPU_COP0_BadA 8
#define PSEMU_CPU_COP0_SR 12
#define PSEMU_CPU_COP0_Cause 13
#define PSEMU_CPU_COP0_EPC 14

// Geometry Transformation Engine (GTE/COP2) data registers
#define PSEMU_CPU_COP2_VXY0 0
#define PSEMU_CPU_COP2_VZ0 1
#define PSEMU_CPU_COP2_IR1 9
#define PSEMU_CPU_COP2_IR2 10
#define PSEMU_CPU_COP2_IR3 11
#define PSEMU_CPU_COP2_SXY0 12
#define PSEMU_CPU_COP2_SXY1 13
#define PSEMU_CPU_COP2_SXY2 14
#define PSEMU_CPU_COP2_SZ3 19
#define PSEMU_CPU_COP2_MAC0 24
#define PSEMU_CPU_COP2_MAC1 25
#define PSEMU_CPU_COP2_MAC2 26
#define PSEMU_CPU_COP2_MAC3 27

// Geometry Transformation Engine (GTE/COP2) control registers
#define PSEMU_CPU_COP2_R11R12 0
#define PSEMU_CPU_COP2_R13R21 1
#define PSEMU_CPU_COP2_R22R23 2
#define PSEMU_CPU_COP2_R31R32 3
#define PSEMU_CPU_COP2_R33 4
#define PSEMU_CPU_COP2_TRX 5
#define PSEMU_CPU_COP2_TRY 6
#define PSEMU_CPU_COP2_TRZ 7
#define PSEMU_CPU_COP2_OFX 24
#define PSEMU_CPU_COP2_OFY 25
#define PSEMU_CPU_COP2_H 26
#define PSEMU_CPU_COP2_DQA 27
#define PSEMU_CPU_COP2_DQB 28

// Status register (SR) flags
#define PSEMU_CPU_SR_IsC (1 << 16)
#define PSEMU_CPU_SR_INT0 (1 << 10)
#define PSEMU_CPU_SR_IEc (1 << 0)

// Cause register flags
#define PSEMU_CPU_CAUSE_INT0 (1 << 10)

// Exception codes
#define PSEMU_CPU_EXCCODE_Int 0
#define PSEMU_CPU_EXCCODE_Sys 8
#define PSEMU_CPU_EXCCODE_AdEL 4
#define PSEMU_CPU_EXCCODE_AdES 5
#define PSEMU_CPU_EXCCODE_Bp 9
#define PSEMU_CPU_EXCCODE_RI 10
#define PSEMU_CPU_EXCCODE_Ov 12

// Forward declaration
struct psemu_bus;

// Defines the structure of an LSI LR33300 CPU interpreter.
struct psemu_cpu
{
    // Current instruction
    union
    {
        struct
        {
            // Function field
            unsigned int funct : 6;

            // Shift amount
            unsigned int shamt : 5;

            // Destination register specifier
            unsigned int rd : 5;

            // Target (source/destination) or branch condition
            unsigned int rt : 5;

            // Source register specifier
            unsigned int rs : 5;

            // Operation code
            unsigned int op : 6;
        };
        uint32_t word;
    } instruction;

    // Program counter
    uint32_t pc;

    // Next program counter (used for emulating branch delay slots)
    uint32_t next_pc;

    // Quotient of a division operation
    uint32_t lo;

    // Remainder of a division operation
    uint32_t hi;

    // General purpose registers
    uint32_t gpr[32];

    // System control co-processor (COP0) registers
    uint32_t cop0[32];

    // Geometry Transformation Engine (GTE/COP2) registers
    struct
    {
        // Data registers
        uint32_t cpr[32];

        // Control registers
        uint32_t ccr[32];
    } cop2;
};

// Sets the system bus to use to `m_bus`.
void psemu_cpu_set_bus(struct psemu_bus* const b);

// Resets an LSI LR33300 CPU interpreter `cpu` to the startup state.
void psemu_cpu_reset(struct psemu_cpu* const cpu);

// Executes one instruction.
void psemu_cpu_step(struct psemu_cpu* const cpu);

#ifdef __cplusplus
}
#endif // __cplusplus

