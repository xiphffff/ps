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
#include "cdrom_drive.h"
#include "debug.h"
#include "utility/memory.h"

// Primitive commands
#define Getstat 0x01
#define Setloc 0x02
#define ReadN 0x06
#define Pause 0x09
#define Init 0x0A
#define Setmode 0x0E
#define SeekL 0x15
#define SubFunction 0x19
#define GetID 0x1A

// Sub function commands
#define GetVersion 0x20

// Converts a binary coded decimal `bcd` to base 10 (decimal).
static inline unsigned int bcd_to_dec(const unsigned int bcd)
{
    return bcd - (6 * (bcd >> 4));
}

// Resets an interrupt line `interrupt`.
static void reset_interrupt(struct psemu_cdrom_drive_interrupt* interrupt)
{
    assert(interrupt != NULL);

    interrupt->next_interrupt = NULL;

    psemu_fifo_reset(&interrupt->response);

    interrupt->pending = false;
    interrupt->cycles = 0;
}

// Pushes a response to interrupt line `interrupt`, delaying its firing by
// `delay_cycles` cycles.
static void push_response(struct psemu_cdrom_drive_interrupt* interrupt,
                          const unsigned int delay_cycles,
                          const unsigned int num_args,
                          ...)
{
    va_list args;
    va_start(args, num_args);

    int arg;

    for (unsigned int i = 0; i < num_args; ++i)
    {
        arg = va_arg(args, int);
        psemu_fifo_enqueue(&interrupt->response, arg);
    }
    va_end(args);

    interrupt->pending = true;
    interrupt->cycles = delay_cycles;
}

// Initializes a CD-ROM drive `cdrom`.
void psemu_cdrom_drive_init(struct psemu_cdrom_drive* cdrom_drive)
{
    assert(cdrom_drive != NULL);

    psemu_fifo_init(&cdrom_drive->parameter_fifo, 16);

    cdrom_drive->int1.type = PSEMU_CDROM_DRIVE_INT1;
    cdrom_drive->int2.type = PSEMU_CDROM_DRIVE_INT2;
    cdrom_drive->int3.type = PSEMU_CDROM_DRIVE_INT3;
    cdrom_drive->int5.type = PSEMU_CDROM_DRIVE_INT5;

    psemu_fifo_init(&cdrom_drive->int1.response, 16);
    psemu_fifo_init(&cdrom_drive->int2.response, 16);
    psemu_fifo_init(&cdrom_drive->int3.response, 16);
    psemu_fifo_init(&cdrom_drive->int5.response, 16);

    cdrom_drive->read_cb    = NULL;
    cdrom_drive->user_param = NULL;
}

void psemu_cdrom_drive_fini(struct psemu_cdrom_drive* cdrom_drive)
{
    assert(cdrom_drive != NULL);

    psemu_fifo_fini(&cdrom_drive->parameter_fifo);

    psemu_fifo_fini(&cdrom_drive->int1.response);
    psemu_fifo_fini(&cdrom_drive->int2.response);
    psemu_fifo_fini(&cdrom_drive->int3.response);
    psemu_fifo_fini(&cdrom_drive->int5.response);
}

// Resets the CD-ROM drive to its initial state.
void psemu_cdrom_drive_reset(struct psemu_cdrom_drive* cdrom_drive)
{
    assert(cdrom_drive != NULL);

    psemu_fifo_reset(&cdrom_drive->parameter_fifo);

    reset_interrupt(&cdrom_drive->int1);
    reset_interrupt(&cdrom_drive->int2);
    reset_interrupt(&cdrom_drive->int3);
    reset_interrupt(&cdrom_drive->int5);

    cdrom_drive->current_interrupt = NULL;

    cdrom_drive->interrupt_flag = 0x00;

    cdrom_drive->status.byte          = 0x18;
    cdrom_drive->response_status.byte = 0x00;

    cdrom_drive->sector_count = 0;
    cdrom_drive->sector_count_max = 0;
    cdrom_drive->sector_read_cycle_count = 0;
    cdrom_drive->sector_read_cycle_count_max = 0;

    cdrom_drive->fire_interrupt = false;
}

// Checks to see if interrupts needs to be fired.
void psemu_cdrom_drive_step(struct psemu_cdrom_drive* cdrom_drive)
{
    assert(cdrom_drive != NULL);

    // This takes priority over everything else.
    if (cdrom_drive->response_status.reading)
    {
        if (cdrom_drive->sector_read_cycle_count >=
            cdrom_drive->sector_read_cycle_count_max)
        {
            // Read a new sector
            const unsigned int address =
            ((cdrom_drive->position.sector + cdrom_drive->sector_count++) +
             (cdrom_drive->position.second * 75) +
             (cdrom_drive->position.minute * 60 * 75) - 150) *
            PSEMU_CDROM_SECTOR_SIZE;

            if (cdrom_drive->read_cb)
            {
                cdrom_drive->read_cb(cdrom_drive->user_param,
                                     address + 24,
                                     cdrom_drive->sector_data);
            }

            push_response(&cdrom_drive->int1,
                          30000,
                          1,
                          cdrom_drive->response_status);

            cdrom_drive->int1.next_interrupt = &cdrom_drive->int1;
            cdrom_drive->current_interrupt   = &cdrom_drive->int1;

            cdrom_drive->sector_read_cycle_count = 0;
        }
        else
        {
            cdrom_drive->sector_read_cycle_count++;
        }
    }

    // Is there an interrupt pending?
    if ((cdrom_drive->current_interrupt) &&
         cdrom_drive->current_interrupt->pending)
    {
        if (cdrom_drive->current_interrupt->cycles != 0)
        {
            cdrom_drive->current_interrupt->cycles--;
        }
        else
        {
            cdrom_drive->response_fifo =
            &cdrom_drive->current_interrupt->response;

            cdrom_drive->current_interrupt->pending = false;
            cdrom_drive->fire_interrupt = true;

            cdrom_drive->interrupt_flag =
            (cdrom_drive->interrupt_flag & ~0x07) |
            (cdrom_drive->current_interrupt->type & 0x07);
        }
    }
}

// Loads indexed CD-ROM register `reg`.
uint8_t psemu_cdrom_drive_register_load(const struct psemu_cdrom_drive* const cdrom_drive,
                                        const unsigned int reg)
{
    assert(cdrom_drive != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            // 0x1F801801.Index1       - Response FIFO (R)
            // 0x1F801801.Index0, 2, 3 - Response FIFO (R) (Mirrors)
            return psemu_fifo_dequeue(cdrom_drive->response_fifo);

        // 0x1F801803
        case 3:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801803.Index0 - Interrupt Enable Register (R)
                case 0:
                    return cdrom_drive->interrupt_enable;
    
                // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    return cdrom_drive->interrupt_flag;

                default:
                    psemu_log("CD-ROM: Unknown indexed load: 0x1F801803.%d",
                              cdrom_drive->status.index);
                    return 0x00;
            }
            break;

        default:
            psemu_log("CD-ROM: Unknown register load: 0x1F80180%d",
                      cdrom_drive->status.index);
            return 0x00;
    }
}

// Stores `data` into indexed CD-ROM register `reg`.
void psemu_cdrom_drive_register_store(struct psemu_cdrom_drive* const cdrom_drive,
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
                        case Getstat:
                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt = &cdrom_drive->int3;
                            break;

                        case Setloc:
                            cdrom_drive->position.minute =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.second =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.sector =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            cdrom_drive->position.minute =
                            bcd_to_dec(cdrom_drive->position.minute);

                            cdrom_drive->position.second =
                            bcd_to_dec(cdrom_drive->position.second);

                            cdrom_drive->position.sector =
                            bcd_to_dec(cdrom_drive->position.sector);

                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            break;

                        case ReadN:
                        {
                            const unsigned int threshold =
                            cdrom_drive->mode.double_speed ? 150 : 75;

                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.standby = 1;
                            cdrom_drive->response_status.reading = 1;

                            cdrom_drive->sector_count = 0;
                            cdrom_drive->sector_read_cycle_count_max =
                            33868800 / threshold;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            break;
                        }

                        case Pause:
                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.standby = 0;
                            cdrom_drive->response_status.reading = 0;

                            push_response(&cdrom_drive->int2,
                                          25000,
                                          1,
                                          cdrom_drive->response_status);
 
                            cdrom_drive->int3.next_interrupt =
                            &cdrom_drive->int2;
                            
                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            break;

                        case Init:
                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->mode.byte = 0x02;

                            push_response(&cdrom_drive->int2,
                                          25000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = &cdrom_drive->int2;
                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt = &cdrom_drive->int3;
                            break;

                        case Setmode:
                            cdrom_drive->mode.byte =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);

                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = NULL;

                            cdrom_drive->current_interrupt =
                            &cdrom_drive->int3;
                            
                            break;

                        case SeekL:
                            cdrom_drive->response_status.seeking = 1;
                            cdrom_drive->response_status.standby = 1;

                            push_response(&cdrom_drive->int3,
                                          20000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->response_status.seeking = 0;
                            cdrom_drive->response_status.standby = 0;

                            push_response(&cdrom_drive->int2,
                                          25000,
                                          1,
                                          cdrom_drive->response_status.byte);

                            cdrom_drive->int3.next_interrupt = &cdrom_drive->int2;
                            cdrom_drive->int2.next_interrupt = NULL;

                            cdrom_drive->current_interrupt = &cdrom_drive->int3;
                            break;

                        case SubFunction:
                        {
                            const uint8_t fn =
                            psemu_fifo_dequeue(&cdrom_drive->parameter_fifo);
                            
                            switch (fn)
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case GetVersion:
                                    push_response(&cdrom_drive->int3,
                                                  20000,
                                                  4,
                                                  0x94, 0x09, 0x19, 0xC0);

                                    cdrom_drive->int3.next_interrupt = NULL;
                                    cdrom_drive->current_interrupt = &cdrom_drive->int3;

                                    break;

                                default:
                                    psemu_log("CD-ROM: Unknown sub-function "
                                              "0x%02X", fn);
                                    break;
                            }
                            break;
                        }

                        case GetID:
                            if (cdrom_drive->read_cb)
                            {
                                push_response(&cdrom_drive->int3,
                                              20000,
                                              1,
                                              cdrom_drive->response_status.byte);

                                push_response(&cdrom_drive->int2,
                                              25000,
                                              8,
                                              0x02, 0x00, 0x20, 0x00,
                                              'S', 'C', 'E', 'A');

                                cdrom_drive->int3.next_interrupt = &cdrom_drive->int2;
                                cdrom_drive->int2.next_interrupt = NULL;
                            }
                            else
                            {
                                push_response(&cdrom_drive->int3,
                                              20000,
                                              1,
                                              cdrom_drive->response_status.byte);

                                push_response(&cdrom_drive->int5,
                                              20000,
                                              8,
                                              0x08, 0x40, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00);

                                cdrom_drive->int3.next_interrupt = &cdrom_drive->int5;
                                cdrom_drive->int5.next_interrupt = NULL;
                            }
                            cdrom_drive->current_interrupt = &cdrom_drive->int3;
                            break;

                        default:
                            psemu_log("CD-ROM: Unknown command: 0x%02X\n",
                                      data);
                            break;
                    }
                    psemu_fifo_reset(&cdrom_drive->parameter_fifo);
                    break;

                default:
                    psemu_log("CD-ROM: Unknown indexed register write: "
                              "0x1F801801.Index%d", cdrom_drive->status.index);
                    break;
            }
            break;

        // 0x1F801802
        case 2:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801802.Index0 - Parameter FIFO (W)
                case 0:
                    psemu_fifo_enqueue(&cdrom_drive->parameter_fifo, data);
                    break;

                // 0x1F801802.Index1 - Interrupt Enable Register (W)
                case 1:
                    cdrom_drive->interrupt_enable = data;
                    break;

                default:
                    psemu_log("CD-ROM: Unknown indexed register write: "
                              "0x1F801802.Index%d", cdrom_drive->status.index);
                    break;
            }
            break;

        // 0x1F801803
        case 3:
            switch (cdrom_drive->status.index)
            {
                // 0x1F801803.Index0 - Request Register (W)
                case 0:
                    cdrom_drive->status.data_fifo_not_empty =
                    (data & 0x80) != 0;
                    
                    break;

                // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    // Has an interrupt that we care about been acknowledged?
                    if ((cdrom_drive->current_interrupt != NULL) &&
                       ((data & 0x07) & cdrom_drive->current_interrupt->type))
                    {
                        // It has, send the next one, if any.
                        if (cdrom_drive->current_interrupt->next_interrupt == NULL)
                        {
                            reset_interrupt(cdrom_drive->current_interrupt);
                            cdrom_drive->current_interrupt = NULL;
                        }
                        else
                        {
                            cdrom_drive->current_interrupt =
                            cdrom_drive->current_interrupt->next_interrupt;
                        }
                    }

                    cdrom_drive->interrupt_flag &= ~(data & 0x1F);
                    break;

                default:
                    psemu_log("CD-ROM: Unknown indexed register write: "
                              "0x1F801803.Index%d", cdrom_drive->status.index);
                    break;
            }
            break;

        default:
            psemu_log("CD-ROM: Unknown register write 0x1F80180%d",
                      cdrom_drive->status.index);
            break;
    }
}