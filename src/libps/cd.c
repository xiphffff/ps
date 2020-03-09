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
#include "utility/math.h"
#include "utility/memory.h"

#include <stdio.h>

// Resets an interrupt line `interrupt`.
static void reset_interrupt(struct libps_cdrom_interrupt* interrupt)
{
    assert(interrupt != NULL);

    interrupt->next_interrupt = NULL;

    libps_fifo_reset(&interrupt->response);

    interrupt->pending = false;
    interrupt->cycles  = 0;
}

// Pushes a response to interrupt line `interrupt`, delaying its firing by
// `delay_cycles` cycles.
static void push_response(struct libps_cdrom_interrupt* interrupt,
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
        libps_fifo_enqueue(&interrupt->response, arg);
    }
    va_end(args);

    interrupt->pending = true;
    interrupt->cycles  = delay_cycles;
}

// Initializes a CD-ROM drive `cdrom`.
void libps_cdrom_setup(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    libps_fifo_setup(&cdrom->parameter_fifo, 16);
    libps_fifo_setup(&cdrom->data_fifo,      4096);

    cdrom->int1.type = LIBPS_CDROM_INT1;
    cdrom->int2.type = LIBPS_CDROM_INT2;
    cdrom->int3.type = LIBPS_CDROM_INT3;
    cdrom->int5.type = LIBPS_CDROM_INT5;

    libps_fifo_setup(&cdrom->int1.response, 16);
    libps_fifo_setup(&cdrom->int2.response, 16);
    libps_fifo_setup(&cdrom->int3.response, 16);
    libps_fifo_setup(&cdrom->int5.response, 16);

    cdrom->user_data = NULL;
}

void libps_cdrom_cleanup(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    libps_fifo_cleanup(&cdrom->parameter_fifo);
    libps_fifo_cleanup(&cdrom->data_fifo);

    libps_fifo_cleanup(&cdrom->int1.response);
    libps_fifo_cleanup(&cdrom->int2.response);
    libps_fifo_cleanup(&cdrom->int3.response);
    libps_fifo_cleanup(&cdrom->int5.response);
}

// Resets the CD-ROM drive to its initial state.
void libps_cdrom_reset(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    libps_fifo_reset(&cdrom->parameter_fifo);
    libps_fifo_reset(&cdrom->data_fifo);

    reset_interrupt(&cdrom->int1);
    reset_interrupt(&cdrom->int2);
    reset_interrupt(&cdrom->int3);
    reset_interrupt(&cdrom->int5);

    cdrom->current_interrupt = NULL;

    cdrom->interrupt_flag = 0x00;

    cdrom->status.raw          = 0x18;
    cdrom->response_status     = 0x00;

    cdrom->sector_count                = 0;
    cdrom->sector_count_max            = 0;
    cdrom->sector_read_cycle_count     = 0;
    cdrom->sector_read_cycle_count_max = 0;

    cdrom->fire_interrupt = false;
}

// Checks to see if interrupts needs to be fired.
void libps_cdrom_step(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    // This takes priority over everything else.
    if (cdrom->response_status & (1 << 5))
    {
        if (cdrom->sector_read_cycle_count >=
            cdrom->sector_read_cycle_count_max)
        {
            // Read a new sector
            const unsigned int address =
          ((cdrom->position.sector + cdrom->sector_count++) +
           (cdrom->position.second * 75) +
           (cdrom->position.minute * 60 * 75) - 150) * LIBPS_CDROM_SECTOR_SIZE;

            cdrom->cdrom_info.read_cb(cdrom->user_data, address + 24);

            push_response(&cdrom->int1,
                          100000,
                          1,
                          cdrom->response_status);

            cdrom->int1.next_interrupt = &cdrom->int1;
            cdrom->current_interrupt   = &cdrom->int1;

            cdrom->sector_read_cycle_count = 0;
        }
        else
        {
            cdrom->sector_read_cycle_count++;
        }
    }

    // Is there an interrupt pending?
    if ((cdrom->current_interrupt != NULL) &&
         cdrom->current_interrupt->pending)
    {
        if (cdrom->current_interrupt->cycles != 0)
        {
            cdrom->current_interrupt->cycles--;
        }
        else
        {
            cdrom->response_fifo = &cdrom->current_interrupt->response;

            cdrom->current_interrupt->pending = false;
            cdrom->fire_interrupt = true;

            cdrom->interrupt_flag = (cdrom->interrupt_flag & ~0x07) |
                                    (cdrom->current_interrupt->type & 0x07);
        }
    }
}

// Loads indexed CD-ROM register `reg`.
uint8_t libps_cdrom_register_load(struct libps_cdrom* cdrom,
                                  const unsigned int reg)
{
    assert(cdrom != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            switch (cdrom->status.index)
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
            switch (cdrom->status.index)
            {
                // 1F801803h.Index0 - Interrupt Enable Register (R)
                case 0:
                    return cdrom->interrupt_enable;

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
void libps_cdrom_register_store(struct libps_cdrom* cdrom,
                                const unsigned int reg,
                                const uint8_t data)
{
    assert(cdrom != NULL);

    switch (reg)
    {
        // 0x1F801801
        case 1:
            switch (cdrom->status.index)
            {
                // 1F801801h.Index0 - Command Register (W)
                case 0:
                    switch (data)
                    {
                        // Getstat
                        case 0x01:
                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        // Setloc
                        case 0x02:
                            cdrom->position.minute =
                            libps_fifo_dequeue(&cdrom->parameter_fifo);
                            
                            cdrom->position.second =
                            libps_fifo_dequeue(&cdrom->parameter_fifo);

                            cdrom->position.sector =
                            libps_fifo_dequeue(&cdrom->parameter_fifo);

                            cdrom->position.minute =
                            LIBPS_BCD_TO_DEC(cdrom->position.minute);

                            cdrom->position.second =
                            LIBPS_BCD_TO_DEC(cdrom->position.second);

                            cdrom->position.sector =
                            LIBPS_BCD_TO_DEC(cdrom->position.sector);

                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        // ReadN
                        case 0x06:
                        {
                            const unsigned int threshold =
                            cdrom->mode ? 150 : 75;
                            
                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status |= (1 << 5);
                            cdrom->response_status |= (1 << 1);

                            cdrom->sector_count     = 0;
                            cdrom->sector_count_max = threshold - 1;
                            
                            cdrom->sector_read_cycle_count_max =
                            33868800 / cdrom->sector_count_max;

                            // Second response comes in `libps_cdrom_step()`.
                            cdrom->current_interrupt = &cdrom->int3;
                            break;
                        }

                        // Pause
                        case 0x09:
                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status &= ~(1 << 5);
                            cdrom->response_status &= ~(1 << 1);

                            push_response(&cdrom->int2,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = &cdrom->int2;
                            cdrom->int2.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        // Init
                        case 0x0A:
                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->mode = 0x02;

                            push_response(&cdrom->int2,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = &cdrom->int2;
                            cdrom->int2.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        // Setmode
                        case 0x0E:
                            cdrom->mode =
                            libps_fifo_dequeue(&cdrom->parameter_fifo);

                            cdrom->sector_size =
                            ((cdrom->mode & (1 << 5)) ? 0x924 : 0x800);

                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        // SeekL
                        case 0x15:
                            cdrom->response_status |= (1 << 6);
                            cdrom->response_status |= (1 << 1);
                            
                            push_response(&cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status &= ~(1 << 6);
                            cdrom->response_status &= ~(1 << 1);
                            
                            push_response(&cdrom->int2,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3.next_interrupt = &cdrom->int2;
                            cdrom->int2.next_interrupt = NULL;

                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        case 0x19:
                            switch (libps_fifo_dequeue(&cdrom->parameter_fifo))
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    push_response(&cdrom->int3,
                                                  20000,
                                                  4,
                                                  0x94, 0x09, 0x19, 0xC0);

                                    cdrom->int3.next_interrupt = NULL;
                                    cdrom->current_interrupt   = &cdrom->int3;

                                    break;

                                default:
                                    __debugbreak();
                                    break;
                            }
                            break;

                        // GetID
                        case 0x1A:
                            // Is there a disc?
                            if (cdrom->cdrom_info.read_cb)
                            {
                                // Yes.
                                push_response(&cdrom->int3,
                                              20000,
                                              1,
                                              cdrom->response_status);

                                push_response(&cdrom->int2,
                                              25000,
                                              8,
                                              0x02, 0x00, 0x20, 0x00,
                                              'S', 'C', 'E', 'A');

                                cdrom->int3.next_interrupt = &cdrom->int2;
                                cdrom->int2.next_interrupt = NULL;
                            }
                            else
                            {
                                // No.
                                push_response(&cdrom->int3,
                                              20000,
                                              1,
                                              cdrom->response_status);

                                push_response(&cdrom->int5,
                                              20000,
                                              8,
                                              0x08, 0x40, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00);

                                cdrom->int3.next_interrupt = &cdrom->int5;
                                cdrom->int5.next_interrupt = NULL;
                            }
                            cdrom->current_interrupt = &cdrom->int3;
                            break;

                        default:
                            __debugbreak();
                            break;
                    }
                    libps_fifo_reset(&cdrom->parameter_fifo);
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        // 0x1F801802
        case 2:
            switch (cdrom->status.index)
            {
                // 1F801802h.Index0 - Parameter Fifo (W)
                case 0:
                    libps_fifo_enqueue(&cdrom->parameter_fifo, data);
                    break;

                // 1F801802h.Index1 - Interrupt Enable Register (W)
                case 1:
                    cdrom->interrupt_enable = data;
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        // 0x1F801803
        case 3:
            switch (cdrom->status.index)
            {
                // 1F801803h.Index0 - Request Register (W)
                case 0:
                    if (data & (1 << 7))
                    {
                        libps_fifo_reset(&cdrom->data_fifo);

                        for (unsigned int index = 0;
                             index < cdrom->sector_size;
                             ++index)
                        {
                            libps_fifo_enqueue(&cdrom->data_fifo,
                                               cdrom->sector_data[index]);
                        }
                    }
                    else
                    {
                        libps_fifo_reset(&cdrom->data_fifo);
                    }
                    break;

                // 1F801803h.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    // Has an interrupt that we care about been acknowledged?
                    if ((cdrom->current_interrupt != NULL) &&
                       ((data & 0x07) & cdrom->current_interrupt->type))
                    {
                        // It has, send the next one, if any.
                        if (cdrom->current_interrupt->next_interrupt == NULL)
                        {
                            reset_interrupt(cdrom->current_interrupt);
                            cdrom->current_interrupt = NULL;
                        }
                        else
                        {
                            cdrom->current_interrupt =
                            cdrom->current_interrupt->next_interrupt;
                        }
                    }

                    cdrom->interrupt_flag &= data;
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