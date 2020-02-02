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

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdbool.h>

struct libps_fifo
{
    int* entries;

    unsigned int current_size;
    unsigned int max_size;

    unsigned int head;
    unsigned int tail;
};

// Creates a fixed-size FIFO.
struct libps_fifo* libps_fifo_create(const unsigned int size);

// Destroys a FIFO.
void libps_fifo_destroy(struct libps_fifo* fifo);

// Clears a FIFO.
void libps_fifo_reset(struct libps_fifo* fifo);

// Returns `true` if the FIFO is empty, or `false` otherwise.
bool libps_fifo_is_empty(struct libps_fifo* fifo);

// Returns `true` if the FIFO is full, or `false` otherwise.
bool libps_fifo_is_full(struct libps_fifo* fifo);

// Adds an item to the FIFO.
void libps_fifo_enqueue(struct libps_fifo* fifo, const int data);

// Removes an item from the FIFO and returns this value.
int libps_fifo_dequeue(struct libps_fifo* fifo);

#ifdef __cplusplus
}
#endif // __cplusplus
