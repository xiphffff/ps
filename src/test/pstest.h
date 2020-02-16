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

#include "emulator.h"
#include "main_window.h"
#include "debug/log.h"

class PSTest : public QObject
{
    Q_OBJECT

public:
    PSTest();
    ~PSTest();

private:
    // Called when the emulator core reports that BIOS call
    // `A(0x40) - SystemErrorUnresolvedException()` was reached.
    void emu_report_system_error();

    // Called when the emulator core reports that a BIOS call other than
    // A(0x40), A(0x3C), or B(0x3D) was reached.
    void emu_bios_call(struct bios_trace_info* bios_trace);

    // Called when the user triggers `Debug -> Display libps log`.
    void display_libps_log();

    // Called when the user triggers `Emulation -> Start`. This function is
    // also called upon startup, and is used also to resume emulation from a
    // paused state.
    void start_emu();

    // Called when the user triggers `Emulation -> Stop`.
    void stop_emu();

    // Called when the user triggers `Emulation -> Pause`.
    void pause_emu();

    // Called when the user triggers `Emulation -> Reset`.
    void reset_emu();

#ifdef LIBPS_DEBUG
    // Called when an unknown word load has been attempted
    void on_debug_unknown_memory_load(const uint32_t paddr,
                                      const unsigned int type);

    // Called when an unknown word store has been attempted
    void on_debug_unknown_memory_store(const uint32_t paddr,
                                       const unsigned int data,
                                       const unsigned int type);

    void on_debug_interrupt_requested(const unsigned int interrupt);
    void on_debug_interrupt_acknowledged(const unsigned int interrupt);
#endif // LIBPS_DEBUG

    // Called when a TTY string has been generated
    void on_tty_string(const QString& tty_string);

    MainWindow* main_window;
    MessageLogger* libps_log;
    Emulator* emulator;
};