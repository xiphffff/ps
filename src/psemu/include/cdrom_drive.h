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

#include <stdbool.h>
#include <stdint.h>
#include "../utility/fifo.h"

typedef void (*psemu_cdrom_read_cb)
(void* user_param,
 const unsigned int address,
 uint8_t* const sector_data);

// Received SECOND (or further) response to ReadS/ReadN (and Play+Report)
#define PSEMU_CDROM_DRIVE_INT1 1

// Received SECOND response (to various commands)
#define PSEMU_CDROM_DRIVE_INT2 2

// Received FIRST response (to any command)
#define PSEMU_CDROM_DRIVE_INT3 3

// DataEnd (when Play/Forward reaches end of disk) (maybe also for Read?)
#define PSEMU_CDROM_DRIVE_INT4 4

// Received error - code(in FIRST or SECOND response)
//
// INT5 also occurs on SECOND GetID response, on unlicensed disks.
// INT5 also occurs when opening the drive door(even if no command
// was sent, ie.even if no read - command or other command is active)
#define PSEMU_CDROM_DRIVE_INT5 5

// Defines the absolute size of a sector in bytes.
#define PSEMU_CDROM_SECTOR_SIZE 2352

// Defines the structure of an interrupt.
struct psemu_cdrom_drive_interrupt
{
    // Does this interrupt need to be fired?
    bool pending;

    // How many cycles to wait before firing this interrupt?
    unsigned int cycles;

    // The type of interrupt
    unsigned int type;

    // Response parameters
    struct psemu_fifo response;

    // The next interrupt to fire, if any
    struct psemu_cdrom_drive_interrupt* next_interrupt;
};

struct psemu_cdrom_drive
{
    // 0x1F801800 - Index/Status Register (Bit0-1 R/W) (Bit2-7 Read Only)
    union
    {
        struct
        {
            // Port 0x1F801801 - 0x1F801803 index (0..3 = Index0..Index3)
            unsigned int index : 2;

            // XA-ADPCM fifo empty  (0=Empty) ;set when playing XA-ADPCM sound
            unsigned int xa_adpcm_fifo_not_empty : 1;

            // Parameter fifo empty (1=Empty) ;triggered before writing 1st byte
            unsigned int parameter_fifo_empty : 1;

            // Parameter fifo full (0 = Full); triggered after writing 16 bytes
            unsigned int parameter_fifo_not_full : 1;

            // Response fifo empty  (0=Empty) ;triggered after reading LAST byte
            unsigned int response_fifo_not_empty : 1;

            // Data fifo empty (0=Empty) ;triggered after reading LAST byte
            unsigned int data_fifo_not_empty : 1;

            // Command/parameter transmission busy (1=Busy)
            unsigned int busy : 1;
        };
        uint8_t byte;
    } status;

    // 0x1F801802.Index1 - Interrupt Enable Register (W)
    // 0x1F801803.Index0 - Interrupt Enable Register (R)
    uint8_t interrupt_enable;

    // 0x1F801803.Index1 - Interrupt Flag Register (R/W)
    uint8_t interrupt_flag;

    // The 8bit status code is returned by Getstat command
    // (and many other commands).
    union
    {
        struct
        {
            // Invalid Command/parameters (followed by Error Byte)
            unsigned int error : 1;

            // (0=Motor off, or in spin-up phase, 1=Motor on)
            unsigned int standby : 1;

            // (0 = Okay, 1 = Seek error)
            unsigned int seek_error : 1;

            // (0=Okay, 1=GetID denied) (also set when Setmode.Bit4=1)
            unsigned int id_error : 1;

            // (0 = Closed, 1 = Is/was Open)
            unsigned int shell_open : 1;

            // Reading data sectors
            unsigned int reading : 1;

            // Seeking to position
            unsigned int seeking : 1;

            // Playing CD-DA
            unsigned int playing : 1;
        };
        uint8_t byte;
    } response_status;

    // Defines the current mode.
    union
    {
        struct
        {
            // (0=Off, 1=Allow to Read CD-DA Sectors; ignore missing EDC)
            unsigned int can_read_cdda_sectors : 1;

            // (0=Off, 1=Auto Pause upon End of Track) ;for Audio Play
            unsigned int auto_pause : 1;

            // (0=Off, 1=Enable Report-Interrupts for Audio Play)
            unsigned int report : 1;

            // (0=Off, 1=Process only XA-ADPCM sectors that match Setfilter)
            unsigned int xa_adpcm_filter : 1;

            // (0=Normal, 1=Ignore Sector Size and Setloc position) (disputed)
            unsigned int ignore_bit : 1;

            // (0=800h=DataOnly, 1=924h=WholeSectorExceptSyncBytes)
            unsigned int sector_size_is_2340 : 1;

            // (0=Off, 1=Send XA-ADPCM sectors to SPU Audio Input)
            unsigned int send_xa_adpcm_sectors : 1;

            // (0=Normal speed, 1=Double speed)
            unsigned int double_speed : 1;
        };
        uint8_t byte;
    } mode;

    struct psemu_fifo parameter_fifo;
    struct psemu_fifo* response_fifo;

    // Interrupt lines
    struct psemu_cdrom_drive_interrupt int1;
    struct psemu_cdrom_drive_interrupt int2;
    struct psemu_cdrom_drive_interrupt int3;
    struct psemu_cdrom_drive_interrupt int5;

    // The current interrupt we're processing.
    struct psemu_cdrom_drive_interrupt* current_interrupt;

    // The current CD-ROM position.
    struct
    {
        uint8_t minute;
        uint8_t second;
        uint8_t sector;
    } position;

    bool fire_interrupt;

    // Current sector read cycle count
    unsigned int sector_read_cycle_count;

    // The number of cycles to wait before reading another sector
    unsigned int sector_read_cycle_count_max;

    // Current sector we're reading
    unsigned int sector_count;

    // The number of sectors we can read. This will only ever be 74 or 149.
    unsigned int sector_count_max;

    // The sector of a sector as defined by the `Setmode` command. This can
    // only ever be 0x800 (2048) or 0x924 (2340).
    unsigned int sector_size;

    // Current sector data
    uint8_t sector_data[PSEMU_CDROM_SECTOR_SIZE];

    // The function to call when it is time to read a sector off of a CD-ROM.
    // Setting this to a non `NULL` value signifies that a CD-ROM has been
    // "inserted". If it is `NULL`, there is no CD-ROM.
    psemu_cdrom_read_cb read_cb;

    // See notes in `psemu_set_user_param_cb()`.
    void* user_param;
};

// Initializes a CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_init(struct psemu_cdrom_drive* const cdrom_drive);

// Destroys all memory held by CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_fini(struct psemu_cdrom_drive* const cdrom_drive);

// Resets the CD-ROM drive `cdrom_drive` to the startup state.
void psemu_cdrom_drive_reset(struct psemu_cdrom_drive* const cdrom_drive);

// Checks to see if interrupts needs to be fired.
void psemu_cdrom_drive_step(struct psemu_cdrom_drive* const cdrom_drive);

// Stores `data` into register `reg` in CD-ROM drive `cdrom_drive`.
void psemu_cdrom_drive_register_store(struct psemu_cdrom_drive* const cdrom_drive,
                                      const unsigned int reg,
                                      const uint8_t data);

// Loads data from CD-ROM drive `cdrom_drive`'s register `reg`.
uint8_t psemu_cdrom_drive_register_load(const struct psemu_cdrom_drive* const cdrom_drive,
                                        const unsigned int reg);

#ifdef __cplusplus
}
#endif // __cplusplus