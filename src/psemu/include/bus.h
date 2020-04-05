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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#include "cdrom_drive.h"
#include "gpu.h"
#ifdef PSEMU_DEBUG
// Pass these as the 3rd parameter to `debug_unknown_memory_load()` or the 4th
// parameter to `debug_unknown_memory_store()`.
#define PSEMU_DEBUG_WORD 0xFFFFFFFF
#define PSEMU_DEBUG_HALFWORD 0x0000FFFF
#define PSEMU_DEBUG_BYTE 0x000000FF
#endif // PSEMU_DEBUG
// Defines the structure of the system bus. This is really just the
// interconnect between the memory and devices.
struct psemu_bus
{
    // CD-ROM drive instance
    struct psemu_cdrom_drive cdrom_drive;

    // GPU instance
    struct psemu_gpu gpu;

    // [0x00000000 - 0x001FFFFF] - Main RAM (first 64K reserved for BIOS)
    uint8_t* ram;

    // [0x1F800000 - 0x1F8003FF] - Scratchpad (D-Cache used as Fast RAM)
    uint8_t scratch_pad[1024];

    // Interrupt structure
    union
    {
        struct
        {
            // (PAL=50Hz, NTSC=60Hz)
            unsigned int vblank : 1;

            // Can be requested via GP0(0x1F) command (rarely used)
            unsigned int gpu : 1;

            // CD-ROM decoder
            unsigned int cdrom : 1;

            // DMA controller
            unsigned int dmac : 1;

            // Root Counter 0 (Sysclk or Dotclk)
            unsigned int rtc0 : 1;

            // Root Counter 1 (Sysclk or H-blank)
            unsigned int rtc1 : 1;

            // Root Counter 2 (Sysclk or Sysclk/8)
            unsigned int rtc2 : 1;

            // Parallel I/O
            unsigned int pio : 1;

            // Serial I/O
            unsigned int sio : 1;

            // Sound Processing Unit
            unsigned int spu : 1;

            // Controller
            unsigned int cntl : 1;
            unsigned int : 21;
        };
        uint32_t word;
    } i_stat, // 0x1F801070 - Interrupt status register
      i_mask; // 0x1F801074 - Interrupt mask register

    // DMA channel structure
    struct
    {
        // 0x1F801080 + (N * 0x10) - Base address (R/W)
        union
        {
            struct
            {
                unsigned int address : 24;
                unsigned int : 8;
            };
            uint32_t word;
        } madr;

        // 0x1F801084 + (N * 0x10) - Block Control (R/W)
        uint32_t bcr;

        // 0x1F801088 + (N * 0x10) - Channel Control (R/W)
        union
        {
            struct
            {
                // Transfer direction - (0=To Main RAM, 1=From Main RAM)
                unsigned int from_main_ram : 1;

                // Memory address step - 0=Forward;+4, 1=Backward;-4 
                unsigned int memstep_backward : 1;

                unsigned int : 6;

                // (0=Normal, 1=Chopping; run CPU during DMA gaps)
                unsigned int chopping_enabled : 1;

                unsigned int sync_mode : 2;

                unsigned int : 5;

                unsigned int chopping_dma_size : 3;
                unsigned int : 1;
                unsigned int chopping_cpu_size : 3;
                unsigned int : 1;
                unsigned int busy : 1;
                unsigned int : 3;
                unsigned int start : 1;
                unsigned int : 3;
            };
            uint32_t word;
        } chcr;
    } dma_gpu, // 0x1F8010Ax - DMA2 - GPU (lists + image data)
      dma_otc; // 0x1F8010Ex - DMA6 - OTC (reverse clear OT) (GPU related)

    // 0x1F8010F0 - DMA Control Register (R/W)
    union
    {
        struct
        {
            unsigned int mdecin_priority  : 3;
            unsigned int mdecin_enabled   : 1;
            unsigned int mdecout_priority : 3;
            unsigned int mdecout_enabled  : 1;
            unsigned int gpu_priority     : 3;
            unsigned int gpu_enabled      : 1;
            unsigned int cdrom_priority   : 3;
            unsigned int cdrom_enabled    : 1;
            unsigned int spu_priority     : 3;
            unsigned int spu_enabled      : 1;
            unsigned int pio_priority     : 3;
            unsigned int pio_enabled      : 1;
            unsigned int otc_priority     : 3;
            unsigned int otc_enabled      : 1;
        };
        uint32_t word;
    } dpcr;

    // 0x1F8010F4 - DMA Interrupt Register (R/W)
    union
    {
        struct
        {
            unsigned int : 15;
            unsigned int force_irq : 1;
            unsigned int dma0_irq_enabled : 1;
            unsigned int dma1_irq_enabled : 1;
            unsigned int dma2_irq_enabled : 1;
            unsigned int dma3_irq_enabled : 1;
            unsigned int dma4_irq_enabled : 1;
            unsigned int dma5_irq_enabled : 1;
            unsigned int dma6_irq_enabled : 1;
            unsigned int irq_dma_master_enabled : 1;
            unsigned int dma0_irq_flag : 1;
            unsigned int dma1_irq_flag : 1;
            unsigned int dma2_irq_flag : 1;
            unsigned int dma3_irq_flag : 1;
            unsigned int dma4_irq_flag : 1;
            unsigned int dma5_irq_flag : 1;
            unsigned int dma6_irq_flag : 1;
            unsigned int irq_master_flag : 1;
        };
        uint32_t word;
    } dicr;
#ifdef PSEMU_DEBUG
    // Called when an unknown memory load has taken place.
    void (*debug_unknown_memory_load)(void* user_data,
                                      const uint32_t paddr,
                                      const unsigned int type);

    // Called when an unknown memory store has taken place.
    void (*debug_unknown_memory_store)(void* user_data,
                                       const uint32_t paddr,
                                       const unsigned int data,
                                       const unsigned int type);

    void* debug_user_data;
#endif // PSEMU_DEBUG
};

// Initializes a system bus `bus`.
void psemu_bus_init(struct psemu_bus* const bus, uint8_t* const m_bios_data);

// Deallocates all memory held by a system bus `bus`.
void psemu_bus_fini(struct psemu_bus* const bus);

// Processes DMA requests and interrupts.
void psemu_bus_step(struct psemu_bus* const bus);

// Clears all memory held by a system bus `bus`.
void psemu_bus_reset(struct psemu_bus* const bus);

// Returns a word from system bus `bus` referenced by virtual address `vaddr`.
// Virtual -> Physical address translation takes place automatically.
uint32_t psemu_bus_load_word(const struct psemu_bus* const bus,
                             const uint32_t vaddr);

// Returns a halfword from system bus `bus` referenced by virtual address
// `vaddr`.
//
// Virtual -> Physical address translation takes place automatically.
uint16_t psemu_bus_load_halfword(const struct psemu_bus* const bus,
                                 const uint32_t vaddr);

// Returns a byte from system bus `bus` referenced by virtual address `vaddr`.
// Virtual -> Physical address translation takes place automatically.
uint8_t psemu_bus_load_byte(const struct psemu_bus* const bus,
                            const uint32_t vaddr);

// Stores a word `word` into system bus `bus` referenced by virtual address
// `vaddr`.
//
// Virtual -> Physical address translation takes place automatically.
void psemu_bus_store_word(struct psemu_bus* const bus,
                          const uint32_t vaddr,
                          const uint32_t word);

// Stores a halfword `halfword` into system bus `bus` referenced by virtual
// address `vaddr`.
//
// Virtual -> Physical address translation takes place automatically.
void psemu_bus_store_halfword(struct psemu_bus* const bus,
                              const uint32_t vaddr,
                              const uint16_t halfword);

// Stores a byte `byte` into system bus `bus` referenced by virtual address
// `vaddr`.
//
// Virtual -> Physical address translation takes place automatically.
void psemu_bus_store_byte(struct psemu_bus* const bus,
                          const uint32_t vaddr,
                          const uint8_t byte);
#ifdef __cplusplus
}
#endif // __cplusplus