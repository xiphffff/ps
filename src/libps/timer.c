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
#include <string.h>
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
    assert(timer != NULL);
}

// Resets the timers to their initial state.
void libps_timer_reset(struct libps_timer* timer)
{
    assert(timer != NULL);
    memset(timer->timers, 0, sizeof(timer->timers));

    timer->fire_interrupt = false;
}

// Advances the timers.
void libps_timer_step(struct libps_timer* timer)
{
    for (unsigned int t = 0; t < 3; ++t)
    {
        if (timer->timers[t].mode & (1 << 3))
        {
            if (timer->timers[t].value == timer->timers[t].target)
            {
                timer->timers[t].value = 0x0000;

                if (timer->timers[t].mode & (1 << 4))
                {
                    timer->fire_interrupt = true;
                }
            }
        }
        else
        {
            if (timer->timers[t].value == 0xFFFF)
            {
                timer->timers[t].value = 0x0000;

                timer->timers[t].mode |= (1 << 12);

                if (timer->timers[t].mode & (1 << 5))
                {
                    timer->fire_interrupt = true;
                }
            }
        }

        // Check if synchronization is enabled.
        if (timer->timers[t].mode & 1)
        {
            // Synchronization type is dependent on the timer in question.
            switch (t)
            {
                case 0: // H-Blank
                case 1: // V-Blank
                    switch (timer->timers[t].mode & 0x06)
                    {
                        // Pause counter during HBlank(s) or VBlanks(s)
                        case 0:
                            timer->timers[t].value +=
                            !timer->vblank_or_hblank_occurred;

                            break;

                        // Reset counter to 0x0000 at HBlank(s) or VBlanks(s)
                        case 1:
                            if (timer->vblank_or_hblank_occurred)
                            {
                                timer->timers[t].value = 0x0000;
                            }
                            else
                            {
                                timer->timers[t].value++;
                            }
                            break;

                        // Reset counter to 0000h at VBlank or HBlank and pause
                        // outside of the blanking period
                        case 2:
                            if (timer->vblank_or_hblank_occurred)
                            {
                                timer->timers[t].value = 0x0000;
                            }
                            break;

                        // Pause until a blanking period occurs once, then
                        // switch to Free Run
                        case 3:
                            if (timer->vblank_or_hblank_occurred)
                            {
                                timer->timers[t].mode;
                            }
                            break;
                    }
                    break;

                case 2:
                    switch (timer->timers[t].mode & 0x06)
                    {
                        // Stop counter at current value (forever, no
                        // h/v-blank start)
                        case 0:
                        case 3:
                            break;

                        // Free Run (same as when Synchronization Disabled)
                        case 1:
                        case 2:
                            timer->timers[t].value++;
                            break;
                    }
                    break;
            }
        }
        else
        {
            // Free run
            timer->timers[t].value++;
        }
    }
}