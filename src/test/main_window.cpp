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

#include <QFileDialog>
#include <QMenuBar>
#include "main_window.h"

MainWindow::MainWindow() noexcept : insert_game_image(new QAction(tr("Insert game image..."), this)),
                                    run_exe(new QAction(tr("Run EXE..."), this)),
                                    emulation_start(new QAction(tr("Start"), this)),
                                    emulation_stop(new QAction(tr("Stop"), this)),
                                    emulation_pause(new QAction(tr("Pause"), this)),
                                    emulation_reset(new QAction(tr("Reset"), this)),
                                    file_menu(menuBar()->addMenu(tr("&File"))),
                                    emulation_menu(menuBar()->addMenu(tr("&Emulation")))
{
    connect(insert_game_image, &QAction::triggered, [this]
    {
        const auto game_image
        {
            QFileDialog::getOpenFileName(this,
                                         tr("Select game image"),
                                         "",
                                         tr("Game images (*.bin)"))
        };

        if (!game_image.isEmpty())
        {
            emit selected_game_image(game_image);
        }
    });

    connect(run_exe, &QAction::triggered, [this]
    {
        const auto exe_file
        {
            QFileDialog::getOpenFileName(this,
                                         tr("Select PlayStation EXE"),
                                         "",
                                         tr("PlayStation executables (*.exe)"))
        };

        if (!exe_file.isEmpty())
        {
            emit selected_exe_file(exe_file);
        }
    });

    file_menu->addAction(insert_game_image);
    file_menu->addAction(run_exe);

    emulation_menu->addAction(emulation_start);
    emulation_menu->addAction(emulation_stop);
    emulation_menu->addAction(emulation_pause);
    emulation_menu->addAction(emulation_reset);

    emulation_stop->setDisabled(true);
    emulation_pause->setDisabled(true);
    emulation_reset->setDisabled(true);
}

MainWindow::~MainWindow() noexcept
{ }