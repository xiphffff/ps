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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

struct libps_cdrom_fifo
{
    uint8_t data[16];
    unsigned int pos;
};

struct libps_cdrom
{
    // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
    uint8_t status;

    // Parameter FIFO
    struct libps_cdrom_fifo parameter_fifo;
};

struct libps_cdrom* libps_cdrom_create(void);
void libps_cdrom_destroy(struct libps_cdrom* cdrom);

void libps_cdrom_reset(struct libps_cdrom* cdrom);

void libps_cdrom_indexed_register_store(struct libps_cdrom* cdrom, const unsigned int reg, const uint8_t data);

#ifdef __cplusplus
}
#endif // __cplusplus