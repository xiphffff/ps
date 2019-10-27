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
// * An illegal instruction would normally raise a Reserved Instruction (RI)
//   exception, but we simply set `good` in `libps_cpu` to `false` to specify
//   this. This is for debugging purposes; we want to preserve the state of the
//   fault as much as possible; raising an emulated RI won't tell us anything.
//
// * The PlayStation is fixed to little endian, therefore there is no support
//   for changing to big endian or even supporting it whatsoever.
//
// * There is no MMU, therefore there is no support for TLB instructions and
//   all address translations are fixed.
//
// * No support for load delays. Probably will be required for games, but
//   apparently they don't seem to be required for the BIOS.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"
#include "cpu_defs.h"

// `libps_cpu` doesn't need to know about this.
static struct libps_bus* bus;

// Throws exception `exccode`.
static void raise_exception(struct libps_cpu* cpu, const unsigned int exccode)
{
    assert(cpu != NULL);

    // Believe it or not, this is enough to get the BIOS to play nice.
    cpu->cop0_cpr[LIBPS_CPU_COP0_REG_EPC]   = cpu->pc + 4;
    cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] = exccode << 2;

    cpu->next_pc = 0x80000080;
    cpu->pc      = 0x80000080 - 4;
}

// Allocates memory for a `libps_cpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
__attribute__((warn_unused_result))
struct libps_cpu* libps_cpu_create(struct libps_bus* b)
{
    struct libps_cpu* cpu = malloc(sizeof(struct libps_cpu));

    if (cpu)
    {
        bus = b;
        return cpu;
    }
    return NULL;
}

// Deallocates the memory held by `cpu`.
void libps_cpu_destroy(struct libps_cpu* cpu)
{
    // `free()` doesn't care whether or not you pass a `NULL` pointer, but
    // `cpu` should never be `NULL` by the time we get here.
    assert(cpu != NULL);
    free(cpu);
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
    memset(cpu->gpr, 0, sizeof(cpu->gpr));

    cpu->pc      = 0xBFC00000;
    cpu->next_pc = 0xBFC00000;

    cpu->good = true;

    cpu->instruction = libps_bus_load_word(bus,
                       LIBPS_CPU_TRANSLATE_ADDRESS(cpu->pc));
}

// Executes one instruction.
void libps_cpu_step(struct libps_cpu* cpu)
{
    assert(cpu != NULL);

    cpu->pc = cpu->next_pc;
    cpu->next_pc += 4;

    switch (LIBPS_CPU_DECODE_OP(cpu->instruction))
    {
        case LIBPS_CPU_OP_GROUP_SPECIAL:
            switch (LIBPS_CPU_DECODE_FUNCT(cpu->instruction))
            {
                // `nop` is a pseudoinstruction, implemented via
                // `sll zero,zero,zero`.
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
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    (int32_t)LIBPS_CPU_DECODE_SHAMT(cpu->instruction);

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
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >>
                    (int32_t)(cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] &
                              0x0000001F);

                    break;

                case LIBPS_CPU_OP_JR:
                    cpu->next_pc =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] - 4;

                    break;

                case LIBPS_CPU_OP_JALR:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->pc + 8;

                    cpu->next_pc =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] - 4;

                    break;

                case LIBPS_CPU_OP_SYSCALL:
                    raise_exception(cpu, LIBPS_CPU_EXCCODE_Sys);
                    break;

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

                case LIBPS_CPU_OP_MULTU:
                {
                    const uint64_t result =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] *
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    cpu->reg_lo = result & 0x00000000FFFFFFFF;
                    cpu->reg_hi = result >> 32;

                    break;
                }

                case LIBPS_CPU_OP_DIV:
                    cpu->reg_lo =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] /
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    cpu->reg_hi =
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] %
                    (int32_t)cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_DIVU:
                    cpu->reg_lo =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] /
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    cpu->reg_hi =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] %
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

                case LIBPS_CPU_OP_ADD: // Temporary, overflow check is required
                case LIBPS_CPU_OP_ADDU:
                    cpu->gpr[LIBPS_CPU_DECODE_RD(cpu->instruction)] =
                    cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] +
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)];

                    break;

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
                    cpu->good = false;
                    break;
            }
            break;

        case LIBPS_CPU_OP_GROUP_BCOND:
            switch (LIBPS_CPU_DECODE_RT(cpu->instruction))
            {
                case LIBPS_CPU_OP_BLTZ:
                    if ((int32_t)(cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)]) < 0)
                    {
                        cpu->next_pc =
                        (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                        cpu->pc;
                    }
                    break;

                case LIBPS_CPU_OP_BGEZ:
                    if ((int32_t)(cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)]) >= 0)
                    {
                        cpu->next_pc =
                        (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                        cpu->pc;
                    }
                    break;

                default:
                    cpu->good = false;
                    break;
            }
            break;

        case LIBPS_CPU_OP_J:
            cpu->next_pc = ((LIBPS_CPU_DECODE_TARGET(cpu->instruction) << 2) |
                           (cpu->pc & 0xF0000000)) - 4;
            break;

        case LIBPS_CPU_OP_JAL:
            cpu->gpr[31] = cpu->pc + 8;

            cpu->next_pc = ((LIBPS_CPU_DECODE_TARGET(cpu->instruction) << 2) |
                           (cpu->pc & 0xF0000000)) - 4;
            break;

        case LIBPS_CPU_OP_BEQ:
            if (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] ==
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)])
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
            }
            break;

        case LIBPS_CPU_OP_BNE:
            if (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] !=
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)])
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
            }
            break;

        case LIBPS_CPU_OP_BLEZ:
            if ((int32_t)cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] <= 0)
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
            }
            break;

        case LIBPS_CPU_OP_BGTZ:
            if (cpu->gpr[LIBPS_CPU_DECODE_RS(cpu->instruction)] > 0)
            {
                cpu->next_pc =
                (int16_t)(LIBPS_CPU_DECODE_OFFSET(cpu->instruction) << 2) +
                cpu->pc;
            }
            break;

        case LIBPS_CPU_OP_ADDI: // Temporary, overflow check is required
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
            LIBPS_CPU_DECODE_IMMEDIATE(cpu->instruction);

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
                        // In the context of the PlayStation, `rfe` is pretty
                        // much going to function as a `nop` since everything
                        // operates in kernel mode.
                        case LIBPS_CPU_OP_RFE:
                            break;

                        default:
                            cpu->good = false;
                            break;
                    }
                    break;
            }
            break;

        case LIBPS_CPU_OP_LB:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            (int8_t)libps_bus_load_byte(bus, paddr);

            break;
        }

        case LIBPS_CPU_OP_LH:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            (int16_t)libps_bus_load_halfword(bus, paddr);

            break;
        }

        case LIBPS_CPU_OP_LWL:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            // XXX: Don't like this. We should be able to remove the switch
            // with some bitwise trickery.
            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr & !3);

            const uint32_t data = libps_bus_load_word(bus, paddr);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x00FFFFFF) |
                    (data << 24);

                    break;

                case 1:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x0000FFFF) |
                    (data << 16);

                    break;

                case 2:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x000000FF) |
                    (data << 8);

                    break;

                case 3:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x00000000) |
                    (data << 0);

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

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            libps_bus_load_word(bus, paddr);

            break;
        }

        case LIBPS_CPU_OP_LBU:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            libps_bus_load_byte(bus, paddr);

            break;
        }

        case LIBPS_CPU_OP_LHU:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
            libps_bus_load_halfword(bus, paddr);

            break;
        }

        case LIBPS_CPU_OP_LWR:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            // XXX: Don't like this. We should be able to remove the switch
            // with some bitwise trickery.
            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr & !3);

            const uint32_t data = libps_bus_load_word(bus, paddr);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x00000000) |
                    (data >> 0);

                    break;

                case 1:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0xFF000000) |
                    (data >> 8);

                    break;

                case 2:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0xFFFF0000) |
                    (data >> 16);

                    break;

                case 3:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0xFFFFFF00) |
                    (data >> 24);

                    break;
            }
            break;
        }

        case LIBPS_CPU_OP_SB:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            libps_bus_store_byte(bus,
                                 paddr,
                cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x000000FF);
            break;
        }

        case LIBPS_CPU_OP_SH:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);

            libps_bus_store_halfword(bus,
                                     paddr,
                 cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x0000FFFF);
            break;
        }

        case LIBPS_CPU_OP_SWL:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            // XXX: Don't like this. We should be able to remove the switch
            // with some bitwise trickery.
            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr & !3);

            const uint32_t data = libps_bus_load_word(bus, paddr);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >> 24) |
                    (data & 0xFFFFFF00);

                    break;

                case 1:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >> 16) |
                    (data & 0xFFFF0000);

                    break;

                case 2:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >> 8) |
                    (data & 0xFF000000);

                    break;

                case 3:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] >> 0) |
                    (data & 0x00000000);

                    break;
            }
            break;
        }

        case LIBPS_CPU_OP_SW:
        {
            if (!(cpu->cop0_cpr[LIBPS_CPU_COP0_REG_SR] & LIBPS_CPU_SR_IsC))
            {
                const uint32_t vaddr =
                (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
                cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

                const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr);
                        
                libps_bus_store_word(bus,
                                     paddr,
                              cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)]);
            }
            break;
        }

        case LIBPS_CPU_OP_SWR:
        {
            const uint32_t vaddr =
            (int16_t)LIBPS_CPU_DECODE_OFFSET(cpu->instruction) +
            cpu->gpr[LIBPS_CPU_DECODE_BASE(cpu->instruction)];

            // XXX: Don't like this. We should be able to remove the switch
            // with some bitwise trickery.
            const uint32_t paddr = LIBPS_CPU_TRANSLATE_ADDRESS(vaddr & !3);

            const uint32_t data = libps_bus_load_word(bus, paddr);

            switch (vaddr & 3)
            {
                case 0:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x00000000) |
                    (data << 0);

                    break;

                case 1:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x000000FF) |
                    (data << 8);

                    break;

                case 2:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x0000FFFF) |
                    (data << 16);

                    break;

                case 3:
                    cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] =
                    (cpu->gpr[LIBPS_CPU_DECODE_RT(cpu->instruction)] & 0x00FFFFFF) |
                    (data << 24);

                    break;
            }
            break;
        }

        default:
            cpu->good = false;
            break;
    }

    cpu->instruction = libps_bus_load_word(bus,
                       LIBPS_CPU_TRANSLATE_ADDRESS(cpu->pc += 4));

    cpu->gpr[0] = 0x00000000;
}