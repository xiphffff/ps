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

#include <array>
#include "pstest.h"
#include "../libps/include/ps.h"

PSTest::PSTest()
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
    connect(emulator, &Emulator::on_debug_unknown_memory_load,    this, &PSTest::on_debug_unknown_memory_load);
    connect(emulator, &Emulator::on_debug_unknown_memory_store,   this, &PSTest::on_debug_unknown_memory_store);
    connect(emulator, &Emulator::on_debug_interrupt_requested,    this, &PSTest::on_debug_interrupt_requested);
    connect(emulator, &Emulator::on_debug_interrupt_acknowledged, this, &PSTest::on_debug_interrupt_acknowledged);
#endif // LIBPS_DEBUG

    // "File" menu
    connect(main_window, &MainWindow::selected_ps_x_exe, emulator, &Emulator::set_injection);

    // "Emulation" menu
    connect(main_window->start_emu, &QAction::triggered, this, &PSTest::start_emu);
    connect(main_window->stop_emu,  &QAction::triggered, this, &PSTest::stop_emu);
    connect(main_window->reset_emu, &QAction::triggered, this, &PSTest::reset_emu);
    connect(main_window->pause_emu, &QAction::triggered, this, &PSTest::pause_emu);

    // "Debug" menu
    connect(main_window->display_libps_log, &QAction::triggered, this, &PSTest::display_libps_log);

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
void PSTest::emu_bios_call(struct bios_trace_info* bios_trace)
{
    if (libps_log && libps_log->bios_calls->isChecked())
    {
        libps_log->append(QString("[BIOS CALL]: %1(%2)\n")
                          .arg(QString::number(bios_trace->origin, 16).toUpper())
                          .arg(QString::number(bios_trace->func, 16).toUpper()));
    }
}

void PSTest::display_libps_log()
{
    libps_log = new MessageLogger(main_window);
    libps_log->setAttribute(Qt::WA_DeleteOnClose);

    connect(emulator,
            &Emulator::tty_string,
            this,
            &PSTest::on_tty_string);

    libps_log->setWindowTitle(tr("libps log"));
    libps_log->resize(500, 500);

    libps_log->show();
}

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
    if (libps_log)
    {
        libps_log->reset();
    }

    emulator->stop_run_loop();
    emulator->begin_run_loop();
}

// Called when a TTY string has been generated
void PSTest::on_tty_string(const QString& tty_string)
{
    if (libps_log && libps_log->tty_strings->isChecked())
    {
        libps_log->append("[TTY]: " + tty_string);
    }
}

#ifdef LIBPS_DEBUG
// Called when an unknown memory load has been attempted
void PSTest::on_debug_unknown_memory_load(const uint32_t paddr,
                                          const unsigned int type)
{
    if (libps_log && libps_log->unknown_memory_load->isChecked())
    {
        QString m_type;

        switch (type)
        {
            case LIBPS_DEBUG_WORD:
                m_type = "word";
                break;

            case LIBPS_DEBUG_HALFWORD:
                m_type = "halfword";
                break;

            case LIBPS_DEBUG_BYTE:
                m_type = "byte";
                break;
        }

        libps_log->append(QString("Unknown %1 load: 0x%2\n")
                          .arg(m_type)
                          .arg(QString::number(paddr, 16).toUpper()));
    }
}

// Called when an unknown word store has been attempted
void PSTest::on_debug_unknown_memory_store(const uint32_t paddr,
                                           const unsigned int data,
                                           const unsigned int type)
{
    if (libps_log && libps_log->unknown_memory_store->isChecked())
    {
        QString m_type;
        QString result;

        switch (type)
        {
            case LIBPS_DEBUG_WORD:
                m_type = "word";
                result = QString("%1").arg(data & type, 8, 16, QChar('0')).toUpper();

                break;

            case LIBPS_DEBUG_HALFWORD:
                m_type = "halfword";
                result = QString("%1").arg(data & type, 4, 16, QChar('0')).toUpper();

                break;

            case LIBPS_DEBUG_BYTE:
                m_type = "byte";
                result = QString("%1").arg(data & type, 2, 16, QChar('0')).toUpper();

                break;
        }

        QString str = QString("Unknown %1 store: 0x%2 <- 0x%3\n")
                      .arg(m_type)
                      .arg(QString::number(paddr, 16).toUpper())
                      .arg(result);

        libps_log->append(str);
    }
}

void PSTest::on_debug_interrupt_requested(const unsigned int interrupt)
{
    static const std::array<const QString, 16> irq_as_string =
    {
        "IRQ0 VBLANK",
        "IRQ1 GPU",
        "IRQ2 CDROM",
        "IRQ3 DMA",
        "IRQ4 TMR0",
        "IRQ5 TMR1",
        "IRQ6 TMR2",
        "IRQ7 Controller and Memory Card",
        "IRQ8 SIO",
        "IRQ9 SPU",
        "IRQ10 Controller"
    };

    if (libps_log && libps_log->irqs->isChecked())
    {
        libps_log->append(QString("[total_cycles=%1], %2 requested\n")
                          .arg(QString::number(emulator->total_cycles, 10))
                          .arg(irq_as_string[interrupt]));
    }
}

void PSTest::on_debug_interrupt_acknowledged(const unsigned int interrupt)
{
    static const std::array<const QString, 16> irq_as_string =
    {
        "IRQ0 VBLANK",
        "IRQ1 GPU",
        "IRQ2 CDROM",
        "IRQ3 DMA",
        "IRQ4 TMR0",
        "IRQ5 TMR1",
        "IRQ6 TMR2",
        "IRQ7 Controller and Memory Card",
        "IRQ8 SIO",
        "IRQ9 SPU",
        "IRQ10 Controller"
    };

    if (libps_log && libps_log->irqs->isChecked())
    {
        libps_log->append(QString("[total_cycles=%1], %2 acknowledged\n")
                          .arg(QString::number(emulator->total_cycles, 10))
                          .arg(irq_as_string[interrupt]));
    }
}

#endif // LIBPS_DEBUG