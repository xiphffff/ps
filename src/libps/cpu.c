// Copyright 2020 Michael Rodriguez
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

// A few key things to note here:
//
// * The PlayStation CPU is *not* a vanilla MIPS R3000, it is an LSI LR33300.
//   There are currently efforts being made to retrieve the manual for a
//   radiation hardened version of the LR33300 which should hopefully yield
//   some further insights. Though, I don't really think we're likely to find
//   too much out of the ordinary.
// 
// * All PlayStation software runs entirely in kernel mode. Accordingly, we do
//   not support any user mode separation whatsoever.
//
// * There is no support for caches, or breakpoint registers (e.g. BDA).
//
// * The PlayStation is fixed to little endian, therefore there is no support
//   for changing to big endian or even supporting it whatsoever.
//
// * There is no MMU, therefore there is no support for TLB instructions and
//   all address translations are fixed.
//
// * No support for load delays. Undoubtedly will be required for games, but
//   apparently they don't seem to be required for the BIOS.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bus.h"
#include "cpu.h"
#include "cpu_defs.h"
#include "utility/memory.h"

// `libps_cpu` doesn't need to know about this.
static struct libps_bus* bus;

// Used for calling `raise_exception()` when throwing an exception that is not
// an address exception.
#define UNUSED 0x00000000

static bool in_delay_slot = false;

// Throws exception `exccode`.
static void raise_exception(struct libps_cpu* cpu,
                            const unsigned int exccode,
                            const uint32_t bad_vaddr)
{
    assert(cpu != NULL);

    // So on an exception, the CPU:

    // 1) sets up EPC to point to the restart location.
    cpu->cop0_cpr[LIBPS_CPU_COP0_REG_EPC] =
    !in_delay_slot ? cpu->pc : cpu->pc - 4;

    // 2) The pre-existing user-mode and interrupt-enable flags in SR are saved
    //    by pushing the 3 - entry stack inside SR, and changing to kernel mode
    //    with interrupts disabled.
    cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] =
    (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & 0xFFFFFFC0) |
    ((cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & 0xF) << 2);

    // 3a) Cause is setup so that software can see the reason for the
    //     exception.
    cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] =
    (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] & ~0x0000007C) |
    (exccode << 2);

#ifdef LIBPS_DEBUG
    // 3b) On address exceptions BadVaddr is also set.
    if (exccode == LIBPS_CPU_EXCCODE_AdEL)
    {
        cpu->cop0_cpr[LIBPS_CPU_COP0_REG_BADVADDR] = bad_vaddr;
    }
#endif // LIBPS_DEBUG

    // 4) Transfers control to the exception entry point.
    cpu->next_pc = 0x80000080;
    cpu->pc      = 0x80000080 - 4;
}

// Allocates memory for a `libps_cpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
struct libps_cpu* libps_cpu_create(struct libps_bus* b)
{
    struct libps_cpu* cpu = libps_safe_malloc(sizeof(struct libps_cpu));
    bus = b;

    return cpu;
}

// Deallocates the memory held by `cpu`.
void libps_cpu_destroy(struct libps_cpu* cpu)
{
    libps_safe_free(cpu);
}

// Triggers a reset exception, thereby initializing the CPU to the predefined
// startup state.
void libps_cpu_reset(struct libps_cpu* cpu)
{
    assert(cpu != NULL);

    // The PlayStation BIOS does clear the general purpose registers early on,
    // but not early enough before it stores a bad word to 0x1F801060 and
    // 0x1F80100C if the registers are not set to zero upon reset. It wouldn't
    // affect anything as we don't handle what those memory addresses represent
    // ("RAM Size" and "Expansion 3 Delay/Size" respectively) but of course, we
    // should still clear these anyway.
    memset(cpu->gpr,      0, sizeof(cpu->gpr));
    memset(cpu->cop0_cpr, 0, sizeof(cpu->cop0_cpr));

    cpu->pc      = 0xBFC00000;
    cpu->next_pc = 0xBFC00000;

    cpu->instruction = libps_bus_load_word(bus, cpu->pc);
}

// Executes one instruction.
void libps_cpu_step(struct libps_cpu* cpu)
{
    assert(cpu != NULL);

    if (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] & (1 << 10) &&
        (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & (1 << 10)) &&
        (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & 1))
    {
        raise_exception(cpu, LIBPS_CPU_EXCCODE_Int, UNUSED);

        cpu->instruction = libps_bus_load_word(bus, cpu->pc += 4);
        return;
    }

    cpu->pc = cpu->next_pc;
    cpu->next_pc += 4;

    in_delay_slot = false;

    switch (LIBPS_CPU_DECODE_OP(cpu->instruction))
    {
        case LIBPS_CPU_OP_GROUP_SPECIAL:
            switch (LIBPS_CPU_DECODE_FUNCT(cpu->instruction))
            {
                case LIBPS_CPU_OP_SLL:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] <<
                    LIBPS_CPU_DECODE_SHAMT(cpu->instruction);

                    break;

                case LIBPS_CPU_OP_SRL:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    LIBPS_CPU_DECODE_SHAMT(cpu->instruction);

                    break;

                case LIBPS_CPU_OP_SRA:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    LIBPS_CPU_DECODE_SHAMT(cpu->instruction);

                    break;

                case LIBPS_CPU_OP_SLLV:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] <<
                    (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
                     0x0000001F);

                    break;

                case LIBPS_CPU_OP_SRLV:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
                     0x0000001F);

                    break;

                case LIBPS_CPU_OP_SRAV:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
                              0x0000001F);

                    break;

                case LIBPS_CPU_OP_JR:
                {
                    const uint32_t target =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] - 4;
#ifdef LIBPS_DEBUG
                    if ((target & 0x00000003) != 0)
                    {
                        raise_exception(cpu, LIBPS_CPU_EXCCODE_AdEL, target);
                        break;
                    }
#endif // LIBPS_DEBUG
                    cpu->next_pc = target;
                    in_delay_slot = true;

                    break;
                }

                case LIBPS_CPU_OP_JALR:
                {
                    const uint32_t target =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] - 4;
#ifdef LIBPS_DEBUG
                    if ((target & 0x00000003) != 0)
                    {
                        raise_exception(cpu, LIBPS_CPU_EXCCODE_AdEL, target);
                        break;
                    }
#endif // LIBPS_DEBUG
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->pc + 8;

                    cpu->next_pc = target;
                    in_delay_slot = true;

                    break;
                }

                case LIBPS_CPU_OP_SYSCALL:
                    raise_exception(cpu, LIBPS_CPU_EXCCODE_Sys, UNUSED);
                    break;

#ifdef LIBPS_DEBUG
                case LIBPS_CPU_OP_BREAK:
                    raise_exception(cpu, LIBPS_CPU_EXCCODE_Bp, UNUSED);
                    break;
#endif // LIBPS_DEBUG

                case LIBPS_CPU_OP_MFHI:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->reg_hi;

                    break;

                case LIBPS_CPU_OP_MTHI:
                    cpu->reg_hi =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_MFLO:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->reg_lo;

                    break;

                case LIBPS_CPU_OP_MTLO:
                    cpu->reg_lo =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_MULT:
                {
                    const uint64_t result =
                    (int64_t)(int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] *
                    (int64_t)(int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    cpu->reg_lo = result & 0x00000000FFFFFFFF;
                    cpu->reg_hi = result >> 32;

                    break;
                }

                case LIBPS_CPU_OP_MULTU:
                {
                    const uint64_t result =
                    (uint64_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] *
                    (uint64_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    cpu->reg_lo = result & 0x00000000FFFFFFFF;
                    cpu->reg_hi = result >> 32;

                    break;
                }

                case LIBPS_CPU_OP_DIV:
                {
                    // The result of a division by zero is consistent with the
                    // result of a simple radix-2 ("one bit at a time")
                    // implementation.
                    const int32_t rt =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    const int32_t rs =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];

#ifdef LIBPS_DEBUG
                    // Divisor is zero
                    if (rt == 0)
                    {
                        // If the dividend is negative, the quotient is 1
                        // (0x00000001), and if the dividend is positive or
                        // zero, the quotient is -1 (0xFFFFFFFF).
                        cpu->reg_lo = (rs < 0) ? 0x00000001 : 0xFFFFFFFF;

                        // In both cases the remainder equals the dividend.
                        cpu->reg_hi = (uint32_t)rs;
                    }
                    // Will trigger an arithmetic exception when dividing
                    // 0x80000000 by 0xFFFFFFFF. The result of the division is
                    // a quotient of 0x80000000 and a remainder of 0x00000000.
                    else if ((uint32_t)rs == 0x80000000 &&
                             (uint32_t)rt == 0xFFFFFFFF)
                    {
                        cpu->reg_lo = (uint32_t)rs;
                        cpu->reg_hi = 0x00000000;
                    }
                    else
                    {
                        cpu->reg_lo = rs / rt;
                        cpu->reg_hi = rs % rt;
                    }
#else
                    cpu->reg_lo = rs / rt;
                    cpu->reg_hi = rs % rt;
#endif // LIBPS_DEBUG
                    break;
                }

                case LIBPS_CPU_OP_DIVU:
                {
                    const uint32_t rt =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];
                    
                    const uint32_t rs =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];
#ifdef LIBPS_DEBUG
                    // In the case of unsigned division, the dividend can't be
                    // negative and thus the quotient is always -1 (0xFFFFFFFF)
                    // and the remainder equals the dividend.
                    if (rt == 0)
                    {
                        cpu->reg_lo = 0xFFFFFFFF;
                        cpu->reg_hi = rs;
                    }
                    else
                    {
                        cpu->reg_lo = rs / rt;
                        cpu->reg_hi = rs % rt;
                    }
#else
                    cpu->reg_lo = rs / rt;
                    cpu->reg_hi = rs % rt;
#endif // LIBPS_DEBUG
                    break;
                }

                case LIBPS_CPU_OP_ADD:
#ifdef LIBPS_DEBUG
                {
                    const uint32_t rs =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];
                    
                    const uint32_t rt =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    const uint32_t result = rs + rt;

                    if (!((rs ^ rt) & 0x80000000) &&
                        ((result ^ rs) & 0x80000000))
                    {
                        raise_exception(cpu, LIBPS_CPU_EXCCODE_Ov, UNUSED);
                        break;
                    }

                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] = result;
                    break;
                }
#endif // LIBPS_DEBUG

                case LIBPS_CPU_OP_ADDU:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] +
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_SUB:
#ifdef LIBPS_DEBUG
                {
                    const uint32_t rs = cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];
                    const uint32_t rt = cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    const uint32_t result = rs - rt;

                    if (((rs ^ rt) & 0x80000000) && ((result ^ rs) & 0x80000000))
                    {
                        raise_exception(cpu, LIBPS_CPU_EXCCODE_Ov, UNUSED);
                        break;
                    }

                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] = result;
                    break;
                }
#endif // LIBPS_DEBUG

                case LIBPS_CPU_OP_SUBU:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] -
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_AND:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_OR:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] |
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_XOR:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] ^
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_NOR:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                  ~(cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] |
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)]);

                    break;

                case LIBPS_CPU_OP_SLT:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_SLTU:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                default:
#ifdef LIBPS_DEBUG
                    raise_exception(cpu, LIBPS_CPU_EXCCODE_RI, UNUSED);
#endif // LIBPS_DEBUG
                    break;
            }
            break;

        case LIBPS_CPU_OP_GROUP_BCOND:
        {
            const unsigned int op = LIBPS_CPU_DECODE_RT(cpu->instruction);

            const bool should_link = (op & 0x1E) == 0x10;

            const bool should_branch =
            (int32_t)(cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] ^ (op << 31)) < 0;

            if (should_link) cpu->gpr[31] = cpu->pc + 8;

            if (should_branch)
            {
                cpu->next_pc =
                (uint32_t)(int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
                in_delay_slot = true;
            }
            break;
        }

        case LIBPS_CPU_OP_J:
            cpu->next_pc = ((LIBPS_CPU_DECODE_TARGET(cpu->instruction) << 2) |
                           (cpu->pc & 0xF0000000)) - 4;
            in_delay_slot = true;
            break;

        case LIBPS_CPU_OP_JAL:
            cpu->gpr[31] = cpu->pc + 8;

            cpu->next_pc = ((LIBPS_CPU_DECODE_TARGET(cpu->instruction) << 2) |
                           (cpu->pc & 0xF0000000)) - 4;
            in_delay_slot = true;
            break;

        case LIBPS_CPU_OP_BEQ:
            if (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] ==
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)])
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
                in_delay_slot = true;
            }
            break;

        case LIBPS_CPU_OP_BNE:
            if (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] !=
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)])
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
                in_delay_slot = true;
            }
            break;

        case LIBPS_CPU_OP_BLEZ:
            if ((int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <= 0)
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
                in_delay_slot = true;
            }
            break;

        case LIBPS_CPU_OP_BGTZ:
            if ((int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] > 0)
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
                in_delay_slot = true;
            }
            break;

        case LIBPS_CPU_OP_ADDI:
#ifdef LIBPS_DEBUG
        {
            const uint32_t imm = (uint32_t)(int16_t)LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);
            const uint32_t rs  = cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)];

            const uint32_t result = imm + rs;

            if (!((rs ^ imm) & 0x80000000) && ((result ^ rs) & 0x80000000))
            {
                raise_exception(cpu, LIBPS_CPU_EXCCODE_Ov, UNUSED);
                break;
            }
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = result;
            break;
        }
#endif // LIBPS_DEBUG

        case LIBPS_CPU_OP_ADDIU:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] +
            (int16_t)LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_SLTI:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <
            (int16_t)LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_SLTIU:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <
            (uint32_t)(int16_t)LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_ANDI:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
            LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_ORI:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] |
            LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_XORI:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] ^
            LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

            break;

        case LIBPS_CPU_OP_LUI:
            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction) << 16;

            break;

        case LIBPS_CPU_OP_GROUP_COP0:
            switch (LIBPS_CPU_DECODE_RS(cpu->instruction))
            {
                case LIBPS_CPU_OP_MF:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    cpu->cop0_cpr[LIBPS_CPU_DECODE_RD(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_MT:
                    cpu->cop0_cpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                default:
                    switch (LIBPS_CPU_DECODE_FUNCT(cpu->instruction))
                    {
                        case LIBPS_CPU_OP_RFE:
                            cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] =
                            (cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & 0xFFFFFFF0) |
                            ((cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & 0x3C) >> 2);

                            break;

                        default:
#ifdef LIBPS_DEBUG
                            raise_exception(cpu, LIBPS_CPU_EXCCODE_RI, UNUSED);
#endif // LIBPS_DEBUG
                            break;
                    }
                    break;
            }
            break;

        case LIBPS_CPU_OP_GROUP_COP2:
            break;

        case LIBPS_CPU_OP_LB:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const int8_t data = (int8_t)libps_bus_load_byte(bus, vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = data;
            break;
        }

        case LIBPS_CPU_OP_LH:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];
#ifdef LIBPS_DEBUG
            if ((vaddr & 1) != 0)
            {
                raise_exception(cpu, LIBPS_CPU_EXCCODE_AdEL, vaddr);
                break;
            }
#endif // LIBPS_DEBUG
            const int16_t data = (int16_t)libps_bus_load_halfword(bus, vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = data;
            break;
        }

        case LIBPS_CPU_OP_LWL:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t data = libps_bus_load_word(bus, vaddr & 0xFFFFFFFC);

            const unsigned int rt = LIBPS_CPU_DECODE_RT(cpu->instruction);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0x00FFFFFF) | (data << 24);
                    break;

                case 1:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0x0000FFFF) | (data << 16);
                    break;

                case 2:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0x000000FF) | (data << 8);
                    break;

                case 3:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0x00000000) | (data << 0);
                    break;
            }
            break;
        }

        // WARNING: At BIOS address `0x80059CA0`, there is an instruction that
        // loads GPUSTAT to $zero for no clear reason, presumably a write to
        // $zero is just a weird way to perform a `nop`.
        case LIBPS_CPU_OP_LW:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

#ifdef LIBPS_DEBUG
            if ((vaddr & 0x00000003) != 0)
            {
                raise_exception(cpu, LIBPS_CPU_EXCCODE_AdEL, vaddr);
                break;
            }
#endif // LIBPS_DEBUG

            const uint32_t data = libps_bus_load_word(bus, vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = data;
            break;
        }

        case LIBPS_CPU_OP_LBU:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint8_t data = libps_bus_load_byte(bus, vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = data;
            break;
        }

        case LIBPS_CPU_OP_LHU:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];
#ifdef LIBPS_DEBUG
            if ((vaddr & 1) != 0)
            {
                raise_exception(cpu, LIBPS_CPU_EXCCODE_AdEL, vaddr);
                break;
            }
#endif // LIBPS_DEBUG

            const uint16_t data = libps_bus_load_halfword(bus, vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] = data;
            break;
        }

        case LIBPS_CPU_OP_LWR:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t data = libps_bus_load_word(bus, vaddr & 0xFFFFFFFC);

            const unsigned int rt = LIBPS_CPU_DECODE_RT(cpu->instruction);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[rt] = data;
                    break;

                case 1:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0xFF000000) | (data >> 8);
                    break;

                case 2:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0xFFFF0000) | (data >> 16);
                    break;

                case 3:
                    cpu->gpr[rt] = (cpu->gpr[rt] & 0xFFFFFF00) | (data >> 24);
                    break;
            }
            break;
        }

        case LIBPS_CPU_OP_SB:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            libps_bus_store_byte(bus,
                                 vaddr,
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x000000FF);
            break;
        }

        case LIBPS_CPU_OP_SH:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

#ifdef LIBPS_DEBUG
            if ((vaddr & 1) != 0)
            {
                raise_exception(cpu, LIBPS_CPU_EXCCODE_AdES, vaddr);
                break;
            }
#endif // LIBPS_DEBUG

            libps_bus_store_halfword(bus,
                                     vaddr,
                 cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x0000FFFF);
            break;
        }

        case LIBPS_CPU_OP_SWL:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const unsigned int rt = LIBPS_CPU_DECODE_RT(cpu->instruction);

            uint32_t data = libps_bus_load_word(bus, vaddr & 0xFFFFFFFC);

            switch (vaddr & 3)
            {
                case 0:
                    data = (data & 0xFFFFFF00) | (cpu->gpr[rt] >> 24);
                    break;

                case 1:
                    data = (data & 0xFFFF0000) | (cpu->gpr[rt] >> 16);
                    break;

                case 2:
                    data = (data & 0xFF000000) | (cpu->gpr[rt] >> 8);

                    break;

                case 3:
                    data = (data & 0x00000000) | (cpu->gpr[rt] >> 0);
                    break;
            }

            libps_bus_store_word(bus, vaddr & 0xFFFFFFFC, data);
            break;
        }

        case LIBPS_CPU_OP_SW:
        {
            if (!(cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & LIBPS_CPU_SR_IsC))
            {
                const uint32_t vaddr =
                (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
                cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

#ifdef LIBPS_DEBUG
                if ((vaddr & 0x00000003) != 0)
                {
                    raise_exception(cpu, LIBPS_CPU_EXCCODE_AdES, vaddr);
                    break;
                }
#endif // LIBPS_DEBUG

                libps_bus_store_word(bus,
                                     vaddr,
                              cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)]);
            }
            break;
        }

        case LIBPS_CPU_OP_SWR:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const unsigned int rt = LIBPS_CPU_DECODE_RT(cpu->instruction);

            uint32_t data = libps_bus_load_word(bus, vaddr & 0xFFFFFFFC);

            switch (vaddr & 3)
            {
                case 0:
                    data = (data & 0x00000000) | (cpu->gpr[rt] << 0);
                    break;

                case 1:
                    data = (data & 0x000000FF) | (cpu->gpr[rt] << 8);
                    break;

                case 2:
                    data = (data & 0x0000FFFF) | (cpu->gpr[rt] << 16);
                    break;

                case 3:
                    data = (data & 0x00FFFFFF) | (cpu->gpr[rt] << 24);
                    break;
            }

            libps_bus_store_word(bus, vaddr & 0xFFFFFFFC, data);
            break;
        }

        default:
#ifdef LIBPS_DEBUG
            raise_exception(cpu, LIBPS_CPU_EXCCODE_RI, UNUSED);
#endif // LIBPS_DEBUG
            break;
    }

    cpu->instruction = libps_bus_load_word(bus, cpu->pc += 4);
    cpu->gpr[0] = 0x00000000;
}
