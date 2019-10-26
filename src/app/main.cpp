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

#include <cstdio>
#include <cstdlib>
#include <iostream>
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

    // We assume that the BIOS file is legitimate in that the hash and size are
    // correct, and that it can be opened without any errors.
    uint8_t* bios_data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * 0x80000));

    FILE* bios_file = fopen(argv[1], "rb");
    fread(bios_data, 1, 0x80000, bios_file);
    fclose(bios_file);

    FILE* output = fopen("output.txt", "w+");

    struct libps_system* ps = libps_system_create(bios_data);

    bool done = false;

    while (!done)
    {
        char disasm[LIBPS_DISASM_MAX_LENGTH];
        libps_disassemble_instruction(ps->cpu->instruction, ps->cpu->pc, disasm);

        printf("0x%08X: %s\n", ps->cpu->pc, disasm);
        fprintf(output, "0x%08X: %s\n", ps->cpu->pc, disasm);

        libps_system_step(ps);

        if (!ps->cpu->good)
        {
            done = true;
            break;
        }
    }

    fflush(output);
    fclose(output);
    libps_system_destroy(ps);

    return 0;
}