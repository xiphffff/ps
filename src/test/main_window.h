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

#include <QMainWindow>
#include <QMenu>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow() noexcept;
    ~MainWindow() noexcept;

    // "File -> Insert game image..."
    QAction* insert_game_image;

    // "File -> Run EXE..."
    QAction* run_exe;

    // "Emulation -> Start"
    QAction* emulation_start;

    // "Emulation -> Stop"
    QAction* emulation_stop;

    // "Emulation -> Pause"
    QAction* emulation_pause;

    // "Emulation -> Reset"
    QAction* emulation_reset;

private:
    QMenu* file_menu;
    QMenu* emulation_menu;

signals:
    // A game image has been selected to be loaded by the user
    void selected_game_image(const QString& game_image_path);

    // An EXE has been selected to be ran by the user
    void selected_exe_file(const QString& exe_file_path);
};