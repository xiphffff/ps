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

#define LIBPS_SCHEDULER_READY 0
#define LIBPS_SCHEDULER_WAITING 1
#define LIBPS_SCHEDULER_INACTIVE 2

// Event is fired in sync with the GPU clock
#define LIBPS_SCHEDULER_SYNC_GPU 0

// Event is fired in sync with the CPU clock
#define LIBPS_SCHEDULER_SYNC_CPU 1

// Event is fired in sync with the dotclock
#define LIBPS_SCHEDULER_SYNC_DOTCLOCK 2

struct libps_scheduler
{
    struct libps_scheduler_tcb
    {
        unsigned int state;
        unsigned int sync_type;
    } tasks[32];
};

// Creates a scheduler. This scheduler
struct libps_scheduler* libps_scheduler_create(void);

// Destroys the scheduler.
void libps_scheduler_destroy(struct libps_scheduler* sched);

// Runs the scheduler.
void libps_scheduler_run(struct libps_scheduler* sched);