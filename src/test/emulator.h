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

struct bios_trace_info
{
    // Function origin
    uint32_t origin;

    // Function
    uint32_t func;

    // Arguments to the BIOS call
    QVector<uint32_t> args;

    // BIOS call return value
    uint32_t return_value;
};

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

    void set_injection(const QString& file_name);

    void inject_ps_exe();

    unsigned int total_cycles;

    // Emulator instance
    struct libps_system* sys;

private:
    // Pointer to the BIOS data
    uint8_t* bios;

    // Is the emulator running?
    bool running;

    // Are we injecting a test EXE?
    bool injecting;
    QString test_exe;

    // Are we currently tracing a BIOS call?
    bool tracing_bios_call;

    // The PC to stop tracing a BIOS call on. This is the PC set by
    // `jr $t2`
    uint32_t bios_call_trace_pc;

    struct bios_trace_info bios_trace;

    void handle_tty_string();

    void trace_bios_call(const uint32_t pc, const uint32_t fn);

signals:
#ifdef LIBPS_DEBUG
    // Exception other than an interrupt or system call was raised by the CPU.
    void exception_raised(const unsigned int exccode, const uint32_t vaddr);

    // Illegal instruction occurred (Reserved Instruction exception).
    void illegal_instruction(const uint32_t instruction, const uint32_t pc);

    // Called when an unknown memory load has been attempted
    void on_debug_unknown_memory_load(const uint32_t paddr,
                                      const unsigned int type);

    // Called when an unknown memory store has been attempted
    void on_debug_unknown_memory_store(const uint32_t paddr,
                                       const unsigned int data,
                                       const unsigned int type);

    // Called when an interrupt has been requested
    void on_debug_interrupt_requested(const unsigned int interrupt);

    // Called when an interrupt has been acknowledged
    void on_debug_interrupt_acknowledged(const unsigned int interrupt);
#endif // LIBPS_DEBUG

    // SystemErrorUnresolvedException() was called by the BIOS.
    void system_error();

    // A BIOS call other than A(0x40), A(0x3C), or B(0x3D) was reached.
    void bios_call(struct bios_trace_info* trace_info);

    // TTY string has been printed
    void tty_string(const QString& string);

    // Time to render a frame
    void render_frame(const uint16_t* vram);
};