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
#include <string.h>
#include "cpu_defs.h"
#include "rcnt.h"
#include "utility/memory.h"

// Creates the root counters.
struct libps_rcnt* libps_rcnt_create(void)
{
    struct libps_rcnt* rcnt = libps_safe_malloc(sizeof(struct libps_rcnt));
    return rcnt;
}

// Destroys the root counters.
void libps_rcnt_destroy(struct libps_rcnt* rcnt)
{
    libps_safe_free(rcnt);
}

// Resets the root counters to their initial state.
void libps_rcnt_reset(struct libps_rcnt* rcnt)
{
    assert(rcnt != NULL);
    memset(rcnt->rcnts, 0, sizeof(rcnt->rcnts));
}

// Adjusts the clock source of the timer specified by `rcnt_id` and resets
// the value for said timer.
void libps_rcnt_set_mode(struct libps_rcnt* rcnt,
                         const unsigned int rcnt_id,
                         const uint32_t mode)
{
    assert(rcnt != NULL);
    assert(rcnt_id < 3);

    rcnt->rcnts[rcnt_id].mode = mode;

    switch (rcnt_id)
    {
        case 2:
            switch (rcnt->rcnts[rcnt_id].mode & 0x300)
            {
                // 2 or 3 = System Clock/8
                case 0x200:
                    rcnt->rcnts[2].threshold = 8;
                    break;

                default:
                    rcnt->rcnts[2].threshold = 1;
                    break;
            }
            break;
    }

    rcnt->rcnts[rcnt_id].counter = 0;
    rcnt->rcnts[rcnt_id].value = 0x0000;
}

// Steps the root counters.
void libps_rcnt_step(struct libps_rcnt* rcnt)
{
    assert(rcnt != NULL);

    if (rcnt->rcnts[2].counter == rcnt->rcnts[2].threshold)
    {
        rcnt->rcnts[2].value++;
        rcnt->rcnts[2].counter = 0;
    }
    else
    {
        rcnt->rcnts[2].counter++;
    }
}