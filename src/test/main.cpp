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
#include <filesystem>
#include <SDL.h>
#include "../libps/include/ps.h"
#include "../libps/include/disasm.h"

// General purpose registers (GPR) as defined by MIPS conventions
static const char* const gpr[32] =
{
    "zero", // 0
    "at",   // 1
    "v0",   // 2
    "v1",   // 3
    "a0",   // 4
    "a1",   // 5
    "a2",   // 6
    "a3",   // 7
    "t0",   // 8
    "t1",   // 9
    "t2",   // 10
    "t3",   // 11
    "t4",   // 12
    "t5",   // 13
    "t6",   // 14
    "t7",   // 15
    "s0",   // 16
    "s1",   // 17
    "s2",   // 18
    "s3",   // 19
    "s4",   // 20
    "s5",   // 21
    "s6",   // 22
    "s7",   // 23
    "t8",   // 24
    "t9",   // 25
    "k0",   // 26
    "k1",   // 27
    "gp",   // 28
    "sp",   // 29
    "fp",   // 30
    "ra"    // 31
};

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << argv[0] << ": Missing required argument." << std::endl;
        std::cerr << "Syntax: " << argv[0] << " biosfile testfile" << std::endl;

        return EXIT_FAILURE;
    }

    FILE* bios_file = fopen(argv[1], "rb");
    uint8_t* bios_data = static_cast<uint8_t*>(malloc(0x80000));
    fread(bios_data, 1, 0x80000, bios_file);
    fclose(bios_file);
#if 0
    FILE* test_file = fopen(argv[2], "rb");
    const auto test_file_size = std::filesystem::file_size(argv[2]);
    uint8_t* test_data = static_cast<uint8_t*>(malloc(test_file_size));
    fread(test_data, 1, test_file_size, test_file);
    fclose(test_file);
#endif
    FILE* disasm_file = fopen("disasm.txt", "w");

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

    bool tracing = false;

    std::string tty_str;

    while (!done)
    {
        SDL_Event event;
        SDL_PollEvent(&event);

        switch (event.type)
        {
            case SDL_KEYDOWN:
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_t:
                            tracing = !tracing;
                            break;

                        default:
                            break;
                    }
                    break;

            default:
                break;
        }

        const auto start_time = std::chrono::steady_clock::now();
            for (unsigned int cycle = 0; cycle != (LIBPS_CPU_CLOCK_RATE / 60); ++cycle)
            {
                if (!ps->cpu->good)
                {
                    fflush(disasm_file);
                    __debugbreak();
                }

                if (((ps->cpu->pc == 0x000000A0) && ps->cpu->gpr[9] == 0x3C) ||
                    ((ps->cpu->pc == 0x000000B0) && ps->cpu->gpr[9] == 0x3D))
                {
                    if (ps->cpu->gpr[4] == '\n')
                    {
                        printf("%s", tty_str.c_str());
                        tty_str.clear();
                    }
                    tty_str += ps->cpu->gpr[4];
                }

                if ((ps->cpu->pc == 0x000000A0) && ps->cpu->gpr[9] == 0x40)
                {
                    __debugbreak();
                }
#if 0
                if (ps->cpu->pc == 0x80030000)
                {
                    uint32_t dest = *(uint32_t *)(test_data + 0x10);
                        
                    for (unsigned int ptr = 0x800; ptr != (test_file_size - 0x800); ++ptr)
                    {
                        *(uint32_t *)(ps->bus->ram + (dest++ & 0x1FFFFFFF)) = test_data[ptr];
                    }

                    ps->cpu->pc      = *(uint32_t *)(test_data + 0x18);
                    ps->cpu->next_pc = *(uint32_t *)(test_data + 0x18);

                    ps->cpu->instruction = libps_bus_load_word(ps->bus, LIBPS_CPU_TRANSLATE_ADDRESS(ps->cpu->pc));
                }
#endif
                if (tracing)
                {
                    char disasm[LIBPS_DISASM_MAX_LENGTH];
                    libps_disassemble_instruction(ps->cpu->instruction, ps->cpu->pc, disasm);

                    fprintf(disasm_file, "%08x: %08x %s\t\t[", ps->cpu->pc, ps->cpu->instruction, disasm);

                    for (unsigned int reg = 0; reg < 32; ++reg)
                    {
                        fprintf(disasm_file, "%s=%08X,", gpr[reg], ps->cpu->gpr[reg]);
                    }
                    fprintf(disasm_file, "]\n");
                    fflush(disasm_file);
                }
                libps_system_step(ps);
            }
            libps_vblank(ps);

            SDL_UpdateTexture(texture,
                              nullptr,
                              ps->bus->gpu->vram,
                              sizeof(uint16_t) * LIBPS_GPU_VRAM_WIDTH);

            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
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