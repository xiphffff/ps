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

#include "pstest.h"

PSTest::PSTest()
{
    QString bios_file = handle_initial_bios_select();

    emulator = new Emulator(bios_file);

    main_window = new MainWindow();

    connect(main_window, &MainWindow::inject_ps_exe, emulator, &Emulator::inject_ps_exe);
    connect(main_window->open_tty_log, &QAction::triggered, this, &PSTest::open_tty_log);

    main_window->setWindowTitle("libps debugging station");
    main_window->resize(1024, 512);

    connect(emulator, &Emulator::finished, emulator, &QObject::deleteLater);
    connect(emulator, &Emulator::render_frame, main_window, &MainWindow::render_frame);

    main_window->show();

    emulator->begin_run_loop();
    emulator->start();
}

PSTest::~PSTest()
{
    emu_thread->quit();
    emu_thread->wait();
}


void PSTest::open_tty_log()
{
    tty_logger = new TTYLogger();

    connect(emulator, &Emulator::tty_string, tty_logger, &TTYLogger::append);

    tty_logger->setWindowTitle(tr("libps TTY log"));
    tty_logger->resize(500, 500);

    tty_logger->show();
}

QString PSTest::handle_initial_bios_select()
{
    QSettings config_file("pstest.ini", QSettings::IniFormat, this);

    QString bios_file = config_file.value("files/bios").toString();

    if (bios_file.isEmpty())
    {
        bios_file = QFileDialog::getOpenFileName(nullptr,
            tr("Select PlayStation BIOS"),
            "",
            tr("PlayStation BIOS files (*.bin)"));

        if (bios_file.isEmpty())
        {
            const int result =
            QMessageBox::warning(nullptr,
                                 tr("BIOS file not selected"),
                                 tr("libps debugging station requires that you"
                                    " provide a BIOS file, preferably SCPH-100"
                                    "1."),
                                 QMessageBox::Retry,
                                 QMessageBox::Cancel);

            switch (result)
            {
                case QMessageBox::Retry:
                    handle_initial_bios_select();
                    break;

                case QMessageBox::Cancel:
                default:
                    qApp->quit();
                    break;
            }
        }
        else
        {
            config_file.setValue("files/bios", bios_file);
            return bios_file;
        }
    }
    else
    {
        return bios_file;
    }
    return nullptr;
}