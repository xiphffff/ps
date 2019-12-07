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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cd.h"

struct libps_cdrom* libps_cdrom_create(void)
{
    struct libps_cdrom* cdrom = malloc(sizeof(struct libps_cdrom));
    return cdrom != NULL ? cdrom : NULL;
}

void libps_cdrom_destroy(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);
    free(cdrom);
}

void libps_cdrom_reset(struct libps_cdrom* cdrom)
{
    assert(cdrom != NULL);

    memset(&cdrom->parameter_fifo, 0, sizeof(cdrom->parameter_fifo));
    cdrom->status = 0x18;
}

void libps_cdrom_indexed_register_store(struct libps_cdrom* cdrom, const unsigned int reg, const uint8_t data)
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
                        case 0x19:
                            switch (cdrom->parameter_fifo.data[0])
                            {
                                // Get cdrom BIOS date/version (yy,mm,dd,ver)
                                case 0x20:
                                    //queue_interrupt(cdrom, INT3, 0x94, 0x09, 0x19, 0xC0, SOME_UNDETERMINED_VALUE_HELP);
                                    break;

                                default:
                                    __debugbreak();
                                    break;
                            }
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
                    cdrom->parameter_fifo.data[cdrom->parameter_fifo.pos++] = data;
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