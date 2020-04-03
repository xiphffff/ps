// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright noticeand this permission notice appear in all copies.
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

#define PSEMU_GPU_VRAM_WIDTH 1024
#define PSEMU_GPU_VRAM_HEIGHT 512

// GP0 port states
#define PSEMU_GP0_AWAITING_COMMAND 0
#define PSEMU_GP0_RECEIVING_PARAMETERS 1
#define PSEMU_GP0_RECEIVING_DATA 2
#define PSEMU_GP0_TRANSFERRING_DATA 3

struct psemu_gpu_vertex
{
    // -1024..+1023
    int16_t x;

    // -1024..+1023
    int16_t y;

    uint16_t texcoord;
    uint32_t color;
};

// Defines the structure of the PlayStation GPU version CXD8514Q.
struct psemu_gpu
{
    // GP0 port state
    unsigned int state;

    // 0x1F801810 - Receive responses to GP0(0xC0) and GP1(0x10) commands
    uint32_t gpuread;

    uint32_t received_data;

    // 0x1F801814 - GPU Status Register
    union
    {
        struct
        {
            // (N * 64)
            unsigned int texpage_x_base : 4;

            // (N * 256) (i.e. 0 or 256)
            unsigned int texpage_y_base : 1;

            // (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
            unsigned int semi_transparency_mode : 2;

            // (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
            unsigned int texpage_color_depth : 2;

            // (0=Off/strip LSBs, 1=Dither Enabled)
            unsigned int dither_enabled : 1;

            // (0=Prohibited, 1=Allowed)
            unsigned int can_draw_to_display_area : 1;

            // Set Mask-bit when drawing pixels (0=No, 1=Yes/Mask)
            unsigned int set_mask_bit_when_drawing_pixels : 1;

            // ;
            unsigned int ignore_draw_to_masked_area : 1;

            // always 1 when GP1(08h).5=0)
            unsigned int interlace : 1;
        };
        uint32_t word;
    } gpustat;

    struct
    {
        unsigned int x1; // (0 - 1023)
        unsigned int y1; // (0 - 511)
        unsigned int x2; // (0 - 1023)
        unsigned int y2; // (0 - 511)
    } drawing_area;

    struct
    {
        signed int x;
        signed int y;
    } drawing_offset;

    struct
    {
        struct
        {
            unsigned int x;
            unsigned int y;
        } mask, offset;
    } texture_window;

    // (512 * 1024), A1B5G5R5
    uint16_t* vram;
#ifdef PSEMU_DEBUG
    void* debug_user_data;

    // Called when an unknown GPU command has been attempted
    void (*debug_unknown_cmd)(void* user_data,
                              const char* const port,
                              const uint32_t cmd);
#endif // PSEMU_DEBUG
};

// Initializes a GPU `gpu`.
void psemu_gpu_init(struct psemu_gpu* const gpu);

// Deallocates the memory held by `gpu`.
void psemu_gpu_fini(struct psemu_gpu* const gpu);

// Resets a GPU `gpu` to the startup state.
void psemu_gpu_reset(struct psemu_gpu* const gpu);

// Executes a GP0 command `cmd` on GPU `gpu`.
void psemu_gpu_gp0(struct psemu_gpu* const gpu, const uint32_t cmd);

// Executes a GP1 command `cmd` on GPU `gpu`.
void psemu_gpu_gp1(struct psemu_gpu* const gpu, const uint32_t cmd);
#ifdef __cplusplus
}
#endif // __cplusplus