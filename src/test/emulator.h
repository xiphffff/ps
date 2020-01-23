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

#pragma once

#include <QtCore>

struct libps_system;

class Emulator : public QThread
{
    Q_OBJECT

public:
    Emulator(QObject* parent, const QString& bios_file);
    ~Emulator();

    // Starts the emulation if it is not running.
    void begin_run_loop();

    // Stops the emulation if it is running, resetting the emulator to the
    // startup state.
    void stop_run_loop();

    // Pauses the emulation if it is running, but *does not* reset the emulator
    // to the startup state.
    void pause_run_loop();

    // Thread entry point
    void run() override;

    // Halts emulation, injects the PS-X EXE specified by `file_name` into RAM,
    // and restarts emulation.
    void inject_ps_exe(const QString& file_name);

private:
    // Emulator instance
    struct libps_system* sys;

    // Pointer to the BIOS data
    uint8_t* bios;

    // Is the emulator running?
    bool running;

    // Are we injecting a test EXE?
    bool injecting;

signals:
#ifdef LIBPS_DEBUG
    // Exception other than an interrupt or system call was raised by the CPU.
    void exception_raised(const unsigned int exccode, const uint32_t vaddr);

    // Illegal instruction occurred (Reserved Instruction exception).
    void illegal_instruction(const uint32_t instruction, const uint32_t pc);
#endif // LIBPS_DEBUG

    // SystemErrorUnresolvedException() was called by the BIOS.
    void system_error();

    // A BIOS call was reached.
    void bios_call(const uint32_t pc, const uint32_t fn);

    // TTY string has been printed
    void tty_string(const QString& string);

    // Time to render a frame
    void render_frame(const uint16_t* vram);
};