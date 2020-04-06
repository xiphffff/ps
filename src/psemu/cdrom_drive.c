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
#include "utility/math.h"

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

    cdrom_drive->read_cb = NULL;
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

    if (cdrom_drive->response_status.reading)
    {
        static unsigned int sector_count            = 0;
        static unsigned int sector_read_cycle_count = 0;

        if (sector_read_cycle_count >=
            cdrom_drive->sector_read_cycle_count_max)
        {
            // Read a new sector
            const unsigned int address =
            ((cdrom_drive->position.sector + sector_count++) +
             (cdrom_drive->position.second * 75) +
             (cdrom_drive->position.minute * 60 * 75) - 150) *
              PSEMU_CDROM_SECTOR_SIZE;

            cdrom_drive->read_cb(address + 24, cdrom_drive->sector_data);

            push_response(cdrom_drive, &cdrom_drive->int1,
                          30000,
                          1,
                          cdrom_drive->response_status.byte);

            cdrom_drive->int1.next_interrupt = &cdrom_drive->int1;
            cdrom_drive->current_interrupt   = &cdrom_drive->int1;

            sector_read_cycle_count = 0;
        }
        else
        {
            sector_read_cycle_count++;
        }
    }

    // Is there an interrupt pending?
    if (cdrom_drive->current_interrupt &&
        cdrom_drive->current_interrupt->pending)
    {
        if (cdrom_drive->current_interrupt->cycles_remaining != 0)
        {
            cdrom_drive->current_interrupt->cycles_remaining--;
        }
        else
        {
            cdrom_drive->current_interrupt->pending = false;
            cdrom_drive->fire_interrupt = true;

            cdrom_drive->interrupt_flag.byte =
            (cdrom_drive->interrupt_flag.byte & ~0x07) |
            (cdrom_drive->current_interrupt->type & 0x07);
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
                            
                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;

                            return;

                        // Setloc
                        case 0x02:
                            cdrom_drive->position.minute =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.second =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.sector =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.minute =
                            psemu_bcd_to_dec(cdrom_drive->position.minute);

                            cdrom_drive->position.second =
                            psemu_bcd_to_dec(cdrom_drive->position.second);

                            cdrom_drive->position.sector =
                            psemu_bcd_to_dec(cdrom_drive->position.sector);

                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000, 1,
                                          cdrom_drive->response_status);

                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        // ReadN
                        case 0x06:
                        {
                            const unsigned int threshold =
                            cdrom_drive->mode.double_speed ? 150 : 75;
                            
                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.reading = 1;
                            cdrom_drive->response_status.standby = 1;

                            cdrom_drive->sector_read_cycle_count_max =
                            33868800 / threshold;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;
                        }

                        // Pause
                        case 0x09:
                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.reading = 0;
                            cdrom_drive->response_status.standby = 0;

                            push_response(cdrom_drive, &cdrom_drive->int2,
                                          25000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt =
                            &cdrom_drive->int2;

                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        // Init
                        case 0x0A:
                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->mode.byte = 0x02;

                            push_response(cdrom_drive, &cdrom_drive->int2,
                                          25000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt =
                            &cdrom_drive->int2;
                            
                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        // Setmode
                        case 0x0E:
                            cdrom_drive->mode.byte =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        // SeekL
                        case 0x15:
                            cdrom_drive->response_status.seeking = 1;
                            cdrom_drive->response_status.standby = 1;

                            push_response(cdrom_drive, &cdrom_drive->int3,
                                          20000, 1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.seeking = 0;
                            cdrom_drive->response_status.standby = 0;

                            push_response(cdrom_drive, &cdrom_drive->int2,
                                          25000, 1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt =
                            &cdrom_drive->int2;
                            
                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            return;

                        case 0x19:
                            switch (psemu_fifo_dequeue(&cdrom_drive->parameter_fifo))
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    push_response
                                    (cdrom_drive, &cdrom_drive->int3, 20000, 4,
                                    0x94, 0x09, 0x19, 0xC0);

                                    cdrom_drive->int3.next_interrupt = NULL;

                                    cdrom_drive->current_interrupt =
                                    &cdrom_drive->int3;

                                    return;

                                default:
                                    __debugbreak();
                                    return;
                            }

                        // GetID
                        case 0x1A:
                            // Is there a disc?
                            if (cdrom_drive->read_cb)
                            {
                                // Yes.
                                push_response(cdrom_drive, &cdrom_drive->int3,
                                              20000, 1,
                                              cdrom_drive->response_status.byte);

                                push_response(cdrom_drive, &cdrom_drive->int2,
                                              25000, 8,
                                              0x02, 0x00, 0x20, 0x00,
                                              'S', 'C', 'E', 'A');

                                cdrom_drive->int3.next_interrupt =
                                &cdrom_drive->int2;
                                
                                cdrom_drive->int2.next_interrupt = NULL;
                            }
                            else
                            {
                                push_response(cdrom_drive, &cdrom_drive->int3,
                                              20000, 1,
                                              cdrom_drive->response_status.byte);

                                push_response(cdrom_drive, &cdrom_drive->int5,
                                              20000, 8,
                                              0x08, 0x40, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00);

                                cdrom_drive->int3.next_interrupt =
                                &cdrom_drive->int5;
                            
                                cdrom_drive->int5.next_interrupt = NULL;
                            }

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
                // 0x1F801803.Index0 - Request Register (W)
                case 0:
                    if (data & (1 << 7))
                    {
                        psemu_fifo_reset(&cdrom_drive->data_fifo);

                        const unsigned int sector_size =
                        cdrom_drive->mode.sector_size_is_2340 ? 2340 : 2048;

                        for (unsigned int index = 0;
                             index < sector_size;
                             ++index)
                        {
                            psemu_fifo_enqueue(&cdrom_drive->data_fifo,
                            cdrom_drive->sector_data[index]);
                        }
                    }
                    else
                    {
                        psemu_fifo_reset(&cdrom_drive->data_fifo);
                    }
                    return;

                // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    // Has an interrupt that we care about been acknowledged?
                    if ((cdrom_drive->current_interrupt) &&
                        ((data & 0x07) & cdrom_drive->current_interrupt->type))
                    {
                        // It has, send the next one, if any.
                        if (cdrom_drive->current_interrupt->next_interrupt == NULL)
                        {
                            memset(&cdrom_drive->current_interrupt, 0, sizeof(cdrom_drive->current_interrupt));
                            cdrom_drive->current_interrupt = NULL;
                        }
                        else
                        {
                            cdrom_drive->current_interrupt =
                            cdrom_drive->current_interrupt->next_interrupt;
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
                // 0x1F801803.Index0 - Interrupt Enable Register (R)
                case 0:
                    return cdrom_drive->interrupt_enable.byte;

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