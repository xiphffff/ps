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

// This file serves to provide the disassembler, CPU implementations and the
// frontend necessary information to carry out their respective functions.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// Instruction groups
#define LIBPS_CPU_OP_GROUP_SPECIAL 0x00
#define LIBPS_CPU_OP_GROUP_BCOND 0x01
#define LIBPS_CPU_OP_GROUP_COP0 0x10
#define LIBPS_CPU_OP_GROUP_COP2 0x12

// Primary instructions
#define LIBPS_CPU_OP_J 0x02
#define LIBPS_CPU_OP_JAL 0x03
#define LIBPS_CPU_OP_BEQ 0x04
#define LIBPS_CPU_OP_BNE 0x05
#define LIBPS_CPU_OP_BLEZ 0x06
#define LIBPS_CPU_OP_BGTZ 0x07
#define LIBPS_CPU_OP_ADDI 0x08
#define LIBPS_CPU_OP_ADDIU 0x09
#define LIBPS_CPU_OP_SLTI 0x0A
#define LIBPS_CPU_OP_SLTIU 0x0B
#define LIBPS_CPU_OP_ANDI 0x0C
#define LIBPS_CPU_OP_ORI 0x0D
#define LIBPS_CPU_OP_XORI 0x0E
#define LIBPS_CPU_OP_LUI 0x0F
#define LIBPS_CPU_OP_LB 0x20
#define LIBPS_CPU_OP_LH 0x21
#define LIBPS_CPU_OP_LWL 0x22
#define LIBPS_CPU_OP_LW 0x23
#define LIBPS_CPU_OP_LBU 0x24
#define LIBPS_CPU_OP_LHU 0x25
#define LIBPS_CPU_OP_LWR 0x26
#define LIBPS_CPU_OP_SB 0x28
#define LIBPS_CPU_OP_SH 0x29
#define LIBPS_CPU_OP_SWL 0x2A
#define LIBPS_CPU_OP_SW 0x2B
#define LIBPS_CPU_OP_SWR 0x2E
#define LIBPS_CPU_OP_LWC2 0x32
#define LIBPS_CPU_OP_SWC2 0x3A

// SPECIAL instruction group
#define LIBPS_CPU_OP_SLL 0x00
#define LIBPS_CPU_OP_SRL 0x02
#define LIBPS_CPU_OP_SRA 0x03
#define LIBPS_CPU_OP_SLLV 0x04
#define LIBPS_CPU_OP_SRLV 0x06
#define LIBPS_CPU_OP_SRAV 0x07
#define LIBPS_CPU_OP_JR 0x08
#define LIBPS_CPU_OP_JALR 0x09
#define LIBPS_CPU_OP_SYSCALL 0x0C
#ifdef LIBPS_DEBUG
#define LIBPS_CPU_OP_BREAK 0x0D
#endif // LIBPS_DEBUG
#define LIBPS_CPU_OP_MFHI 0x10
#define LIBPS_CPU_OP_MTHI 0x11
#define LIBPS_CPU_OP_MFLO 0x12
#define LIBPS_CPU_OP_MTLO 0x13
#define LIBPS_CPU_OP_MULT 0x18
#define LIBPS_CPU_OP_MULTU 0x19
#define LIBPS_CPU_OP_DIV 0x1A
#define LIBPS_CPU_OP_DIVU 0x1B
#define LIBPS_CPU_OP_ADD 0x20
#define LIBPS_CPU_OP_ADDU 0x21
#define LIBPS_CPU_OP_SUB 0x22
#define LIBPS_CPU_OP_SUBU 0x23
#define LIBPS_CPU_OP_AND 0x24
#define LIBPS_CPU_OP_OR 0x25
#define LIBPS_CPU_OP_XOR 0x26
#define LIBPS_CPU_OP_NOR 0x27
#define LIBPS_CPU_OP_SLT 0x2A
#define LIBPS_CPU_OP_SLTU 0x2B

// BCOND instruction group
#define LIBPS_CPU_OP_BLTZ 0x00
#define LIBPS_CPU_OP_BGEZ 0x01
#define LIBPS_CPU_OP_BLTZAL 0x10
#define LIBPS_CPU_OP_BGEZAL 0x11

// Inherent co-processor instructions
#define LIBPS_CPU_OP_MF 0x00
#define LIBPS_CPU_OP_CF 0x02
#define LIBPS_CPU_OP_MT 0x04
#define LIBPS_CPU_OP_CT 0x06

// System control co-processor (COP0) instructions
#define LIBPS_CPU_OP_RFE 0x10

// Geometry Transformation Engine (COP2, GTE) instructions
#define LIBPS_CPU_OP_RTPS 0x01
#define LIBPS_CPU_OP_NCLIP 0x06
#define LIBPS_CPU_OP_OP 0x0C
#define LIBPS_CPU_OP_DPCS 0x10
#define LIBPS_CPU_OP_INTPL 0x11
#define LIBPS_CPU_OP_MVMVA 0x12
#define LIBPS_CPU_OP_NCDS 0x13
#define LIBPS_CPU_OP_CDP 0x14
#define LIBPS_CPU_OP_NCDT 0x16
#define LIBPS_CPU_OP_NCCS 0x1B
#define LIBPS_CPU_OP_NCS 0x1E
#define LIBPS_CPU_OP_NCT 0x20
#define LIBPS_CPU_OP_SQR 0x28
#define LIBPS_CPU_OP_DCPL 0x29
#define LIBPS_CPU_OP_DPCT 0x2A
#define LIBPS_CPU_OP_AVSZ3 0x2D
#define LIBPS_CPU_OP_AVSZ4 0x2E
#define LIBPS_CPU_OP_RTPT 0x30
#define LIBPS_CPU_OP_GPF 0x3D
#define LIBPS_CPU_OP_GPL 0x3E
#define LIBPS_CPU_OP_NCCT 0x3F

// Instruction decoders
#define LIBPS_CPU_DECODE_OP(instruction) (instruction >> 26)
#define LIBPS_CPU_DECODE_RS(instruction) ((instruction >> 21) & 0x1F)
#define LIBPS_CPU_DECODE_RT(instruction) ((instruction >> 16) & 0x1F)
#define LIBPS_CPU_DECODE_RD(instruction) ((instruction >> 11) & 0x1F)
#define LIBPS_CPU_DECODE_SHAMT(instruction) ((instruction >> 6) & 0x1F)
#define LIBPS_CPU_DECODE_FUNCT(instruction) (instruction & 0x3F)
#define LIBPS_CPU_DECODE_IMMEDIATE(instruction) (instruction & 0x0000FFFF)
#define LIBPS_CPU_DECODE_TARGET(instruction) (instruction & 0x03FFFFFF)

// Instruction decoder aliases as per MIPS conventions
#define LIBPS_CPU_DECODE_BASE(instruction) LIBPS_CPU_DECODE_RS(instruction)

#define LIBPS_CPU_DECODE_OFFSET(instruction) \
LIBPS_CPU_DECODE_IMMEDIATE(instruction)

// System control co-processor (COP0) registers
#ifdef LIBPS_DEBUG
#define LIBPS_CPU_COP0_REG_BADVADDR 8
#endif // LIBPS_DEBUG
#define LIBPS_CPU_COP0_REG_SR 12
#define LIBPS_CPU_COP0_REG_CAUSE 13
#define LIBPS_CPU_COP0_REG_EPC 14

// Status register (SR) flags
#define LIBPS_CPU_SR_IsC (1 << 16)

// Exception codes (ExcCode)
#define LIBPS_CPU_EXCCODE_Int 0
#define LIBPS_CPU_EXCCODE_Sys 8

#ifdef LIBPS_DEBUG
#define LIBPS_CPU_EXCCODE_AdEL 4
#define LIBPS_CPU_EXCCODE_AdES 5
#define LIBPS_CPU_EXCCODE_Bp 9
#define LIBPS_CPU_EXCCODE_RI 10
#define LIBPS_CPU_EXCCODE_Ov 12
#endif // LIBPS_DEBUG

#ifdef __cplusplus
}
#endif // __cplusplus
