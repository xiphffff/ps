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

#include <Windows.h>
#include "../libps/include/ps.h"
#include "framework/ini_file.h"
#include "pstest.h"

PSTest::PSTest() noexcept
{
    INIFile config_file(L"pstest.ini");

    std::wstring bios_file = config_file.read_string(L"files", L"bios");

    if (bios_file.empty())
    {
        bios_file = FileDialog::open(nullptr,
                                     L"Select PlayStation BIOS",
                                     L"PlayStation BIOS file (*.bin)");

        if (bios_file.empty())
        {
            MessageBox(nullptr,
                       L"libps testing station cannot operate without a BIOS file.",
                       nullptr,
                       MB_OK | MB_ICONERROR);

            return;
        }

        config_file.write_string(L"files", L"bios", bios_file);
    }

    FILE* bios_file_p = _wfopen(bios_file.c_str(), L"rb");
    uint8_t* bios_data = new uint8_t[0x80000];
    fread(bios_data, 1, 0x80000, bios_file_p);
    fclose(bios_file_p);

    main_window.set_title(L"libps debugging station");
    main_window.set_size(LIBPS_GPU_VRAM_WIDTH, LIBPS_GPU_VRAM_HEIGHT);
    main_window.set_position(WindowPosition::Center);

    main_window.show(SW_SHOWDEFAULT);
}

PSTest::~PSTest() noexcept
{ }

int PSTest::run() const noexcept
{
    MSG msg = { };

    for (;;)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(1);
        }
    }
    return EXIT_SUCCESS;
}