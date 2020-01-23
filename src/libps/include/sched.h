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

#pragma once

#define LIBPS_SCHEDULER_MAX_EVENTS 10

enum libps_scheduler_tcb_state
{
    LIBPS_SCHEDULER_STATE_READY,
    LIBPS_SCHEDULER_STATE_INACTIVE,
    LIBPS_SCHEDULER_STATE_WAITING
};

enum libps_scheduler_tcb_sync_mode
{
    // Event is fired in sync with the CPU clock
    LIBPS_SCHEDULER_SYNC_CPU,

    // Event is fired in sync with the GPU clock
    LIBPS_SCHEDULER_SYNC_GPU,

    // Event is fired in sync with the dotclock
    LIBPS_SCHEDULER_SYNC_DOTCLOCK
};

struct libps_scheduler
{
    struct libps_scheduler_tcb
    {
        enum libps_scheduler_tcb_state state;
        enum libps_scheduler_tcb_sync_mode sync_mode;
        signed int cycles;
        void (*dispatch_func)(void);
    } tasks[LIBPS_SCHEDULER_MAX_EVENTS];

    // Set the master clock *only* through
    // `libps_scheduler_set_master_clock()`.
    unsigned int master_clock;

    // Do not set these directly. These are automatically adjusted upon a call
    // to `libps_scheduler_set_master_clock()`.
    unsigned int cpu_clock;
    unsigned int gpu_clock;
    unsigned int dot_clock;
    unsigned int frame_rate;

    unsigned int cycles_per_instruction;
};

// Creates the scheduler.
struct libps_scheduler* libps_scheduler_create(void);

// Destroys the scheduler.
void libps_scheduler_destroy(struct libps_scheduler* sched);

// Adjusts the master clock of the scheduler to `clock_rate`, and adjusts the
// other clocks and target frame rate as well.
void libps_scheduler_set_master_clock(struct libps_scheduler* sched,
                                      const unsigned int clock_rate);

// Runs the scheduler until a frame needs to be rendered.
void libps_scheduler_step(struct libps_scheduler* sched);

void libps_scheduler_add_task(struct libps_scheduler* sched,
                              const enum libps_scheduler_tcb_sync_mode sync_type,
                              void (*cb)(void));