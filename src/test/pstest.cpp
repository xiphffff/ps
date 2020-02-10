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
#include "../libps/include/ps.h"

PSTest::PSTest() : bios_calls(nullptr)
{
    goto staging;

staging:
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
                    goto staging;
                    break;

                case QMessageBox::Cancel:
                    exit(EXIT_FAILURE);
            }
        }
        else
        {
            config_file.setValue("files/bios", bios_file);
        }
    }

    main_window = new MainWindow();
    main_window->setAttribute(Qt::WA_QuitOnClose);

    emulator = new Emulator(this, bios_file);

    connect(emulator, &Emulator::finished,     emulator,    &QObject::deleteLater);
    connect(emulator, &Emulator::render_frame, main_window, &MainWindow::render_frame);
    connect(emulator, &Emulator::system_error, this,        &PSTest::emu_report_system_error);
    connect(emulator, &Emulator::bios_call,    this,        &PSTest::emu_bios_call);

#ifdef LIBPS_DEBUG
    connect(emulator, &Emulator::on_debug_unknown_byte_load,            this, &PSTest::on_debug_unknown_byte_load);
    connect(emulator, &Emulator::on_debug_unknown_halfword_load,        this, &PSTest::on_debug_unknown_halfword_load);
    connect(emulator, &Emulator::on_debug_unknown_word_load,            this, &PSTest::on_debug_unknown_word_load);
    connect(emulator, &Emulator::on_debug_unknown_byte_store,           this, &PSTest::on_debug_unknown_byte_store);
    connect(emulator, &Emulator::on_debug_unknown_halfword_store,       this, &PSTest::on_debug_unknown_halfword_store);
    connect(emulator, &Emulator::on_debug_unknown_word_store,           this, &PSTest::on_debug_unknown_word_store);
    connect(emulator, &Emulator::on_debug_unknown_dma_otc_channel_chcr, this, &PSTest::on_debug_unknown_dma_otc_channel_chcr);
#endif // LIBPS_DEBUG

    // "File" menu
    connect(main_window, &MainWindow::selected_ps_x_exe, emulator, &Emulator::inject_ps_exe);

    // "Emulation" menu
    connect(main_window->start_emu, &QAction::triggered, this, &PSTest::start_emu);
    connect(main_window->stop_emu,  &QAction::triggered, this, &PSTest::stop_emu);
    connect(main_window->reset_emu, &QAction::triggered, this, &PSTest::reset_emu);
    connect(main_window->pause_emu, &QAction::triggered, this, &PSTest::pause_emu);

    // "Debug" menu
    connect(main_window->display_tty_log, &QAction::triggered, this, &PSTest::display_tty_log);
#ifdef LIBPS_DEBUG
    connect(main_window->display_libps_log, &QAction::triggered, this, &PSTest::display_libps_log);
#endif // LIBPS_DEBUG
    connect(main_window->display_bios_call_log, &QAction::triggered, this, &PSTest::display_bios_call_log);

    main_window->setWindowTitle("libps debugging station");
    main_window->resize(1024, 512);

    main_window->show();
}

PSTest::~PSTest()
{ }

// Called when the emulator core reports that BIOS call
// `A(0x40) - SystemErrorUnresolvedException()` was reached.
void PSTest::emu_report_system_error()
{
    QMessageBox::critical(main_window,
                          tr("Emulation failure"),
                          tr("A($40): SystemErrorUnresolvedException() "
                             "reached. Emulation halted."));
}

// Called when the emulator core reports that a BIOS call other than
// A(0x40), A(0x3C), or B(0x3D) was reached.
void PSTest::emu_bios_call(const quint32 pc, const quint32 fn)
{
    if (bios_calls)
    {
        bios_calls->add(pc, fn);
    }
}

// Called when the user clicks `Debug -> Display TTY Log`.
void PSTest::display_tty_log()
{
    tty_logger = new MessageLogger(main_window, "tty_logger");
    tty_logger->setAttribute(Qt::WA_DeleteOnClose);

    connect(emulator,
            &Emulator::tty_string,
            tty_logger,
            &MessageLogger::append);

    tty_logger->setWindowTitle(tr("libps TTY log"));
    tty_logger->resize(500, 500);

    tty_logger->show();
}

// Called when the user triggers `Debug -> Display BIOS call log`.
void PSTest::display_bios_call_log()
{
    bios_calls = new BIOSCalls();

    bios_calls->resize(640, 480);
    bios_calls->setWindowTitle(tr("libps BIOS call log"));

    bios_calls->show();
}

#ifdef LIBPS_DEBUG
void PSTest::display_libps_log()
{
    libps_log = new LibraryLogger(main_window);
    libps_log->setAttribute(Qt::WA_DeleteOnClose);

    libps_log->setWindowTitle(tr("libps log"));
    libps_log->resize(500, 500);

    libps_log->show();
}
#endif // LIBPS_DEBUG

// Called when the user triggers `Emulation -> Start`. This function is also
// called upon startup, and is used also to resume emulation from a paused
// state.
void PSTest::start_emu()
{
    if (main_window->start_emu->text() == tr("Resume"))
    {
        main_window->start_emu->setText(tr("Start"));
    }

    main_window->start_emu->setDisabled(true);
    main_window->stop_emu->setEnabled(true);
    main_window->pause_emu->setEnabled(true);

    emulator->begin_run_loop();
}

// Called when the user triggers `Emulation -> Stop`.
void PSTest::stop_emu()
{
    emulator->stop_run_loop();

    main_window->start_emu->setEnabled(true);
    main_window->stop_emu->setDisabled(true);
    main_window->pause_emu->setDisabled(true);
    main_window->reset_emu->setDisabled(true);
}

// Called when the user triggers `Emulation -> Pause`.
void PSTest::pause_emu()
{
    emulator->pause_run_loop();

    main_window->start_emu->setText(tr("Resume"));

    main_window->pause_emu->setDisabled(true);
    main_window->start_emu->setEnabled(true);
    main_window->reset_emu->setEnabled(true);
}

// Called when the user triggers `Emulation -> Reset`.
void PSTest::reset_emu()
{
    if (tty_logger)
    {
        tty_logger->clear_log();
    }

    emulator->stop_run_loop();
    emulator->begin_run_loop();
}

#ifdef LIBPS_DEBUG
// Called when an unknown word load has been attempted
void PSTest::on_debug_unknown_word_load(const uint32_t paddr)
{
    if (libps_log && libps_log->unknown_word_load->isChecked())
    {
        libps_log->append(QString("Unknown word load: 0x%1\n").arg(QString::number(paddr, 16).toUpper()));
    }
}

// Called when an unknown halfword load has been attempted
void PSTest::on_debug_unknown_halfword_load(const uint32_t paddr)
{
    if (libps_log && libps_log->unknown_halfword_load->isChecked())
    {
        libps_log->append(QString("Unknown halfword load: 0x%1\n").arg(QString::number(paddr, 16).toUpper()));
    }
}

// Called when an unknown byte load has been attempted
void PSTest::on_debug_unknown_byte_load(const uint32_t paddr)
{
    if (libps_log && libps_log->unknown_byte_load->isChecked())
    {
        libps_log->append(QString("Unknown byte load: 0x%1\n").arg(QString::number(paddr, 16).toUpper()));
    }
}

// Called when an unknown word store has been attempted
void PSTest::on_debug_unknown_word_store(const uint32_t paddr,
                                         const uint32_t data)
{
    if (libps_log && libps_log->unknown_word_store->isChecked())
    {
        libps_log->append(QString("Unknown word store: 0x%1 <- 0x%2\n")
                          .arg(QString::number(paddr, 16).toUpper())
                          .arg(QString::number(data, 16).toUpper()));
    }
}

// Called when an unknown halfword store has been attempted
void PSTest::on_debug_unknown_halfword_store(const uint32_t paddr,
                                             const uint16_t data)
{
    if (libps_log && libps_log->unknown_halfword_store->isChecked())
    {
        libps_log->append(QString("Unknown halfword store: 0x%1 <- 0x%2\n")
                         .arg(QString::number(paddr, 16).toUpper())
                         .arg(QString::number(data, 16).toUpper()));
    }
}

// Called when an unknown byte store has been attempted
void PSTest::on_debug_unknown_byte_store(const uint32_t paddr,
                                         const uint8_t data)
{
    if (libps_log && libps_log->unknown_byte_store->isChecked())
    {
        libps_log->append(QString("Unknown byte store: 0x%1 <- 0x%2\n")
                          .arg(QString::number(paddr, 16).toUpper())
                          .arg(QString::number(data, 16).toUpper()));
    }
}

// Called when DMA6 CHCR is not known
void PSTest::on_debug_unknown_dma_otc_channel_chcr(const uint32_t chcr)
{
    if (libps_log && libps_log->unknown_dma_otc_channel_chcr->isChecked())
    {
        libps_log->append(QString("Unknown DMA6 OTC: Expected 0x11000002, got 0x%1\n")
                          .arg(QString::number(chcr, 16).toUpper()));
    }
}
#endif // LIBPS_DEBUG