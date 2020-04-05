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
#include <stdarg.h>
#include <string.h>
#include "cdrom_drive.h"

// Pushes a response to interrupt line `interrupt`, delaying its firing by
// `delay_cycles` cycles.
static void push_response(struct psemu_cdrom_drive* const cdrom_drive,
                          struct psemu_cdrom_drive_interrupt* const interrupt,
                          const unsigned int cycles_remaining,
                          const unsigned int num_args,
                          ...)
{
    assert(cdrom_drive != NULL);
    assert(interrupt != NULL);

    va_list args;
    va_start(args, num_args);

    int arg;

    for (unsigned int i = 0; i < num_args; ++i)
    {
        arg = va_arg(args, int);
        psemu_fifo_enqueue(&cdrom_drive->response_fifo, arg);
    }
    va_end(args);

    interrupt->pending = true;
    interrupt->cycles_remaining = cycles_remaining;
}

// Initializes a CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_init(struct psemu_cdrom_drive* const cdrom_drive)
{
    assert(cdrom_drive != NULL);

    psemu_fifo_init(&cdrom_drive->parameter_fifo, 16);
    psemu_fifo_init(&cdrom_drive->response_fifo,  16);
    psemu_fifo_init(&cdrom_drive->data_fifo,      4096);
}

// Destroys all memory held by CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_fini(struct psemu_cdrom_drive* const cdrom_drive)
{
    assert(cdrom_drive != NULL);

    psemu_fifo_fini(&cdrom_drive->parameter_fifo);
    psemu_fifo_fini(&cdrom_drive->response_fifo);
    psemu_fifo_fini(&cdrom_drive->data_fifo);
}

// Resets the CD-ROM drive `cdrom_drive` to the startup state.
void psemu_cdrom_drive_reset(struct psemu_cdrom_drive* const cdrom_drive)
{
    assert(cdrom_drive != NULL);

    cdrom_drive->status.byte          = 0x18;
    cdrom_drive->response_status.byte = 0x00;

    cdrom_drive->interrupt_enable.byte = 0x00;
    cdrom_drive->interrupt_flag.byte   = 0x00;

    psemu_fifo_reset(&cdrom_drive->parameter_fifo);
    psemu_fifo_reset(&cdrom_drive->response_fifo);
    psemu_fifo_reset(&cdrom_drive->data_fifo);

    memset(&cdrom_drive->int1, 0, sizeof(cdrom_drive->int1));
    memset(&cdrom_drive->int2, 0, sizeof(cdrom_drive->int2));
    memset(&cdrom_drive->int3, 0, sizeof(cdrom_drive->int3));
    memset(&cdrom_drive->int5, 0, sizeof(cdrom_drive->int5));

    cdrom_drive->int1.type = PSEMU_CDROM_DRIVE_INT1;
    cdrom_drive->int2.type = PSEMU_CDROM_DRIVE_INT2;
    cdrom_drive->int3.type = PSEMU_CDROM_DRIVE_INT3;
    cdrom_drive->int5.type = PSEMU_CDROM_DRIVE_INT5;

    cdrom_drive->current_interrupt = NULL;
}

// Checks to see if interrupts needs to be fired.
void psemu_cdrom_drive_step(struct psemu_cdrom_drive* cdrom_drive)
{
    assert(cdrom_drive != NULL);

    // Is there an interrupt pending?
    if (cdrom_drive->current_interrupt)
    {
        if (cdrom_drive->current_interrupt->cycles_remaining != 0)
        {
            cdrom_drive->current_interrupt->cycles_remaining--;
        }
        else
        {
            cdrom_drive->current_interrupt->pending = false;
            cdrom_drive->fire_interrupt = true;

            cdrom_drive->interrupt_flag.response |=
            cdrom_drive->current_interrupt->type;
        }
    }
}

// Stores `data` into register `reg` in CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_register_store
(struct psemu_cdrom_drive* const cdrom_drive,
const unsigned int reg,
const uint8_t data)
{
    assert(cdrom_drive != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801801.Index0 - Command Register (W)
                case 0:
                    switch (data)
                    {
                        // Getstat
                        case 0x01:
                            push_response
                            (cdrom_drive, &cdrom_drive->int3,
                             20000, 1, cdrom_drive->response_status.byte);

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            cdrom_drive->int3.next_interrupt = NULL;
                            return;

                        case 0x19:
                            switch (psemu_fifo_dequeue(&cdrom_drive->parameter_fifo))
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    push_response
                                    (cdrom_drive, &cdrom_drive->int3, 20000, 4,
                                    0x94, 0x09, 0x19, 0xC0);
                                    
                                    cdrom_drive->current_interrupt =
                                    &cdrom_drive->int3;

                                    cdrom_drive->int3.next_interrupt = NULL;
                                    return;

                                default:
                                    __debugbreak();
                                    return;
                            }

                        // GetID
                        case 0x1A:
                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000, 1,
                                          cdrom_drive->response_status);

                            push_response(cdrom_drive, &cdrom_drive->int5,
                                          20000, 8,
                                          0x08, 0x40, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00);

                            cdrom_drive->int3.next_interrupt =
                            &cdrom_drive->int5;
                            
                            cdrom_drive->int5.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        default:
                            __debugbreak();
                            return;
                    }

                default:
                    __debugbreak();
                    return;
            }

        // 0x1F801802
        case 2:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801802.Index0 - Parameter Fifo (W)
                case 0:
                    psemu_fifo_enqueue(&cdrom_drive->parameter_fifo, data);
                    return;

                // 0x1F801802.Index1 - Interrupt Enable Register (W)
                case 1:
                    cdrom_drive->interrupt_enable.byte = data;
                    return;
            
                default:
                    __debugbreak();
                    return;
            }

        // 0x1F801803
        case 3:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    // Has an interrupt that we care about been acknowledged?
                    if ((cdrom_drive->current_interrupt) &&
                        ((data & 0x07) & cdrom_drive->current_interrupt->type))
                    {
                        // It has, send the next one, if any.
                        if (cdrom_drive->current_interrupt->next_interrupt)
                        {
                            cdrom_drive->current_interrupt =
                            cdrom_drive->current_interrupt->next_interrupt;
                        }
                        else
                        {
                            cdrom_drive->current_interrupt = NULL;
                        }
                    }

                    cdrom_drive->interrupt_flag.byte &= data;
                    return;

                default:
                    __debugbreak();
                    return;
            }
    }
}

// Loads data from CD-ROM drive `cdrom_drive`'s register `reg`.
uint8_t psemu_cdrom_drive_register_load
(const struct psemu_cdrom_drive* const cdrom_drive, const unsigned int reg)
{
    assert(cdrom_drive != NULL);

    switch (reg)
    {
        case 1:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801801.Index1 - Response Fifo (R)
                case 1:
                    return psemu_fifo_dequeue(&cdrom_drive->response_fifo);

                default:
                    __debugbreak();
            }

        case 3:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    return cdrom_drive->interrupt_flag.byte;

                default:
                    __debugbreak();
            }

        default:
            __debugbreak();
    }
}