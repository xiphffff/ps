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

#include "bus.h"
#include "cd.h"
#include "cpu.h"
#include "cpu_defs.h"
#include "gpu.h"

struct libps_system
{
    struct libps_bus* bus;
    struct libps_cpu* cpu;
};

// Creates a PlayStation emulator and returns a pointer to it if memory
// allocation was successful, or `NULL` otherwise.
struct libps_system* libps_system_create(uint8_t* const bios_data);

// Destroys PlayStation emulator `ps` and deallocates all memory held by it.
void libps_system_destroy(struct libps_system* ps);

// Resets a PlayStation to the startup state. This is called automatically by
// `libps_system_create()`.
void libps_system_reset(struct libps_system* ps);

// Executes one full system step.
void libps_system_step(struct libps_system* ps);

// Triggers a V-Blank interrupt. This *must* be called once per frame.
void libps_vblank(struct libps_system* ps);

#ifdef __cplusplus
}
#endif // __cplusplus
