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

#include <QThread>

// Forward declarations
struct psemu_system;
class QFile;

class Emulator : public QThread
{
    Q_OBJECT

public:
    explicit Emulator(QObject* parent, const QString& bios_file) noexcept;
    ~Emulator() noexcept;

    // Starts the emulator, if not running. This function has no effect if the
    // emulator is already running.
    void start_run_loop() noexcept;

    // Stops the emulator, if running. This function has no effect if the
    // emulator is not running.
    void stop_run_loop() noexcept;

    // Pauses the emulator, if running. This function has no effect if the
    // emulator is not running.
    void pause_run_loop() noexcept;

    // Restarts the emulator, if running. This function has no effect if the
    // emulator is not running.
    void restart_run_loop() noexcept;

    // Inserts a game image `game_image_path` into the CD-ROM drive.
    void insert_game_image(const QString& game_image_path) noexcept;

    // Restarts emulation, injects the EXE located at `file_path` on the host
    // system into emulated RAM and executes it.
    void run_exe(const QString& file_path) noexcept;

    // Set to `true` to pause emulation if an exception has been raised, or
    // `false` otherwise. The only reason for this variable to ever be set to
    // `false` is to run tests that verify exception behavior.
    //
    // A reserved instruction exception (RI) will always emit the
    // `exception_raised()` signal, regardless of the value of this variable.
    bool break_on_exception;

    // Sony PlayStation® emulator
    struct psemu_system* ps;

private:
    // Is the emulator running?
    bool running;

    // Are we injecting an EXE?
    bool inject_exe;

    // The path to the EXE file we're injecting, if any.
    QString exe_file_path;

    // Handle to the current game image file, if any.
    QFile* game_image;

    // Thread entry point
    void run() override;

    // BIOS data
    uint8_t* bios_data;

    // Called whenever we have to read a sector off of the current CD-ROM
    // image.
    void do_game_image_read(const unsigned int address,
                            uint8_t* const sector_data) noexcept;
signals:
    // The emulated CPU raised an internal exception.
    void exception_raised();

    // `std_out_putchar` was called by the BIOS.
    void std_out_putchar(const char c);

    // Time to render a frame
    void render_frame();

    // `SystemErrorUnresolvedException()` was called by the BIOS.
    void system_error();
};