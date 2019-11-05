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

// Handles processing of DMA channel 2 - GPU (lists + image data) in VRAM write
// mode.
static void dma_gpu_vram_write_process(struct libps_bus* bus)
{
    const uint16_t ba = bus->dma_gpu_channel.bcr >> 16;
    const uint16_t bs = bus->dma_gpu_channel.bcr & 0x0000FFFF;

    for (uint32_t count = 0; count != (ba * bs); ++count)
    {
        const uint32_t data =
        *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x00FFFFFF));

        libps_gpu_process_gp0(bus->gpu, data);
        bus->dma_gpu_channel.madr += 4;
    }
}

// Handles processing of DMA channel 2 - GPU (lists + image data) in linked
// list mode.
static void dma_gpu_linked_list_process(struct libps_bus* bus)
{
    assert(bus != NULL);

    // * SyncMode=0 (bits 9-10), transfer cannot be interrupted
    // * Transfer direction is from main RAM (bit 0)
    for (;;)
    {
        // Grab the header word first.
        const uint32_t header =
        *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x00FFFFFF));

        // Upper 8 bits tell us the number of words in this packet, not
        // counting the header word.
        uint32_t packet_size = (header >> 24);

        while (packet_size != 0)
        {
            bus->dma_gpu_channel.madr =
            (bus->dma_gpu_channel.madr + 4) & 0x001FFFFC;

            const uint32_t entry =
            *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x00FFFFFF));

            libps_gpu_process_gp0(bus->gpu, entry);
            packet_size--;
        }

        // Break the loop if the end of list marker has been reached
        if (header & 0x00800000)
        {
            break;
        }
        bus->dma_gpu_channel.madr = header & 0x001FFFFC;
    }
}

// Handles processing of DMA channel 6 - OTC (reverse clear OT).
static void dma_otc_process(struct libps_bus* bus)
{
    assert(bus != NULL);

    // Apparently, DMA6's CHCR is always 0x11000002. The most important things
    // about this value:
    //
    // * SyncMode=0 (bits 9-10), transfer cannot be interrupted
    // * Memory address step is forward +4 (bit 1)
    // * Memory transfer direction is to main RAM (bit 0)

    if (bus->dma_otc_channel.chcr != 0x11000002)
    {
        // XXX: Should never happen and should be logged.
        bus->dma_otc_channel.chcr &= ~(1 << 24);
        return;
    }

    uint32_t count   = bus->dma_otc_channel.bcr;
    uint32_t address = bus->dma_otc_channel.madr;

    while (count--)
    {
        *(uint32_t *)(bus->ram + (address & 0x00FFFFFF)) =
        (address - 4) & 0x00FFFFFF;

        address -= 4;
    }

    *(uint32_t *)(bus->ram + ((address + 4) & 0x00FFFFFF)) = 0x00FFFFFF;

    // Transfer complete.
    bus->dma_otc_channel.chcr &= ~(1 << 24);
}

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

// Resets the system bus, which resets the peripherals to their startup state.
void libps_bus_reset(struct libps_bus* bus)
{
    assert(bus != NULL);

    bus->dpcr = 0x07654321;
    libps_gpu_reset(bus->gpu);
}

// Handles DMA requests.
void libps_bus_step(struct libps_bus* bus)
{
    assert(bus != NULL);

    // Thanks to Ravenslofty for this idea.
    int dpcr = bus->dpcr & 0x08888888;

    while (dpcr)
    {
        // Extract least significant bit.
        const int dma_enable_bit = __builtin_ctzl(dpcr);

        // Zero least significant bit.
        dpcr &= dpcr - 1;

        switch (dma_enable_bit)
        {
            // DMA channel 2 - GPU (lists + image data)
            case 11:
                switch (bus->dma_gpu_channel.chcr)
                {
                    // VramWrite (unimplemented)
                    case 0x01000201:
                        dma_gpu_vram_write_process(bus);
                        break;

                    // List
                    case 0x01000401:
                        dma_gpu_linked_list_process(bus);
                        break;
                }

                // Transfer complete.
                bus->dma_gpu_channel.chcr &= ~(1 << 24);
                break;

            // DMA channel 6 - OTC (reverse clear OT)
            case 27:
                dma_otc_process(bus);
                break;
        }
    }
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
                        // 0x1F801074 - I_MASK - Interrupt mask register (R/W)
                        case 0x074:
                            return bus->i_mask;

                        // 0x1F8010A8 - DMA Channel 2 (GPU) control (R/W)
                        case 0x0A8:
                            return bus->dma_gpu_channel.chcr;

                        // 0x1F8010E8 - DMA Channel 6 (OTC) control (R/W)
                        case 0x0E8:
                            return bus->dma_otc_channel.chcr;

                        // 0x1F8010F0 - DMA Control Register (R/W)
                        case 0x0F0:
                            return bus->dpcr;

                        // 0x1F8010F4 - DMA Interrupt Register (R/W)
                        case 0x0F4:
                            return bus->dicr;

                        // 0x1F801810 - Read responses to GP0(C0h) and GP1(10h)
                        // commands
                        case 0x810:
                            return bus->gpu->gpuread;

                        // 0x1F801814 - GPU Status Register (R)
                        case 0x814:
                            return 0x1FF00000;

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
           // printf("libps_bus_load_halfword(bus=%p,paddr=0x%08X): Unknown physical "
            //       "address!\n", (void*)&bus, paddr);
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
                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
                            bus->i_stat = data;
                            break;

                        // 0x1F801074 - I_MASK - Interrupt mask register (R/W)
                        case 0x074:
                            bus->i_mask = data;
                            break;

                        // 0x1F8010A0 - DMA Channel 2 (GPU) base address (R/W)
                        case 0x0A0:
                            bus->dma_gpu_channel.madr = data;
                            break;

                        // 0x1F8010A4 - DMA Channel 2 (GPU) block control (R/W)
                        case 0x0A4:
                            bus->dma_gpu_channel.bcr = data;
                            break;

                        // 0x1F8010A8 - DMA Channel 2 (GPU) control (R/W)
                        case 0x0A8:
                            bus->dma_gpu_channel.chcr = data;
                            break;

                        // 0x1F8010E0 - DMA Channel 6 (OTC) base address (R/W)
                        case 0x0E0:
                            bus->dma_otc_channel.madr = data;
                            break;

                        // 0x1F8010E4 - DMA Channel 6 (OTC) block control (R/W)
                        case 0x0E4:
                            bus->dma_otc_channel.bcr = data;
                            break;

                        // 0x1F8010E8 - DMA Channel 6 (OTC) control (R/W)
                        case 0x0E8:
                            bus->dma_otc_channel.chcr = data;
                            break;

                        // 0x1F8010F0 - DMA Control Register (R/W)
                        case 0x0F0:
                            bus->dpcr = data;
                            break;

                        // 0x1F8010F4 - DMA Interrupt Register (R/W)
                        case 0x0F4:
                            bus->dicr = data;
                            break;

                        // 0x1F801810 - GP0 Commands/Packets (Rendering and
                        // VRAM Access)
                        case 0x810:
                            libps_gpu_process_gp0(bus->gpu, data);
                            break;

                        // 0x1F801814 - GP1 Commands (Display Control)
                        case 0x814:
                            libps_gpu_process_gp1(bus->gpu, data);
                            break;

                        default:
                            //printf("libps_bus_store_word(bus=%p,paddr=0x%08X,data=0x%08X): "
                               // "Unknown physical address!\n", (void*)&bus, paddr, data);
                            break;
                    }
                    break;
            }
            break;

        default:
           // printf("libps_bus_store_word(bus=%p,paddr=0x%08X,data=0x%08X): "
             //      "Unknown physical address!\n", (void*)&bus, paddr, data);
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
            printf("libps_bus_store_halfword(bus=%p,paddr=0x%08X,data=0x%02X) "
                   ": Unknown physical address!\n", (void*)&bus, paddr, data);
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