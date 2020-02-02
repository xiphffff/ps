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
#include <stdarg.h>
#include "cd.h"
#include "utility/fifo.h"
#include "utility/memory.h"

// Queues an interrupt `interrupt`, delaying its firing by `delay_cycles`.
static void queue_interrupt(struct libps_cdrom* cdrom,
                            const enum libps_cdrom_interrupt_type interrupt,
                            const unsigned int delay_cycles,
                            const unsigned int num_args,
                            ...)
{
    assert(cdrom != NULL);

    va_list args;
    va_start(args, num_args);

    int arg;

    for (unsigned int i = 0; i < num_args; ++i)
    {
        arg = va_arg(args, int);
        libps_fifo_enqueue(cdrom->response_fifo, arg);
    }
    va_end(args);

    cdrom->interrupts[0].pending = true;
    cdrom->interrupts[0].cycles = delay_cycles;
    cdrom->interrupts[0].type = interrupt;
}

// Allocates memory for a `libps_cdrom` structure and returns a pointer to it
// if memory allocation was successful, or `NULL` otherwise.
struct libps_cdrom* libps_cdrom_create(void)
{
    struct libps_cdrom* cdrom =
    libps_safe_malloc(sizeof(struct libps_cdrom));

    cdrom->response_fifo  = libps_fifo_create(16);
    cdrom->parameter_fifo = libps_fifo_create(16);

    return cdrom;
}

// Deallocates the memory held by `cdrom`.
void libps_cdrom_destroy(struct libps_cdrom* cdrom)
{
    libps_fifo_destroy(cdrom->response_fifo);
    libps_fifo_destroy(cdrom->parameter_fifo);

    libps_safe_free(cdrom);
}

// Resets the CD-ROM drive to its initial state.
void libps_cdrom_reset(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    libps_fifo_reset(cdrom->parameter_fifo);
    libps_fifo_reset(cdrom->response_fifo);

    memset(cdrom->interrupts, 0, sizeof(cdrom->interrupts));

    cdrom->fire_interrupt = false;
    cdrom->status = 0x18;
}

// Checks to see if interrupts needs to be fired.
void libps_cdrom_step(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    for (unsigned int i = 0; i < 2; ++i)
    {
        if (cdrom->interrupts[i].pending)
        {
            if (cdrom->interrupts[i].cycles != 0)
            {
                cdrom->interrupts[i].cycles--;
            }
            else
            {
                cdrom->interrupts[i].pending = false;
                cdrom->fire_interrupt = true;

                cdrom->interrupt_flag = (cdrom->interrupt_flag & ~0x07) |
                    (cdrom->interrupts[i].type & 0x07);
            }
        }
    }
}

// Loads indexed CD-ROM register `reg`.
uint8_t libps_cdrom_indexed_register_load(struct libps_cdrom* cdrom,
                                          const unsigned int reg)
{
    assert(cdrom != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            switch (cdrom->status & 0x03)
            {
                // 1F801801h.Index1 - Response Fifo (R)
                case 1:
                    return libps_fifo_dequeue(cdrom->response_fifo);

                default:
                    __debugbreak();
                    break;
            }
            break;

        // 0x1F801803
        case 3:
            switch (cdrom->status & 0x03)
            {
                // 1F801803h.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    return cdrom->interrupt_flag;

                default:
                    __debugbreak();
                    break;
            }
            break;

        default:
            __debugbreak();
            break;
    }
}

// Stores `data` into indexed CD-ROM register `reg`.
void libps_cdrom_indexed_register_store(struct libps_cdrom* cdrom,
                                        const unsigned int reg,
                                        const uint8_t data)
{
    assert(cdrom != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            switch (cdrom->status & 0x03)
            {
                // 1F801801h.Index0 - Command Register (W)
                case 0:
                    switch (data)
                    {
                        // Getstat
                        case 0x01:
                            queue_interrupt(cdrom, INT3, 20000, 1, cdrom->status);
                            break;

                        case 0x19:
                            switch (libps_fifo_dequeue(cdrom->parameter_fifo))
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    queue_interrupt(cdrom, INT3, 20000, 4,
                                                    0x94, 0x09, 0x19, 0xC0);
                                    break;

                                default:
                                    __debugbreak();
                                    break;
                            }
                            break;

                        // GetID
                        case 0x1A:
                            queue_interrupt(cdrom, INT5, 20000, 8,
                                            0x08, 0x40, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00);
                            break;

                        default:
                            __debugbreak();
                            break;
                    }
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        // 0x1F801802
        case 2:
            switch (cdrom->status & 0x03)
            {
                // 1F801802h.Index0 - Parameter Fifo (W)
                case 0:
                    libps_fifo_enqueue(cdrom->parameter_fifo, data);
                    break;

                // 1F801802h.Index1 - Interrupt Enable Register (W)
                case 1:
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        // 0x1F801803
        case 3:
            switch (cdrom->status & 0x03)
            {
                // 1F801803h.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        default:
            __debugbreak();
            break;
    }
}
