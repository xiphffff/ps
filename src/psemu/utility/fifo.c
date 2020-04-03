// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright noticeand this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <assert.h>
#include <string.h>
#include "fifo.h"
#include "memory.h"

// Initializes a fixed-size FIFO `fifo` with a maximum size of `size`.
void psemu_fifo_init(struct psemu_fifo* const fifo, const unsigned int size)
{
    assert(fifo != NULL);

    fifo->max_size = size;
    fifo->entries = psemu_safe_malloc(sizeof(int) * size);
}

// Deallocates the memory held by `fifo`.
void psemu_fifo_fini(struct psemu_fifo* const fifo)
{
    assert(fifo != NULL);

    psemu_safe_free(fifo->entries);
}

// Clears the contents of a FIFO `fifo`.
void psemu_fifo_reset(struct psemu_fifo* const fifo)
{
    assert(fifo != NULL);

    fifo->current_size = 0;

    fifo->head = 0;
    fifo->tail = fifo->max_size - 1;

    memset(fifo->entries, 0, sizeof(int) * fifo->max_size);
}

// Returns `true` if the FIFO is empty, or `false` otherwise.
bool psemu_fifo_is_empty(struct psemu_fifo* fifo)
{
    assert(fifo != NULL);

    return fifo->current_size == 0;
}

// Returns `true` if the FIFO is full, or `false` otherwise.
bool psemu_fifo_is_full(struct psemu_fifo* fifo)
{
    assert(fifo != NULL);

    return fifo->current_size == fifo->max_size;
}

// Enqueues `entry`.
void psemu_fifo_enqueue(struct psemu_fifo* const fifo, const int entry)
{
    assert(fifo != NULL);

    if (psemu_fifo_is_full(fifo))
    {
        return;
    }

    fifo->tail = (fifo->tail + 1) % fifo->max_size;
    fifo->current_size++;

    fifo->entries[fifo->tail] = entry;
}

// Dequeues and returns the value.
int psemu_fifo_dequeue(struct psemu_fifo* const fifo)
{
    assert(fifo != NULL);

    if (psemu_fifo_is_empty(fifo))
    {
        return 0;
    }

    const int entry = fifo->entries[fifo->head];

    fifo->head = (fifo->head + 1) % fifo->max_size;
    fifo->current_size--;

    return entry;
}