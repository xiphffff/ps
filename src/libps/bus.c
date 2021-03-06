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
#include <string.h>
#include "bus.h"
#include "utility/fifo.h"
#include "utility/memory.h"

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
        *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x1FFFFFFF));

        libps_gpu_process_gp0(&bus->gpu, data);
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
        libps_gpu_process_gp0(&bus->gpu, 0);

        *(uint32_t *)(bus->ram + (bus->dma_gpu_channel.madr & 0x1FFFFFFF)) =
        bus->gpu.gpuread;

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

            libps_gpu_process_gp0(&bus->gpu, entry);
            packet_size--;
        }

        // Break the loop if the end of list marker has been reached
        //
        // XXX: In a debugging/testing setting this might be dangerous; there
        // is no way to guarantee that the end of list marker is actually
        // *there*. If it's not there, this loop will never exit.
        if (header & 0x00800000)
        {
            break;
        }
        bus->dma_gpu_channel.madr = header & 0x001FFFFC;
    }
}

// Handles processing of DMA channel 3 - CDROM in normal mode.
static void dma_cdrom(struct libps_bus* bus)
{
    assert(bus != NULL);

    unsigned int num_words = (bus->dma_cdrom_channel.bcr & 0x0000FFFF) * 4;
    uint32_t address = bus->dma_cdrom_channel.madr;

    while (num_words != 0)
    {
        const uint8_t data = libps_fifo_dequeue(&bus->cdrom.data_fifo);

        libps_bus_store_byte(bus, address++, data);
        num_words--;
    }
}

// Handles processing of DMA channel 3 - CDROM in chopping mode.
static void dma_cdrom_chopped(struct libps_bus* bus)
{
    assert(bus != NULL);
    __debugbreak();
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

// Initializes the system bus. The system bus is the interconnect between the
// CPU and devices, and accordingly has primary ownership of devices. The
// system bus does not directly know about the CPU, however.
//
// `bios_data_ptr` is a pointer to the BIOS data loaded by the caller, passed
// by `libps_system_create()`.
//
// Do not call this function directly.
void libps_bus_setup(struct libps_bus* bus, uint8_t* const bios_data_ptr)
{
    assert(bus != NULL);

    bios = bios_data_ptr;
#ifdef LIBPS_DEBUG
    bus->debug_unknown_memory_load    = NULL;
    bus->debug_unknown_memory_store   = NULL;
    bus->debug_interrupt_requested    = NULL;
    bus->debug_interrupt_acknowledged = NULL;
#endif // LIBPS_DEBUG
    bus->ram = libps_safe_malloc(0x200000);

    libps_gpu_setup(&bus->gpu);
    libps_cdrom_setup(&bus->cdrom);
}

// Destroys the system bus, destroying all memory and devices. Please note that
// connected peripherals WILL NOT BE DESTROYED with this function; refer to the
// peripherals' own destroy functions and use them accordingly.
void libps_bus_cleanup(struct libps_bus* bus)
{
    libps_gpu_cleanup(&bus->gpu);
    libps_cdrom_cleanup(&bus->cdrom);

    libps_safe_free(bus->ram);
}

// Resets the system bus, which resets the peripherals to their startup state
// and clears memory.
void libps_bus_reset(struct libps_bus* bus)
{
    assert(bus != NULL);

    bus->dpcr = 0x07654321;
    bus->dicr = 0x00000000;

    bus->i_stat = 0x00000000;
    bus->i_mask = 0x00000000;

    memset(bus->ram,              0, sizeof(uint8_t) * 0x200000);
    memset(&bus->dma_gpu_channel, 0, sizeof(bus->dma_gpu_channel));
    memset(&bus->dma_otc_channel, 0, sizeof(bus->dma_otc_channel));

    libps_gpu_reset(&bus->gpu);
    libps_cdrom_reset(&bus->cdrom);
    libps_rcnt_reset(&bus->rcnt);
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

            case 15:
                switch (bus->dma_cdrom_channel.chcr)
                {
                    case 0x11000000:
                        dma_cdrom(bus);
                        break;

                    case 0x11400100:
                        dma_cdrom_chopped(bus);
                        break;
                }

                bus->dma_cdrom_channel.chcr &= ~(1 << 24);
                break;

            // DMA channel 6 - OTC (reverse clear OT)
            case 27:
                dma_otc_process(bus);
                break;
        }
    }

    if (bus->cdrom.fire_interrupt)
    {
        bus->cdrom.fire_interrupt = false;
        bus->i_stat |= LIBPS_IRQ_CDROM;

#ifdef LIBPS_DEBUG
        if (bus->debug_interrupt_requested)
        {
            bus->debug_interrupt_requested(bus->debug_user_data,
                                           2);
        }
#endif // LIBPS_DEBUG
    }

    libps_cdrom_step(&bus->cdrom);
    libps_rcnt_step(&bus->rcnt);
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
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
#ifdef LIBPS_DEBUG
                            if (bus->debug_interrupt_acknowledged)
                            {
                                for (unsigned int bit = 0; bit < 11; ++bit)
                                {
                                    if ((bus->i_stat & (1 << bit)) &&
                                        !(data & (1 << bit)))
                                    {
                                        bus->debug_interrupt_acknowledged(bus->debug_user_data, bit);
                                    }
                                }
                            }
#endif // LIBPS_DEBUG
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

                        // 0x1F8010B0 - DMA Channel 3 (CDROM) base address (R/W)
                        case 0x0B0:
                            bus->dma_cdrom_channel.madr = data;
                            break;

                        // 0x1F8010B4 - DMA Channel 3 (CDROM) block control (R/W)
                        case 0x0B4:
                            bus->dma_cdrom_channel.bcr = data;
                            break;

                        // 0x1F8010B8 - DMA Channel 3 (CDROM) control (R/W)
                        case 0x0B8:
                            bus->dma_cdrom_channel.chcr = data;
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
                            libps_rcnt_set_mode(&bus->rcnt, 1, data);
                            break;

                        // 0x1F801118 - Timer 1 Counter Target Value (R/W)
                        case 0x118:
                            bus->rcnt.rcnts[1].target = data;
                            break;

                        // 0x1F801810 - GP0 Commands/Packets (Rendering and
                        // VRAM Access)
                        case 0x810:
                            libps_gpu_process_gp0(&bus->gpu, data);
                            break;

                        // 0x1F801814 - GP1 Commands (Display Control)
                        case 0x814:
                            libps_gpu_process_gp1(&bus->gpu, data);
                            break;
#ifdef LIBPS_DEBUG
                        default:
                            if (bus->debug_unknown_memory_store)
                            {
                                bus->debug_unknown_memory_store
                                (bus->debug_user_data, paddr, data, LIBPS_DEBUG_WORD);
                            }
                            break;
#endif // LIBPS_DEBUG
                    }
                    break;
            }
            break;
#ifdef LIBPS_DEBUG
        default:
            if (bus->debug_unknown_memory_store)
            {
                bus->debug_unknown_memory_store
                (bus->debug_user_data, paddr, data, LIBPS_DEBUG_WORD);
            }
            break;
#endif // LIBPS_DEBUG
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
                    *(uint16_t *)(bus->scratch_pad + (paddr & 0x00000FFF)) =
                    data;
                    
                    break;

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
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
                            bus->rcnt.rcnts[0].value = data;
                            break;

                        // 0x1F801104 - Timer 0 Counter Mode (R/W)
                        case 0x104:
                            libps_rcnt_set_mode(&bus->rcnt, 0, data);
                            break;

                        // 0x1F801108 - Timer 0 Counter Target Value (R/W)
                        case 0x108:
                            bus->rcnt.rcnts[0].target = data;
                            break;

                        // 0x1F801110 - Timer 1 Counter Value (R/W)
                        case 0x110:
                            bus->rcnt.rcnts[1].value = data;
                            break;

                        // 0x1F801114 - Timer 1 Counter Mode (R/W)
                        case 0x114:
                            libps_rcnt_set_mode(&bus->rcnt, 1, data);
                            break;

                        // 0x1F801118 - Timer 1 Counter Target Value (R/W)
                        case 0x118:
                            bus->rcnt.rcnts[1].target = data;
                            break;

                        // 0x1F801120 - Timer 2 Counter Value (R/W)
                        case 0x120:
                            bus->rcnt.rcnts[2].value = data;
                            break;

                        // 0x1F801124 - Timer 2 Counter Mode (R/W)
                        case 0x124:
                            libps_rcnt_set_mode(&bus->rcnt, 2, data);
                            break;

                        // 0x1F801128 - Timer 2 Counter Target Value (R/W)
                        case 0x128:
                            bus->rcnt.rcnts[2].target = data;
                            break;
#ifdef LIBPS_DEBUG
                        default:
                            if (bus->debug_unknown_memory_store)
                            {
                                bus->debug_unknown_memory_store
                                (bus->debug_user_data,
                                 paddr,
                                 data,
                                 LIBPS_DEBUG_HALFWORD);
                            }
                            break;
#endif // LIBPS_DEBUG
                    }
                    break;
#ifdef LIBPS_DEBUG
                default:
                    if (bus->debug_unknown_memory_store)
                    {
                        bus->debug_unknown_memory_store
                        (bus->debug_user_data, paddr, data, LIBPS_DEBUG_HALFWORD);
                    }
#endif // LIBPS_DEBUG
                    break;
            }
            break;
#ifdef LIBPS_DEBUG
        default:
            if (bus->debug_unknown_memory_store)
            {
                bus->debug_unknown_memory_store
                (bus->debug_user_data, paddr, data, LIBPS_DEBUG_HALFWORD);
            }
            break;
#endif // LIBPS_DEBUG
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
            *(uint8_t *)(bus->ram + paddr) = data;
            break;

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    *(uint8_t *)(bus->scratch_pad + (paddr & 0x00000FFF)) =
                    data;

                    break;

                // I/O Ports
                case 0x1:
                    switch (paddr & 0x00000FFF)
                    {
                        // 0x1F801800 - Index/Status Register (Bit0-1 R/W)
                        // (Bit2-7 Read Only)
                        case 0x800:
                            bus->cdrom.status.raw =
                            (bus->cdrom.status.raw & ~0x03) | (data & 0x03);

                            break;

                        // 0x1F801801 - CD-ROM register store
                        case 0x801:
                            libps_cdrom_register_store(&bus->cdrom, 1, data);
                            break;

                        // 0x1F801802 - CD-ROM register store
                        case 0x802:
                            libps_cdrom_register_store(&bus->cdrom, 2, data);
                            break;

                        // 0x1F801803 - CD-ROM register store
                        case 0x803:
                            libps_cdrom_register_store(&bus->cdrom, 3, data);
                            break;

                        default:
#ifdef LIBPS_DEBUG
                            if (bus->debug_unknown_memory_store)
                            {
                                bus->debug_unknown_memory_store
                                (bus->debug_user_data, paddr, data, LIBPS_DEBUG_BYTE);
                            }
#endif // LIBPS_DEBUG
                            break;
                    }
                    break;

                default:
#ifdef LIBPS_DEBUG
                    if (bus->debug_unknown_memory_store)
                    {
                        bus->debug_unknown_memory_store
                        (bus->debug_user_data, paddr, data, LIBPS_DEBUG_BYTE);
                    }
#endif // LIBPS_DEBUG
                    break;
            }
            break;

        default:
#ifdef LIBPS_DEBUG
            if (bus->debug_unknown_memory_store)
            {
                bus->debug_unknown_memory_store
                (bus->debug_user_data, paddr, data, LIBPS_DEBUG_BYTE);
            }
#endif // LIBPS_DEBUG
            break;
    }
}

// Returns a word from memory referenced by virtual address `vaddr`.
uint32_t libps_bus_load_word(struct libps_bus* bus, const uint32_t vaddr)
{
    assert(bus != NULL);

    // This technically isn't accurate as it clobbers the Cache Control register
    // (0xFFFE0130), but for now it works.
    const uint32_t paddr = vaddr & 0x1FFFFFFF;

    switch ((paddr & 0xFFFF0000) >> 16)
    {
        case 0x0000 ... 0x001F:
            return *(uint32_t *)(bus->ram + (paddr & 0x1FFFFFFF));

        case 0x1F80:
            switch ((paddr & 0x0000F000) >> 12)
            {
                // Scratchpad
                case 0x0:
                    return *(uint32_t *)(bus->scratch_pad +
                                        (paddr & 0x00000FFF));

                // I/O Ports
                case 0x1:
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
                            return bus->rcnt.rcnts[1].value;

                        // 0x1F801810 - Read responses to GP0(C0h) and GP1(10h)
                        // commands
                        case 0x810:
                            return bus->gpu.gpuread;

                        // 0x1F801814 - GPU Status Register (R)
                        case 0x814:
                            return 0x1FF00000;

                        default:
#ifdef LIBPS_DEBUG
                            if (bus->debug_unknown_memory_load)
                            {
                                bus->debug_unknown_memory_load
                                (bus->debug_user_data, paddr, LIBPS_DEBUG_WORD);
                            }
#endif // LIBPS_DEBUG
                            return 0x00000000;
                    }
                    break;

                default:
#ifdef LIBPS_DEBUG
                    if (bus->debug_unknown_memory_load)
                    {
                        bus->debug_unknown_memory_load
                        (bus->debug_user_data, paddr, LIBPS_DEBUG_WORD);
                    }
#endif // LIBPS_DEBUG
                    return 0x00000000;
            }
            break;

        case 0x1FC0 ... 0x1FC7:
            return *(uint32_t *)(bios + (paddr & 0x000FFFFF));

        default:
#ifdef LIBPS_DEBUG
            if (bus->debug_unknown_memory_load)
            {
                bus->debug_unknown_memory_load
                (bus->debug_user_data, paddr, LIBPS_DEBUG_WORD);
            }
#endif // LIBPS_DEBUG
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
                        // 0x1F801070 - I_STAT - Interrupt status register
                        // (R=Status, W=Acknowledge)
                        case 0x070:
                            return bus->i_stat & 0x0000FFFF;

                        // 0x1F801074 - I_MASK - Interrupt mask register (R/W)
                        case 0x074:
                            return bus->i_mask & 0x0000FFFF;

                        // 0x1F801120 - Timer 2 (1/8 system clock) value
                        case 0x120:
                            return bus->rcnt.rcnts[2].value & 0x0000FFFF;

                        default:
#ifdef LIBPS_DEBUG
                            if (bus->debug_unknown_memory_load)
                            {
                                bus->debug_unknown_memory_load
                                (bus->debug_user_data, paddr, LIBPS_DEBUG_HALFWORD);
                            }
#endif // LIBPS_DEBUG
                            return 0x0000;
                    }
                    break;

                default:
#ifdef LIBPS_DEBUG
                    if (bus->debug_unknown_memory_load)
                    {
                        bus->debug_unknown_memory_load
                        (bus->debug_user_data, paddr, LIBPS_DEBUG_HALFWORD);
                    }
#endif // LIBPS_DEBUG
                    return 0x0000;
            }
            break;

        default:
#ifdef LIBPS_DEBUG
            if (bus->debug_unknown_memory_load)
            {
                bus->debug_unknown_memory_load
                (bus->debug_user_data, paddr, LIBPS_DEBUG_HALFWORD);
            }
#endif // LIBPS_DEBUG
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
                        // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
                        case 0x800:
                            return bus->cdrom.status.raw;

                        // 0x1F801801 - CD-ROM register load
                        case 0x801:
                            return libps_cdrom_register_load(&bus->cdrom, 1);

                        // 0x1F801803 - CD-ROM register load
                        case 0x803:
                            return libps_cdrom_register_load(&bus->cdrom, 3);

                        default:
#ifdef LIBPS_DEBUG
                            if (bus->debug_unknown_memory_load)
                            {
                                bus->debug_unknown_memory_load
                                (bus->debug_user_data, paddr, LIBPS_DEBUG_BYTE);
                            }
#endif // LIBPS_DEBUG
                            return 0x00;
                    }
                    break;

                default:
#ifdef LIBPS_DEBUG
                    if (bus->debug_unknown_memory_load)
                    {
                        bus->debug_unknown_memory_load
                        (bus->debug_user_data, paddr, LIBPS_DEBUG_BYTE);
                    }
#endif // LIBPS_DEBUG
                    return 0x00;
            }
            break;

        case 0x1FC0 ... 0x1FC7:
            return *(uint8_t *)(bios + (paddr & 0x000FFFFF));

        default:
#ifdef LIBPS_DEBUG
            if (bus->debug_unknown_memory_load)
            {
                bus->debug_unknown_memory_load
                (bus->debug_user_data, paddr, LIBPS_DEBUG_BYTE);
            }
#endif // LIBPS_DEBUG
            return 0x00;
    }
}