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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "bus.h"
#include "gpu.h"

// `libps_bus` doesn't need to know about this, since the operator of the
// library has the BIOS data loaded already and no other part of the system
// needs to know or care.
static uint8_t* bios;

// Allocates memory for a `libps_bus` structure and returns a pointer to it if
// memory allocation was successful, or `NULL` otherwise. This function should
// not be called anywhere other than `libps_system_create()`.
//
// `bios_data_ptr` should be a pointer to BIOS data, passed by
// `libps_system_create()`.
__attribute__((warn_unused_result))
struct libps_bus* libps_bus_create(uint8_t* const bios_data_ptr)
{
    struct libps_bus* bus = malloc(sizeof(struct libps_bus));

    if (bus)
    {
        bios = bios_data_ptr;

        // XXX: This will fail on some systems (ironically, the PlayStation).
        // It may be better to consider using `realloc()` after a certain
        // amount of emulated RAM has been used. The initial safe "certain
        // amount" will have to be tested with relation to standard memory
        // sizes on certain systems and how much RAM most PlayStation games use
        // on average.
        bus->ram = malloc(sizeof(uint8_t) * 0x200000);

        bus->gpu = libps_gpu_create();
        return bus;
    }
    return NULL;
}

// Deallocates memory held by `bus`. This function should not be called
// anywhere other than `libps_system_destroy()`.
void libps_bus_destroy(struct libps_bus* bus)
{
    assert(bus != NULL);

    libps_gpu_destroy(bus->gpu);
    free(bus->ram);
    free(bus);
}

// Returns a word from memory referenced by physical address `paddr`.
uint32_t libps_bus_load_word(struct libps_bus* bus, const uint32_t paddr)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            return *(uint32_t *)(bus->ram + (paddr & 0x00FFFFFF));

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // I/O Ports
                case 0x1:
                    // XXX: I think perhaps this could be made into an array,
                    // however there'd be a lot of unused addresses unless we
                    // could compartmentalize it somehow; must investigate.
                    //
                    // But for now, the compiler remains smarter than me.
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801814 - GPU Status Register (R)
                        case 0x814:
                            return bus->gpu->gpustat;

                        default:
                            printf("libps_bus_load_word(bus=%p,paddr=0x%08X): Unknown "
                                   "physical address!\n", (void*)&bus, paddr);
                            return 0x00000000;
                    }
                    break;

                default:
                    printf("libps_bus_load_word(bus=%p,paddr=0x%08X): Unknown "
                            "physical address!\n", (void*)&bus, paddr);
                    return 0x00000000;
            }
            break;

        case 0x1FC0 ... 0x1FC7:
            return *(uint32_t *)(bios + (paddr & 0x000FFFFF));

        default:
            printf("libps_bus_load_word(bus=%p,paddr=0x%08X): Unknown "
                   "physical address!\n", (void*)&bus, paddr);
            return 0x00000000;
    }
}

// Returns a halfword from memory referenced by physical address `paddr`.
uint16_t libps_bus_load_halfword(struct libps_bus* bus, const uint32_t paddr)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            return *(uint16_t *)(bus->ram + (paddr & 0x00FFFFFF));

        default:
            printf("libps_bus_load_halfword(bus=%p,paddr=0x%08X): Unknown physical "
                   "address!\n", (void*)&bus, paddr);
            return 0x0000;
    }
}

// Returns a byte from memory referenced by physical address `paddr`.
uint8_t libps_bus_load_byte(struct libps_bus* bus, const uint32_t paddr)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            return *(uint8_t *)(bus->ram + (paddr & 0x00FFFFFF));

        case 0x1FC0 ... 0x1FC7:
            return *(uint8_t *)(bios + (paddr & 0x000FFFFF));

        default:
            printf("libps_bus_load_byte(bus=%p,paddr=0x%08X): Unknown "
                   "physical address!\n", (void*)&bus, paddr);
            return 0x00;
    }
}

// Stores word `data` into memory referenced by phsyical address `paddr`.
void libps_bus_store_word(struct libps_bus* bus,
                          const uint32_t paddr,
                          const uint32_t data)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            *(uint32_t *)(bus->ram + (paddr & 0x00FFFFFF)) = data;
            break;

        default:
            printf("libps_bus_store_word(bus=%p,paddr=0x%08X,data=0x%08X): "
                   "Unknown physical address!\n", (void*)&bus, paddr, data);
            break;
    }
}

// Stores halfword `data` into memory referenced by phsyical address `paddr`.
void libps_bus_store_halfword(struct libps_bus* bus,
                              const uint32_t paddr,
                              const uint16_t data)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            *(uint16_t *)(bus->ram + (paddr & 0x00FFFFFF)) = data;
            break;

        default:
            //printf("libps_bus_store_halfword(bus=%p,paddr=0x%08X,data=0x%02X) "
             //      ": Unknown physical address!\n", (void*)&bus, paddr, data);
            break;
    }
}

// Stores byte `data` into memory referenced by phsyical address `paddr`.
void libps_bus_store_byte(struct libps_bus* bus,
                          const uint32_t paddr,
                          const uint8_t data)
{
    assert(bus != NULL);

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x0020:
            *(uint8_t *)(bus->ram + (paddr & 0x00FFFFFF)) = data;
            break;

        default:
            printf("libps_bus_store_byte(bus=%p,paddr=0x%08X,data=0x%02X): Unknown "
                   "physical address!\n", (void*)&bus, paddr, data);
            break;
    }
}