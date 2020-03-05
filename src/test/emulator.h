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

// Forward declaration
struct libps_system;

struct bios_trace_info
{
    // Function origin
    quint32 origin;

    // Function
    quint32 func;

    // Arguments to the BIOS call
    QVector<quint32> args;

    // BIOS call return value
    quint32 return_value;
};

class Emulator : public QThread
{
    Q_OBJECT

public:
    Emulator(QObject* parent, const QString& bios_file);
    ~Emulator();

    // Starts the emulation if it is not running.
    void start_run_loop();

    // Stops the emulation if it is running, resetting the emulator to the
    // startup state.
    void stop_run_loop();

    // Pauses the emulation if it is running, but *does not* reset the emulator
    // to the startup state.
    void pause_run_loop();

    // Called when the user selects a game image to load after triggering
    // "File -> Insert CD-ROM image..." on the main window.
    void insert_cdrom_image(const QString& file_name);

    // Called when the user selects a PS-X EXE to inject after triggering
    // "File -> Run PS-X EXE..." on the main window.
    void run_ps_x_exe(const QString& file_name);

    // Returns the number of total cycles taken by the emulator.
    unsigned int total_cycles_taken() noexcept;

private:
    // Called when it is time to inject the PS-X EXE specified by
    // `run_ps_x_exe()`.
    void inject_ps_x_exe();

    // Called when the BIOS reaches the `std_out_putchar()` call.
    void handle_tty_string();

    // Initiates a trace of a BIOS call.
    void trace_bios_call(const uint32_t pc, const uint32_t fn);

    // Called when it is time to seek to a specified position on the CD-ROM
    // image.
    void handle_cdrom_image_seek();

    // Thread entry point
    void run() override;

    // Called when it is time to read data off of the CD-ROM image.
    uint8_t handle_cdrom_image_read();

    // The file handle of the CD-ROM image, if any.
    FILE* cdrom_image_file;

    // Is the emulator running?
    bool running;

    // Are we injecting a PS-X EXE?
    bool injecting_ps_x_exe;

    // Are we currently tracing a BIOS call?
    bool tracing_bios_call;

    // File name of the PS-X EXE we are injecting
    QString ps_x_exe;

    // Pointer to the BIOS data
    uint8_t* bios;

    // The PC to stop tracing a BIOS call on. This is the PC set by
    // `jr $t2`
    quint32 bios_call_trace_pc;

    // The total number of cycles taken by the emulator.
    unsigned int total_cycles;

    // BIOS trace information
    struct bios_trace_info bios_trace;

    // Emulator instance
    struct libps_system* sys;

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