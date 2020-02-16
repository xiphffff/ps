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

#include "log.h"

MessageLogger::MessageLogger(QWidget* parent) : QMainWindow(parent)
{
    file_menu = menuBar()->addMenu(tr("&File"));

    save_log = new QAction(tr("&Save..."), this);

    connect(save_log,
            &QAction::triggered,
            this,
            &MessageLogger::on_save_log);

    file_menu->addAction(save_log);

    view_menu = menuBar()->addMenu(tr("&View"));

    select_font = new QAction(tr("&Font..."), this);
    clear_log   = new QAction(tr("&Clear"),   this);

    connect(select_font,
            &QAction::triggered,
            this,
            &MessageLogger::on_select_font);

    connect(clear_log, &QAction::triggered, this, &MessageLogger::reset);

    view_menu->addAction(select_font);
    view_menu->addAction(clear_log);

    events_menu = menuBar()->addMenu(tr("&Events"));

    tty_strings = new QAction(tr("TTY output"), this);
    bios_calls  = new QAction(tr("BIOS calls"), this);

#ifdef LIBPS_DEBUG
    unknown_memory_load  = new QAction(tr("Unknown memory load"),  this);
    unknown_memory_store = new QAction(tr("Unknown memory store"), this);
    irqs                 = new QAction(tr("IRQs"),                 this);
#endif // LIBPS_DEBUG

    tty_strings->setCheckable(true);
    bios_calls->setCheckable(true);

#ifdef LIBPS_DEBUG
    unknown_memory_load->setCheckable(true);
    unknown_memory_store->setCheckable(true);
    irqs->setCheckable(true);
#endif // LIBPS_DEBUG

    events_menu->addAction(tty_strings);
    events_menu->addAction(bios_calls);

#ifdef LIBPS_DEBUG
    events_menu->addAction(unknown_memory_load);
    events_menu->addAction(unknown_memory_store);
    events_menu->addAction(irqs);
#endif // LIBPS_DEBUG

    text_edit = new QPlainTextEdit(this);
    text_edit->setReadOnly(true);

    QSettings config_file("pstest.ini", QSettings::IniFormat, this);

    const QString font_name = config_file.value("message_logger/font_name",
                                                "Lucida Console").toString();

    const int font_size = config_file.value("message_logger/font_size",
                                            10).toInt();
    QFont font(font_name, font_size);

    QTextDocument* doc = text_edit->document();
    doc->setDefaultFont(font);

    setCentralWidget(text_edit);
}

MessageLogger::~MessageLogger()
{ }

void MessageLogger::append(const QString& data)
{
    text_edit->insertPlainText(data);
}

void MessageLogger::reset()
{
    text_edit->clear();
}

void MessageLogger::on_select_font()
{
    bool ok;

    QFont font = QFontDialog::getFont(&ok,
                                      QFont("Lucida Console", 10),
                                      this);
    if (ok)
    {
        QTextDocument* doc = text_edit->document();
        doc->setDefaultFont(font);

        QSettings config_file("pstest.ini", QSettings::IniFormat, this);

        config_file.setValue("message_logger/font_name", font.toString());
        config_file.setValue("message_logger/font_size", font.pointSize());
    }
}

// Called when the user triggers "File -> Save" to save the log contents.
void MessageLogger::on_save_log()
{
    QString file_name = QFileDialog::getSaveFileName(this,
                                                     tr("Save log"),
                                                     "",
                                                     tr("Log files (*.txt)"));

    if (!file_name.isEmpty())
    {
        QFile log_file(file_name);

        log_file.open(QIODevice::WriteOnly | QIODevice::Text);

        QTextStream out(&log_file);
        out << text_edit->toPlainText();

        log_file.close();
    }
}