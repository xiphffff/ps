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

#ifdef LIBPS_DEBUG

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

// Maximum possible length of a result from `libps_disassemble_instruction()`.
#define LIBPS_DISASM_MAX_LENGTH 30

// Converts `instruction` to MIPS-I assembly language, and stores this result
// in `result`. `pc` is required so as to compute a proper branch target in the
// event `instruction` is a branch instruction.
void libps_disassemble_instruction(const uint32_t instruction,
                                   const uint32_t pc,
                                   char* result);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIBPS_DEBUG