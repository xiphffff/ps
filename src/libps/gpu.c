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
#include <stdio.h>
#include "cpu_defs.h"
#include "gpu.h"
#include "utility/memory.h"
#include "renderer/sw.h"

static void (*cmd_func)(struct libps_gpu*);
static unsigned int params_pos;

// Handles the GP0(A0h) command - Copy Rectangle (CPU to VRAM)
static void copy_rect_from_cpu(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    // Current X position
    static unsigned int vram_x_pos;

    // Current Y position
    static unsigned int vram_y_pos;

    // Maximum length of a line (should be Xxxx+Xsiz)
    static unsigned int vram_x_pos_max;

    if (gpu->state == LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS)
    {
        const uint16_t width =
        (((gpu->cmd_packet.params[1] & 0x0000FFFF) - 1) & 0x000003FF) + 1;

        const uint16_t height =
        (((gpu->cmd_packet.params[1] >> 16) - 1) & 0x000001FF) + 1;

        vram_x_pos =
        ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);

        vram_y_pos =
        ((gpu->cmd_packet.params[0] >> 16) & 0x000001FF);

        vram_x_pos_max = vram_x_pos + width;

        gpu->cmd_packet.remaining_words = (width * height) / 2;

        // Lock the GP0 state to this function.
        gpu->state = LIBPS_GPU_RECEIVING_COMMAND_DATA;

        // Again, we don't want to do anything until we receive at least one
        // data word.
        return;
    }

    if (gpu->state == LIBPS_GPU_RECEIVING_COMMAND_DATA)
    {
        if (gpu->cmd_packet.remaining_words != 0)
        {
            gpu->vram[vram_x_pos++ + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos)] =
            gpu->received_data & 0x0000FFFF;

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);
            }

            gpu->vram[vram_x_pos++ + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos)] =
            gpu->received_data >> 16;

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);
            }
            gpu->cmd_packet.remaining_words--;
        }
        else
        {
            // All of the expected data has been sent. Return to normal
            // operation.
            memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
            params_pos = 0;

            gpu->state = LIBPS_GPU_AWAITING_COMMAND;
        }
    }
}

// Handles the GP0(C0h) command - Copy Rectangle (VRAM to CPU)
static void copy_rect_to_cpu(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    // Current X position
    static unsigned int vram_x_pos;

    // Current Y position
    static unsigned int vram_y_pos;

    // Maximum length of a line (should be Xxxx+Xsiz)
    static unsigned int vram_x_pos_max;

    if (gpu->state == LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS)
    {
        const uint16_t width =
        (((gpu->cmd_packet.params[1] & 0x0000FFFF) - 1) & 0x000003FF) + 1;

        const uint16_t height =
        (((gpu->cmd_packet.params[1] >> 16) - 1) & 0x000001FF) + 1;

        vram_x_pos =
        ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);

        vram_y_pos =
        ((gpu->cmd_packet.params[0] >> 16) & 0x000001FF);

        vram_x_pos_max = vram_x_pos + width;

        gpu->cmd_packet.remaining_words = (width * height) / 2;

        // Lock the GP0 state to this function.
        gpu->state = LIBPS_GPU_TRANSFERRING_DATA;

        return;
    }

    if (gpu->state == LIBPS_GPU_TRANSFERRING_DATA)
    {
        if (gpu->cmd_packet.remaining_words != 0)
        {
            const uint16_t pixel0 =
            gpu->vram[vram_x_pos++ + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos)];

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);
            }

            const uint16_t pixel1 =
            gpu->vram[vram_x_pos++ + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos)];

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF);
            }

            gpu->gpuread = ((pixel1 << 16) | pixel0);
            gpu->cmd_packet.remaining_words--;
        }
        else
        {
            // All of the expected data has been sent. Return to normal
            // operation.
            memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
            params_pos = 0;

            gpu->state = LIBPS_GPU_AWAITING_COMMAND;
        }
    }
}

static void fill_rect_in_vram(struct libps_gpu* gpu)
{
    const unsigned int color = gpu->cmd_packet.params[0];

    const unsigned int x_pos = gpu->cmd_packet.params[1] & 0x0000FFFF;
    const unsigned int y_pos = gpu->cmd_packet.params[1] >> 16;

    const unsigned int width  = gpu->cmd_packet.params[2] & 0x0000FFFF;
    const unsigned int height = gpu->cmd_packet.params[2] >> 16;

    for (unsigned int x = x_pos; x != (x_pos + width); ++x)
    {
        for (unsigned int y = y_pos; y != (y_pos + height); ++y)
        {
            const unsigned int pixel_r = (color & 0x000000FF) / 8;
            const unsigned int pixel_g = ((color >> 8) & 0xFF) / 8;
            const unsigned int pixel_b = ((color >> 16) & 0xFF) / 8;

            gpu->vram[(x & 0x3FF) + (LIBPS_GPU_VRAM_WIDTH * (y & 0x1FF))] =
            (pixel_g << 5) | (pixel_b << 10) | pixel_r;
        }
    }

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
    params_pos = 0;

    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
    return;
}

// This function is called whenever we must render a polygon. It assembles
// vertices primarily based on the following flags:
//
// * DRAW_FLAG_MONOCHROME
// * DRAW_FLAG_TEXTURED
// * DRAW_FLAG_SHADED
//
// It will call the renderer's `draw_polygon()` function once all of the
// vertices have been assembled, and resets the parameter FIFO.
static void draw_polygon_helper(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    if (gpu->cmd_packet.flags & DRAW_FLAG_MONOCHROME)
    {
        const struct libps_gpu_vertex v0 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[1] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[1] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v1 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[2] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[2] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v2 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[3] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[3] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        gpu->draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x     = (int16_t)(gpu->cmd_packet.params[4] & 0x0000FFFF),
                .y     = (int16_t)(gpu->cmd_packet.params[4] >> 16),
                .color = gpu->cmd_packet.params[0]
            };
            gpu->draw_polygon(gpu, &v1, &v2, &v3);
        }

        memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
        params_pos = 0;

        gpu->state = LIBPS_GPU_AWAITING_COMMAND;
        return;
    }

    if (gpu->cmd_packet.flags & DRAW_FLAG_TEXTURED)
    {
        const struct libps_gpu_vertex v0 =
        {
            .x        = (int16_t)(gpu->cmd_packet.params[1] & 0x0000FFFF),
            .y        = (int16_t)(gpu->cmd_packet.params[1] >> 16),
            .palette  = gpu->cmd_packet.params[2] >> 16,
            .texcoord = gpu->cmd_packet.params[2] & 0x0000FFFF,
            .color    = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v1 =
        {
            .x        = (int16_t)(gpu->cmd_packet.params[3] & 0x0000FFFF),
            .y        = (int16_t)(gpu->cmd_packet.params[3] >> 16),
            .palette  = gpu->cmd_packet.params[2] >> 16, // Hack
            .texpage  = gpu->cmd_packet.params[4] >> 16,
            .texcoord = gpu->cmd_packet.params[4] & 0x0000FFFF,
            .color    = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v2 =
        {
            .x        = (int16_t)(gpu->cmd_packet.params[5] & 0x0000FFFF),
            .y        = (int16_t)(gpu->cmd_packet.params[5] >> 16),
            .texpage  = gpu->cmd_packet.params[4] >> 16, // Hack
            .texcoord = gpu->cmd_packet.params[6] & 0x0000FFFF,
            .color    = gpu->cmd_packet.params[0]
        };

        gpu->draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x        = (int16_t)(gpu->cmd_packet.params[7] & 0x0000FFFF),
                .y        = (int16_t)(gpu->cmd_packet.params[7] >> 16),
                .texcoord = gpu->cmd_packet.params[8] & 0x0000FFFF,
                .color    = gpu->cmd_packet.params[0]
            };
            gpu->draw_polygon(gpu, &v1, &v2, &v3);
        }

        memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
        params_pos = 0;

        gpu->state = LIBPS_GPU_AWAITING_COMMAND;
        return;
    }

    if (gpu->cmd_packet.flags & DRAW_FLAG_SHADED)
    {
        const struct libps_gpu_vertex v0 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[1] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[1] >> 16),
            .color = gpu->cmd_packet.params[0] & 0x00FFFFFF
        };

        const struct libps_gpu_vertex v1 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[3] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[3] >> 16),
            .color = gpu->cmd_packet.params[2] & 0x00FFFFFF
        };

        const struct libps_gpu_vertex v2 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[5] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[5] >> 16),
            .color = gpu->cmd_packet.params[4] & 0x00FFFFFF
        };

        gpu->draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x     = (int16_t)(gpu->cmd_packet.params[7] & 0x0000FFFF),
                .y     = (int16_t)(gpu->cmd_packet.params[7] >> 16),
                .color = gpu->cmd_packet.params[6] & 0x00FFFFFF
            };
            gpu->draw_polygon(gpu, &v1, &v2, &v3);
        }
    }

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
    params_pos = 0;

    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
}

static void draw_rect_helper(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    const struct libps_gpu_vertex vertex =
    {
        .color    = gpu->cmd_packet.params[0],
        .y        = gpu->cmd_packet.params[1] >> 16,
        .x        = gpu->cmd_packet.params[1] & 0x0000FFFF,
        .texcoord = gpu->cmd_packet.params[2] >> 16,
        .palette  = gpu->cmd_packet.params[2] & 0x0000FFFF
    };

    gpu->draw_rect(gpu, &vertex);

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
    params_pos = 0;

    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
}

// Creates a PlayStation GPU.
struct libps_gpu* libps_gpu_create(void)
{
    struct libps_gpu* gpu = libps_safe_malloc(sizeof(struct libps_gpu));

    gpu->draw_polygon = &libps_renderer_sw_draw_polygon;
    gpu->draw_rect    = &libps_renderer_sw_draw_rect;

    gpu->vram = libps_safe_malloc(LIBPS_GPU_VRAM_WIDTH * LIBPS_GPU_VRAM_HEIGHT * sizeof(uint16_t));
    return gpu;
}

// Destroys the PlayStation GPU.
void libps_gpu_destroy(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    libps_safe_free(gpu->vram);
    libps_safe_free(gpu);
}

// Resets the GPU to the initial state.
void libps_gpu_reset(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    gpu->gpustat = 0x14802000;
    gpu->gpuread = 0x00000000;

    gpu->drawing_offset_x = 0x00000000;
    gpu->drawing_offset_y = 0x00000000;

    gpu->received_data = 0x00000000;

    memset(&gpu->cmd_packet,   0, sizeof(gpu->cmd_packet));
    memset(&gpu->drawing_area, 0, sizeof(gpu->drawing_area));
    memset(gpu->vram,          0, (LIBPS_GPU_VRAM_WIDTH * LIBPS_GPU_VRAM_HEIGHT) * sizeof(uint16_t));

    params_pos = 0;
    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
}

// Processes a GP0 packet.
void libps_gpu_process_gp0(struct libps_gpu* gpu, const uint32_t packet)
{
    assert(gpu != NULL);

    switch (gpu->state)
    {
        case LIBPS_GPU_AWAITING_COMMAND:
            switch (packet >> 24)
            {
                // GP0(00h) - NOP(?)
                case 0x00:
                    break;

                // GP0(01h) - Clear Cache
                case 0x01:
                    break;

                // GP0(02h) - Fill Rectangle in VRAM
                case 0x02:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 2;

                    gpu->cmd_packet.raw = packet;
                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &fill_rect_in_vram;
                    break;

                // GP0(28h) - Monochrome four-point polygon, opaque
                //
                // XXX: monochrome means "uses constant color"
                case 0x28:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 4;

                    gpu->cmd_packet.flags |= DRAW_FLAG_MONOCHROME;
                    gpu->cmd_packet.flags |= DRAW_FLAG_QUAD;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_polygon_helper;
                    break;

                // GP0(2Dh) - Textured four-point polygon, opaque, raw-texture
                case 0x2D:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 8;

                    gpu->cmd_packet.flags |= DRAW_FLAG_TEXTURED;
                    gpu->cmd_packet.flags |= DRAW_FLAG_QUAD;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;
                    gpu->cmd_packet.flags |= DRAW_FLAG_RAW_TEXTURE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_polygon_helper;
                    break;

                // GP0(2Ch) - Textured four-point polygon, opaque, texture-blending
                case 0x2C:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 8;

                    gpu->cmd_packet.flags |= DRAW_FLAG_TEXTURED;
                    gpu->cmd_packet.flags |= DRAW_FLAG_QUAD;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;
                    gpu->cmd_packet.flags |= DRAW_FLAG_TEXTURE_BLENDING;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_polygon_helper;
                    break;

                // GP0(30h) - Shaded three-point polygon, opaque
                case 0x30:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 5;

                    gpu->cmd_packet.flags |= DRAW_FLAG_SHADED;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_polygon_helper;
                    break;

                // GP0(38h) - Shaded four-point polygon, opaque
                case 0x38:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 7;

                    gpu->cmd_packet.flags |= DRAW_FLAG_SHADED;
                    gpu->cmd_packet.flags |= DRAW_FLAG_QUAD;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_polygon_helper;
                    break;

                // GP0(65h) - Textured Rectangle, variable size, opaque,
                // raw-texture
                case 0x65:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 3;

                    gpu->cmd_packet.flags |= DRAW_FLAG_TEXTURED;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;
                    gpu->cmd_packet.flags |= DRAW_FLAG_RAW_TEXTURE;
                    gpu->cmd_packet.flags |= DRAW_FLAG_VARIABLE_SIZE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_rect_helper;
                    break;

                // GP0(68h) - Monochrome Rectangle (1x1) (Dot) (opaque)
                case 0x68:
                    gpu->cmd_packet.params[params_pos++] =
                    packet & 0x00FFFFFF;

                    gpu->cmd_packet.remaining_words = 1;

                    gpu->cmd_packet.flags |= DRAW_FLAG_MONOCHROME;
                    gpu->cmd_packet.flags |= DRAW_FLAG_OPAQUE;

                    gpu->cmd_packet.raw = packet;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &draw_rect_helper;
                    break;

                // GP0(A0h) - Copy Rectangle (CPU to VRAM)
                case 0xA0:
                    gpu->cmd_packet.remaining_words = 2;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    cmd_func = &copy_rect_from_cpu;
                    break;

                // GP0(C0h) - Copy Rectangle (VRAM to CPU)
                case 0xC0:
                    gpu->cmd_packet.remaining_words = 2;

                    gpu->state = LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS;

                    gpu->cmd_packet.raw = packet;

                    cmd_func = &copy_rect_to_cpu;
                    break;

                // GP0(E1h) - Draw Mode setting(aka "Texpage")
                case 0xE1:
                    break;

                // GP0(E2h) - Texture Window setting
                case 0xE2:
                    break;

                // GP0(E3h) - Set Drawing Area top left (X1, Y1)
                case 0xE3:
                    gpu->drawing_area.x1 = packet & 0x000003FF;
                    gpu->drawing_area.y1 = (packet >> 10) & 0x000001FF;

                    break;

                // GP0(E4h) - Set Drawing Area bottom right (X2,Y2)
                case 0xE4:
                    gpu->drawing_area.x2 = packet & 0x000003FF;
                    gpu->drawing_area.y2 = (packet >> 10) & 0x000001FF;

                    break;

                // GP0(E5h) - Set Drawing Offset (X,Y)
                case 0xE5:
                    gpu->drawing_offset_x = packet & 0x000003FF;
                    gpu->drawing_offset_y = (packet >> 10) & 0x000001FF;

                    break;

                // GP0(E6h) - Mask Bit Setting
                case 0xE6:
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        case LIBPS_GPU_RECEIVING_COMMAND_PARAMETERS:
            gpu->cmd_packet.params[params_pos++] = packet;
            gpu->cmd_packet.remaining_words--;

            if (gpu->cmd_packet.remaining_words == 0)
            {
                cmd_func(gpu);
            }
            break;

        case LIBPS_GPU_RECEIVING_COMMAND_DATA:
            gpu->received_data = packet;
            cmd_func(gpu);

            break;

        // Used only by GP0(C0h)
        case LIBPS_GPU_TRANSFERRING_DATA:
            cmd_func(gpu);
            break;
    }
}

// Processes a GP1 packet.
void libps_gpu_process_gp1(struct libps_gpu* gpu, const uint32_t packet)
{
    assert(gpu != NULL);

    switch (packet >> 24)
    {
        // GP1(00h) - Reset GPU
        case 0x00:
            gpu->gpustat = 0x14802000;
            break;

        // GP1(01h) - Reset Command Buffer
        case 0x01:
            break;

        // GP1(02h) - Acknowledge GPU Interrupt (IRQ1)
        case 0x02:
            break;

        // GP1(03h) - Display Enable
        case 0x03:
            break;

        // GP1(04h) - DMA Direction / Data Request
        case 0x04:
            break;

        // GP1(05h) - Start of Display area (in VRAM)
        case 0x05:
            break;

        // GP1(06h) - Horizontal Display range (on Screen)
        case 0x06:
            break;

        // GP1(07h) - Vertical Display range (on Screen)
        case 0x07:
            break;

        // GP1(08h) - Display mode
        case 0x08:
            break;

        // GP1(10h) - Get GPU Info
        case 0x10:
            switch (packet & 0x00FFFFFF)
            {
                // Returns Nothing (old value in GPUREAD remains unchanged)
                case 0x07:
                    gpu->gpuread = 2;
                    break;

                default:
                    __debugbreak();
                    break;
            }
            break;

        default:
            __debugbreak();
            break;
    }
}
