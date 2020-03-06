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

// Forward declarations
struct libps_fifo;

#include <stdbool.h>
#include <stdint.h>

// Received SECOND (or further) response to ReadS/ReadN (and Play+Report)
#define LIBPS_CDROM_INT1 1

// Received SECOND response (to various commands)
#define LIBPS_CDROM_INT2 2

// Received FIRST response (to any command)
#define LIBPS_CDROM_INT3 3

// DataEnd (when Play/Forward reaches end of disk) (maybe also for Read?)
#define LIBPS_CDROM_INT4 4

// Received error - code(in FIRST or SECOND response)
//
// INT5 also occurs on SECOND GetID response, on unlicensed disks.
// INT5 also occurs when opening the drive door(even if no command
// was sent, ie.even if no read - command or other command is active)
#define LIBPS_CDROM_INT5 5

// Interrupts
#define LIBPS_IRQ_CDROM (1 << 2)

// Bits for the response status (see `struct libps_cdrom::response_status`)
#define LIBPS_CDROM_RESPONSE_STATUS_READING (1 << 5)
#define LIBPS_CDROM_RESPONSE_STATUS_SEEKING (1 << 6)

#define LIBPS_CDROM_STATUS_DRQSTS (1 << 6)

#define LIBPS_CDROM_MODE_SPEED (1 << 7)

struct libps_cdrom_interrupt
{
    // Does this interrupt need to be fired?
    bool pending;

    // How many cycles to wait before firing this interrupt?
    unsigned int cycles;

    // Response parameters
    struct libps_fifo* response;

    // The type of interrupt
    unsigned int type;
};

struct libps_cdrom_seek_target
{
    uint8_t minute;
    uint8_t second;
    uint8_t sector;
};

// Pass this structure to `libps_system_set_cdrom()`.
struct libps_cdrom_info
{
    // Function call when it is time to seek
    void (*seek_cb)(void* user_data);

    // Function to call when it is time to read a sector
    uint8_t (*read_cb)(void* user_data);
};

struct libps_cdrom
{
    // 0x1F801800 - Index/Status Register (Bit 0-1 R/W) (Bit 2-7 Read Only)
    //
    // 0 - 1: Index - Port 1F801801h - 1F801803h index(0..3 = Index0..Index3)   (R / W)
    // 2:     ADPBUSY XA - ADPCM fifo empty(0 = Empty); set when playing XA - ADPCM sound
    // 3:     PRMEMPT Parameter fifo empty(1 = Empty); triggered before writing 1st byte
    // 4:     PRMWRDY Parameter fifo full(0 = Full); triggered after writing 16 bytes
    // 5:     RSLRRDY Response fifo empty(0 = Empty); triggered after reading LAST byte
    // 6:     DRQSTS  Data fifo empty(0 = Empty); triggered after reading LAST byte
    // 7:     BUSYSTS Command / parameter transmission busy(1 = Busy)
    uint8_t status;

    // 1F801802h.Index1 - Interrupt Enable Register (W)
    // 1F801803h.Index0 - Interrupt Enable Register (R)
    uint8_t interrupt_enable;

    // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
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
    uint8_t response_status;

    // Set by the Setmode command
    //
    // 7 - Speed (0 = Normal speed, 1 = Double speed)
    // 6   XA - ADPCM(0 = Off, 1 = Send XA - ADPCM sectors to SPU Audio Input)
    // 5   Sector Size(0 = 800h = DataOnly, 1 = 924h = WholeSectorExceptSyncBytes)
    // 4   Ignore Bit(0 = Normal, 1 = Ignore Sector Size and Setloc position)
    // 3   XA - Filter(0 = Off, 1 = Process only XA - ADPCM sectors that match Setfilter)
    // 2   Report(0 = Off, 1 = Enable Report - Interrupts for Audio Play)
    // 1   AutoPause(0 = Off, 1 = Auto Pause upon End of Track); for Audio Play
    // 0   CDDA(0 = Off, 1 = Allow to Read CD - DA Sectors; ignore missing EDC)
    uint8_t mode;

    struct libps_fifo* parameter_fifo;
    struct libps_fifo* response_fifo;
    struct libps_fifo* data_fifo;

    struct libps_cdrom_interrupt* first_interrupt;
    struct libps_cdrom_interrupt* second_interrupt;

    struct libps_cdrom_seek_target seek_target;

    bool fire_interrupt;

    // This should not be set directly; use `libps_system_set_cdrom()`.
    struct libps_cdrom_info cdrom_info;

    // The number of cycles to wait before reading another sector
    unsigned int sector_read_cycle_threshold;

    // Current sector read cycle count
    unsigned int sector_read_cycle_count;

    // Current sector count
    unsigned int sector_count;

    // The number of sectors we can read
    unsigned int sector_threshold;

    // Current sector data
    uint8_t sector_data;

    void* user_data;
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
