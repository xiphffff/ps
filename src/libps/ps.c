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

#include <assert.h>
#include <stdlib.h>
#include "ps.h"

// Allocates memory for a `libps_system` structure and returns a pointer to it
// if memory allocation was successful, `NULL` otherwise.
struct libps_system* libps_system_create(uint8_t* const bios_data)
{
    // XXX: I don't like the notion of passing a pointer to the BIOS data,
    // loaded entirely by the caller. That's 512KB just sitting in RAM and I'm
    // not exactly okay with that. The only alternative I can think of is just
    // doing file I/O on every BIOS read (also known as a very bad idea), or
    // splitting the BIOS data into 16 32KB blocks and loading a block when the
    // read falls within a certain range. The more blocks we have, the more
    // memory will be used and file I/O will be less frequent, whereas if we
    // have fewer blocks, more memory will be used, but file I/O will become
    // more frequent.
    //
    // Another alternative is HLEing the BIOS.
    //
    // This is important for systems with extremely scarce amounts of RAM.
    struct libps_system* ps = malloc(sizeof(struct libps_system));

    if (ps)
    {
        ps->bus = libps_bus_create(bios_data);

        if (ps->bus)
        {
            ps->cpu = libps_cpu_create(ps->bus);

            if (ps->cpu)
            {
                libps_system_reset(ps);
                return ps;
            }
            return NULL;
        }
        return NULL;
    }
    return NULL;
}

// Deallocates the memory held by `ps`.
void libps_system_destroy(struct libps_system* ps)
{
    assert(ps != NULL);

    libps_cpu_destroy(ps->cpu);
    libps_bus_destroy(ps->bus);

    free(ps);
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

    // Check for DMAs first before involving the CPU in any way.
    libps_bus_step(ps->bus);

    if ((ps->bus->i_mask & ps->bus->i_stat) != 0)
    {
        ps->cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] |= (1 << 10);
    }
    else
    {
        ps->cpu->cop0_cpr[LIBPS_CPU_COP0_REG_CAUSE] &= ~(1 << 10);
    }
    libps_cpu_step(ps->cpu);
}

void libps_vblank(struct libps_system* ps)
{
    ps->bus->i_stat |= (1 << 0);
}