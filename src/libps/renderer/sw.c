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
#include "gpu.h"
#include "sw.h"

// XXX: This should probably be a helper function.
static uint16_t process_pixel_through_clut(struct libps_gpu* gpu,
                                           const unsigned int x,
                                           const uint16_t texel,
                                           const unsigned int texpage_color_depth,
                                           const uint16_t clut)
{
    assert(gpu != NULL);

    const unsigned int clut_x = (clut & 0x3F) * 16;
    const unsigned int clut_y = (clut >> 6) & 0x1FF;

    switch (texpage_color_depth)
    {
        case 4:
        {
            const unsigned int offset = (texel >> (x & 3) * 4) & 0xF;
            return gpu->vram[(clut_x + offset) + (LIBPS_GPU_VRAM_WIDTH * clut_y)];
        }

        case 16:
            return texel;

        default:
            return 0xFFFF;
    }
}

static float edge_function(const struct libps_gpu_vertex* const v0,
                           const struct libps_gpu_vertex* const v1,
                           const struct libps_gpu_vertex* const v2)
{
    return ((v1->x - v0->x) * (v2->y - v0->y)) -
           ((v1->y - v0->y) * (v2->x - v0->x));
}

void libps_renderer_sw_draw_polygon(struct libps_gpu* gpu,
                                    const struct libps_gpu_vertex* const v0,
                                    struct libps_gpu_vertex* const v1,
                                    struct libps_gpu_vertex* const v2)
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

    const float area = edge_function(v0, v1_real, v2_real);

    for (p.y = gpu->drawing_area.y1; p.y <= gpu->drawing_area.y2; p.y++)
    {
        for (p.x = gpu->drawing_area.x1; p.x <= gpu->drawing_area.x2; p.x++)
        {
            float w0 = edge_function(v1_real, v2_real, &p);
            float w1 = edge_function(v2_real, v0, &p);
            float w2 = edge_function(v0, v1_real, &p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                unsigned int pixel_r;
                unsigned int pixel_g;
                unsigned int pixel_b;

                if (gpu->cmd_packet.flags & DRAW_FLAG_TEXTURED)
                {
                    const uint16_t texcoord_x = ((w0 * (v0->texcoord & 0x00FF)) +
                                                 (w1 * (v1_real->texcoord & 0x00FF)) +
                                                 (w2 * (v2_real->texcoord & 0x00FF))) / area;
  
                    const uint16_t texcoord_y = ((w0 * (v0->texcoord >> 8)) +
                                                 (w1 * (v1_real->texcoord >> 8)) +
                                                 (w2 * (v2_real->texcoord >> 8))) / area;

                    const unsigned int texpage_x_base = v1->texpage & 0x0F;
                    const unsigned int texpage_y_base = (v1->texpage & (1 << 4)) ? 256 : 0;

                    unsigned int texpage_color_depth;

                    uint16_t final_texcoord_x;

                    switch ((v1->texpage >> 7) & 0x03)
                    {
                        case 0:
                            texpage_color_depth = 4;
                            final_texcoord_x = (texpage_x_base * 64) + (texcoord_x / 4);

                            break;

                        case 1:
                            texpage_color_depth = 8;
                            final_texcoord_x = (texpage_x_base * 64) + (texcoord_x / 8);

                            break;

                        case 2:
                            texpage_color_depth = 16;
                                
                            final_texcoord_x = (texpage_x_base * 64) + texcoord_x;
                            break;
                    }

                    const uint16_t final_texcoord_y = texpage_y_base + texcoord_y;

                    const uint16_t texel = gpu->vram[final_texcoord_x +
                    (LIBPS_GPU_VRAM_WIDTH * final_texcoord_y)];

                    const uint16_t color =
                    process_pixel_through_clut(gpu, texcoord_x, texel, texpage_color_depth, v0->palette);

                    if (color == 0x0000)
                    {
                        continue;
                    }

                    // G5B5R5A1
                    gpu->vram[p.x + (LIBPS_GPU_VRAM_WIDTH * p.y)] = color;
                }
                else
                {
                    pixel_r = ((w0 * (v0->color & 0x000000FF)) +
                               (w1 * (v1_real->color & 0x000000FF)) +
                               (w2 * (v2_real->color & 0x000000FF))) / area / 8;

                    pixel_g = ((w0 * ((v0->color >> 8) & 0xFF)) +
                               (w1 * ((v1_real->color >> 8) & 0xFF)) +
                               (w2 * ((v2_real->color >> 8) & 0xFF))) / area / 8;

                    pixel_b = ((w0 * ((v0->color >> 16) & 0xFF)) +
                               (w1 * ((v1_real->color >> 16) & 0xFF)) +
                               (w2 * ((v2_real->color >> 16) & 0xFF))) / area / 8;

                    // G5B5R5A1
                    gpu->vram[p.x + (LIBPS_GPU_VRAM_WIDTH * p.y)] =
                    (pixel_g << 5) | (pixel_b << 10) | pixel_r;
                }
            }
        }
    }
}

void libps_renderer_sw_draw_rect(struct libps_gpu* gpu,
                                 const struct libps_gpu_vertex* const vertex)
{
    assert(gpu != NULL);
    assert(vertex != NULL);

    const unsigned int pixel_r = (vertex->color & 0x000000FF) / 8;
    const unsigned int pixel_g = ((vertex->color >> 8) & 0xFF) / 8;
    const unsigned int pixel_b = ((vertex->color >> 16) & 0xFF) / 8;

    gpu->vram[vertex->x + (LIBPS_GPU_VRAM_WIDTH * vertex->y)] =
    (pixel_g << 5) | (pixel_b << 10) | pixel_r;
}