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

#include <filesystem>
#include "emulator.h"
#include "../libps/include/ps.h"

Emulator::Emulator(QObject* parent, const QString& bios_file) : QThread(parent)
{
    FILE* bios_file_handle = fopen(qPrintable(bios_file), "rb");
    bios = new uint8_t[0x80000];
    fread(bios, 1, 0x80000, bios_file_handle);
    fclose(bios_file_handle);

    sys = libps_system_create(bios);

    sys->bus->cdrom->user_data = this;

#ifdef LIBPS_DEBUG
    sys->bus->debug_user_data = this;

    sys->bus->debug_unknown_memory_load = [](void* user_data,
                                             const uint32_t paddr,
                                             const unsigned int type)
    {
        Emulator* ps = reinterpret_cast<Emulator*>(user_data);
        emit ps->on_debug_unknown_memory_load(paddr, type);
    };

    sys->bus->debug_unknown_memory_store = [](void* user_data,
                                              const uint32_t paddr,
                                              const unsigned int data,
                                              const unsigned int type)
    {
        Emulator* ps = reinterpret_cast<Emulator*>(user_data);
        emit ps->on_debug_unknown_memory_store(paddr, data, type);
    };

    sys->bus->debug_interrupt_requested = [](void* user_data,
                                             const unsigned int interrupt)
    {
        Emulator* ps = reinterpret_cast<Emulator*>(user_data);
        emit ps->on_debug_interrupt_requested(interrupt);
    };

    sys->bus->debug_interrupt_acknowledged = [](void* user_data,
                                                const unsigned int interrupt)
    {
        Emulator* ps = reinterpret_cast<Emulator*>(user_data);
        emit ps->on_debug_interrupt_acknowledged(interrupt);
    };
#endif // LIBPS_DEBUG
    running            = false;
    injecting_ps_x_exe = false;

    total_cycles = 0;
}

Emulator::~Emulator()
{
    libps_system_destroy(sys);
}

// Starts the emulation if it is not running.
void Emulator::start_run_loop()
{
    if (!running)
    {
        running = true;
        start();
    }
}

// Stops the emulation if it is running, resetting the emulator to the
// startup state.
void Emulator::stop_run_loop()
{
    if (running)
    {
        running = false;
        libps_system_reset(sys);

        total_cycles = 0;
        exit();
    }
}

// Pauses the emulation if it is running, but *does not* reset the emulator to
// the startup state.
void Emulator::pause_run_loop()
{
    if (running)
    {
        running = false;
        exit();
    }
}

// Called when the user selects a game image to load after triggering
// "File -> Insert CD-ROM image..." on the main window.
void Emulator::insert_cdrom_image(const QString& file_name)
{
    struct libps_cdrom_info cdrom_info;

    cdrom_image_file = fopen(qPrintable(file_name), "rb");

    cdrom_info.read_cb = [](void* user_data) -> uint8_t
    {
        Emulator* emu = reinterpret_cast<Emulator*>(user_data);
        return emu->handle_cdrom_image_read();
    };

    cdrom_info.seek_cb = [](void* user_data)
    {
        Emulator* emu = reinterpret_cast<Emulator*>(user_data);
        emu->handle_cdrom_image_seek();
    };

    libps_system_set_cdrom(sys, &cdrom_info);
}

// Called when the user selects a PS-X EXE to inject after triggering
// "File -> Run PS-X EXE..." on the main window.
void Emulator::run_ps_x_exe(const QString& file_name)
{
    ps_x_exe = file_name;

    injecting_ps_x_exe = true;

    // Restart emulation.
    stop_run_loop();
    start_run_loop();
}

// Returns the number of total cycles taken by the emulator.
unsigned int Emulator::total_cycles_taken() noexcept
{
    return total_cycles;
}

// Called when it is time to inject the PS-X EXE specified by `run_ps_x_exe()`.
void Emulator::inject_ps_x_exe()
{
    const auto file_name{ qPrintable(ps_x_exe) };

    FILE* ps_x_exe_handle = fopen(file_name, "rb");
    const auto file_size = std::filesystem::file_size(file_name);
    uint8_t* ps_x_exe_data = static_cast<uint8_t*>(malloc(file_size));
    fread(ps_x_exe_data, 1, file_size, ps_x_exe_handle);
    fclose(ps_x_exe_handle);

    uint32_t dest = *(uint32_t *)(ps_x_exe_data + 0x10);

    for (unsigned int ptr = 0x800;
         ptr != (file_size - 0x800);
         ++ptr)
    {
        *(uint32_t *)(sys->bus->ram + (dest++ & 0x1FFFFFFF)) =
        ps_x_exe_data[ptr];
    }

    sys->cpu->pc      = *(uint32_t *)(ps_x_exe_data + 0x18);
    sys->cpu->next_pc = sys->cpu->pc;

    sys->cpu->instruction = libps_bus_load_word(sys->bus, sys->cpu->pc);

    injecting_ps_x_exe = false;
    ps_x_exe           = nullptr;
}

// Called when `std_out_putchar` has been called by the BIOS.
void Emulator::handle_tty_string()
{
    static QString tty_str;

    tty_str += sys->cpu->gpr[4];

    if (sys->cpu->gpr[4] == '\n')
    {
        emit tty_string(tty_str);
        tty_str.clear();
    }
}

// Initiates a trace of a BIOS call.
void Emulator::trace_bios_call(const uint32_t pc, const uint32_t fn)
{ }

// Called when it is time to seek to a specified position on the CD-ROM image.
void Emulator::handle_cdrom_image_seek()
{
    const unsigned int seconds = sys->bus->cdrom->seek_target.second - 2;

    const unsigned int pos =
    sys->bus->cdrom->seek_target.sector +
    (seconds * 75) +
    (sys->bus->cdrom->seek_target.minute * 60 * 75);

    fseek(cdrom_image_file, pos, SEEK_SET);
}

// Thread entry point
void Emulator::run()
{
    while (running)
    {
        QElapsedTimer timer;
        timer.start();

        for (unsigned int cycle = 0;
             cycle < 33868800 / 60;
             cycle++,
             total_cycles++)
        {
            if (tracing_bios_call && bios_call_trace_pc == sys->cpu->pc)
            {
                emit bios_call(&bios_trace);
                tracing_bios_call = false;
            }

            // We can only inject a PS-X EXE at this point. This is the
            // earliest point during boot-up where the kernel is initialized
            // far enough to allow execution of PS-X EXEs.
            if (injecting_ps_x_exe && sys->cpu->pc == 0x80030000)
            {
                inject_ps_x_exe();
            }

            if (sys->cpu->pc == 0x000000A0)
            {
                switch (sys->cpu->gpr[9])
                {
                    case 0x3C:
                        handle_tty_string();
                        break;

                    case 0x40:
                        emit system_error();
                        running = false;

                        break;

                    default:
                        bios_trace.func = sys->cpu->gpr[9];
                        bios_trace.origin = sys->cpu->pc;

                        emit bios_call(&bios_trace);
                        break;
                }
            }
            
            if (sys->cpu->pc == 0x000000B0)
            {
                switch (sys->cpu->gpr[9])
                {
                    case 0x3D:
                        handle_tty_string();
                        break;

                    default:
                        bios_trace.func   = sys->cpu->gpr[9];
                        bios_trace.origin = sys->cpu->pc;

                        emit bios_call(&bios_trace);
                        break;
                }
            }

            if (sys->cpu->pc == 0x000000C0)
            {
                bios_trace.func   = sys->cpu->gpr[9];
                bios_trace.origin = sys->cpu->pc;

                emit bios_call(&bios_trace);
            }
            libps_system_step(sys);
        }

        sys->bus->i_stat |= LIBPS_IRQ_VBLANK;

        emit render_frame(sys->bus->gpu->vram);

        const qint64 elapsed = timer.elapsed();
           
        if (elapsed < (1000 / 60))
        {
            QThread::msleep((1000 / 60) - elapsed);
        }
    }
}

// Called when it is time to read data off of the CD-ROM image.
uint8_t Emulator::handle_cdrom_image_read()
{
    uint8_t result;
    fread(&result, 1, 1, cdrom_image_file);

    return result;
}