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
#include "cpu_defs.h"
#include "timer.h"

// Allocates memory for a `libps_timer` structure and returns a pointer to it
// if memory allocation was successful, `NULL` otherwise. This function does
// not automatically initialize initial state.
struct libps_timer* libps_timer_create(void)
{
    struct libps_timer* timer = malloc(sizeof(struct libps_timer));
    return timer;
}

// Deallocates the memory held by `timer`.
void libps_timer_destroy(struct libps_timer* timer)
{
    // `free()` doesn't care whether or not you pass it a `NULL` pointer, but
    // `timer` should never be `NULL` when we get here.
    assert(timer != NULL);
    free(timer);
}

// Resets the timers to their initial state.
void libps_timer_reset(struct libps_timer* timer)
{
    assert(timer != NULL);

    memset(timer->timers, 0, sizeof(timer->timers));

    timer->fire_interrupt            = false;
    timer->vblank_or_hblank_occurred = false;
}

// Adjusts the clock source of the timer specified by `timer_id` and resets
// the value for said timer.
void libps_timer_set_mode(struct libps_timer* timer,
                          const unsigned int timer_id,
                          const uint32_t mode)
{
    assert(timer != NULL);
    assert(timer_id < 3);

    timer->timers[timer_id].mode = mode;

    switch (timer_id)
    {
        case 2:
            switch (timer->timers[timer_id].mode & 0x300)
            {
                // 2 or 3 = System Clock/8
                case 0x200:
                    timer->timers[2].threshold = 8;
                    break;

                default:
                    timer->timers[2].threshold = 1;
                    break;
            }
            break;
    }

    timer->timers[timer_id].counter = 0;
    timer->timers[timer_id].value = 0x0000;
}

// Advances the timers.
void libps_timer_step(struct libps_timer* timer)
{
    if (timer->timers[2].counter == timer->timers[2].threshold)
    {
        timer->timers[2].value++;
        timer->timers[2].counter = 0;
    }
    else
    {
        timer->timers[2].counter++;
    }
}