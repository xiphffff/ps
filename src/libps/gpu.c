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
#include <string.h>
#include "gpu.h"

static void (*cmd_func)(struct libps_gpu*);
static unsigned int params_pos;

static int edge_function(const struct libps_gpu_vertex* const v0,
                         const struct libps_gpu_vertex* const v1,
                         const struct libps_gpu_vertex* const v2)
{
    return ((v1->x - v0->x) * (v2->y - v0->y)) -
           ((v1->y - v0->y) * (v2->x - v0->x));
}

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
            gpu->vram[vram_x_pos + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos) + 0] =
            gpu->received_data >> 16;

            gpu->vram[vram_x_pos + (LIBPS_GPU_VRAM_WIDTH * vram_y_pos) + 1] =
            gpu->received_data & 0x0000FFFF;

            if (vram_x_pos == vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = ((gpu->cmd_packet.params[0] & 0x0000FFFF) & 0x000003FF) + 1;
            }
            else
            {
                vram_x_pos++;
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

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
    params_pos = 0;

    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
}

static void draw_polygon(struct libps_gpu* gpu,
                         const struct libps_gpu_vertex* const v0,
                         const struct libps_gpu_vertex* const v1,
                         const struct libps_gpu_vertex* const v2)
{
    assert(gpu != NULL);
    assert(v0 != NULL);
    assert(v1 != NULL);
    assert(v2 != NULL);

    // https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/
    struct libps_gpu_vertex p;
    const struct libps_gpu_vertex* v1_real;
    const struct libps_gpu_vertex* v2_real;

    if (edge_function(v0, v1, v2) < 0)
    {
        v1_real = v2;
        v2_real = v1;
    }
    else
    {
        v1_real = v1;
        v2_real = v2;
    }

    for (p.y = gpu->drawing_area.y1; p.y <= gpu->drawing_area.y2; p.y++)
    {
        for (p.x = gpu->drawing_area.x1; p.x <= gpu->drawing_area.x2; p.x++)
        {
            const int w0 = edge_function(v1_real, v2_real, &p);
            const int w1 = edge_function(v2_real, v0, &p);
            const int w2 = edge_function(v0, v1_real, &p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                // Color interpolation not supported (yet)
                const uint8_t red   = (v0->color >> 3) & 0x1F;
                const uint8_t green = (v0->color >> 11) & 0x1F;
                const uint8_t blue  = (v0->color >> 19) & 0x1F;

                // A1G5B5R5
                gpu->vram[p.x + (LIBPS_GPU_VRAM_WIDTH * p.y)] =
                (green << 5) | (blue << 10) | red;
            }
        }
    }
}

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

        draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x     = (int16_t)(gpu->cmd_packet.params[4] & 0x0000FFFF),
                .y     = (int16_t)(gpu->cmd_packet.params[4] >> 16),
                .color = gpu->cmd_packet.params[0]
            };
            draw_polygon(gpu, &v1, &v2, &v3);
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
            .x     = (int16_t)(gpu->cmd_packet.params[1] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[1] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v1 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[3] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[3] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        const struct libps_gpu_vertex v2 =
        {
            .x     = (int16_t)(gpu->cmd_packet.params[5] & 0x0000FFFF),
            .y     = (int16_t)(gpu->cmd_packet.params[5] >> 16),
            .color = gpu->cmd_packet.params[0]
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x     = (int16_t)(gpu->cmd_packet.params[7] & 0x0000FFFF),
                .y     = (int16_t)(gpu->cmd_packet.params[7] >> 16),
                .color = gpu->cmd_packet.params[0]
            };
            draw_polygon(gpu, &v1, &v2, &v3);
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

        draw_polygon(gpu, &v0, &v1, &v2);

        if (gpu->cmd_packet.flags & DRAW_FLAG_QUAD)
        {
            const struct libps_gpu_vertex v3 =
            {
                .x     = (int16_t)(gpu->cmd_packet.params[7] & 0x0000FFFF),
                .y     = (int16_t)(gpu->cmd_packet.params[7] >> 16),
                .color = gpu->cmd_packet.params[6] & 0x00FFFFFF
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }
    }

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));
    params_pos = 0;

    gpu->state = LIBPS_GPU_AWAITING_COMMAND;
}

// Allocates memory for a `libps_gpu` structure and returns a pointer to it if
// memory allocation was successful, `NULL` otherwise. This function does not
// automatically initialize initial state.
struct libps_gpu* libps_gpu_create(void)
{
    struct libps_gpu* gpu = malloc(sizeof(struct libps_gpu));

    gpu->vram = calloc(sizeof(uint16_t) * LIBPS_GPU_VRAM_WIDTH * LIBPS_GPU_VRAM_HEIGHT, sizeof(uint16_t));
    return gpu;
}

// Deallocates the memory held by `gpu`.
void libps_gpu_destroy(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    free(gpu->vram);
    free(gpu);
}

// Resets the GPU to the initial state.
void libps_gpu_reset(struct libps_gpu* gpu)
{
    assert(gpu != NULL);

    gpu->gpustat = 0x14802000;
    gpu->gpuread = 0x00000000;

    memset(&gpu->cmd_packet, 0, sizeof(gpu->cmd_packet));

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

                // GP0(28h) - Monochrome four-point polygon, opaque
                //
                // XXX: I have no idea why nocash says this is monochrome, is
                // the color saturated in some way?
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

                    // Bug, should be 7. I have no idea why this works yet.
                    gpu->cmd_packet.remaining_words = 10;

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

        default:
            __debugbreak();
            break;
    }
}