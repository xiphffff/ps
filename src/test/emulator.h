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
    Emulator(const QString& bios_file);
    ~Emulator();

    void begin_run_loop();
    void stop_run_loop();
    void pause_run_loop();

    void run() override;

    // Halts emulation, injects the PS-X EXE specified by `file_name` into RAM,
    // and restarts emulation at its specified program counter.
    void inject_ps_exe(const QString& file_name);

private:
    struct libps_system* sys;
    uint8_t* bios;

    bool running;

signals:
#ifdef LIBPS_DEBUG
    void exception_raised(const unsigned int exccode, const uint32_t vaddr);
    void illegal_instruction(const uint32_t instruction, const uint32_t pc);
#endif // LIBPS_DEBUG

    void system_error();
    void tty_string(const QString& string);
    void render_frame(const uint16_t* vram);
};