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
#include "gpu.h"

// Allocates memory for a `libps_gpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
struct libps_gpu* libps_gpu_create(void)
{
    struct libps_gpu* gpu = malloc(sizeof(struct libps_gpu));
    return gpu;
}

// Deallocates the memory held by `gpu`.
void libps_gpu_destroy(struct libps_gpu* gpu)
{
    assert(gpu != NULL);
    free(gpu);
}

// Resets the GPU to the initial state.
void libps_gpu_reset(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    gpu->gpustat = 0x14802000;
    gpu->gpuread = 0x00000000;
}

// Processes a GP0 packet.
void libps_gpu_process_gp0(struct libps_gpu* gpu, const uint32_t packet)
{
    assert(gpu != NULL);
    __debugbreak();
}

// Processes a GP1 packet.
void libps_gpu_process_gp1(struct libps_gpu* gpu, const uint32_t packet)
{
    assert(gpu != NULL);
    __debugbreak();
}