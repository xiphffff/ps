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

#include "main_window.h"

MainWindow::MainWindow()
{
    vram_image = new QImage(1024, 512, QImage::Format_RGB555);
    vram_image->fill(Qt::black);

    vram_image_view = new QLabel(this);
    vram_image_view->setPixmap(QPixmap::fromImage(*vram_image));

    inject_ps_exe = new QAction(tr("Inject PS-X EXE..."), this);

    connect(inject_ps_exe, &QAction::triggered, this, &MainWindow::on_inject_ps_exe);

    display_tty_log = new QAction(tr("Display TTY Log"), this);

    display_bios_call_log = new QAction(tr("Display BIOS call log"), this);

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(inject_ps_exe);

    emulation_menu = menuBar()->addMenu(tr("&Emulation"));

    start_emu = new QAction(tr("Start"), this);
    stop_emu  = new QAction(tr("Stop"),  this);
    pause_emu = new QAction(tr("Pause"), this);
    reset_emu = new QAction(tr("Reset"), this);

    // The emulator is already running when we get here
    start_emu->setDisabled(true);

    emulation_menu->addAction(start_emu);
    emulation_menu->addAction(stop_emu);
    emulation_menu->addAction(pause_emu);
    emulation_menu->addAction(reset_emu);

    debug_menu = menuBar()->addMenu(tr("&Debug"));

    debug_menu->addAction(display_tty_log);
    debug_menu->addAction(display_bios_call_log);

    setWindowFlags(Qt::MSWindowsFixedSizeDialogHint);
    setCentralWidget(vram_image_view);
}

MainWindow::~MainWindow()
{ }

// Called when the user triggers "File -> Inject PS-X EXE..."
void MainWindow::on_inject_ps_exe()
{
    QString file_name = QFileDialog::getOpenFileName(this,
                                                     tr("Select PS-X EXE"),
                                                     "",
                                                     tr("PS-X EXEs (*.exe)"));

    if (!file_name.isEmpty())
    {
        emit selected_ps_x_exe(file_name);
    }
}

// Updates the VRAM image displayed.
void MainWindow::render_frame(const uint16_t* vram)
{
    memcpy(vram_image->bits(), vram, vram_image->sizeInBytes());

    // This is seriously awful.
    QImage img = vram_image->rgbSwapped();

    vram_image_view->setPixmap(QPixmap::fromImage(img));
}