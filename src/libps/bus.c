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

// XXX: Get rid of separate load_byte()/load_halfword()/load_word() and such
// and try to condense all of those functions down to load_memory() and
// store_memory().
//
// If we were using C++, we could do:
//
// `const auto data = bus.load_memory<MIPS::Word>(paddr);` and the problem
// would be solved.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"
#include "cd.h"
#include "gpu.h"
#include "timer.h"

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

// Handles processing of DMA channel 2 - GPU (lists + image data) in VRAM read
// mode.
static void dma_gpu_vram_read_process(struct libps_bus* bus)
{
    assert(bus != NULL);

    const uint16_t ba = bus->dma_gpu_channel.bcr >> 16;
    const uint16_t bs = bus->dma_gpu_channel.bcr & 0x0000FFFF;

    for (uint32_t count = 0; count != (ba * bs); ++count)
    {
        // Hack (state should be `LIBPS_GPU_TRANSFERRING_DATA`)
        libps_gpu_process_gp0(bus->gpu, 0);

        *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x00FFFFFF)) =
        bus->gpu->gpuread;

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
        bus->ram = malloc(0x200000);

        bus->gpu   = libps_gpu_create();
        bus->cdrom = libps_cdrom_create();
        bus->timer = libps_timer_create();

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
    libps_cdrom_destroy(bus->cdrom);
    libps_timer_destroy(bus->timer);

    free(bus->ram);
    free(bus);
}

// Resets the system bus, which resets the peripherals to their startup state.
void libps_bus_reset(struct libps_bus* bus)
{
    assert(bus != NULL);

    bus->dpcr = 0x07654321;
    bus->dicr = 0x00000000;

    bus->i_stat = 0x00000000;
    bus->i_mask = 0x00000000;

    memset(bus->ram, 0, sizeof(uint8_t) * 0x200000);
    memset(&bus->dma_gpu_channel, 0, sizeof(bus->dma_gpu_channel));
    memset(&bus->dma_otc_channel, 0, sizeof(bus->dma_otc_channel));

    libps_gpu_reset(bus->gpu);
    libps_cdrom_reset(bus->cdrom);
    libps_timer_reset(bus->timer);
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
                    // VramRead
                    case 0x01000200:
                        dma_gpu_vram_read_process(bus);
                        break;

                    // VramWrite
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

    if (bus->cdrom->fire_interrupt)
    {
        bus->cdrom->fire_interrupt = false;
        bus->i_stat |= (1 << 2);
    }

    libps_cdrom_step(bus->cdrom);
    libps_timer_step(bus->timer);
}

// Returns a word from memory referenced by virtual address `vaddr`.
uint32_t libps_bus_load_word(struct libps_bus* bus, const uint32_t vaddr)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            return *(uint32_t *)(bus->ram + (paddr & 0x1FFFFFFF));

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    return *(uint32_t *)(bus->scratch_pad + (paddr & 0x00000FFF));

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
                            return bus->i_stat;

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

                        // 0x1F801110 - Timer 1 Counter Value (R/W)
                        case 0x110:
                            return bus->timer->timers[1].value;

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
                            __debugbreak();
                            return 0x00000000;
                    }

                default:
                    printf("libps_bus_load_word(bus=%p,paddr=0x%08X): Unknown "
                           "physical address!\n", (void*)&bus, paddr);
                    __debugbreak();
                    return 0x00000000;
            }

        case 0x1FC0 ... 0x1FC7:
            return *(uint32_t *)(bios + (paddr & 0x000FFFFF));

        default:
            printf("libps_bus_load_word(bus=%p,paddr=0x%08X): Unknown "
                   "physical address!\n", (void*)&bus, paddr);
            __debugbreak();
            return 0x00000000;
    }
}

// Returns a halfword from memory referenced by virtual address `vaddr`.
uint16_t libps_bus_load_halfword(struct libps_bus* bus, const uint32_t vaddr)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            return *(uint16_t *)(bus->ram + (paddr & 0x1FFFFFFF));

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    return *(uint16_t *)(bus->scratch_pad + (paddr & 0x00000FFF));

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801044 - JOY_STAT (R)
                        case 0x044:
                            return 0xFFFF;

                        // 0x1F80104A - JOY_CTRL (R/W)
                        // (usually 1003h,3003h,0000h)
                        case 0x04A:
                            break;

                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
                            return bus->i_stat & 0xFFFF;

                        // 0x1F801074 - I_MASK - Interrupt mask register (R/W)
                        case 0x074:
                            return bus->i_mask & 0xFFFF;

                        // 0x1F801120 - Timer 2 (1/8 system clock) value
                        case 0x120:
                            return 0xFFFF;

                        // 0x1F801C00 .. 0x1F801E5F - SPU Area (stubbed)
                        case 0xC00 ... 0xE5F:
                            return 0x0000;

                        default:
                            printf("libps_bus_load_halfword(bus=%p,paddr=0x%08X): Unknown physical "
                                   "address!\n", (void*)&bus, paddr);
                            __debugbreak();
                            return 0x0000;
                    }
                    break;

                default:
                    printf("libps_bus_load_halfword(bus=%p,paddr=0x%08X): Unknown physical "
                           "address!\n", (void*)&bus, paddr);
                    __debugbreak();
                    return 0x0000;
            }
            break;

        default:
            printf("libps_bus_load_halfword(bus=%p,paddr=0x%08X): Unknown physical "
                   "address!\n", (void*)&bus, paddr);
            __debugbreak();
            return 0x0000;
    }
}

// Returns a byte from memory referenced by virtual address `vaddr`.
uint8_t libps_bus_load_byte(struct libps_bus* bus, const uint32_t vaddr)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        // Main RAM(first 64K reserved for BIOS)
        case 0x0000 ... 0x001F:
            return *(uint8_t *)(bus->ram + (paddr & 0x1FFFFFFF));

        // Expansion Region 1 (ROM/RAM)
        case 0x1F00:
            return 0x00;

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    return *(uint8_t *)(bus->scratch_pad + (paddr & 0x00000FFF));

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801040 - JOY_RX_DATA (R)
                        case 0x040:
                            return 0x0000;

                        // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
                        case 0x800:
                            return bus->cdrom->status;

                        // 0x1F801801 - Indexed CD-ROM register load
                        case 0x801:
                            return libps_cdrom_indexed_register_load(bus->cdrom, 1);

                        // 0x1F801803 - Indexed CD-ROM register load
                        case 0x803:
                            return libps_cdrom_indexed_register_load(bus->cdrom, 3);

                        default:
                            printf("libps_bus_load_byte(bus=%p,paddr=0x%08X): Unknown "
                                "physical address!\n", (void*)&bus, paddr);
                            __debugbreak();
                            return 0x00;
                    }
                    break;

                default:
                    printf("libps_bus_load_byte(bus=%p,paddr=0x%08X): Unknown "
                            "physical address!\n", (void*)&bus, paddr);
                    __debugbreak();
                    return 0x00;
            }
            break;

        case 0x1FC0 ... 0x1FC7:
            return *(uint8_t *)(bios + (paddr & 0x000FFFFF));

        default:
            printf("libps_bus_load_byte(bus=%p,paddr=0x%08X): Unknown "
                   "physical address!\n", (void*)&bus, paddr);
            __debugbreak();
            return 0x00;
    }
}

// Stores word `data` into memory referenced by virtual address `vaddr`.
void libps_bus_store_word(struct libps_bus* bus,
                          const uint32_t vaddr,
                          const uint32_t data)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            *(uint32_t *)(bus->ram + (paddr & 0x1FFFFFFF)) = data;
            break;

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    *(uint32_t *)(bus->scratch_pad + (paddr & 0x00000FFF)) = data;
                    break;

                // I/O Ports
                case 0x1:
                    // XXX: I think perhaps this could be made into an array,
                    // however there'd be a lot of unused addresses unless we
                    // could compartmentalize it somehow; must investigate.
                    //
                    // But for now, the compiler remains smarter than me.
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801000 - Expansion 1 Base Address (usually 1F000000h)
                        case 0x000:
                            break;

                        // 0x1F801004 - Expansion 2 Base Address (usually 1F802000h)
                        case 0x004:
                            break;

                        // 0x1F801008 - Expansion 1 Delay / Size (usually
                        // 0x0013243F) (512Kbytes, 8bit bus)
                        case 0x008:
                            break;

                        // 0x1F80100C - Expansion 3 Delay/Size (usually
                        // 00003022h) (1 byte)
                        case 0x00c:
                            break;

                        // 0x1F801010 - BIOS ROM Delay/Size (usually 0013243Fh)
                        // (512Kbytes, 8bit bus)
                        case 0x010:
                            break;

                        // 0x1F801014 - SPU Delay/Size (200931E1h) (use
                        // 220931E1h for SPU-RAM reads)
                        case 0x014:
                            break;

                        // 0x1F801018 - CDROM Delay/Size (00020843h or 00020943h)
                        case 0x018:
                            break;

                        // 0x1F80101C - Expansion 2 Delay/Size (usually
                        // 00070777h) (128 bytes, 8bit bus)
                        case 0x01C:
                            break;

                        // 0x1F801020 - COM_DELAY (00031125h or 0000132Ch or
                        // 00001325h)
                        case 0x020:
                            break;

                        // 0x1F801060 - RAM_SIZE (usually 00000B88h; 2MB RAM
                        // mirrored in first 8MB)
                        case 0x060:
                            break;

                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
                            bus->i_stat &= data;
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

                        // 0x1F801114 - Timer 1 Counter Mode (R/W)
                        case 0x114:
                            bus->timer->timers[1].mode  = data;
                            bus->timer->timers[1].value = 0x0000;

                            break;

                        // 0x1F801118 - Timer 1 Counter Target Value (R/W)
                        case 0x118:
                            bus->timer->timers[1].target = data;
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
                            printf("libps_bus_store_word(bus=%p,paddr=0x%08X,data=0x%08X): "
                                   "Unknown physical address!\n", (void*)&bus, paddr, data);
                            __debugbreak();
                            break;
                    }
                    break;
            }
            break;

        // 0xFFFE0130 - Cache Control (R/W). This is a hack due to the way
        // `LIBPS_CPU_TRANSLATE_ADDRESS()` is implemented.
        case 0x1FFE:
            break;

        default:
            printf("libps_bus_store_word(bus=%p,paddr=0x%08X,data=0x%08X): "
                   "Unknown physical address!\n", (void*)&bus, paddr, data);
            __debugbreak();
            break;
    }
}

// Stores halfword `data` into memory referenced by virtual address `paddr`.
void libps_bus_store_halfword(struct libps_bus* bus,
                              const uint32_t vaddr,
                              const uint16_t data)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            *(uint16_t *)(bus->ram + (paddr & 0x1FFFFFFF)) = data;
            break;

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    *(uint16_t *)(bus->scratch_pad + (paddr & 0x00000FFF)) = data;
                    break;

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801048 - JOY_MODE (R/W) (usually 000Dh, ie.
                        // 8bit, no parity, MUL1)
                        case 0x048:
                            break;

                        // 0x1F80104A - JOY_CTRL (R/W)
                        // (usually 1003h,3003h,0000h)
                        case 0x04A:
                            break;

                        // 0x1F80104E - JOY_BAUD (R/W) (usually 0088h, ie.
                        // circa 250kHz, when Factor=MUL1)
                        case 0x04E:
                            break;

                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
                            bus->i_stat &= data;
                            break;

                        // 0x1F801074 - I_MASK - Interrupt mask register (R/W)
                        case 0x074:
                            bus->i_mask = data;
                            break;

                        // 0x1F801100 - Timer 0 Counter Value (R/W)
                        case 0x100:
                            bus->timer->timers[0].value = data;
                            break;

                        // 0x1F801104 - Timer 0 Counter Mode (R/W)
                        case 0x104:
                            bus->timer->timers[0].mode  = data;
                            bus->timer->timers[0].value = 0x0000;

                            break;

                        // 0x1F801108 - Timer 0 Counter Target Value (R/W)
                        case 0x108:
                            bus->timer->timers[0].target = data;
                            break;

                        // 0x1F801110 - Timer 1 Counter Value (R/W)
                        case 0x110:
                            bus->timer->timers[1].value = data;
                            break;

                        // 0x1F801114 - Timer 1 Counter Mode (R/W)
                        case 0x114:
                            bus->timer->timers[1].mode  = data;
                            bus->timer->timers[1].value = 0x0000;

                            break;

                        // 0x1F801118 - Timer 1 Counter Target Value (R/W)
                        case 0x118:
                            bus->timer->timers[1].target = data;
                            break;

                        // 0x1F801120 - Timer 2 Counter Value (R/W)
                        case 0x120:
                            bus->timer->timers[2].value = data;
                            break;

                        // 0x1F801124 - Timer 2 Counter Mode (R/W)
                        case 0x124:
                            bus->timer->timers[2].mode  = data;
                            bus->timer->timers[2].value = 0x0000;

                            break;

                        // 0x1F801128 - Timer 2 Counter Target Value (R/W)
                        case 0x128:
                            bus->timer->timers[2].target = data;
                            break;

                        // 0x1F801C00 .. 0x1F801E5F - SPU Area (stubbed)
                        case 0xC00 ... 0xE5F:
                            break;

                        default:
                            printf("libps_bus_store_halfword(bus=%p,paddr=0x%08X,data=0x%02X) "
                                   ": Unknown physical address!\n", (void*)&bus, paddr, data);
                            __debugbreak();
                            break;
                    }
                    break;

                default:
                    printf("libps_bus_store_halfword(bus=%p,paddr=0x%08X,data=0x%02X) "
                           ": Unknown physical address!\n", (void*)&bus, paddr, data);
                    __debugbreak();
                    break;
            }
            break;

        default:
            printf("libps_bus_store_halfword(bus=%p,paddr=0x%08X,data=0x%02X) "
                ": Unknown physical address!\n", (void*)&bus, paddr, data);
            __debugbreak();
            break;
    }
}

// Stores byte `data` into memory referenced by virtual address `paddr`.
void libps_bus_store_byte(struct libps_bus* bus,
                          const uint32_t vaddr,
                          const uint8_t data)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    // XXX: I think the handling of this can be a bit more sound.
    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            *(uint8_t *)(bus->ram + (paddr & 0x1FFFFFFF)) = data;
            break;

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    *(uint8_t *)(bus->scratch_pad + (paddr & 0x00000FFF)) = data;
                    break;

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801040 JOY_TX_DATA (W)
                        case 0x040:
                            break;

                        // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
                        case 0x800:
                            bus->cdrom->status = (bus->cdrom->status & ~0x03) | (data & 0x03);
                            break;

                        // 0x1F801801 - Indexed CD-ROM register store
                        case 0x801:
                            libps_cdrom_indexed_register_store(bus->cdrom, 1, data);
                            break;

                        // 0x1F801802 - Indexed CD-ROM register store
                        case 0x802:
                            libps_cdrom_indexed_register_store(bus->cdrom, 2, data);
                            break;

                        // 0x1F801803 - Indexed CD-ROM register store
                        case 0x803:
                            libps_cdrom_indexed_register_store(bus->cdrom, 3, data);
                            break;

                        default:
                            printf("libps_bus_store_byte(bus=%p,paddr=0x%08X,data=0x%02X) "
                                ": Unknown physical address!\n", (void*)&bus, paddr, data);
                            __debugbreak();
                            break;
                    }
                    break;

                // EXP2 Post Registers
                case 0x2:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F802041 - POST - External 7 - segment Display (W)
                        case 0x041:
                            bus->post_status = data;
                            break;

                        default:
                            printf("libps_bus_store_byte(bus=%p,paddr=0x%08X,data=0x%02X) "
                                ": Unknown physical address!\n", (void*)&bus, paddr, data);
                            __debugbreak();
                            break;
                    }
                    break;

                default:
                    printf("libps_bus_store_byte(bus=%p,paddr=0x%08X,data=0x%02X) "
                           ": Unknown physical address!\n", (void*)&bus, paddr, data);
                    __debugbreak();
                    break;
            }
            break;

        default:
            printf("libps_bus_store_byte(bus=%p,paddr=0x%08X,data=0x%02X): Unknown "
                   "physical address!\n", (void*)&bus, paddr, data);
            __debugbreak();
            break;
    }
}
