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
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>
#include "emulator.h"
#include "main_window.h"
#include "pstest.h"
#include "renderer.h"
#include "../psemu/include/psemu.h"

PSTest::PSTest() noexcept : tty_log("tty_log.txt", this)
{
    QSettings config_file{ "pstest.ini", QSettings::IniFormat };

    auto bios_file{ config_file.value("files/bios").toString() };

    if (bios_file.isEmpty())
    {
        bool done = false;

        while (!done)
        {
            bios_file = QFileDialog::getOpenFileName
                        (nullptr,
                         tr("Select PlayStation BIOS"),
                         "",
                         tr("PlayStation BIOS files (*.bin)"));

            if (bios_file.isEmpty())
            {
                const auto response
                {
                    QMessageBox::critical(nullptr,
                                          tr("Error"),
                                          tr("psemu debugging station requires"
                                             " that you select a BIOS file."),
                                          QMessageBox::Retry |
                                          QMessageBox::Cancel)
                };

                switch (response)
                {
                    case QMessageBox::Retry:
                        continue;

                    case QMessageBox::Cancel:
                        // It seems safe to call `std::exit()` here, as
                        // `QApplication::exec()` hasn't been called yet and
                        // thus, `qApp::exit()` is not available.
                        std::exit(EXIT_FAILURE);
                }
            }
            else
            {
                config_file.setValue("files/bios", bios_file);
                done = true;
            }
        }
    }

    tty_log.open(QIODevice::WriteOnly);

    emulator = new Emulator(this, bios_file);

    connect(emulator, &Emulator::exception_raised, [&]
    {
        __debugbreak();
    });

    connect(emulator, &Emulator::std_out_putchar, [&](const char c)
    {
        QTextStream out(&tty_log);
        out << c;
    });

    connect(emulator, &Emulator::render_frame, [&]
    {
        renderer->update();
    });

    connect(emulator, &Emulator::system_error, [&]
    {
        
    });

    main_window = new MainWindow();

    connect(main_window->emulation_start, &QAction::triggered, [&]
    {
        main_window->emulation_start->setEnabled(false);
        main_window->emulation_stop->setEnabled(true);
        main_window->emulation_reset->setEnabled(true);
        main_window->emulation_pause->setEnabled(true);

        emulator->start_run_loop();
    });

    connect(main_window->emulation_stop, &QAction::triggered, [&]
    {
        main_window->emulation_start->setEnabled(true);
        main_window->emulation_stop->setEnabled(false);
        main_window->emulation_pause->setEnabled(false);
        main_window->emulation_reset->setEnabled(false);

        emulator->stop_run_loop();
    });

    connect(main_window->emulation_pause, &QAction::triggered, [&]
    {
        main_window->emulation_start->setText("Resume");
        main_window->emulation_start->setEnabled(true);

        main_window->emulation_stop->setEnabled(true);
        main_window->emulation_reset->setEnabled(true);
        main_window->emulation_pause->setEnabled(false);

        emulator->pause_run_loop();
    });

    connect(main_window->emulation_reset, &QAction::triggered, [&]
    {
        main_window->emulation_start->setEnabled(false);
        main_window->emulation_stop->setEnabled(true);
        main_window->emulation_pause->setEnabled(true);
        main_window->emulation_reset->setEnabled(true);

        emulator->restart_run_loop();
    });

    connect(main_window, &MainWindow::selected_game_image,
    [&](const QString& game_image)
    {
        emulator->insert_game_image(game_image);
    });

    connect(main_window, &MainWindow::selected_exe_file,
    [&](const QString& exe_file)
    {
        emulator->run_exe(exe_file);
    });

    main_window->setWindowTitle("psemu debugging station");
    main_window->resize(1024, 512);

    renderer = new Renderer(main_window, emulator->ps->bus.gpu.vram);

    main_window->setCentralWidget(renderer);
    main_window->show();
}

PSTest::~PSTest() noexcept
{ }