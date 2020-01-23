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
#include "cpu_defs.h"
#include "gpu.h"
#include "sched.h"
#include "utility.h"

// By default, the scheduler is configured based on the following information:
//
// PSX/NTSC
// PU-7 and PU-8 boards are using three separate oscillators:
//
// X101: 67.737MHz
// X201: 53.69MHz (GPU Clock)
// X302: 4.000MHz (for CDROM SUB CPU)
//
// Master clock: 67.767MHz
// CPU clock:   (67.767MHz / 2) = 33.8685MHz
// Audio clock: (67.767MHz / 1536) = 44.1kHz
// GPU clock:
//
// Frame Rates
// PAL:  53.222400MHz / 314 / 3406 = ca. 49.76 Hz (~50Hz)
// NTSC: 53.222400MHz / 263 / 3413 = ca. 59.29 Hz (~60Hz)
//
// The speed of the system can be changed using
// `libps_scheduler_set_master_clock(struct libps_scheduler*, const unsigned int clock_rate)`.

// Creates the scheduler.
struct libps_scheduler* libps_scheduler_create(void)
{
    struct libps_scheduler* sched =
    libps_safe_malloc(sizeof(struct libps_scheduler));

    sched->cycles_per_instruction = 2;

    libps_scheduler_set_master_clock(sched, 67737600);
    return sched;
}

// Destroys the scheduler.
void libps_scheduler_destroy(struct libps_scheduler* sched)
{
    libps_safe_free(sched);
}

// Adjusts the master clock of the scheduler to `clock_rate`, and adjusts the
// other clocks as well.
void libps_scheduler_set_master_clock(struct libps_scheduler* sched,
                                      const unsigned int clock_rate)
{
    sched->master_clock = clock_rate;
    sched->cpu_clock    = sched->master_clock * 0x300;
    sched->gpu_clock    = sched->cpu_clock * (11 / 7);

    sched->frame_rate = sched->gpu_clock / 263 / 3413;
}

// Runs the scheduler until V-Blank.
void libps_scheduler_step(struct libps_scheduler* sched)
{
    for (unsigned int ev = 0; ev < LIBPS_SCHEDULER_MAX_EVENTS; ++ev)
    {
        if (sched->tasks[ev].cycles <= 0)
        {
            sched->tasks[ev].dispatch_func();

            switch (sched->tasks[ev].sync_mode)
            {
                case LIBPS_SCHEDULER_SYNC_CPU:
                    sched->tasks[ev].cycles = sched->cpu_clock;
                    break;

                case LIBPS_SCHEDULER_SYNC_GPU:
                    sched->tasks[ev].cycles = sched->gpu_clock;
                    break;

                case LIBPS_SCHEDULER_SYNC_DOTCLOCK:
                    break;
            }
        }
        else
        {
            sched->tasks[ev].cycles -= sched->cycles_per_instruction;
        }
    }
}

void libps_scheduler_add_task(struct libps_scheduler* sched,
                              const enum libps_scheduler_tcb_sync_mode sync_type,
                              void (*cb)(void))
{
    assert(sched != NULL);
}