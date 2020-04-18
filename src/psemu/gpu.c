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
#include "debug.h"
#include "gpu.h"
#include "utility/fifo.h"
#include "utility/memory.h"

// (1024 * 512 * sizeof(uint16_t))
//
// This could have very well been a #define and likely would've been turned
// into a constant, whatever.
static const size_t VRAM_SIZE =
sizeof(uint16_t) * PSEMU_GPU_VRAM_WIDTH * PSEMU_GPU_VRAM_HEIGHT;

#define COLOR_DEPTH_4BPP 0
#define COLOR_DEPTH_8BPP 1
#define COLOR_DEPTH_15BPP 2

// Defines the structure of the internal command state.
static struct
{
    // Draw flags
    struct
    {
        // At least one of these variables *must* be set to `true` for the
        // helper functions to operate properly.

        // GP0(0x20) - three-point polygon, opaque
        // GP0(0x22) - three-point polygon, semi-transparent
        // GP0(0x28) - four-point polygon, opaque
        // GP0(0x2A) - four-point polygon, semi-transparent
        // GP0(0x40) - line, opaque
        // GP0(0x42) - line, semi-transparent
        // GP0(0x48) - poly-line, opaque
        // GP0(0x4A) - poly-line, semi-transparent
        // GP0(0x60) - rectangle (variable size) (opaque)
        // GP0(0x62) - rectangle(variable size) (semi-transparent)
        // GP0(0x68) - rectangle(1x1) (Dot) (opaque)
        // GP0(0x6A) - rectangle(1x1) (Dot) (semi-transparent)
        // GP0(0x70) - rectangle(8x8) (opaque)
        // GP0(0x72) - rectangle(8x8) (semi-transparent)
        // GP0(0x78) - rectangle(16x16) (opaque)
        // GP0(0x7A) - rectangle(16x16) (semi-transparent)
        bool monochrome;

        // GP0(0x24) - three-point polygon, opaque, texture-blending
        // GP0(0x25) - three-point polygon, opaque, raw-texture
        // GP0(0x26) - three-point polygon, semi-transparent, texture-blending
        // GP0(0x27) - three-point polygon, semi-transparent, raw-texture
        // GP0(0x2C) - four-point polygon, opaque, texture-blending
        // GP0(0x2D) - four-point polygon, opaque, raw-texture
        // GP0(0x2E) - four-point polygon, semi-transparent, texture-blending
        // GP0(0x2F) - four-point polygon, semi-transparent, raw-texture
        bool textured;

        // GP0(0x30) - three-point polygon, opaque
        // GP0(0x32) - three-point polygon, semi-transparent
        // GP0(0x38) - four-point polygon, opaque
        // GP0(0x3A) - four-point polygon, semi-transparent
        // GP0(0x50) - line, opaque
        // GP0(0x52) - line, semi - transparent
        // GP0(0x58) - poly-line, opaque
        // GP0(0x5A) - poly-line, semi-transparent
        bool shaded;

        // You'll need to set both `textured` and `shaded` to 1 for the
        // following commands:
        //
        // GP0(0x34) - Shaded Textured three-point polygon, opaque, texture-blending
        // GP0(0x36) - Shaded Textured three-point polygon, semi-transparent, tex-blend
        // GP0(0x3C) - Shaded Textured four-point polygon, opaque, texture-blending
        // GP0(0x3E) - Shaded Textured four-point polygon, semi-transparent, tex-blend

        // Draws a quadrangle.
        bool quad;
    } draw_flags;

    // Draw Mode setting
    union
    {
        struct
        {
            // (N * 64) (i.e. in 64-halfword steps)
            unsigned int x_base : 4;

            // (N * 256) (i.e. 0 or 256)
            unsigned int y_base_is_256 : 1;

            // (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
            unsigned int semi_transparency_mode : 2;

            // (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
            unsigned int color_depth : 2;
            unsigned int : 2;

            // (0=Normal, 1=Disable if GP1(0x09).Bit0=1)
            unsigned int texture_disable : 1;
            unsigned int : 4;
        };
        uint16_t halfword;
    } texpage;

    // CLUT Attribute (Color Lookup Table)
    //
    // This attribute is used in all Textured Polygon/Rectangle commands. Of
    // course, it's relevant only for 4bit/8bit textures (don't care for 15bit
    // textures).
    //
    // Specifies the location of the CLUT data within VRAM.
    union
    {
        struct
        {
            // X coordinate X/16 (i.e. in 16-halfword steps)
            unsigned int x : 6;

            // Y coordinate 0-511 (i.e. in 1-line steps)
            unsigned int y : 9;

            unsigned int : 1;
        };
        uint16_t halfword;
    } clut;

    // How many words remaining?
    unsigned int remaining_words;

    // Command parameters
    struct psemu_fifo params;

    // Function to call when `remaining_words` becomes 0. This should always be
    // one of the following:
    //
    // `draw_polygon_helper()`,
    // `draw_rect_helper()`,
    // `draw_line_helper()` 
    void (*func)(struct psemu_gpu* const gpu);

    // Data received.
    uint32_t received_data;
} cmd_state;

// Returns a pixel from the CLUT.
static uint16_t clut_lookup(const struct psemu_gpu* const gpu,
                            const unsigned int x,
                            const uint16_t texel)
{
    assert(gpu != NULL);

    switch (cmd_state.texpage.color_depth)
    {
        case COLOR_DEPTH_4BPP:
        {
            const unsigned int offset = (texel >> (x & 3) * 4) & 0xF;
            return gpu->vram[((cmd_state.clut.x * 16) + offset) +
                             (PSEMU_GPU_VRAM_WIDTH * cmd_state.clut.y)];
        }

        case COLOR_DEPTH_15BPP:
            return texel;

        default:
            psemu_log("GPU: 8bpp CLUT lookup not implemented, results will be "
                      "wrong");
            return 0xBEE6;
    }
}

// Determines edges based on Pineda's method
// (see "A Parallel Algorithm for Polygon Rasterization" by Juan Pineda)
static inline float edge_function(const struct psemu_gpu_vertex* const v0,
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

    memset(&cmd_state.draw_flags, 0, sizeof(cmd_state.draw_flags));

    cmd_state.remaining_words = 0;

    cmd_state.texpage.halfword = 0x0000;
    cmd_state.clut.halfword    = 0x0000;

    cmd_state.func = NULL;
    gpu->gp0_state = PSEMU_GP0_AWAITING_COMMAND;
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

    if (edge_function(v0, v1, v2) < 0.0F)
    {
#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)
        SWAP(*v1, *v2);
#undef SWAP
    }

    const float area = edge_function(v0, v1, v2);

    for (p.y = gpu->drawing_area.y1; p.y <= gpu->drawing_area.y2; p.y++)
    {
        for (p.x = gpu->drawing_area.x1; p.x <= gpu->drawing_area.x2; p.x++)
        {
            const float w0 = edge_function(v1, v2, &p);
            const float w1 = edge_function(v2, v0, &p);
            const float w2 = edge_function(v0, v1, &p);

            if (w0 >= 0.0F && w1 >= 0.0F && w2 >= 0.0F)
            {
                if (cmd_state.draw_flags.textured)
                {
                    const uint16_t texcoord_x = ((w0 * v0->texcoord.x) +
                                                 (w1 * v1->texcoord.x) +
                                                 (w2 * v2->texcoord.x)) / area;

                    uint16_t texcoord_y = ((w0 * v0->texcoord.y) +
                                           (w1 * v1->texcoord.y) +
                                           (w2 * v2->texcoord.y)) / area;

                    texcoord_y += cmd_state.texpage.y_base_is_256 ? 256 : 0;

                    uint16_t texel_x = cmd_state.texpage.x_base * 64;

                    switch (cmd_state.texpage.color_depth)
                    {
                        case COLOR_DEPTH_4BPP:
                            texel_x += (texcoord_x / 4);
                            break;

                        case COLOR_DEPTH_8BPP:
                            texel_x += (texcoord_x / 8);
                            break;

                        case COLOR_DEPTH_15BPP:
                            texel_x += texcoord_x;
                            break;
                    }
                    
                    const uint16_t texel =
                    gpu->vram[texel_x + (PSEMU_GPU_VRAM_WIDTH * texcoord_y)];

                    const uint16_t color = clut_lookup(gpu, texcoord_x, texel);

                    if (color == 0x0000)
                    {
                        continue;
                    }
                    gpu->vram[p.x + (PSEMU_GPU_VRAM_WIDTH * p.y)] = color;
                }
                else
                {
                    const unsigned int r = ((w0 * v0->color.r) +
                                            (w1 * v1->color.r) +
                                            (w2 * v2->color.r)) / area / 8;

                    const unsigned int g = ((w0 * v0->color.g) +
                                            (w1 * v1->color.g) +
                                            (w2 * v2->color.g)) / area / 8;
                     
                    const unsigned int b = ((w0 * v0->color.b) +
                                            (w1 * v1->color.b) +
                                            (w2 * v2->color.b)) / area / 8;

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

    if (cmd_state.draw_flags.monochrome)
    {
        const uint32_t color = psemu_fifo_dequeue(&cmd_state.params); // 1st

        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params); // 2nd
        const struct psemu_gpu_vertex v0 =
        {
            .x          = (int16_t)(v0_data & 0x0000FFFF),
            .y          = (int16_t)(v0_data >> 16),
            .color.word = color
        };

        const uint32_t v1_data = psemu_fifo_dequeue(&cmd_state.params); // 3rd
        struct psemu_gpu_vertex v1 =
        {
            .x          = (int16_t)(v1_data & 0x0000FFFF),
            .y          = (int16_t)(v1_data >> 16),
            .color.word = color
        };

        const uint32_t v2_data = psemu_fifo_dequeue(&cmd_state.params); // 4th
        struct psemu_gpu_vertex v2 =
        {
            .x          = (int16_t)(v2_data & 0x0000FFFF),
            .y          = (int16_t)(v2_data >> 16),
            .color.word = color
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.draw_flags.quad)
        {
            const uint32_t v3_data = psemu_fifo_dequeue(&cmd_state.params); // 5th
            struct psemu_gpu_vertex v3 =
            {
                .x          = (int16_t)(v3_data & 0x0000FFFF),
                .y          = (int16_t)(v3_data >> 16),
                .color.word = color
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }

        reset_gp0(gpu);
        return;
    }

    if (cmd_state.draw_flags.textured)
    {
        const uint32_t color = psemu_fifo_dequeue(&cmd_state.params); // 1st

        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params); // 2nd
        const uint32_t v0_pt   = psemu_fifo_dequeue(&cmd_state.params); // 3rd
        const struct psemu_gpu_vertex v0 =
        {
            .x                 = (int16_t)(v0_data & 0x0000FFFF),
            .y                 = (int16_t)(v0_data >> 16),
            .texcoord.halfword = v0_pt & 0x0000FFFF,
            .color.word        = color
        };

        cmd_state.clut.halfword = v0_pt >> 16;

        const uint32_t v1_data = psemu_fifo_dequeue(&cmd_state.params); // 4th
        const uint32_t v1_tt   = psemu_fifo_dequeue(&cmd_state.params); // 5th
        struct psemu_gpu_vertex v1 =
        {
            .x                 = (int16_t)(v1_data & 0x0000FFFF),
            .y                 = (int16_t)(v1_data >> 16),
            .texcoord.halfword = v1_tt & 0x0000FFFF,
            .color.word        = color
        };

        cmd_state.texpage.halfword = v1_tt >> 16;

        const uint32_t v2_data = psemu_fifo_dequeue(&cmd_state.params); // 6th
        const uint32_t v2_tt   = psemu_fifo_dequeue(&cmd_state.params); // 7th
        struct psemu_gpu_vertex v2 =
        {
            .x                 = (int16_t)(v2_data & 0x0000FFFF),
            .y                 = (int16_t)(v2_data >> 16),
            .texcoord.halfword = v2_tt & 0x0000FFFF,
            .color.word        = color
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.draw_flags.quad)
        {
            const uint32_t v3_data = psemu_fifo_dequeue(&cmd_state.params); // 8th
            const uint32_t v3_tt   = psemu_fifo_dequeue(&cmd_state.params); // 9th
            struct psemu_gpu_vertex v3 =
            {
                .x                 = (int16_t)(v3_data & 0x0000FFFF),
                .y                 = (int16_t)(v3_data >> 16),
                .texcoord.halfword = v3_tt & 0x0000FFFF,
                .color.word        = color
            };
            draw_polygon(gpu, &v1, &v2, &v3);
        }

        reset_gp0(gpu);
        return;
    }

    if (cmd_state.draw_flags.shaded)
    {
        const uint32_t v0_color = psemu_fifo_dequeue(&cmd_state.params); // 1st
        const uint32_t v0_data  = psemu_fifo_dequeue(&cmd_state.params); // 2nd
        const struct psemu_gpu_vertex v0 =
        {
            .x          = (int16_t)(v0_data & 0x0000FFFF),
            .y          = (int16_t)(v0_data >> 16),
            .color.word = v0_color & 0x00FFFFFF
        };

        const uint32_t v1_color = psemu_fifo_dequeue(&cmd_state.params); // 3rd
        const uint32_t v1_data  = psemu_fifo_dequeue(&cmd_state.params); // 4th
        struct psemu_gpu_vertex v1 =
        {
            .x          = (int16_t)(v1_data & 0x0000FFFF),
            .y          = (int16_t)(v1_data >> 16),
            .color.word = v1_color & 0x00FFFFFF
        };

        const uint32_t v2_color = psemu_fifo_dequeue(&cmd_state.params); // 5th
        const uint32_t v2_data  = psemu_fifo_dequeue(&cmd_state.params); // 6th
        struct psemu_gpu_vertex v2 =
        {
            .x          = (int16_t)(v2_data & 0x0000FFFF),
            .y          = (int16_t)(v2_data >> 16),
            .color.word = v2_color & 0x00FFFFFF
        };

        draw_polygon(gpu, &v0, &v1, &v2);

        if (cmd_state.draw_flags.quad)
        {
            const uint32_t v3_color = psemu_fifo_dequeue(&cmd_state.params); // 7th 
            const uint32_t v3_data  = psemu_fifo_dequeue(&cmd_state.params); // 8th
            struct psemu_gpu_vertex v3 =
            {
                .x          = (int16_t)(v3_data & 0x0000FFFF),
                .y          = (int16_t)(v3_data >> 16),
                .color.word = v3_color & 0x00FFFFFF
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

    const unsigned int r = v0->color.r / 8;
    const unsigned int g = v0->color.g / 8;
    const unsigned int b = v0->color.b / 8;

    gpu->vram[v0->x + (PSEMU_GPU_VRAM_WIDTH * v0->y)] =
    (g << 5) | (b << 10) | r;
}

// This function is called whenever a rectangle needs to be drawn. It assembles
// the vertex data based on specific flags.
static void draw_rect_helper(struct psemu_gpu* const gpu)
{
    assert(gpu != NULL);

    if (cmd_state.draw_flags.monochrome)
    {
        const uint32_t color   = psemu_fifo_dequeue(&cmd_state.params);
        const uint32_t v0_data = psemu_fifo_dequeue(&cmd_state.params);

        const struct psemu_gpu_vertex v0 =
        {
            .x          = (int16_t)(v0_data & 0x0000FFFF),
            .y          = (int16_t)(v0_data >> 16),
            .color.word = color
        };

        draw_rect(gpu, &v0);
    }
    reset_gp0(gpu);
    psemu_fifo_reset(&cmd_state.params);
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

    if (gpu->gp0_state == PSEMU_GP0_RECEIVING_PARAMETERS)
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
        gpu->gp0_state = PSEMU_GP0_RECEIVING_DATA;

        // We can't do anything until we receive at least one data word.
        return;
    }

    if (gpu->gp0_state == PSEMU_GP0_RECEIVING_DATA)
    {
        if (cmd_state.remaining_words != 0)
        {
            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)] =
            cmd_state.received_data & 0x0000FFFF;

            if (vram_x_pos >= vram_x_pos_max)
            {
                vram_y_pos++;
                vram_x_pos = vram_x_pos_origin;
            }

            gpu->vram[vram_x_pos++ + (PSEMU_GPU_VRAM_WIDTH * vram_y_pos)] =
            cmd_state.received_data >> 16;

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

    if (gpu->gp0_state == PSEMU_GP0_RECEIVING_PARAMETERS)
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
        gpu->gp0_state = PSEMU_GP0_TRANSFERRING_DATA;
        return;
    }

    if (gpu->gp0_state == PSEMU_GP0_TRANSFERRING_DATA)
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

    gpu->gp0_state = PSEMU_GP0_AWAITING_COMMAND;
    gpu->gpuread = 0x00000000;

    memset(gpu->vram, 0x0000, VRAM_SIZE);

    memset(&gpu->drawing_area,   0x00000000, sizeof(gpu->drawing_area));
    memset(&gpu->drawing_offset, 0x00000000, sizeof(gpu->drawing_offset));
    memset(&gpu->texture_window, 0x00000000, sizeof(gpu->texture_window));

    psemu_fifo_reset(&cmd_state.params);
    reset_gp0(gpu);
}

// Executes a GP0 command `cmd` on GPU `gpu`.
void psemu_gpu_gp0(struct psemu_gpu* const gpu, const uint32_t cmd)
{
    assert(gpu != NULL);

    switch (gpu->gp0_state)
    {
        case PSEMU_GP0_AWAITING_COMMAND:
            switch (cmd >> 24)
            {
                // GP0(0x02) - Fill Rectangle in VRAM
                case 0x02:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 2;
                    cmd_state.func = &fill_rect_in_vram;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x28) - Monochrome four-point polygon, opaque
                case 0x28:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 4;

                    cmd_state.draw_flags.monochrome = true;
                    cmd_state.draw_flags.quad       = true;

                    cmd_state.func = &draw_polygon_helper;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x2C) - Textured four-point polygon, opaque,
                // texture-blending
                case 0x2C:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 8;

                    cmd_state.draw_flags.textured = true;
                    cmd_state.draw_flags.quad     = true;

                    cmd_state.func = &draw_polygon_helper;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x2D) - Textured four-point polygon, opaque, raw-texture
                case 0x2D:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 8;

                    cmd_state.draw_flags.textured = true;
                    cmd_state.draw_flags.quad     = true;

                    cmd_state.func = &draw_polygon_helper;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x30) - Shaded three-point polygon, opaque
                case 0x30:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 5;

                    cmd_state.draw_flags.shaded = true;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    cmd_state.func = &draw_polygon_helper;
                    return;

                // GP0(0x38) - Shaded four-point polygon, opaque
                case 0x38:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 7;

                    cmd_state.draw_flags.shaded = true;
                    cmd_state.draw_flags.quad   = true;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    cmd_state.func = &draw_polygon_helper;
                    return;

                // GP0(0x65) - Textured Rectangle, variable size, opaque,
                // raw-texture
                case 0x65:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.remaining_words = 3;

                    cmd_state.draw_flags.textured = true;

                    cmd_state.func = &draw_rect_helper;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0x68) - Monochrome Rectangle (1x1) (Dot) (opaque)
                case 0x68:
                    psemu_fifo_enqueue(&cmd_state.params, cmd & 0x00FFFFFF);

                    cmd_state.draw_flags.monochrome = 1;
                    cmd_state.func = &draw_rect_helper;

                    cmd_state.remaining_words = 1;
                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;

                    return;

                // GP0(0xA0) - Copy Rectangle (CPU to VRAM)
                case 0xA0:
                    cmd_state.remaining_words = 2;
                    cmd_state.func = &copy_rect_from_cpu;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
                    return;

                // GP0(0xC0) - Copy Rectangle (VRAM to CPU)
                case 0xC0:
                    cmd_state.remaining_words = 2;
                    cmd_state.func = &copy_rect_to_cpu;

                    gpu->gp0_state = PSEMU_GP0_RECEIVING_PARAMETERS;
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

                default:
                    psemu_log("Unknown GP0 packet: 0x%08X", cmd);
                    return;
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
            cmd_state.received_data = cmd;

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
            gpu->gpustat.word = 0x14802000;
            return;

        // GP1(0x01) - Reset Command Buffer
        case 0x01:
            psemu_fifo_reset(&cmd_state.params);
            return;

        // GP1(0x10) - Get GPU Info
        case 0x10:
            switch (cmd & 0x00FFFFFF)
            {
                // Returns Nothing (old value in GPUREAD remains unchanged)
                case 0x07:
                    return;

                default:
                    return;
            }

        default:
            return;
    }
}