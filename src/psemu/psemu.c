// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright noticeand this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <assert.h>
#include "psemu.h"
#include "utility/memory.h"

// Creates a Sony PlayStation® system emulator.
struct psemu_system* psemu_create(uint8_t* const bios_data)
{
    struct psemu_system* ps_emu =
    psemu_safe_malloc(sizeof(struct psemu_system));

    psemu_bus_init(&ps_emu->bus, bios_data);
    psemu_cpu_set_bus(&ps_emu->bus);

    psemu_reset(ps_emu);
    return ps_emu;
}

// Destroys a Sony PlayStation® system emulator `ps_emu`.
void psemu_destroy(struct psemu_system* const ps_emu)
{
    assert(ps_emu != NULL);

    psemu_bus_fini(&ps_emu->bus);
    psemu_safe_free(ps_emu);
}

// Resets a Sony PlayStation® system emulator `ps_emu` to the startup state.
void psemu_reset(struct psemu_system* const ps_emu)
{
    assert(ps_emu != NULL);

    psemu_bus_reset(&ps_emu->bus);
    psemu_cpu_reset(&ps_emu->cpu);
}

// Executes one full system step on a Sony PlayStation® system emulator
// `ps_emu`.
void psemu_step(struct psemu_system* const ps_emu)
{
    assert(ps_emu != NULL);

    // 2 hardware steps per instruction
    psemu_bus_step(&ps_emu->bus);
    psemu_bus_step(&ps_emu->bus);

    psemu_cpu_step(&ps_emu->cpu);
}