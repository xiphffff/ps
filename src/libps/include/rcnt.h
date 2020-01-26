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

#include <stdbool.h>
#include <stdint.h>

struct libps_rcnt
{
    struct rcnt_spec
    {
        // 1F801100h+N*10h - Timer 0..2 Current Counter Value (R/W)
        uint32_t value;

        // 1F801104h + N * 10h - Timer 0..2 Counter Mode(R / W)
        uint32_t mode;

        // 1F801108h + N * 10h - Timer 0..2 Counter Target Value(R / W)
        uint32_t target;

        // Current cycle count
        unsigned int counter;

        // The number of cycles we need to wait before incrementing the timer
        unsigned int threshold;
    } rcnts[3];
};

// Creates the root counters.
struct libps_rcnt* libps_rcnt_create(void);

// Destroys the root counters.
void libps_rcnt_destroy(struct libps_rcnt* rcnt);

// Resets the timers to their initial state.
void libps_rcnt_reset(struct libps_rcnt* rcnt);

// Adjusts the clock source of the timer specified by `timer_id` and resets
// the value for said timer.
void libps_rcnt_set_mode(struct libps_rcnt* rcnt,
                         const unsigned int rcnt_id,
                         const uint32_t mode);

// Steps the root counters.
void libps_rcnt_step(struct libps_rcnt* rcnt);