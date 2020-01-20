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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdbool.h>
#include <stdint.h>

struct libps_bus;

struct libps_cpu
{
    // Current program counter (PC)
    uint32_t pc;

    // Next program counter
    uint32_t next_pc;

    // Current instruction
    uint32_t instruction;

    // Quotient part of a division operation
    uint32_t reg_lo;

    // Remainder part of a division operation
    uint32_t reg_hi;

    // General purpose registers
    uint32_t gpr[32];

    // System control co-processor (COP0) registers
    uint32_t cop0_cpr[32];
};

// Allocates memory for a `libps_cpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
struct libps_cpu* libps_cpu_create(struct libps_bus* b);

// Deallocates the memory held by `cpu`.
void libps_cpu_destroy(struct libps_cpu* cpu);

// Triggers a reset exception, thereby initializing the CPU to the predefined
// startup state.
void libps_cpu_reset(struct libps_cpu* cpu);

// Executes one instruction.
void libps_cpu_step(struct libps_cpu* cpu);

#ifdef __cplusplus
}
#endif // __cplusplus
