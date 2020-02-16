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
    FILE* handle = fopen(qPrintable(bios_file), "rb");
    bios = new uint8_t[0x80000];
    fread(bios, 1, 0x80000, handle);
    fclose(handle);

    sys = libps_system_create(bios);

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
    running   = false;
    injecting = false;

    total_cycles = 0;
}

Emulator::~Emulator()
{
    delete[] bios;
    libps_system_destroy(sys);
}

// Thread entry point
void Emulator::run()
{
    while (running)
    {
        QElapsedTimer timer;
        timer.start();

        for (unsigned int cycle = 0; cycle < 33868800 / 60; cycle+=2, total_cycles+=2)
        {
            if (tracing_bios_call && bios_call_trace_pc == sys->cpu->pc)
            {
                emit bios_call(&bios_trace);
                tracing_bios_call = false;
            }

            if (injecting && sys->cpu->pc == 0x80030000)
            {
                inject_ps_exe();
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

void Emulator::trace_bios_call(const uint32_t pc, const uint32_t fn)
{ }

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

// Starts the emulation if it is not running.
void Emulator::begin_run_loop()
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

void Emulator::set_injection(const QString& file_name)
{
    test_exe = file_name;
    injecting = true;

    stop_run_loop();
    begin_run_loop();
}

void Emulator::inject_ps_exe()
{
    FILE* test_file = fopen(qPrintable(test_exe), "rb");
    const auto test_file_size = std::filesystem::file_size(qPrintable(test_exe));
    uint8_t* test_data = static_cast<uint8_t*>(malloc(test_file_size));
    fread(test_data, 1, test_file_size, test_file);
    fclose(test_file);

    uint32_t dest = *(uint32_t *)(test_data + 0x10);

    for (unsigned int ptr = 0x800; ptr != (test_file_size - 0x800); ++ptr)
    {
        *(uint32_t *)(sys->bus->ram + (dest++ & 0x1FFFFFFF)) = test_data[ptr];
    }

    sys->cpu->pc      = *(uint32_t*)(test_data + 0x18);
    sys->cpu->next_pc = *(uint32_t*)(test_data + 0x18);

    sys->cpu->instruction = libps_bus_load_word(sys->bus, sys->cpu->pc);
    free(test_data);
}