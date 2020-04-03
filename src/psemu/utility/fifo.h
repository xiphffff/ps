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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdbool.h>

// Defines the structure of a fixed-size FIFO.
struct psemu_fifo
{
    int* entries;

    unsigned int current_size;
    unsigned int max_size;

    unsigned int head;
    unsigned int tail;
};

// Initializes a fixed-size FIFO `fifo` with a maximum size of `size`.
void psemu_fifo_init(struct psemu_fifo* const fifo, const unsigned int size);

// Deallocates the memory held by `fifo`.
void psemu_fifo_fini(struct psemu_fifo* const fifo);

// Clears the contents of a FIFO `fifo`.
void psemu_fifo_reset(struct psemu_fifo* const fifo);

// Returns `true` if the FIFO is empty, or `false` otherwise.
bool psemu_fifo_is_empty(struct psemu_fifo* const fifo);

// Returns `true` if the FIFO is full, or `false` otherwise.
bool psemu_fifo_is_full(struct psemu_fifo* const fifo);

// Enqueues `entry`.
void psemu_fifo_enqueue(struct psemu_fifo* const fifo, const int entry);

// Dequeues and returns the value.
int psemu_fifo_dequeue(struct psemu_fifo* const fifo);

#ifdef __cplusplus
}
#endif // __cplusplus