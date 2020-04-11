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

// Some important things to note:
//
// * No support for user mode or the co-processor usability exception (CpU).
//   All software runs in kernel mode.
//
// * There is no memory management unit (MMU). All address translations are
//   fixed and any TLB related instruction will raise a Reserved Instruction
//   (RI) exception.
//
// * Floating point co-processor (COP1) is not present. All instructions
//   related to COP1 will raise a Reserved Instruction (RI) exception.
//
// * Debug registers (TAR, DCIC, etc) are not implemented.
//
// * Caches are not implemented.

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"
#include "utility/math.h"

// `psemu_cpu` does not need to know about this.
static struct psemu_bus* bus = NULL;

// Set to `true` if a branch has been taken, or `false` otherwise.
static bool in_delay_slot = false;

// Unsigned Newton-Raphson (UNR) division table
static uint8_t division_table[257];

// Pass this as the 3rd parameter to `raise_exception()` if the exception code
// passed is not an address exception (i.e. AdEL or AdES).
#define UNUSED_PARAMETER 0x00000000

// Unsigned GTE limiters.
#define limA1U(cpu, value) gte_unsigned_lim(cpu, value, 24, 32767)
#define limA2U(cpu, value) gte_unsigned_lim(cpu, value, 23, 32767)
#define limA3U(cpu, value) gte_unsigned_lim(cpu, value, 22, 32767)
#define limB1(cpu, value) gte_unsigned_lim(cpu, value, 21, 255)
#define limB2(cpu, value) gte_unsigned_lim(cpu, value, 20, 255)
#define limB3(cpu, value) gte_unsigned_lim(cpu, value, 19, 255)
#define limC(cpu, value) gte_unsigned_lim(cpu, value, 18, 65535)
#define limE(cpu, value) gte_unsigned_lim(cpu, value, 12, 4095)

// Signed GTE limiters.
#define limA1S(cpu, value) gte_signed_lim(cpu, value, 24, -32768, 32767)
#define limA2S(cpu, value) gte_signed_lim(cpu, value, 23, -32768, 32767)
#define limA3S(cpu, value) gte_signed_lim(cpu, value, 22, -32768, 32767)
#define limD1(cpu, value) gte_signed_lim(cpu, value, 14, -1024, 1023)
#define limD2(cpu, value) gte_signed_lim(cpu, value, 13, -1024, 1023)

// GTE limiters dependent on the `lim` argument.
#define limA1C(cpu, value) gte_lim(cpu, value, 24)
#define limA2C(cpu, value) gte_lim(cpu, value, 23)
#define limA3C(cpu, value) gte_lim(cpu, value, 22)

// Clamps `value` to `limit` if `value` has exceeded `limit`, and sets FLAG bit
// `bit` if so, and returns `value`.
static uint32_t gte_unsigned_lim(struct psemu_cpu* const cpu,
                                 uint32_t value,
                                 const unsigned int bit,
                                 const unsigned int limit)
{
    assert(cpu != NULL);
    assert(bit <= 31);

    if (value > limit)
    {
        value = limit;
        cpu->cop2.FLAG |= (1 << bit);
    }
    return value;
}

// Clamps `value` to `lower_limit` or `upper_limit` if `value` exceeds or is
// under either, and sets FLAG bit `bit`. Returns `value.`
static int32_t gte_signed_lim(struct psemu_cpu* const cpu,
                              int32_t value,
                              const unsigned int bit,
                              const signed int lower_limit,
                              const unsigned int upper_limit)
{
    assert(cpu != NULL);
    assert(bit <= 31);

    if (value < lower_limit)
    {
        value = lower_limit;
        cpu->cop2.FLAG |= (1 << bit);
    }
    else if (value > upper_limit)
    {
        value = upper_limit;
        cpu->cop2.FLAG |= (1 << bit);
    }
    return value;
}

// Clamps `value` in an unsigned or signed manner depending on the value of the
// `lm` argument in the instruction.
static int32_t gte_lim(struct psemu_cpu* const cpu, int32_t value, const unsigned int bit)
{
    assert(cpu != NULL);
    assert(bit <= 31);

    if (cpu->instruction.word & (1 << 10))
    {
        return gte_unsigned_lim(cpu, value, bit, 32767);
    }
    return gte_signed_lim(cpu, value, bit, -32768, 32767);
}

// Handles perspective transformation.
static void gte_rpt(struct psemu_cpu* const cpu)
{
    int n = 0x1FFFF;
#if 0
    if (H < (SZ3 * 2))
    {
        int z = __builtin_clz(SZ3);
        n = (H << z);
        int d = (SZ3 << z);
        uint8_t u = division_table[(d - 0x7FC0) >> 7] + 0x101;
        d = ((0x2000080 - (d * u)) >> 8);
        d = ((0x0000080 + (d * u)) >> 8);
        n = psemu_min(0x1FFFF, (((n * d) + 0x8000) >> 16));
    }
    else
    {
        n = 0x1FFFF;
    }
#endif
}

// Handles light source calculation.
static void gte_ncd(struct psemu_cpu* const cpu)
{ }

// Handles the `nclip` GTE instruction.
static void gte_nclip(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);
}

// Handles the `ncds` GTE instruction.
static void gte_ncds(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);
}

// Handles the `avsz3` GTE instruction.
static void gte_avsz3(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);
    __debugbreak();
}

// Handles the `rtpt` GTE instruction.
static void gte_rtpt(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);
}

// Returns the current virtual address in LSI LR33300 CPU interpreter `cpu`.
static inline uint32_t vaddr(const struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);

    return (int16_t)(PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word)) +
           cpu->gpr[cpu->instruction.rs];
}

static void branch_if(struct psemu_cpu* const cpu, const bool condition_met)
{
    assert(cpu != NULL);

    if (condition_met)
    {
        cpu->next_pc =
        (int16_t)(PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word) << 2) +
        cpu->pc;

        in_delay_slot = true;
    }
}

// Raises an exception `exc_code` on an LSI LR33300 CPU interpreter `cpu`.
// `bad_vaddr` is required to be passed in the event of an address exception.
// If an address exception is not passed, pass `UNUSED_PARAMETER` for the
// `bad_vaddr` parameter.
static void raise_exception(struct psemu_cpu* const cpu,
                            const unsigned int exc_code,
                            const uint32_t bad_vaddr)
{
    assert(cpu != NULL);

    // On an exception, the CPU:

    // 1) sets up EPC to point to the restart location.
    cpu->cop0[PSEMU_CPU_COP0_EPC] = !in_delay_slot ? cpu->pc : cpu->pc - 4;

    // 2) The pre-existing user mode and interrupt enable flags in SR are saved
    //    by pushing the 3 entry stack inside SR, and changing to kernel mode
    //    with interrupts disabled.
    cpu->cop0[PSEMU_CPU_COP0_SR] =
    (cpu->cop0[PSEMU_CPU_COP0_SR] & 0xFFFFFFC0) |
    ((cpu->cop0[PSEMU_CPU_COP0_SR] & 0x0000000F) << 2);

    // 3a) Cause is setup so that software can see the reason for the
    //     exception.
    cpu->cop0[PSEMU_CPU_COP0_Cause] =
    (cpu->cop0[PSEMU_CPU_COP0_Cause] & ~0xFFFF00FF) | (exc_code << 2);

    // 3b) On address exceptions BadA is also set.
    if (exc_code == PSEMU_CPU_EXCCODE_AdEL ||
        exc_code == PSEMU_CPU_EXCCODE_AdES)
    {
        cpu->cop0[PSEMU_CPU_COP0_BadA] = bad_vaddr;
    }

    // 4) Transfers control to the exception entry point.
    cpu->pc      = 0x80000080 - 4;
    cpu->next_pc = 0x80000080;
}

static uint32_t load_cop2_cpr(const struct psemu_cpu* const cpu,
                              const unsigned int reg)
{
    assert(cpu != NULL);
    assert(reg <= 31);

    switch (reg)
    {
        case PSEMU_CPU_COP2_VXY0: return cpu->cop2.VXY0.word;
        case PSEMU_CPU_COP2_VZ0:  return cpu->cop2.VZ0.word;
        case PSEMU_CPU_COP2_VXY1: return cpu->cop2.VXY1.word;
        case PSEMU_CPU_COP2_VZ1:  return cpu->cop2.VZ1.word;
        case PSEMU_CPU_COP2_VXY2: return cpu->cop2.VXY2.word;
        case PSEMU_CPU_COP2_VZ2:  return cpu->cop2.VZ2.word;
        case PSEMU_CPU_COP2_RGB:  return cpu->cop2.RGB.word;
        case PSEMU_CPU_COP2_OTZ:  return cpu->cop2.OTZ.word;
        case PSEMU_CPU_COP2_IR0:  return cpu->cop2.IR0.word;
        case PSEMU_CPU_COP2_IR1:  return cpu->cop2.IR1.word;
        case PSEMU_CPU_COP2_IR2:  return cpu->cop2.IR2.word;
        case PSEMU_CPU_COP2_IR3:  return cpu->cop2.IR3.word;
        case PSEMU_CPU_COP2_SXY0: return cpu->cop2.SXY0.word;
        case PSEMU_CPU_COP2_SXY1: return cpu->cop2.SXY1.word;
        case PSEMU_CPU_COP2_SXY2: return cpu->cop2.SXY2.word;
        case PSEMU_CPU_COP2_SXYP: return cpu->cop2.SXYP.word;
        case PSEMU_CPU_COP2_SZ0:  return cpu->cop2.SZ0.word;
        case PSEMU_CPU_COP2_SZ1:  return cpu->cop2.SZ1.word;
        case PSEMU_CPU_COP2_SZ2:  return cpu->cop2.SZ2.word;
        case PSEMU_CPU_COP2_SZ3:  return cpu->cop2.SZ3.word;
        case PSEMU_CPU_COP2_RGB0: return cpu->cop2.RGB0.word;
        case PSEMU_CPU_COP2_RGB1: return cpu->cop2.RGB1.word;
        case PSEMU_CPU_COP2_RGB2: return cpu->cop2.RGB2.word;
        case PSEMU_CPU_COP2_MAC0: return cpu->cop2.MAC0;
        case PSEMU_CPU_COP2_MAC1: return cpu->cop2.MAC1;
        case PSEMU_CPU_COP2_IRGB: return cpu->cop2.IRGB.word;
        case PSEMU_CPU_COP2_ORGB: return cpu->cop2.ORGB.word;
        case PSEMU_CPU_COP2_LZCS: return cpu->cop2.LZCS;
        case PSEMU_CPU_COP2_LZCR: return cpu->cop2.LZCR.word;
        default:                  return 0x00000000;
    }
}

static uint32_t load_cop2_ccr(const struct psemu_cpu* const cpu,
                              const unsigned int reg)
{
    assert(cpu != NULL);
    assert(reg <= 31);

    switch (reg)
    {
        case PSEMU_CPU_COP2_R11R12: return cpu->cop2.R11R12.word;
        case PSEMU_CPU_COP2_R13R21: return cpu->cop2.R13R21.word;
        case PSEMU_CPU_COP2_R22R23: return cpu->cop2.R22R23.word;
        case PSEMU_CPU_COP2_R31R32: return cpu->cop2.R31R32.word;
        case PSEMU_CPU_COP2_R33:    return cpu->cop2.R33.word;
        case PSEMU_CPU_COP2_TRX:    return cpu->cop2.TRX;
        case PSEMU_CPU_COP2_TRY:    return cpu->cop2.TRY;
        case PSEMU_CPU_COP2_TRZ:    return cpu->cop2.TRZ;
        case PSEMU_CPU_COP2_L11L12: return cpu->cop2.L11L12.word;
        case PSEMU_CPU_COP2_L13L21: return cpu->cop2.L13L21.word;
        case PSEMU_CPU_COP2_L22L23: return cpu->cop2.L22L23.word;
        case PSEMU_CPU_COP2_L31L32: return cpu->cop2.L31L32.word;
        case PSEMU_CPU_COP2_L33:    return cpu->cop2.L33.word;
        case PSEMU_CPU_COP2_RBK:    return cpu->cop2.RBK;
        case PSEMU_CPU_COP2_GBK:    return cpu->cop2.GBK;
        case PSEMU_CPU_COP2_BBK:    return cpu->cop2.BBK;
        case PSEMU_CPU_COP2_LR1LR2: return cpu->cop2.LR1LR2.word;
        case PSEMU_CPU_COP2_LR3LG1: return cpu->cop2.LR3LG1.word;
        case PSEMU_CPU_COP2_LG2LG3: return cpu->cop2.LG2LG3.word;
        case PSEMU_CPU_COP2_LB1LB2: return cpu->cop2.LB1LB2.word;
        case PSEMU_CPU_COP2_LB3:    return cpu->cop2.LB3.word;
        case PSEMU_CPU_COP2_RFC:    return cpu->cop2.RFC;
        case PSEMU_CPU_COP2_GFC:    return cpu->cop2.GFC;
        case PSEMU_CPU_COP2_BFC:    return cpu->cop2.BFC;
        case PSEMU_CPU_COP2_OFX:    return cpu->cop2.OFX;
        case PSEMU_CPU_COP2_OFY:    return cpu->cop2.OFY;
        case PSEMU_CPU_COP2_H:      return cpu->cop2.H.word;
        case PSEMU_CPU_COP2_DQA:    return cpu->cop2.DQA.word;
        case PSEMU_CPU_COP2_DQB:    return cpu->cop2.DQB;
        case PSEMU_CPU_COP2_ZSF3:   return cpu->cop2.ZSF3.word;
        case PSEMU_CPU_COP2_ZSF4:   return cpu->cop2.ZSF4.word;
        case PSEMU_CPU_COP2_FLAG:   return cpu->cop2.FLAG;
        default:                    return 0x00000000;
    }
}

static void store_cop2_cpr(struct psemu_cpu* const cpu,
                           const unsigned int reg,
                           const uint32_t value)
{
    assert(cpu != NULL);
    assert(reg <= 31);

    switch (reg)
    {
        case PSEMU_CPU_COP2_VXY0: cpu->cop2.VXY0.word = value; return;
        case PSEMU_CPU_COP2_VZ0:  cpu->cop2.VZ0.word  = value; return;
        case PSEMU_CPU_COP2_VXY1: cpu->cop2.VXY1.word = value; return;
        case PSEMU_CPU_COP2_VZ1:  cpu->cop2.VZ1.word  = value; return;
        case PSEMU_CPU_COP2_VXY2: cpu->cop2.VXY2.word = value; return;
        case PSEMU_CPU_COP2_VZ2:  cpu->cop2.VZ2.word  = value; return;
        case PSEMU_CPU_COP2_RGB:  cpu->cop2.RGB.word  = value; return;
        case PSEMU_CPU_COP2_OTZ:  cpu->cop2.OTZ.word  = value; return;
        case PSEMU_CPU_COP2_IR0:  cpu->cop2.IR0.word  = value; return;
        case PSEMU_CPU_COP2_IR1:  cpu->cop2.IR1.word  = value; return;
        case PSEMU_CPU_COP2_IR2:  cpu->cop2.IR2.word  = value; return;
        case PSEMU_CPU_COP2_IR3:  cpu->cop2.IR3.word  = value; return;
        case PSEMU_CPU_COP2_SXY0: cpu->cop2.SXY0.word = value; return;
        case PSEMU_CPU_COP2_SXY1: cpu->cop2.SXY1.word = value; return;
        case PSEMU_CPU_COP2_SXY2: cpu->cop2.SXY2.word = value; return;
        case PSEMU_CPU_COP2_SXYP: cpu->cop2.SXYP.word = value; return;
        case PSEMU_CPU_COP2_SZ0:  cpu->cop2.SZ0.word  = value; return;
        case PSEMU_CPU_COP2_SZ1:  cpu->cop2.SZ1.word  = value; return;
        case PSEMU_CPU_COP2_SZ2:  cpu->cop2.SZ2.word  = value; return;
        case PSEMU_CPU_COP2_SZ3:  cpu->cop2.SZ3.word  = value; return;
        case PSEMU_CPU_COP2_RGB0: cpu->cop2.RGB0.word = value; return;
        case PSEMU_CPU_COP2_RGB1: cpu->cop2.RGB1.word = value; return;
        case PSEMU_CPU_COP2_RGB2: cpu->cop2.RGB2.word = value; return;
        case PSEMU_CPU_COP2_MAC0: cpu->cop2.MAC0      = value; return;
        case PSEMU_CPU_COP2_MAC1: cpu->cop2.MAC1      = value; return;
        case PSEMU_CPU_COP2_IRGB: cpu->cop2.IRGB.word = value; return;
        case PSEMU_CPU_COP2_ORGB: cpu->cop2.ORGB.word = value; return;
        case PSEMU_CPU_COP2_LZCS: cpu->cop2.LZCS      = value; return;
        case PSEMU_CPU_COP2_LZCR: cpu->cop2.LZCR.word = value; return;
    }
}

static void store_cop2_ccr(struct psemu_cpu* const cpu,
                           const unsigned int reg,
                           const uint32_t value)
{
    assert(cpu != NULL);
    assert(reg <= 31);

    switch (reg)
    {
        case PSEMU_CPU_COP2_R11R12: cpu->cop2.R11R12.word = value; return;
        case PSEMU_CPU_COP2_R13R21: cpu->cop2.R13R21.word = value; return;
        case PSEMU_CPU_COP2_R22R23: cpu->cop2.R22R23.word = value; return;
        case PSEMU_CPU_COP2_R31R32: cpu->cop2.R31R32.word = value; return;
        case PSEMU_CPU_COP2_R33:    cpu->cop2.R33.word    = value; return;
        case PSEMU_CPU_COP2_TRX:    cpu->cop2.TRX         = value; return;
        case PSEMU_CPU_COP2_TRY:    cpu->cop2.TRY         = value; return;
        case PSEMU_CPU_COP2_TRZ:    cpu->cop2.TRZ         = value; return;
        case PSEMU_CPU_COP2_L11L12: cpu->cop2.L11L12.word = value; return;
        case PSEMU_CPU_COP2_L13L21: cpu->cop2.L13L21.word = value; return;
        case PSEMU_CPU_COP2_L22L23: cpu->cop2.L22L23.word = value; return;
        case PSEMU_CPU_COP2_L31L32: cpu->cop2.L31L32.word = value; return;
        case PSEMU_CPU_COP2_L33:    cpu->cop2.L33.word    = value; return;
        case PSEMU_CPU_COP2_RBK:    cpu->cop2.RBK         = value; return;
        case PSEMU_CPU_COP2_GBK:    cpu->cop2.GBK         = value; return;
        case PSEMU_CPU_COP2_BBK:    cpu->cop2.BBK         = value; return;
        case PSEMU_CPU_COP2_LR1LR2: cpu->cop2.LR1LR2.word = value; return;
        case PSEMU_CPU_COP2_LR3LG1: cpu->cop2.LR3LG1.word = value; return;
        case PSEMU_CPU_COP2_LG2LG3: cpu->cop2.LG2LG3.word = value; return;
        case PSEMU_CPU_COP2_LB1LB2: cpu->cop2.LB1LB2.word = value; return;
        case PSEMU_CPU_COP2_LB3:    cpu->cop2.LB3.word    = value; return;
        case PSEMU_CPU_COP2_RFC:    cpu->cop2.RFC         = value; return;
        case PSEMU_CPU_COP2_GFC:    cpu->cop2.GFC         = value; return;
        case PSEMU_CPU_COP2_BFC:    cpu->cop2.BFC         = value; return;
        case PSEMU_CPU_COP2_OFX:    cpu->cop2.OFX         = value; return;
        case PSEMU_CPU_COP2_OFY:    cpu->cop2.OFY         = value; return;
        case PSEMU_CPU_COP2_H:      cpu->cop2.H.word      = value; return;
        case PSEMU_CPU_COP2_DQA:    cpu->cop2.DQA.word    = value; return;
        case PSEMU_CPU_COP2_DQB:    cpu->cop2.DQB         = value; return;
        case PSEMU_CPU_COP2_ZSF3:   cpu->cop2.ZSF3.word   = value; return;
        case PSEMU_CPU_COP2_ZSF4:   cpu->cop2.ZSF4.word   = value; return;
        case PSEMU_CPU_COP2_FLAG:   cpu->cop2.FLAG        = value; return;
    }
}
// Sets the system bus to use to `m_bus`.
void psemu_cpu_set_bus(struct psemu_bus* const m_bus)
{
    assert(m_bus != NULL);
    bus = m_bus;

    for (unsigned int i = 0; i < 257; ++i)
    {
        // Generate UNR division table.
        division_table[i] =
        psemu_max(0, (0x40000 / (i + 0x100) + 1) / 2 - 0x101);
    }
}

// Resets an LSI LR33300 CPU interpreter `cpu` to the startup state.
void psemu_cpu_reset(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);

    cpu->pc      = 0xBFC00000;
    cpu->next_pc = 0xBFC00000;

    cpu->instruction.word = psemu_bus_load_word(bus, cpu->pc);

    memset(cpu->gpr,   0x00000000, sizeof(cpu->gpr));
    memset(cpu->cop0,  0x00000000, sizeof(cpu->cop0));
    memset(&cpu->cop2, 0x00000000, sizeof(cpu->cop2));
}

// Executes one instruction.
void psemu_cpu_step(struct psemu_cpu* const cpu)
{
    assert(cpu != NULL);

    if (cpu->cop0[PSEMU_CPU_COP0_Cause] & PSEMU_CPU_CAUSE_INT0 &&
       (cpu->cop0[PSEMU_CPU_COP0_SR]    & PSEMU_CPU_SR_INT0)   &&
       (cpu->cop0[PSEMU_CPU_COP0_SR]    & PSEMU_CPU_SR_IEc))
    {
        raise_exception(cpu, PSEMU_CPU_EXCCODE_Int, UNUSED_PARAMETER);

        cpu->instruction.word = psemu_bus_load_word(bus, cpu->pc += 4);
        return;
    }

    in_delay_slot = false;

    cpu->pc = cpu->next_pc;
    cpu->next_pc += 4;

    switch (cpu->instruction.op)
    {
        case PSEMU_CPU_OP_GROUP_SPECIAL:
            switch (cpu->instruction.funct)
            {
                case PSEMU_CPU_OP_SLL:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rt] <<
                    cpu->instruction.shamt;

                    break;

                case PSEMU_CPU_OP_SRL:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rt] >>
                    cpu->instruction.shamt;

                    break;

                case PSEMU_CPU_OP_SRA:
                    cpu->gpr[cpu->instruction.rd] =
                    (int32_t)cpu->gpr[cpu->instruction.rt] >>
                    cpu->instruction.shamt;

                    break;

                case PSEMU_CPU_OP_SLLV:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rt] <<
                   (cpu->gpr[cpu->instruction.rs] & 0x0000001F);

                    break;

                case PSEMU_CPU_OP_SRLV:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rt] >>
                    (cpu->gpr[cpu->instruction.rs] & 0x0000001F);

                    break;

                case PSEMU_CPU_OP_SRAV:
                    cpu->gpr[cpu->instruction.rd] =
                    (int32_t)cpu->gpr[cpu->instruction.rt] >>
                    (cpu->gpr[cpu->instruction.rs] & 0x0000001F);

                    break;

                case PSEMU_CPU_OP_JR:
                {
                    const uint32_t address = cpu->gpr[cpu->instruction.rs] - 4;

                    if ((address & 0x00000003) != 0)
                    {
                        raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, address);
                        break;
                    }

                    cpu->next_pc = address;
                    in_delay_slot = true;

                    break;
                }

                case PSEMU_CPU_OP_JALR:
                {
                    const uint32_t address = cpu->gpr[cpu->instruction.rs] - 4;

                    cpu->gpr[cpu->instruction.rd] = cpu->pc + 8;

                    if ((address & 0x00000003) != 0)
                    {
                        raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, address);
                        break;
                    }

                    cpu->next_pc = address;
                    in_delay_slot = true;

                    break;
                }

                case PSEMU_CPU_OP_SYSCALL:
                    raise_exception
                    (cpu, PSEMU_CPU_EXCCODE_Sys, UNUSED_PARAMETER);
                    
                    break;

                case PSEMU_CPU_OP_BREAK:
                    raise_exception
                    (cpu, PSEMU_CPU_EXCCODE_Bp, UNUSED_PARAMETER);
                    
                    break;

                case PSEMU_CPU_OP_MFHI:
                    cpu->gpr[cpu->instruction.rd] = cpu->hi;
                    break;

                case PSEMU_CPU_OP_MTHI:
                    cpu->hi = cpu->gpr[cpu->instruction.rs];
                    break;

                case PSEMU_CPU_OP_MFLO:
                    cpu->gpr[cpu->instruction.rd] = cpu->lo;
                    break;

                case PSEMU_CPU_OP_MTLO:
                    cpu->lo = cpu->gpr[cpu->instruction.rs];
                    break;

                case PSEMU_CPU_OP_MULT:
                {
                    const uint64_t result =
                    (int64_t)(int32_t)cpu->gpr[cpu->instruction.rs] *
                    (int64_t)(int32_t)cpu->gpr[cpu->instruction.rt];

                    cpu->lo = result & 0x00000000FFFFFFFF;
                    cpu->hi = result >> 32;

                    break;
                }

                case PSEMU_CPU_OP_MULTU:
                {
                    const uint64_t result =
                    (uint64_t)cpu->gpr[cpu->instruction.rs] *
                    (uint64_t)cpu->gpr[cpu->instruction.rt];

                    cpu->lo = result & 0x00000000FFFFFFFF;
                    cpu->hi = result >> 32;

                    break;
                }

                case PSEMU_CPU_OP_DIV:
                {
                    // The result of a division by zero is consistent with the
                    // result of a simple radix-2 ("one bit at a time")
                    // implementation.
                    const int32_t rt = (int32_t)cpu->gpr[cpu->instruction.rt];
                    const int32_t rs = (int32_t)cpu->gpr[cpu->instruction.rs];

                    // Divisor is zero
                    if (rt == 0)
                    {
                        // If the dividend is negative, the quotient is 1
                        // (0x00000001), and if the dividend is positive or
                        // zero, the quotient is -1 (0xFFFFFFFF).
                        cpu->lo = (rs < 0) ? 0x00000001 : 0xFFFFFFFF;

                        // In both cases the remainder equals the dividend.
                        cpu->hi = (uint32_t)rs;
                    }
                    // Will trigger an arithmetic exception when dividing
                    // 0x80000000 by 0xFFFFFFFF. The result of the division is
                    // a quotient of 0x80000000 and a remainder of 0x00000000.
                    else if ((uint32_t)rs == 0x80000000 &&
                             (uint32_t)rt == 0xFFFFFFFF)
                    {
                        cpu->lo = (uint32_t)rs;
                        cpu->hi = 0x00000000;
                    }
                    else
                    {
                        cpu->lo = rs / rt;
                        cpu->hi = rs % rt;
                    }
                    break;
                }

                case PSEMU_CPU_OP_DIVU:
                {
                    const uint32_t rt = cpu->gpr[cpu->instruction.rt];
                    const uint32_t rs = cpu->gpr[cpu->instruction.rs];

                    // In the case of unsigned division, the dividend can't be
                    // negative and thus the quotient is always -1 (0xFFFFFFFF)
                    // and the remainder equals the dividend.
                    if (rt == 0)
                    {
                        cpu->lo = 0xFFFFFFFF;
                        cpu->hi = rs;
                    }
                    else
                    {
                        cpu->lo = rs / rt;
                        cpu->hi = rs % rt;
                    }
                    break;
                }

                case PSEMU_CPU_OP_ADD:
                {
                    const uint32_t rs = cpu->gpr[cpu->instruction.rs];
                    const uint32_t rt = cpu->gpr[cpu->instruction.rt];

                    const uint32_t result = rs + rt;

                    if (!((rs ^ rt) & 0x80000000) &&
                        ((result ^ rs) & 0x80000000))
                    {
                        raise_exception
                        (cpu, PSEMU_CPU_EXCCODE_Ov, UNUSED_PARAMETER);

                        break;
                    }

                    cpu->gpr[cpu->instruction.rd] = result;
                    break;
                }

                case PSEMU_CPU_OP_ADDU:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] +
                    cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_SUB:
                {
                    const uint32_t rs = cpu->gpr[cpu->instruction.rs];
                    const uint32_t rt = cpu->gpr[cpu->instruction.rt];

                    const uint32_t result = rs - rt;

                    if (((rs ^ rt) & 0x80000000) &&
                        ((result ^ rs) & 0x80000000))
                    {
                        raise_exception
                        (cpu, PSEMU_CPU_EXCCODE_Ov, UNUSED_PARAMETER);

                        break;
                    }

                    cpu->gpr[cpu->instruction.rd] = result;
                    break;
                }

                case PSEMU_CPU_OP_SUBU:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] -
                    cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_AND:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] &
                    cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_OR:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] |
                    cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_XOR:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] ^
                    cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_NOR:
                    cpu->gpr[cpu->instruction.rd] =
                  ~(cpu->gpr[cpu->instruction.rs] |
                    cpu->gpr[cpu->instruction.rt]);

                    break;

                case PSEMU_CPU_OP_SLT:
                    cpu->gpr[cpu->instruction.rd] =
                    (int32_t)cpu->gpr[cpu->instruction.rs] <
                    (int32_t)cpu->gpr[cpu->instruction.rt];

                    break;

                case PSEMU_CPU_OP_SLTU:
                    cpu->gpr[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rs] <
                    cpu->gpr[cpu->instruction.rt];

                    break;

                default:
                    raise_exception
                    (cpu, PSEMU_CPU_EXCCODE_RI, UNUSED_PARAMETER);
                    
                    break;
            }
            break;

        case PSEMU_CPU_OP_GROUP_BCOND:
        {
            const bool should_link =
            (cpu->instruction.rt & 0x0000001E) == 0x10;

            const bool should_branch =
            (int32_t)
            (cpu->gpr[cpu->instruction.rs] ^ (cpu->instruction.rt << 31)) < 0;

            if (should_link)
            {
                cpu->gpr[31] = cpu->pc + 8;
            }

            branch_if(cpu, should_branch);
            break;
        }

        case PSEMU_CPU_OP_J:
            cpu->next_pc =
            ((PSEMU_CPU_DECODE_TARGET(cpu->instruction.word) << 2) |
            (cpu->pc & 0xF0000000)) - 4;
            in_delay_slot = true;
            
            break;

        case PSEMU_CPU_OP_JAL:
            cpu->gpr[31] = cpu->pc + 8;

            cpu->next_pc =
            ((PSEMU_CPU_DECODE_TARGET(cpu->instruction.word) << 2) |
            (cpu->pc & 0xF0000000)) - 4;
            in_delay_slot = true;

            break;

        case PSEMU_CPU_OP_BEQ:
            branch_if
            (cpu,
             cpu->gpr[cpu->instruction.rs] == cpu->gpr[cpu->instruction.rt]);

            break;

        case PSEMU_CPU_OP_BNE:
            branch_if
            (cpu,
             cpu->gpr[cpu->instruction.rs] != cpu->gpr[cpu->instruction.rt]);
            
            break;

        case PSEMU_CPU_OP_BLEZ:
            branch_if(cpu, (int32_t)cpu->gpr[cpu->instruction.rs] <= 0);
            break;

        case PSEMU_CPU_OP_BGTZ:
            branch_if(cpu, (int32_t)cpu->gpr[cpu->instruction.rs] > 0);
            break;

        case PSEMU_CPU_OP_ADDI:
        {
            const uint32_t imm =
            (uint32_t)(int16_t)
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            const uint32_t rs = cpu->gpr[cpu->instruction.rs];

            const uint32_t result = imm + rs;

            if (!((rs ^ imm) & 0x80000000) && ((result ^ rs) & 0x80000000))
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_Ov, UNUSED_PARAMETER);
                break;
            }
            cpu->gpr[cpu->instruction.rt] = result;
            break;
        }

        case PSEMU_CPU_OP_ADDIU:
            cpu->gpr[cpu->instruction.rt] =
            cpu->gpr[cpu->instruction.rs] +
            (int16_t)PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_SLTI:
            cpu->gpr[cpu->instruction.rt] =
            (int32_t)cpu->gpr[cpu->instruction.rs] <
            (int16_t)PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_SLTIU:
            cpu->gpr[cpu->instruction.rt] =
            cpu->gpr[cpu->instruction.rs] <
            (uint32_t)(int16_t)
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_ANDI:
            cpu->gpr[cpu->instruction.rt] =
            cpu->gpr[cpu->instruction.rs] &
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_ORI:
            cpu->gpr[cpu->instruction.rt] =
            cpu->gpr[cpu->instruction.rs] |
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_XORI:
            cpu->gpr[cpu->instruction.rt] =
            cpu->gpr[cpu->instruction.rs] ^
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word);

            break;

        case PSEMU_CPU_OP_LUI:
            cpu->gpr[cpu->instruction.rt] =
            PSEMU_CPU_DECODE_IMMEDIATE(cpu->instruction.word) << 16;
            
            break;

        case PSEMU_CPU_OP_GROUP_COP0:
            switch (cpu->instruction.rs)
            {
                case PSEMU_CPU_OP_MF:
                    cpu->gpr[cpu->instruction.rt] =
                    cpu->cop0[cpu->instruction.rd];

                    break;

                case PSEMU_CPU_OP_MT:
                    cpu->cop0[cpu->instruction.rd] =
                    cpu->gpr[cpu->instruction.rt];
                    
                    break;

                 default:
                     switch (cpu->instruction.funct)
                     {
                         case PSEMU_CPU_OP_RFE:
                             cpu->cop0[PSEMU_CPU_COP0_SR] =
                            (cpu->cop0[PSEMU_CPU_COP0_SR] & 0xFFFFFFF0) |
                           ((cpu->cop0[PSEMU_CPU_COP0_SR] & 0x0000003C) >> 2);

                             break;

                         default:
                             raise_exception
                             (cpu, PSEMU_CPU_EXCCODE_RI, UNUSED_PARAMETER);

                             break;
                     }
                     break;
            }
            break;

        case PSEMU_CPU_OP_GROUP_COP2:
            switch (cpu->instruction.rs)
            {
                case PSEMU_CPU_OP_MF:
                    cpu->gpr[cpu->instruction.rt] =
                    load_cop2_cpr(cpu, cpu->instruction.rd);

                    break;

                case PSEMU_CPU_OP_CF:
                    cpu->gpr[cpu->instruction.rt] =
                    load_cop2_ccr(cpu, cpu->instruction.rd);

                    break;

                case PSEMU_CPU_OP_MT:
                    store_cop2_cpr(cpu,
                                   cpu->instruction.rd,
                                   cpu->gpr[cpu->instruction.rt]);
                    break;

                case PSEMU_CPU_OP_CT:
                    store_cop2_ccr(cpu,
                                   cpu->instruction.rd,
                                   cpu->gpr[cpu->instruction.rt]);
                    break;

                default:
                    switch (cpu->instruction.funct)
                    {
                        case PSEMU_CPU_OP_NCLIP:
                            gte_nclip(cpu);
                            break;

                        case PSEMU_CPU_OP_NCDS:
                            gte_ncds(cpu);
                            break;

                        case PSEMU_CPU_OP_AVSZ3:
                            gte_avsz3(cpu);
                            break;

                        case PSEMU_CPU_OP_RTPT:
                            gte_rtpt(cpu);
                            break;

                        default:
                            raise_exception
                            (cpu, PSEMU_CPU_EXCCODE_RI, UNUSED_PARAMETER);

                            break;
                    }
                    break;
            }
            break;

        case PSEMU_CPU_OP_LB:
            cpu->gpr[cpu->instruction.rt] =
            (int8_t)psemu_bus_load_byte(bus, vaddr(cpu));

            break;

        case PSEMU_CPU_OP_LH:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 0x00000001) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, m_vaddr);
                break;
            }

            cpu->gpr[cpu->instruction.rt] =
            (int16_t)psemu_bus_load_halfword(bus, m_vaddr);

            break;
        }

        case PSEMU_CPU_OP_LWL:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            const uint32_t data =
            psemu_bus_load_word(bus, m_vaddr & 0xFFFFFFFC);

            switch (m_vaddr & 0x00000003)
            {
                case 0:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0x00FFFFFF) |
                    (data << 24);

                    break;

                case 1:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0x0000FFFF) |
                    (data << 16);

                    break;

                case 2:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0x000000FF) | (data << 8);

                    break;

                case 3:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0x00000000) | (data << 0);

                    break;
            }
            break;
        }

        case PSEMU_CPU_OP_LW:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 0x00000003) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, m_vaddr);
                break;
            }

            cpu->gpr[cpu->instruction.rt] = psemu_bus_load_word(bus, m_vaddr);
            break;
        }

        case PSEMU_CPU_OP_LBU:
            cpu->gpr[cpu->instruction.rt] =
            psemu_bus_load_byte(bus, vaddr(cpu));

            break;

        case PSEMU_CPU_OP_LHU:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 0x00000001) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, m_vaddr);
                break;
            }

            cpu->gpr[cpu->instruction.rt] =
            psemu_bus_load_halfword(bus, m_vaddr);

            break;
        }

        case PSEMU_CPU_OP_LWR:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            const uint32_t data =
            psemu_bus_load_word(bus, m_vaddr & 0xFFFFFFFC);

            switch (m_vaddr & 0x00000003)
            {
                case 0:
                    cpu->gpr[cpu->instruction.rt] = data;
                    break;

                case 1:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0xFF000000) | (data >> 8);

                    break;

                 case 2:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0xFFFF0000) |
                    (data >> 16);
                    
                    break;

                  case 3:
                    cpu->gpr[cpu->instruction.rt] =
                    (cpu->gpr[cpu->instruction.rt] & 0xFFFFFF00) |
                    (data >> 24);
                    
                    break;
            }
            break;
        }

        case PSEMU_CPU_OP_SB:
            psemu_bus_store_byte
            (bus, vaddr(cpu), cpu->gpr[cpu->instruction.rt] & 0x000000FF);

            break;

        case PSEMU_CPU_OP_SH:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 1) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdES, m_vaddr);
                break;
            }

            psemu_bus_store_halfword
            (bus, m_vaddr, cpu->gpr[cpu->instruction.rt] & 0x0000FFFF);

            break;
        }

        case PSEMU_CPU_OP_SWL:
        {
            const uint32_t m_vaddr = vaddr(cpu);
            const uint32_t address = m_vaddr & 0xFFFFFFFC;

            uint32_t data = psemu_bus_load_word(bus, address);

            switch (m_vaddr & 0x00000003)
            {
                case 0:
                    data = (data & 0xFFFFFF00) |
                           (cpu->gpr[cpu->instruction.rt] >> 24);
                    break;

                case 1:
                    data = (data & 0xFFFF0000) |
                           (cpu->gpr[cpu->instruction.rt] >> 16);
                    break;

                case 2:
                    data = (data & 0xFF000000) |
                           (cpu->gpr[cpu->instruction.rt] >> 8);
                    break;

                case 3:
                    data = (data & 0x00000000) |
                           (cpu->gpr[cpu->instruction.rt] >> 0);
                    break;
            }

            psemu_bus_store_word(bus, address, data);
            break;
        }

        case PSEMU_CPU_OP_SW:
            if (!(cpu->cop0[PSEMU_CPU_COP0_SR] & PSEMU_CPU_SR_IsC))
            {
                const uint32_t m_vaddr = vaddr(cpu);

                if ((m_vaddr & 0x00000003) != 0)
                {
                    raise_exception(cpu, PSEMU_CPU_EXCCODE_AdES, m_vaddr);
                    break;
                }
                psemu_bus_store_word(bus, m_vaddr, cpu->gpr[cpu->instruction.rt]);
            }
            break;

        case PSEMU_CPU_OP_SWR:
        {
            const uint32_t m_vaddr = vaddr(cpu);
            const uint32_t address = m_vaddr & 0xFFFFFFFC;

            uint32_t data = psemu_bus_load_word(bus, address);

            switch (m_vaddr & 0x00000003)
            {
                case 0:
                    data = (data & 0x00000000) |
                           (cpu->gpr[cpu->instruction.rt] << 0);
                    break;

                case 1:
                    data = (data & 0x000000FF) |
                           (cpu->gpr[cpu->instruction.rt] << 8);
                    break;

                case 2:
                    data = (data & 0x0000FFFF) |
                           (cpu->gpr[cpu->instruction.rt] << 16);
                    break;

                case 3:
                    data = (data & 0x00FFFFFF) |
                           (cpu->gpr[cpu->instruction.rt] << 24);
                    break;
            }

            psemu_bus_store_word(bus, address, data);
            break;
        }

        case PSEMU_CPU_OP_LWC2:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 0x00000003) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, UNUSED_PARAMETER);
                break;
            }

            store_cop2_cpr(cpu,
                           cpu->instruction.rt,
                           psemu_bus_load_word(bus, m_vaddr));
            break;
        }

        case PSEMU_CPU_OP_SWC2:
        {
            const uint32_t m_vaddr = vaddr(cpu);

            if ((m_vaddr & 0x00000003) != 0)
            {
                raise_exception(cpu, PSEMU_CPU_EXCCODE_AdEL, UNUSED_PARAMETER);
                break;
            }

            psemu_bus_store_word
            (bus, m_vaddr, load_cop2_cpr(cpu, cpu->instruction.rt));
            
            break;
        }

        default:
            raise_exception(cpu, PSEMU_CPU_EXCCODE_RI, UNUSED_PARAMETER);
            break;
    }

    cpu->instruction.word = psemu_bus_load_word(bus, cpu->pc += 4);
    cpu->gpr[0] = 0x00000000;
}