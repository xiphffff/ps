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

#include <QFile>
#include "emulator.h"
#include "../psemu/include/psemu.h"

Emulator::Emulator(QObject* parent, const QString& bios_file) noexcept :
QThread(parent),
break_on_exception{ true },
running{ false },
inject_exe{ false }
{
    QFile bios_file_handle(bios_file);
    bios_file_handle.open(QIODevice::ReadOnly);

    const auto byte_array{ bios_file_handle.readAll() };
    const auto size{ bios_file_handle.size() };

    bios_data = new uint8_t[size];
    memcpy(bios_data, byte_array.constData(), size);

    bios_file_handle.close();

    ps = psemu_create(bios_data);
    psemu_set_user_param_cb(ps, this);
}

Emulator::~Emulator() noexcept
{
    psemu_destroy(ps);
}

// Starts the emulator, if not running. This function has no effect if the
// emulator is already running.
void Emulator::start_run_loop() noexcept
{
    if (!running)
    {
        running = true;
        start();
    }
}

// Stops the emulator, if running. This function has no effect if the emulator
// is not running.
void Emulator::stop_run_loop() noexcept
{
    if (running)
    {
        running = false;

        psemu_reset(ps);
        quit();
    }
}

// Pauses the emulator, if running. This function has no effect if the emulator
// is not running.
void Emulator::pause_run_loop() noexcept
{
    if (running)
    {
        running = false;
        quit();
    }
}

// Restarts the emulator, if running. This function has no effect if the
// emulator is not running.
void Emulator::restart_run_loop() noexcept
{
    stop_run_loop();
    start_run_loop();
}

// Inserts a game image `game_image_path` into the CD-ROM drive.
void Emulator::insert_game_image(const QString& game_image_path) noexcept
{
    game_image = new QFile(game_image_path, this);
    game_image->open(QIODevice::ReadOnly);

    psemu_set_cdrom(ps, [](void* user_param,
                           const unsigned int address,
                           uint8_t* const sector_data)
    {
        Emulator* emu = reinterpret_cast<Emulator*>(user_param);
        emu->do_game_image_read(address, sector_data);
    });
}

void Emulator::do_game_image_read(const unsigned int address,
                                  uint8_t* const sector_data) noexcept
{
    game_image->seek(address);

    const auto sector{ game_image->read(PSEMU_CDROM_SECTOR_SIZE) };
    memcpy(sector_data, sector.constData(), PSEMU_CDROM_SECTOR_SIZE);
}

// Restarts emulation, injects the EXE located at `file_path` on the host
// system into emulated RAM and executes it.
void Emulator::run_exe(const QString& file_path) noexcept
{
    inject_exe    = true;
    exe_file_path = file_path;
}

// Thread entry point
void Emulator::run()
{
    constexpr auto freq{ 33868800 / 60 };

    while (running)
    {
        for (auto cycle{ 0 }; cycle < freq; ++cycle)
        {
            if (ps->cpu.pc == 0x000000A0)
            {
                switch (ps->cpu.gpr[9])
                {
                    case 0x3C:
                        emit std_out_putchar(ps->cpu.gpr[4]);
                        break;

                    case 0x40:
                        emit system_error();

                        running = false;
                        break;
                }
            }

            if (ps->cpu.pc == 0x000000B0)
            {
                switch (ps->cpu.gpr[9])
                {
                    case 0x3D:
                        emit std_out_putchar(ps->cpu.gpr[4]);
                        break;
                }
            }

            if (ps->cpu.pc == 0x80000080)
            {
                const auto exc_code
                {
                    (ps->cpu.cop0[PSEMU_CPU_COP0_Cause] >> 2) & 0x0000001F
                };

                if (exc_code != PSEMU_CPU_EXCCODE_Sys &&
                    exc_code != PSEMU_CPU_EXCCODE_Int)
                {
                    // A reserved instruction exception will always halt
                    // emulation, regardless of the `break_on_exception`
                    // value.
                    if (exc_code == PSEMU_CPU_EXCCODE_RI)
                    {
                        emit exception_raised();

                        running = false;
                        break;
                    }

                    if (break_on_exception)
                    {
                        emit exception_raised();

                        running = false;
                        break;
                    }
                }
            }

            if ((ps->cpu.pc == 0x80030000) && inject_exe)
            {
                QFile exe_file(exe_file_path);
                exe_file.open(QIODevice::ReadOnly);

                const auto byte_array{ exe_file.readAll() };
                const auto exe_data{ byte_array.constData() };

                const auto exe_size{ exe_file.size() };

                auto dest{ *(uint32_t *)&exe_data[0x10] };

                for (auto ptr{ 0x800 }; ptr != (exe_size - 0x800); ++ptr)
                {
                    *(uint32_t *)&ps->bus.ram[dest++ & 0x1FFFFFFF] =
                    exe_data[ptr];
                }

                ps->cpu.pc      = *(uint32_t *)&exe_data[0x18];
                ps->cpu.next_pc = ps->cpu.pc;

                ps->cpu.instruction.word =
                psemu_bus_load_word(&ps->bus, ps->cpu.pc);

                exe_file.close();

                inject_exe    = false;
                exe_file_path = "";
            }
            psemu_step(ps);
        }
        psemu_vblank(ps);
        emit render_frame();
    }
}