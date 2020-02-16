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

struct libps_fifo;

#include <stdbool.h>
#include <stdint.h>

#define LIBPS_IRQ_CDROM (1 << 2)

enum libps_cdrom_interrupt_type
{
    // No response received (no interrupt request)
    INT0,

    // Received SECOND (or further) response to ReadS/ReadN (and Play+Report)
    INT1,

    // Received SECOND response (to various commands)
    INT2,

    // Received FIRST response (to any command)
    INT3,

    // DataEnd (when Play/Forward reaches end of disk) (maybe also for Read?)
    INT4,

    // Received error - code(in FIRST or SECOND response)
    //
    // INT5 also occurs on SECOND GetID response, on unlicensed disks.
    // INT5 also occurs when opening the drive door(even if no command
    // was sent, ie.even if no read - command or other command is active)
    INT5,

    INT6,
    INT7
};

struct libps_cdrom_interrupt
{
    bool pending;
    unsigned int cycles;
    enum libps_cdrom_interrupt_type type;
};

struct libps_cdrom
{
    // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
    uint8_t status;

    // 1F801803h.Index1 - Interrupt Flag Register (R/W)
    uint8_t interrupt_flag;

    // The 8-bit status code is returned by Getstat command (and many other
    // commands), the meaning of the separate stat bits is:
    //
    // 7 - Play:          Playing CD-DA
    // 6 - Seek:          Seeking
    // 5 - Read:          Reading data sectors
    // 4 - ShellOpen:     Once shell open (0=Closed, 1=Is/was open)
    // 3 - IdError:       (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
    // 2 - SeekError:     (0=Okay, 1=Seek error)   (followed by Error Byte)
    // 1 - Spindle Motor: (0=Motor off, or in spin-up phase, 1=Motor on)
    // 0 - Error          Invalid Command/parameters (followed by Error Byte)
    uint8_t stat_code;

    struct libps_fifo* parameter_fifo;
    struct libps_fifo* response_fifo;

    bool fire_interrupt;
    struct libps_cdrom_interrupt interrupts[2];

    void* user_data;

    // Set this to `NULL` if there's no disc inserted, otherwise set to a
    // function.
    uint8_t (*read_cb)(void* user_data,
                       const uint8_t minute,
                       const uint8_t second,
                       const uint8_t sector);
};

// Allocates memory for a `libps_cdrom` structure and returns a pointer to it
// if memory allocation was successful, or `NULL` otherwise.
struct libps_cdrom* libps_cdrom_create(void);

// Deallocates the memory held by `cdrom`.
void libps_cdrom_destroy(struct libps_cdrom* cdrom);

// Resets the CD-ROM drive to its initial state.
void libps_cdrom_reset(struct libps_cdrom* cdrom);

// Checks to see if interrupts needs to be fired.
void libps_cdrom_step(struct libps_cdrom* cdrom);

// Loads indexed CD-ROM register `reg`.
uint8_t libps_cdrom_indexed_register_load(struct libps_cdrom* cdrom,
                                          const unsigned int reg);

// Stores `data` into indexed CD-ROM register `reg`.
void libps_cdrom_indexed_register_store(struct libps_cdrom* cdrom,
                                        const unsigned int reg,
                                        const uint8_t data);

#ifdef __cplusplus
}
#endif // __cplusplus
