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

#include <assert.h>
#include <stdlib.h>
#include "ps.h"
#include "utility/memory.h"

// Creates a PlayStation emulator. `bios_data` is a pointer to the BIOS data
// supplied by the caller and cannot be `NULL`. If `bios_data` is `NULL`, this
// function will return `NULL` and the emulator will not be created.
struct libps_system* libps_system_create(uint8_t* const bios_data)
{
    if (bios_data == NULL)
    {
        return NULL;
    }

    // XXX: I don't like the notion of passing a pointer to the BIOS data,
    // loaded entirely by the caller. That's 512KB just sitting in RAM and I'm
    // not okay with that. There are a few possibilities to avoid this:
    //
    // 1) doing file I/O on every BIOS read (slow, garbage solution)
    // 
    // 2) splitting the BIOS into x number of yKB blocks and loading a block
    //    when a read falls within a certain range. The more blocks we have,
    //    the more memory will be used and file I/O will be less frequent,
    //    whereas if we have fewer blocks, less memory will be used, but file
    //    I/O will become more frequent. The amount of blocks and their size
    //    should be configurable at runtime.
    //
    // 3) HLE of the BIOS, the most complicated option but possibly the most
    //    effective one, worst case.
    //
    // This may not be worth it however, I don't think there's a single system
    // that will fail to allocate 512KB outright or will suffer from
    // detrimental effects for doing so. We'll have to see. If no system falls
    // under this circumstance, this idea will be discarded.
    struct libps_system* ps = libps_safe_malloc(sizeof(struct libps_system));

    ps->bus = libps_bus_create(bios_data);
    ps->cpu = libps_cpu_create(ps->bus);

    libps_system_reset(ps);
    return ps;
}

// Destroys a PlayStation emulator.
void libps_system_destroy(struct libps_system* ps)
{
    libps_cpu_destroy(ps->cpu);
    libps_bus_destroy(ps->bus);

    libps_safe_free(ps);
}

// Resets the PlayStation to the startup state. This is called automatically by
// `libps_system_create()`.
void libps_system_reset(struct libps_system* ps)
{
    assert(ps != NULL);

    libps_bus_reset(ps->bus);
    libps_cpu_reset(ps->cpu);
}

// Executes one full system step.
void libps_system_step(struct libps_system* ps)
{
    assert(ps != NULL);

    // Step 1: Check for DMAs and tick the hardware.
    libps_bus_step(ps->bus);

    // Step 2: Check to see if the interrupt line needs to be enabled.
    if ((ps->bus->i_mask & ps->bus->i_stat) != 0)
    {
        ps->cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] |= (1 << 10);
    }
    else
    {
        ps->cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] &= ~(1 << 10);
    }

    // Step 3: Execute one instruction.
    libps_cpu_step(ps->cpu);
}