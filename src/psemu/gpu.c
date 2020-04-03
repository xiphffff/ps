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

#include <assert.h>
#include <string.h>
#include "gpu.h"
#include "utility/fifo.h"
#include "utility/memory.h"

static const size_t VRAM_SIZE =
PSEMU_GPU_VRAM_WIDTH * PSEMU_GPU_VRAM_HEIGHT * sizeof(uint16_t);

struct
{
    unsigned int flags;
    unsigned int remaining_words;
    struct psemu_fifo params;
    void (*func)(struct psemu_gpu* const gpu);
} static cmd_state;

#define DRAW_FLAG_MONOCHROME (1 << 0)
#define DRAW_FLAG_TEXTURED (1 << 1)
#define DRAW_FLAG_SHADED (1 << 2)
#define DRAW_FLAG_QUAD (1 << 2)

// Returns a pixel.
static uint16_t clut_lookup(const struct psemu_gpu* const gpu,
                            const unsigned int x,
                            const uint16_t texel,
                            const struct psemu_gpu_vertex* const v0)
{
    assert(gpu != NULL);

    const unsigned int clut_x = (v0->palette & 0x3F) * 16;
    const unsigned int clut_y = (v0->palette >> 6) & 0x1FF;

    switch (v0->texpage.color_depth)
    {
        case 0: // 4bit
        {
            const unsigned int offset = (texel >> (x & 3) * 4) & 0xF;
            return gpu->vram[(clut_x + offset) + (PSEMU_GPU_VRAM_WIDTH * clut_y)];
        }

        case 1: // 8bit
            return 0xFFFF;

        default:
            return texel;
    }
}

// Determines edges based on Pineda's method
// (see "A Parallel Algorithm for Polygon Rasterization" by Juan Pineda)
static inline int edge_function(const struct psemu_gpu_vertex* const v0,
                                const struct psemu_gpu_vertex* const v1,
                                const struct psemu_gpu_vertex* const v2)
{
    return ((v1->x - v0->x) * (v2->y - v0->y)) -
           ((v1->y - v0->y) * (v2->x - v0->x));
}

// Returns the GP0 port to normal operation.
static void reset_gp0(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    cmd_state.flags           = 0;
    cmd_state.remaining_words = 0;

    cmd_state.func = NULL;

    psemu_fifo_reset(&cmd_state.params);

    gpu->state = PSEMU_GP0_AWAITING_COMMAND;
}

// Draws a polygon uses vertices `v0`, `v1`, and `v2`.
static void draw_polygon(struct psemu_gpu* const gpu,
                         const struct psemu_gpu_vertex* const v0,
                         struct psemu_gpu_vertex* v1,
                         struct psemu_gpu_vertex* v2)
{
    assert(gpu != NULL);
    assert(v0 != NULL);
    assert(v1 != NULL);
    assert(v2 != NULL);

    // https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/
    struct psemu_gpu_vertex p;

    if (edge_function(v0, v1, v2) < 0)
    {
#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)
        SWAP(v1->x, v2->x);
        SWAP(v1->y, v2->y);
    }

    const int area = edge_function(v0, v1, v2);

    for (p.y = gpu->drawing_area.y1; p.y <= gpu->drawing_area.y2; p.y++)
    {
        for (p.x = gpu->drawing_area.x1; p.x <= gpu->drawing_area.x2; p.x++)
        {
            const int w0 = edge_function(v1, v2, &p);
            const int w1 = edge_function(v2, v0, &p);
            const int w2 = edge_function(v0, v1, &p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                if (cmd_state.flags & DRAW_FLAG_TEXTURED)
                {
                    uint16_t texcoord_x =
                    ((w0 * (v0->texcoord & 0x00FF)) +
                     (w1 * (v1->texcoord & 0x00FF)) +
                     (w2 * (v2->texcoord & 0x00FF))) / area;

                    uint16_t texcoord_y =
                    ((w0 * (v0->texcoord >> 8)) +
                     (w1 * (v1->texcoord >> 8)) +
                     (w2 * (v2->texcoord >> 8))) / area;

                    texcoord_y += v1->texpage.y_base_is_256 ? 256 : 0;

                    switch (v1->texpage.color_depth)
                    {
                        case 0:
                            texcoord_x =
                            (v1->texpage.x_base * 64) + (texcoord_x / 4);
                            
                            break;

                        case 1:
                            texcoord_x =
                            (v1->texpage.x_base * 64) + (texcoord_x / 8);

                            break;

                        case 2:
                            texcoord_x += (v1->texpage.x_base * 64);
                            break;
                    }

                    const uint16_t texel =
                    gpu->vram[texcoord_x + (PSEMU_GPU_VRAM_WIDTH * texcoord_y)];

                    const uint16_t color =
                    clut_lookup(gpu, texcoord_x, texel, v0);

                    if (color == 0x0000)
                    {
                        continue;
                    }
                    gpu->vram[p.x + (PSEMU_GPU_VRAM_WIDTH * p.y)] = color;
                }
                else
                {
                    const unsigned int r =
                    ((w0 * (v0->color & 0x000000FF)) +
                     (w1 * (v1->color & 0x000000FF)) +
                     (w2 * (v2->color & 0x000000FF))) / area / 8;

                    const unsigned int g =
                    ((w0 * ((v0->color >> 8) & 0xFF)) +
                     (w1 * ((v1->color >> 8) & 0xFF)) +
                     (w2 * ((v2->color >> 8) & 0xFF))) / area / 8;
                     
                    const unsigned int b =
                    ((w0 * ((v0->color >> 16) & 0xFF)) +
                     (w1 * ((v1->color >> 16) & 0xFF)) +
                     (w2 * ((v2->color >> 16) & 0xFF))) / area / 8;

                    // G5B5R5A1
                    gpu->vram[p.x + (PSEMU_GPU_VRAM_WIDTH * p.y)] =
                    (g << 5) | (b << 10) | r;
                }
            }
        }
    }
}

// This function is called whenever a polygon needs to be drawn. It assembles
// the vertex data based on specific flags.
static void draw_polygon_helper(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    if (cmd_state.flags & DRAW_FLAG_MONOCHROME)
    {
        const uint32_t color = psemu_fifo_dequeue(&cmd_state.params);

        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params);
        const struct psemu_gpu_vertex v0 =
        {
            .x     = (int16_t)(v0_data & 0x0000FFFF),
            .y     = (int16_t)(v0_data >> 16),
            .color = color
        };

        const uint32_t v1_data = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v1 =
        {
            .x     = (int16_t)(v1_data & 0x0000FFFF),
            .y     = (int16_t)(v1_data >> 16),
            .color = color
        };

        const uint32_t v2_data = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v2 =
        {
            .x     = (int16_t)(v2_data & 0x0000FFFF),
            .y     = (int16_t)(v2_data >> 16),
            .color = color
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.flags & DRAW_FLAG_QUAD)
        {
            const uint32_t v3_data = psemu_fifo_dequeue(&cmd_state.params);
            struct psemu_gpu_vertex v3 =
            {
                .x     = (int16_t)(v3_data & 0x0000FFFF),
                .y     = (int16_t)(v3_data >> 16),
                .color = color
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }

        reset_gp0(gpu);
        return;
    }

    if (cmd_state.flags & DRAW_FLAG_TEXTURED)
    {
        const uint32_t color = psemu_fifo_dequeue(&cmd_state.params);

        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v0_pt   = psemu_fifo_dequeue(&cmd_state.params);
        const struct psemu_gpu_vertex v0 =
        {
            .x        = (int16_t)(v0_data & 0x0000FFFF),
            .y        = (int16_t)(v0_data >> 16),
            .palette  = v0_pt >> 16,
            .texcoord = v0_pt & 0x0000FFFF,
            .color    = color
        };

        const uint32_t v1_data = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v1_tt   = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v1 =
        {
            .x        = (int16_t)(v1_data & 0x0000FFFF),
            .y        = (int16_t)(v1_data >> 16),
            .texpage  = v1_tt >> 16,
            .texcoord = v1_tt & 0x0000FFFF,
            .color    = color
        };

        const uint32_t v2_data = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v2_tt   = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v2 =
        {
            .x        = (int16_t)(v2_data & 0x0000FFFF),
            .y        = (int16_t)(v2_data >> 16),
            .texcoord = v2_tt & 0x0000FFFF,
            .color    = color
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.flags & DRAW_FLAG_QUAD)
        {
            const uint32_t v3_data = psemu_fifo_dequeue(&cmd_state.params);
            const uint32_t v3_tt   = psemu_fifo_dequeue(&cmd_state.params);
            struct psemu_gpu_vertex v3 =
            {
                .x        = (int16_t)(v3_data & 0x0000FFFF),
                .y        = (int16_t)(v3_data >> 16),
                .texcoord = v3_tt & 0x0000FFFF,
                .color    = color
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }

        reset_gp0(gpu);
        return;
    }

    if (cmd_state.flags & DRAW_FLAG_SHADED)
    {
        const uint32_t v0_color = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v0_data  = psemu_fifo_dequeue(&cmd_state.params);
        const struct psemu_gpu_vertex v0 =
        {
            .x     = (int16_t)(v0_data & 0x0000FFFF),
            .y     = (int16_t)(v0_data >> 16),
            .color = v0_color & 0x00FFFFFF
        };

        const uint32_t v1_color = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v1_data  = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v1 =
        {
            .x     = (int16_t)(v1_data & 0x0000FFFF),
            .y     = (int16_t)(v1_data >> 16),
            .color = v1_color & 0x00FFFFFF
        };

        const uint32_t v2_color = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v2_data  = psemu_fifo_dequeue(&cmd_state.params);
        struct psemu_gpu_vertex v2 =
        {
            .x     = (int16_t)(v2_data & 0x0000FFFF),
            .y     = (int16_t)(v2_data >> 16),
            .color = v2_color & 0x00FFFFFF
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.flags & DRAW_FLAG_QUAD)
        {
            const uint32_t v3_color = psemu_fifo_dequeue(&cmd_state.params);
            const uint32_t v3_data  = psemu_fifo_dequeue(&cmd_state.params);
            struct psemu_gpu_vertex v3 =
            {
                .x     = (int16_t)(v3_data & 0x0000FFFF),
                .y     = (int16_t)(v3_data >> 16),
                .color = v3_color & 0x00FFFFFF
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }

        reset_gp0(gpu);
        return;
    }
}

// Draws a rectangle based on vertex data `v0`.
static void draw_rect(struct psemu_gpu* const gpu,
                      const struct psemu_gpu_vertex* const v0)
{
    assert(gpu != NULL);
    assert(v0 != NULL);

    const unsigned int r = (v0->color & 0x000000FF) / 8;
    const unsigned int g = ((v0->color >> 8) & 0xFF) / 8;
    const unsigned int b = ((v0->color >> 16) & 0xFF) / 8;

    gpu->vram[v0->x + (PSEMU_GPU_VRAM_WIDTH * v0->y)] =
    (g << 5) | (b << 10) | r;
}

// This function is called whenever a rectangle needs to be drawn. It assembles
// the vertex data based on specific flags.
static void draw_rect_helper(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    if (cmd_state.flags & DRAW_FLAG_MONOCHROME)
    {
        const uint32_t color   = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params);

        const struct psemu_gpu_vertex v0 =
        {
            .x     = (int16_t)(v0_data & 0x0000FFFF),
            .y     = (int16_t)(v0_data >> 16),
            .color = color
        };

        draw_rect(gpu, &v0);
        reset_gp0(gpu);

        return;
    }
}

// Handles the "GP0(0x02) - Fill Rectangle in VRAM" command.
static void fill_rect_in_vram(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    // Parameters
    const unsigned int color = psemu_fifo_dequeue(&cmd_state.params);
    const unsigned int xy    = psemu_fifo_dequeue(&cmd_state.params);
    const unsigned int wh    = psemu_fifo_dequeue(&cmd_state.params);

    // Resolved parameters
    const unsigned int x_pos = xy & 0x0000FFFF;
    const unsigned int y_pos = xy >> 16;

    const unsigned int width  = wh & 0x0000FFFF;
    const unsigned int height = wh >> 16;

    for (unsigned int x = x_pos; x != (x_pos + width); ++x)
    {
        for (unsigned int y = y_pos; y != (y_pos + height); ++y)
        {
            const unsigned int r = (color & 0x000000FF) / 8;
            const unsigned int g = ((color >> 8) & 0xFF) / 8;
            const unsigned int b = ((color >> 16) & 0xFF) / 8;

            gpu->vram[(x & 0x3FF) + (PSEMU_GPU_VRAM_WIDTH * (y & 0x1FF))] =
            (g << 5) | (b << 10) | r;
        }
    }
    reset_gp0(gpu);
}

// Handles the "GP0(0xA0) - Copy Rectangle (CPU to VRAM)" command.
static void copy_rect_from_cpu(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    // Original X position
    static unsigned int vram_x_pos_origin;

    // Current X position
    static unsigned int vram_x_pos;

    // Current Y position
    static unsigned int vram_y_pos;

    // Maximum length of a line (should be Xxxx+Xsiz)
    static unsigned int vram_x_pos_max;

    if (gpu->state == PSEMU_GP0_RECEIVING_PARAMETERS)
    {
        const uint32_t xy = psemu_fifo_dequeue(&cmd_state.params);
        vram_x_pos = ((xy & 0x0000FFFF) & 0x000003FF);
        vram_y_pos = ((xy >> 16) & 0x000001FF);

        vram_x_pos_origin = vram_x_pos;

        const uint32_t wh = psemu_fifo_dequeue(&cmd_state.params);
        const uint16_t width  = (((wh & 0x0000FFFF) - 1) & 0x000003FF) + 1;
        const uint16_t height = (((wh >> 16) - 1) & 0x000001FF) + 1;

        vram_x_pos_max = vram_x_pos + width;

        cmd_state.remaining_words = (width * height) / 2;

        // Lock the GP0 state to this function.
        gpu->state = PSEMU_GP0_RECEIVING_DATA;

        // Again, we don't want to do anything until we receive at least one
        // data word.
        return;
    }

    if (gpu->state == PSEMU_GP0_RECEIVING_DATA)
    {
        if (cmd_state.remaining_words != 0)
        {
            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)] =
            gpu->received_data & 0x0000FFFF;

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = vram_x_pos_origin;
            }

            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)] =
            gpu->received_data >> 16;

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = vram_x_pos_origin;
            }
            cmd_state.remaining_words--;
        }
        else
        {
            // All of the expected data has been sent. Return to normal
            // operation.
            reset_gp0(gpu);
        }
    }
}

// Handles the "GP0(0xC0) - Copy Rectangle (VRAM to CPU)" command.
static void copy_rect_to_cpu(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    // Original X position
    static unsigned int vram_x_pos_origin;

    // Current X position
    static unsigned int vram_x_pos;

    // Current Y position
    static unsigned int vram_y_pos;

    // Maximum length of a line (should be Xxxx+Xsiz)
    static unsigned int vram_x_pos_max;

    if (gpu->state == PSEMU_GP0_RECEIVING_PARAMETERS)
    {
        const uint32_t xy = psemu_fifo_dequeue(&cmd_state.params);
        vram_x_pos = (xy & 0x0000FFFF) & 0x000003FF;
        vram_y_pos = (xy >> 16) & 0x000001FF;

        vram_x_pos_origin = vram_x_pos;

        const uint32_t wh = psemu_fifo_dequeue(&cmd_state.params);
        const uint16_t width  = (((wh & 0x0000FFFF) - 1) & 0x000003FF) + 1;
        const uint16_t height = (((wh >> 16) - 1) & 0x000001FF) + 1;

        vram_x_pos_max = vram_x_pos + width;

        cmd_state.remaining_words = (width * height) / 2;

        // Lock the GP0 state to this function.
        gpu->state = PSEMU_GP0_TRANSFERRING_DATA;
        return;
    }

    if (gpu->state == PSEMU_GP0_TRANSFERRING_DATA)
    {
        if (cmd_state.remaining_words != 0)
        {
            const uint16_t p0 =
            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)];

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = vram_x_pos_origin;
            }

            const uint16_t p1 =
            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)];

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = vram_x_pos_origin;
            }

            gpu->gpuread = ((p1 << 16) | p0);
            cmd_state.remaining_words--;
        }
        else
        {
            reset_gp0(gpu);
        }
    }
}

// Initializes a GPU `gpu`.
void psemu_gpu_init(struct psemu_gpu* const gpu)
{
    gpu->vram = psemu_safe_malloc(VRAM_SIZE);
    psemu_fifo_init(&cmd_state.params, 16);
#ifdef PSEMU_DEBUG
    gpu->debug_user_data = NULL;

    gpu->debug_unknown_cmd = NULL;
#endif // PSEMU_DEBUG
}

// Deallocates the memory held by `gpu`.
void psemu_gpu_fini(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    psemu_fifo_fini(&cmd_state.params);
    psemu_safe_free(gpu->vram);
}

// Resets a GPU `gpu` to the startup state.
void psemu_gpu_reset(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    gpu->state = PSEMU_GP0_AWAITING_COMMAND;
    gpu->gpuread = 0x00000000;

    gpu->received_data = 0x00000000;

    memset(gpu->vram, 0x0000, VRAM_SIZE);

    memset(&gpu->drawing_area,   0x00000000, sizeof(gpu->drawing_area));
    memset(&gpu->drawing_offset, 0x00000000, sizeof(gpu->drawing_offset));
    memset(&gpu->texture_window, 0x00000000, sizeof(gpu->texture_window));

    reset_gp0(gpu);
}

// Executes a GP0 command `cmd` on GPU `gpu`.
void psemu_gpu_gp0(struct psemu_gpu* const gpu, const uint32_t cmd)
{
    assert(gpu != NULL);

    switch (gpu->state)
    {
        case PSEMU_GP0_AWAITING_COMMAND:
            switch (cmd >> 24)
            {
                // GP0(0x00) - NOP(?)
                case 0x00:
                    return;

                // GP0(0x01) - Clear Cache
                case 0x01:
                    return;

                // GP0(0x02) - Fill Rectangle in VRAM
                case 0x02:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 2;
                    cmd_state.func = &fill_rect_in_vram;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x28) - Monochrome four-point polygon, opaque
                case 0x28:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 4;

                    cmd_state.flags |= DRAW_FLAG_MONOCHROME;
                    cmd_state.flags |= DRAW_FLAG_QUAD;

                    cmd_state.func = &draw_polygon_helper;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(2Ch) - Textured four-point polygon, opaque, texture-blending
                case 0x2C:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 8;

                    cmd_state.flags |= DRAW_FLAG_TEXTURED;
                    cmd_state.flags |= DRAW_FLAG_QUAD;

                    cmd_state.func = &draw_polygon_helper;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;


                // GP0(0x2D) - Textured four-point polygon, opaque, raw-texture
                case 0x2D:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 8;

                    cmd_state.flags |= DRAW_FLAG_TEXTURED;
                    cmd_state.flags |= DRAW_FLAG_QUAD;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x30) - Shaded three-point polygon, opaque
                case 0x30:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 5;

                    cmd_state.flags |= DRAW_FLAG_SHADED;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    cmd_state.func = &draw_polygon_helper;
                    return;

                // GP0(0x38) - Shaded four-point polygon, opaque
                case 0x38:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 7;

                    cmd_state.flags |= DRAW_FLAG_SHADED;
                    cmd_state.flags |= DRAW_FLAG_QUAD;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    cmd_state.func = &draw_polygon_helper;
                    return;

                // GP0(0x65) - Textured Rectangle, variable size, opaque,
                // raw-texture
                case 0x65:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 3;
                    cmd_state.flags |= DRAW_FLAG_TEXTURED;
                    cmd_state.func = &draw_rect_helper;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x68) - Monochrome Rectangle (1x1) (Dot) (opaque)
                case 0x68:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.flags |= DRAW_FLAG_MONOCHROME;
                    cmd_state.func = &draw_rect_helper;

                    cmd_state.remaining_words = 1;
                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    return;

                // GP0(0xA0) - Copy Rectangle (CPU to VRAM)
                case 0xA0:
                    cmd_state.remaining_words = 2;
                    cmd_state.func = &copy_rect_from_cpu;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0xC0) - Copy Rectangle (VRAM to CPU)
                case 0xC0:
                    cmd_state.remaining_words = 2;
                    cmd_state.func = &copy_rect_to_cpu;

                    gpu->state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0xE1) - Draw Mode setting (aka "Texpage")
                case 0xE1:
                    return;

                // GP0(0xE2) - Texture Window setting
                case 0xE2:
                    gpu->texture_window.mask.x = cmd & 0x0000001F;
                    gpu->texture_window.mask.y = (cmd >> 5) & 0x0000001F;

                    gpu->texture_window.offset.x = (cmd >> 10) & 0x0000001F;
                    gpu->texture_window.offset.y = (cmd >> 15) & 0x0000001F;

                    return;

                // GP0(0xE3) - Set Drawing Area top left(X1, Y1)
                case 0xE3:
                    gpu->drawing_area.x1 = cmd & 0x000003FF;
                    gpu->drawing_area.y1 = (cmd >> 10) & 0x000001FF;

                    return;

                // GP0(E4h) - Set Drawing Area bottom right(X2, Y2)
                case 0xE4:
                    gpu->drawing_area.x2 = cmd & 0x000003FF;
                    gpu->drawing_area.y2 = (cmd >> 10) & 0x000001FF;

                    return;

                // GP0(E5h) - Set Drawing Offset (X,Y)
                case 0xE5:
                    gpu->drawing_offset.x = cmd & 0x000003FF;
                    gpu->drawing_offset.y = (cmd >> 10) & 0x000001FF;

                    return;

                // GP0(E6h) - Mask Bit Setting
                case 0xE6:
                    return;
#ifdef PSEMU_DEBUG
                default:
                    if (gpu->debug_unknown_cmd)
                    {
                        gpu->debug_unknown_cmd(gpu->debug_user_data, "GP0", cmd);
                    }
                    return;
#endif // PSEMU_DEBUG
            }

        case PSEMU_GP0_RECEIVING_PARAMETERS:
            psemu_fifo_enqueue(&cmd_state.params, cmd);
            cmd_state.remaining_words--;

            if (cmd_state.remaining_words == 0)
            {
                cmd_state.func(gpu);
            }
            return;

        case PSEMU_GP0_RECEIVING_DATA:
            gpu->received_data = cmd;

            cmd_state.func(gpu);
            return;

        // Used only by GP0(0xC0).
        case PSEMU_GP0_TRANSFERRING_DATA:
            cmd_state.func(gpu);
            return;
    }
}

// Executes a GP1 command `cmd` on GPU `gpu`.
void psemu_gpu_gp1(struct psemu_gpu* const gpu, const uint32_t cmd)
{
    assert(gpu != NULL);

    switch (cmd >> 24)
    {
        // GP1(0x00) - Reset GPU
        case 0x00:
            gpu->gpustat.word = 0x1FF00000;
            return;

        // GP1(0x01) - Reset Command Buffer
        case 0x01:
            psemu_fifo_reset(&cmd_state.params);
            return;

        // GP1(10h) - Get GPU Info
        case 0x10:
            switch (cmd & 0x00FFFFFF)
            {
                // Returns Nothing (old value in GPUREAD remains unchanged)
                case 0x07:
                    return;
#ifdef PSEMU_DEBUG
                default:
                    if (gpu->debug_unknown_cmd)
                    {
                        gpu->debug_unknown_cmd
                        (gpu->debug_user_data, "GP1(0x10)", cmd);
                    }
                    return;
#endif // PSEMU_DEBUG
            }
#ifdef PSEMU_DEBUG
        default:
            if (gpu->debug_unknown_cmd)
            {
                gpu->debug_unknown_cmd(gpu->debug_user_data, "GP1", cmd);
            }
            return;
#endif // PSEMU_DEBUG
    }
}