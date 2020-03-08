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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "cd.h"
#include "utility/fifo.h"
#include "utility/math.h"
#include "utility/memory.h"

static void reset_interrupt(struct libps_cdrom_interrupt* interrupt)
{
    assert(interrupt != NULL);

    interrupt->next_interrupt = NULL;

    libps_fifo_reset(interrupt->response);

    interrupt->pending = false;
    interrupt->cycles  = 0;
}

// Queues an interrupt `interrupt`, delaying its firing by `delay_cycles`.
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
        libps_fifo_enqueue(interrupt->response, arg);
    }
    va_end(args);

    interrupt->pending = true;
    interrupt->cycles  = delay_cycles;
}

// Allocates memory for a `libps_cdrom` structure and returns a pointer to it
// if memory allocation was successful, or `NULL` otherwise.
struct libps_cdrom* libps_cdrom_create(void)
{
    struct libps_cdrom* cdrom =
    libps_safe_malloc(sizeof(struct libps_cdrom));

    cdrom->parameter_fifo = libps_fifo_create(16);
    cdrom->data_fifo      = libps_fifo_create(4096);

    cdrom->int1 = libps_safe_malloc(sizeof(struct libps_cdrom_interrupt));
    cdrom->int2 = libps_safe_malloc(sizeof(struct libps_cdrom_interrupt));
    cdrom->int3 = libps_safe_malloc(sizeof(struct libps_cdrom_interrupt));
    cdrom->int5 = libps_safe_malloc(sizeof(struct libps_cdrom_interrupt));

    cdrom->int1->type = LIBPS_CDROM_INT1;
    cdrom->int2->type = LIBPS_CDROM_INT2;
    cdrom->int3->type = LIBPS_CDROM_INT3;
    cdrom->int5->type = LIBPS_CDROM_INT5;

    cdrom->int1->response = libps_fifo_create(16);
    cdrom->int2->response = libps_fifo_create(16);
    cdrom->int3->response = libps_fifo_create(16);
    cdrom->int5->response = libps_fifo_create(16);

    cdrom->user_data = NULL;
    return cdrom;
}

// Deallocates the memory held by `cdrom`.
void libps_cdrom_destroy(struct libps_cdrom* cdrom)
{
    libps_fifo_destroy(cdrom->parameter_fifo);
    libps_fifo_destroy(cdrom->data_fifo);

    libps_fifo_destroy(cdrom->int1->response);
    libps_fifo_destroy(cdrom->int2->response);
    libps_fifo_destroy(cdrom->int3->response);
    libps_fifo_destroy(cdrom->int5->response);

    libps_safe_free(cdrom->int1);
    libps_safe_free(cdrom->int2);
    libps_safe_free(cdrom->int3);
    libps_safe_free(cdrom->int5);

    libps_safe_free(cdrom);
}

// Resets the CD-ROM drive to its initial state.
void libps_cdrom_reset(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    libps_fifo_reset(cdrom->parameter_fifo);
    libps_fifo_reset(cdrom->data_fifo);

    reset_interrupt(cdrom->int1);
    reset_interrupt(cdrom->int2);
    reset_interrupt(cdrom->int3);
    reset_interrupt(cdrom->int5);

    cdrom->current_interrupt = NULL;

    cdrom->interrupt_flag = 0x00;

    cdrom->status          = 0x18;
    cdrom->response_status = 0x00;

    cdrom->sector_count                = 1;
    cdrom->sector_read_cycle_count     = 0;
    cdrom->sector_read_cycle_threshold = 0;
    cdrom->sector_threshold            = 0;

    cdrom->fire_interrupt = false;
}

// Checks to see if interrupts needs to be fired.
void libps_cdrom_step(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    // This takes priority over everything else.
    if (cdrom->response_status & LIBPS_CDROM_RESPONSE_STATUS_READING)
    {
        if (cdrom->sector_read_cycle_count >=
            cdrom->sector_read_cycle_threshold)
        {
            if (cdrom->sector_count >= cdrom->sector_threshold)
            {
                cdrom->sector_count = 0;
            }

            // Read a new sector
            unsigned int address =
            ((cdrom->seek_target.sector + cdrom->sector_count) +
            (cdrom->seek_target.second * 75) +
            (cdrom->seek_target.minute * 60 * 75)) - 150;

            address *= LIBPS_SECTOR_SIZE;

            cdrom->cdrom_info.read_cb(cdrom->user_data, address + 24);

            push_response(cdrom->int1,
                          20000,
                          1,
                          cdrom->response_status);

            cdrom->int1->next_interrupt = cdrom->int1;
            cdrom->current_interrupt = cdrom->int1;

            cdrom->sector_count++;
            cdrom->sector_read_cycle_count = 0;
        }
        else
        {
            cdrom->sector_read_cycle_count++;
        }
    }

    // Is there an interrupt pending?
    if ((cdrom->current_interrupt != NULL) && cdrom->current_interrupt->pending)
    {
        if (cdrom->current_interrupt->cycles != 0)
        {
            cdrom->current_interrupt->cycles--;
        }
        else
        {
            cdrom->response_fifo = cdrom->current_interrupt->response;

            cdrom->current_interrupt->pending = false;
            cdrom->fire_interrupt = true;

            cdrom->interrupt_flag =
            (cdrom->interrupt_flag & ~0x07) |
            (cdrom->current_interrupt->type & 0x07);

            printf("INT%d done, IF=0x%02X\n", cdrom->current_interrupt->type, cdrom->interrupt_flag);
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
                    printf("Command is 0x%02X\n", data);
                    switch (data)
                    {
                        // Getstat
                        case 0x01:
                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = NULL;

                            cdrom->current_interrupt = cdrom->int3;
                            break;

                        // Setloc
                        case 0x02:
                            cdrom->seek_target.minute =
                            libps_fifo_dequeue(cdrom->parameter_fifo);
                            
                            cdrom->seek_target.second =
                            libps_fifo_dequeue(cdrom->parameter_fifo);

                            cdrom->seek_target.sector =
                            libps_fifo_dequeue(cdrom->parameter_fifo);

                            cdrom->seek_target.minute =
                            LIBPS_BCD_TO_DEC(cdrom->seek_target.minute);

                            cdrom->seek_target.second =
                            LIBPS_BCD_TO_DEC(cdrom->seek_target.second);

                            cdrom->seek_target.sector =
                            LIBPS_BCD_TO_DEC(cdrom->seek_target.sector);

                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = NULL;

                            cdrom->current_interrupt = cdrom->int3;
                            break;

                        // ReadN
                        case 0x06:
                        {
                            const unsigned int threshold =
                            (cdrom->mode & LIBPS_CDROM_MODE_SPEED) ? 150 : 75;
                            
                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status |=
                            LIBPS_CDROM_RESPONSE_STATUS_READING;

                            cdrom->sector_count = 0;
                            cdrom->sector_threshold = threshold - 1;
                            
                            cdrom->sector_read_cycle_threshold =
                            33868800 / cdrom->sector_threshold;

                            // Second response comes in `libps_cdrom_step()`.
                            break;
                        }

                        // Pause
                        case 0x09:
                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status &=
                            ~LIBPS_CDROM_RESPONSE_STATUS_READING;

                            push_response(cdrom->int2,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = cdrom->int2;
                            cdrom->int2->next_interrupt = NULL;

                            cdrom->current_interrupt = cdrom->int3;
                            break;

                        // Init
                        case 0x0A:
                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->mode = 0x00;

                            push_response(cdrom->int5,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = cdrom->int5;
                            cdrom->int5->next_interrupt = NULL;

                            break;

                        // Setmode
                        case 0x0E:
                            cdrom->mode =
                            libps_fifo_dequeue(cdrom->parameter_fifo);

                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = NULL;

                            cdrom->current_interrupt = cdrom->int3;
                            break;

                        // SeekL
                        case 0x15:
                            cdrom->response_status |=
                            LIBPS_CDROM_RESPONSE_STATUS_SEEKING;
                            
                            push_response(cdrom->int3,
                                          20000,
                                          1,
                                          cdrom->response_status);

                            cdrom->response_status &=
                            ~LIBPS_CDROM_RESPONSE_STATUS_SEEKING;
                            
                            push_response(cdrom->int2,
                                          25000,
                                          1,
                                          cdrom->response_status);

                            cdrom->int3->next_interrupt = cdrom->int2;
                            cdrom->int2->next_interrupt = NULL;

                            cdrom->current_interrupt = cdrom->int3;
                            break;

                        case 0x19:
                            switch (libps_fifo_dequeue(cdrom->parameter_fifo))
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    push_response(cdrom->int3,
                                                  20000,
                                                  4,
                                                  0x94, 0x09, 0x19, 0xC0);

                                    cdrom->current_interrupt = cdrom->int3;
                                    cdrom->int3->next_interrupt = NULL;

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
                                push_response(cdrom->int3,
                                              20000,
                                              1,
                                              cdrom->response_status);

                                push_response(cdrom->int2,
                                              25000,
                                              8,
                                              0x02, 0x00, 0x20, 0x00,
                                              'S', 'C', 'E', 'A');

                                cdrom->int3->next_interrupt = cdrom->int2;
                                cdrom->int2->next_interrupt = NULL;

                                cdrom->current_interrupt = cdrom->int3;
                            }
                            else
                            {
                                // No.
                                push_response(cdrom->int3,
                                              20000,
                                              1,
                                              cdrom->response_status);

                                push_response(cdrom->int5,
                                              20000,
                                              8,
                                              0x08, 0x40, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00);

                                cdrom->int3->next_interrupt = cdrom->int5;
                                cdrom->int5->next_interrupt = NULL;
                            }
                            break;

                        default:
                            __debugbreak();
                            break;
                    }
                    libps_fifo_reset(cdrom->parameter_fifo);
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
                    cdrom->interrupt_enable = data;
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
                // 1F801803h.Index0 - Request Register (W)
                case 0:
                    for (unsigned int i = 0; i < 2352; ++i)
                    {
                        libps_fifo_enqueue(cdrom->data_fifo, cdrom->sector_data[i]);
                    }
                    cdrom->status |= LIBPS_CDROM_STATUS_DRQSTS;
                    break;

                // 1F801803h.Index1 - Interrupt Flag Register (R/W)
                case 1:
                    // Has an interrupt that we care about been acknowledged?
                    if ((cdrom->current_interrupt != NULL) && (data & 0x07) & cdrom->current_interrupt->type)
                    {
                        // It has, send the next one, if any.
                        if (cdrom->current_interrupt->next_interrupt == NULL)
                        {
                            printf("INT%d acknowledged, no further interrupts\n", cdrom->current_interrupt->type);
                            reset_interrupt(cdrom->current_interrupt);
                        }
                        else
                        {
                            printf("INT%d acknowledged, next interrupt is INT%d\n", cdrom->current_interrupt->type, cdrom->current_interrupt->next_interrupt->type);
                            cdrom->current_interrupt = cdrom->current_interrupt->next_interrupt;
                        }
                    }

                    cdrom->interrupt_flag = data;
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