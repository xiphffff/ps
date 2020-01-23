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

#include "tty_log.h"

TTYLogger::TTYLogger(QWidget* parent) : QMainWindow(parent)
{
    save_log    = new QAction(tr("&Save..."), this);
    select_font = new QAction(tr("&Font..."), this);

    clear = new QAction(tr("&Clear"), this);

    connect(save_log,
            &QAction::triggered,
            this,
            &TTYLogger::on_save_log);

    connect(clear, &QAction::triggered, this, &TTYLogger::clear_log);

    connect(select_font,
            &QAction::triggered,
            this,
            &TTYLogger::on_select_font);

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(save_log);
    
    view_menu = menuBar()->addMenu(tr("&View"));

    view_menu->addAction(clear);
    view_menu->addAction(select_font);

    text_edit = new QPlainTextEdit(this);
    text_edit->setReadOnly(true);

    QSettings config_file("pstest.ini", QSettings::IniFormat, this);

    const QString font_name = config_file.value("tty_viewer/font_name",
                                                "Lucida Console").toString();

    const int font_size = config_file.value("tty_viewer/font_size",
                                            10).toInt();


    QFont font(font_name, font_size);

    QTextDocument* doc = text_edit->document();
    doc->setDefaultFont(font);

    setCentralWidget(text_edit);
}

TTYLogger::~TTYLogger()
{ }

void TTYLogger::append(const QString& data)
{
    text_edit->moveCursor(QTextCursor::End);
    text_edit->insertPlainText(data);
    text_edit->moveCursor(QTextCursor::End);
}

void TTYLogger::clear_log()
{
    text_edit->clear();
}

void TTYLogger::on_select_font()
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

        config_file.setValue("tty_viewer/font_name", font.toString());
        config_file.setValue("tty_viewer/font_size", font.pointSize());
    }
}

// Called when the user triggers "File -> Save" to save the log contents.
void TTYLogger::on_save_log()
{
    QString file_name = QFileDialog::getSaveFileName(this,
                                                     tr("Save TTY Log"),
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