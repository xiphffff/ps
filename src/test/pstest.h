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
#include "debug/bios_calls.h"
#include "debug/tty_log.h"

class PSTest : public QObject
{
    Q_OBJECT

public:
    PSTest();
    ~PSTest();

private:
    // Returns the BIOS file to use.
    QString handle_initial_bios_select();

    // Called when the emulator core reports that BIOS call
    // `A(0x40) - SystemErrorUnresolvedException()` was reached.
    void emu_report_system_error();

    // Called when the emulator core reports that a BIOS call other than
    // A(0x40), A(0x3C), or B(0x3D) was reached.
    void emu_bios_call(const quint32 pc, const quint32 fn);

    // Called when the user triggers `Debug -> Display TTY Log`.
    void display_tty_log();

    // Called when the user triggers `Debug -> Display BIOS call log`.
    void display_bios_call_log();

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

    BIOSCalls* bios_calls;
    MainWindow* main_window;
    TTYLogger* tty_logger;
    Emulator* emulator;
};