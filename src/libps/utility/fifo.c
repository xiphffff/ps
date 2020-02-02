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
#include <string.h>
#include "fifo.h"
#include "memory.h"

// Creates a fixed-sized FIFO.
struct libps_fifo* libps_fifo_create(const unsigned int size)
{
    struct libps_fifo* fifo = libps_safe_malloc(sizeof(struct libps_fifo));
    fifo->entries = libps_safe_malloc(sizeof(int) * size);

    fifo->max_size = size;

    libps_fifo_reset(fifo);
    return fifo;
}

// Destroys a FIFO.
void libps_fifo_destroy(struct libps_fifo* fifo)
{
    libps_safe_free(fifo);
}

// Clears a FIFO.
void libps_fifo_reset(struct libps_fifo* fifo)
{
    fifo->current_size = 0;
    fifo->head		   = 0;
    fifo->tail		   = fifo->max_size - 1;

    memset(fifo->entries, 0, sizeof(*fifo->entries));
}

// Returns `true` if the FIFO is empty, or `false` otherwise.
bool libps_fifo_is_empty(struct libps_fifo* fifo)
{
    assert(fifo != NULL);

    return fifo->current_size == 0;
}

// Returns `true` if the FIFO is full, or `false` otherwise.
bool libps_fifo_is_full(struct libps_fifo* fifo)
{
    assert(fifo != NULL);

    return fifo->current_size == fifo->max_size;
}

// Adds an item to the FIFO.
void libps_fifo_enqueue(struct libps_fifo* fifo, const int data)
{
    assert(fifo != NULL);

    if (libps_fifo_is_full(fifo))
    {
        return;
    }

    fifo->tail = (fifo->tail + 1) % fifo->max_size;
    fifo->current_size++;

    fifo->entries[fifo->tail] = data;
}

// Removes an item from the FIFO and returns this value.
int libps_fifo_dequeue(struct libps_fifo* fifo)
{
    assert(fifo != NULL);

    if (libps_fifo_is_empty(fifo))
    {
        return 0;
    }

    const int entry = fifo->entries[fifo->head];

    fifo->head = (fifo->head + 1) % fifo->max_size;
    fifo->current_size--;

    return entry;
}