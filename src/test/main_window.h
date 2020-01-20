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

#include <QtWidgets>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

    // Renders the VRAM data.
    void render_frame(const uint16_t* vram);

    QAction* open_ps_exe;

    QAction* open_tty_log;
    QAction* bios_calls;

    QAction* start_emu;
    QAction* stop_emu;
    QAction* pause_emu;
    QAction* reset_emu;

private:
    QImage* vram_image;
    QLabel* vram_image_view;

    QMenu* file_menu;
    QMenu* emulation_menu;
    QMenu* debug_menu;

    void on_open_ps_exe();

signals:
    void inject_ps_exe(const QString& exe_file);
};