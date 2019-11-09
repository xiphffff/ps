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

#include <iostream>
#include <thread>
#include <cstdio>
#include <chrono>
#include <SDL.h>
#include "../libps/include/ps.h"
#include "../libps/include/disasm.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << argv[0] << ": Missing required argument." << std::endl;
        std::cerr << "Syntax: " << argv[0] << " biosfile" << std::endl;

        return EXIT_FAILURE;
    }

    FILE* bios_file = fopen(argv[1], "rb");
    uint8_t* bios_data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * 0x80000));
    fread(bios_data, 1, 0x80000, bios_file);
    fclose(bios_file);

    struct libps_system* ps = libps_system_create(bios_data);

    bool done = false;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("libps debugging station",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          LIBPS_GPU_VRAM_WIDTH,
                                          LIBPS_GPU_VRAM_HEIGHT,
                                          SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_Renderer* renderer = SDL_CreateRenderer(window,
                                                -1,
                                                SDL_RENDERER_ACCELERATED);

    SDL_Texture* texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_ABGR1555,
                                             SDL_TEXTUREACCESS_TARGET,
                                             LIBPS_GPU_VRAM_WIDTH,
                                             LIBPS_GPU_VRAM_HEIGHT);

    while (!done)
    {
        SDL_Event event;
        SDL_PollEvent(&event);

        const auto start_time = std::chrono::steady_clock::now();
            for (unsigned int cycle = 0; cycle < (LIBPS_CPU_CLOCK_RATE / 60); ++cycle)
            {
                libps_system_step(ps);
            }

            SDL_UpdateTexture(texture,
                              NULL,
                              ps->bus->gpu->vram,
                              sizeof(uint16_t) * LIBPS_GPU_VRAM_WIDTH);

            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        const auto end_time = std::chrono::steady_clock::now();

        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (diff.count() < (1000 / 60))
        {
            std::this_thread::sleep_for(diff);
        }
    }
    return 0;
}