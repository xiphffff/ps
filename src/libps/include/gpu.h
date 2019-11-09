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

#define LIBPS_GPU_VRAM_WIDTH 1024
#define LIBPS_GPU_VRAM_HEIGHT 512

enum libps_gpu_state
{
    LIBPS_GPU_AWAITING_COMMAND,
    LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS,
    LIBPS_GPU_RECEIVING_COMMAND_DATA
};

struct libps_gpu_vertex
{
    // (-1024..+1023)
    int16_t x;

    // (-1024..+1023)
    int16_t y;

    uint32_t color;
    uint32_t texcoord;
    uint32_t texpage;
    uint32_t palette;
};

struct libps_gpu
{
    // 0x1F801810 - Read responses to GP0(C0h) and GP1(10h) commands
    uint32_t gpuread;

    // 0x1F801814 - GPU Status Register (R)
    struct
    {
        union
        {
            // 0-3 - Texture page X Base (N * 64)
            unsigned int texture_page_x_base : 4;

            // 4 - Texture page Y Base(N * 256) (ie. 0 or 256)
            unsigned int texture_page_y_base : 1;

            // 5 - 6 - Semi Transparency(0 = B / 2 + F / 2, 1 = B + F, 2 = B - F, 3 = B + F / 4)
            unsigned int semi_transparency : 2;

            // 7-8   Texture page colors   (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
            unsigned int texture_page_colors : 2;

            // 9 - Dither 24bit to 15bit(0 = Off / strip LSBs, 1 = Dither Enabled)
            unsigned int dither_enabled : 1;

            // 10    Drawing to display area (0=Prohibited, 1=Allowed)
            unsigned int drawing_to_display_area : 1;

            // 11    Set Mask-bit when drawing pixels (0=No, 1=Yes/Mask)
            unsigned int set_mask_bit : 1;

            // 12    Draw Pixels           (0=Always, 1=Not to Masked areas)
            unsigned int draw_pixels : 1;

            // 13    Interlace Field
            unsigned int interlace_field : 1;

            // 14    "Reverseflag"         (0=Normal, 1=Distorted)
            unsigned int reverse_flag : 1;

            // 15    Texture Disable(0 = Normal, 1 = Disable Textures)
            unsigned int disable_textures : 1;

            // 16    Horizontal Resolution 2     (0=256/320/512/640, 1=368)
            unsigned int horizonal_resolution_2 : 1;

            // 17-18 Horizontal Resolution 1     (0=256, 1=320, 2=512, 3=640)
            unsigned int horitzonal_resolution_1 : 2;

            // 19    Vertical Resolution         (0=240, 1=480, when Bit22=1)
            unsigned int vertical_resolution : 1;

            // 20    Video Mode                  (0=NTSC/60Hz, 1=PAL/50Hz)
            unsigned int video_mode : 1;

            //  21    Display Area Color Depth    (0=15bit, 1=24bit)
            unsigned int display_area_color_depth : 1;

            // 22    Vertical Interlace(0 = Off, 1 = On)
            unsigned int vertical_interlace : 1;

            // 23    Display Enable              (0=Enabled, 1=Disabled)
            unsigned int display_enabled : 1;

            // 24    Interrupt Request (IRQ1)    (0=Off, 1=IRQ)
            unsigned int irq : 1;

            // 25    DMA / Data Request, meaning depends on GP1(04h) DMA Direction:
            // When GP1(04h) = 0 --->Always zero(0)
            // When GP1(04h) = 1 --->FIFO State(0 = Full, 1 = Not Full)
            // When GP1(04h) = 2 --->Same as GPUSTAT.28
            // When GP1(04h) = 3 --->Same as GPUSTAT.27
            unsigned int dma_request : 1;

            // 26    Ready to receive Cmd Word   (0=No, 1=Ready)
            unsigned int ready_to_receive_cmd_word : 1;

            // 27    Ready to send VRAM to CPU   (0=No, 1=Ready)
            unsigned int ready_to_send_vram_to_cpu : 1;

            // 28    Ready to receive DMA Block  (0=No, 1=Ready)
            unsigned int ready_to_receive_dma_block : 1;

            // 29-30 DMA Direction (0=Off, 1=?, 2=CPUtoGP0, 3=GPUREADtoCPU)
            unsigned int dma_direction : 2;

            // 31    Drawing even/odd lines in interlace mode (0=Even or Vblank, 1=Odd)
            unsigned int draw_mode : 1;
        };
        uint32_t raw;
    } gpustat;

    // The 1MByte VRAM is organized as 512 lines of 2048 bytes.
    uint16_t* vram;

    // State of the GP0 port.
    enum libps_gpu_state state;

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

// Allocates memory for a `libps_gpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
struct libps_gpu* libps_gpu_create(void);

// Deallocates the memory held by `gpu`.
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