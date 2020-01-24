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

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

// Drawing flags
#define DRAW_FLAG_MONOCHROME (1 << 0)
#define DRAW_FLAG_SHADED (1 << 1)
#define DRAW_FLAG_TEXTURED (1 << 2)
#define DRAW_FLAG_QUAD (1 << 3)
#define DRAW_FLAG_OPAQUE (1 << 4)
#define DRAW_FLAG_TEXTURE_BLENDING (1 << 5)
#define DRAW_FLAG_RAW_TEXTURE (1 << 6)
#define DRAW_FLAG_VARIABLE_SIZE (1 << 7)

#define LIBPS_GPU_VRAM_WIDTH 1024
#define LIBPS_GPU_VRAM_HEIGHT 512

// Interrupts
#define LIBPS_IRQ_VBLANK (1 << 0)

enum libps_gpu_state
{
    LIBPS_GPU_AWAITING_COMMAND,
    LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS,
    LIBPS_GPU_RECEIVING_COMMAND_DATA,
    LIBPS_GPU_TRANSFERRING_DATA,
};

struct libps_gpu_vertex
{
    // (-1024..+1023)
    int16_t x;

    // (-1024..+1023)
    int16_t y;

    uint16_t palette;
    uint16_t texcoord;
    uint16_t texpage;

    uint32_t color;
};

struct libps_gpu
{
    // 0x1F801810 - Read responses to GP0(C0h) and GP1(10h) commands
    uint32_t gpuread;

    // 0x1F801814 - GPU Status Register (R)
    uint32_t gpustat;

    // The 1MByte VRAM is organized as 512 lines of 2048 bytes.
    uint16_t* vram;

    // State of the GP0 port.
    enum libps_gpu_state state;

    void (*draw_polygon)(struct libps_gpu* gpu,
                         const struct libps_gpu_vertex* const v0,
                         struct libps_gpu_vertex* const v1,
                         struct libps_gpu_vertex* const v2);

    void (*draw_rect)(struct libps_gpu* gpu,
                      const struct libps_gpu_vertex* const vertex);

    struct
    {
        uint32_t params[32];
        unsigned int remaining_words;
        unsigned int flags;
		uint32_t raw;
    } cmd_packet;

    struct
    {
        // (0..1023)
        uint16_t x1;
        uint16_t x2;

        // (0..511)
        uint16_t y1;
        uint16_t y2;
    } drawing_area;

    // (-1024..+1023)
    int16_t drawing_offset_x;

    // (-1024..+1023)
    int16_t drawing_offset_y;

    uint32_t received_data;
};

// Creates the PlayStation GPU.
struct libps_gpu* libps_gpu_create(void);

// Destroys the PlayStation GPU.
void libps_gpu_destroy(struct libps_gpu* gpu);

// Resets the GPU to the initial state.
void libps_gpu_reset(struct libps_gpu* gpu);

// Processes a GP0 packet.
void libps_gpu_process_gp0(struct libps_gpu* gpu, const uint32_t packet);

// Processes a GP1 packet.
void libps_gpu_process_gp1(struct libps_gpu* gpu, const uint32_t packet);

#ifdef __cplusplus
}
#endif // __cplusplus
