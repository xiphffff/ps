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

// Size of VRAM.
#define PSEMU_GPU_VRAM_WIDTH 1024
#define PSEMU_GPU_VRAM_HEIGHT 512

// GP0 port states
enum psemu_gp0_port_state
{
    // Ready to receive command
    PSEMU_GP0_AWAITING_COMMAND,

    // Receiving command parameters
    PSEMU_GP0_RECEIVING_PARAMETERS,

    // Receiving data for the current command
    PSEMU_GP0_RECEIVING_DATA,

    // Transferring data to GPUREAD
    PSEMU_GP0_TRANSFERRING_DATA
};

// Defines the structure of a vertex.
struct psemu_gpu_vertex
{
    // -1024..+1023
    int16_t x, y;

    union
    {
        struct
        {
            unsigned int x : 8;
            unsigned int y : 8;
        };
        uint16_t halfword;
    } texcoord;

    // 24-bit
    union
    {
        struct
        {
            // Red
            unsigned int r : 8;

            // Green
            unsigned int g : 8;

            // Blue
            unsigned int b : 8;

            unsigned int : 8;
        };
        uint32_t word;
    } color;
};

// Defines the structure of the PlayStation GPU version CXD8514Q.
struct psemu_gpu
{
    // 0x1F801810 - Receive responses to GP0(0xC0) and GP1(0x10) commands
    uint32_t gpuread;

    // 0x1F801814 - GPU Status Register
    union
    {
        struct
        {
            // (N * 64)
            unsigned int texpage_x_base : 4;

            // (N * 256) (i.e. 0 or 256)
            unsigned int texpage_y_base_is_256 : 1;

            // (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
            unsigned int semi_transparency_mode : 2;

            // (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
            unsigned int texpage_color_depth : 2;

            // (0=Off/strip LSBs, 1=Dither Enabled)
            unsigned int dithering_is_enabled : 1;

            // (0 = Prohibited, 1 = Allowed)
            unsigned int can_draw_to_display_area : 1;

            // (0=No, 1=Yes/Mask)
            unsigned int set_mask_bit_when_drawing : 1;

            // (0=Always, 1=Not to Masked areas)
            unsigned int ignore_masked_area_drawing : 1;

            // or always 1 when GP1(0x08).5=0
            unsigned int interlace_field : 1;

            // (0=Normal, 1=Distorted)
            unsigned int reverse_flag : 1;

            // (0=Normal, 1=Disable Textures)
            unsigned int textures_disabled : 1;

            // (0 = 256/320/512/640, 1 = 368)
            unsigned int hres_2 : 1;

            // (0=256, 1=320, 2=512, 3=640)
            unsigned int hres_1 : 2;

            // (0=240, 1=480, when Bit22=1)
            unsigned int vres : 1;

            // (0=NTSC/60Hz, 1=PAL/50Hz)
            unsigned int video_mode_is_pal : 1;

            // (0=15bit, 1=24bit)
            unsigned int display_area_color_depth_is_24bpp : 1;

            // (0=Off, 1=On)
            unsigned int vertical_interlace_is_enabled : 1;

            // (0=Enabled, 1=Disabled)
            unsigned int display_is_not_enabled : 1;

            // (0=Off, 1=IRQ)
            unsigned int irq_is_enabled : 1;

            unsigned int dma_direction_s : 1;

            // Ready to receive Cmd Word   (0=No, 1=Ready)
            unsigned int ready_to_receive_cmd : 1;

            // Ready to send VRAM to CPU(0 = No, 1 = Ready)
            unsigned int ready_to_send_vram_to_cpu : 1;

            // Ready to receive DMA Block  (0=No, 1=Ready)
            unsigned int ready_to_receive_dma_block : 1;

            // DMA Direction (0=Off, 1=?, 2=CPUtoGP0, 3=GPUREADtoCPU)
            unsigned int dma_direction : 2;

            // Drawing even/odd lines in interlace mode (0=Even or Vblank,
            // 1=Odd)
            unsigned int odd_lines : 1;
        };
        uint32_t word;
    } gpustat;

    // The Render commands GP0(0x20..0x7F) are automatically clipping any
    // pixels that are outside of this region.
    struct
    {
        // (0 - 1023)
        unsigned int x1, x2;

        // (0 - 511)
        unsigned int y1, y2;
    } drawing_area;

    struct
    {
        int16_t x, y;
    } drawing_offset;

    // The area within a texture window is repeated throughout the texture
    // page. The data is not actually stored all over the texture page but the
    // GPU reads the repeated patterns as if they were there.
    struct
    {
        struct
        {
            unsigned int x;
            unsigned int y;
        } mask,   // specifies the bits that are to be manipulated
          offset; // contains the new values for these bits
    } texture_window;

    // (512 * 1024), A1B5G5R5
    uint16_t* vram;

    // GP0 port state
    enum psemu_gp0_port_state gp0_state;
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