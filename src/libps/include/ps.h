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
#define LIBPS_API_VERSION_MAJOR 1
#define LIBPS_API_VERSION_MINOR 0
#define LIBPS_API_VERSION_PATCH 0

#include "bus.h"
#include "cd.h"
#include "cpu.h"
#include "cpu_defs.h"
#include "gpu.h"

// Defines the structure of a PlayStation emulator.
struct libps_system
{
    struct libps_bus* bus;
    struct libps_cpu* cpu;
};

// Creates a PlayStation emulator. `bios_data` is a pointer to the BIOS data
// supplied by the caller and cannot be `NULL`. If `bios_data` is `NULL`, this
// function will return `NULL` and the emulator will not be created.
struct libps_system* libps_system_create(uint8_t* const bios_data);

// Destroys a PlayStation emulator.
void libps_system_destroy(struct libps_system* ps);

// Resets a PlayStation to the startup state. This is called automatically by
// `libps_system_create()`.
void libps_system_reset(struct libps_system* ps);

// Executes one full system step.
void libps_system_step(struct libps_system* ps);

// "Inserts" a CD-ROM `cdrom_info` into a PlayStation emulator `ps`. If
// `cdrom_info` is `NULL`, the CD-ROM, if any will be removed.
//
// Returns `true` if `cdrom_info` is valid and a CD-ROM has been inserted, or
// `false` otherwise.
bool libps_system_set_cdrom(struct libps_system* ps,
                            struct libps_cdrom_info* cdrom_info);
#ifdef __cplusplus
}
#endif // __cplusplus
