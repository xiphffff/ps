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

class TTYLogger : public QMainWindow
{
    Q_OBJECT

public:
    TTYLogger(QWidget* parent);
    ~TTYLogger();

    void append(const QString& data);
    void clear_log();

private:
    void on_select_font();
    void on_save_log();

    QList<QString> breakpoints;

    QMenu* file_menu;
    QMenu* view_menu;

    QAction* save_log;
    QAction* clear;
    QAction* select_font;

    QPlainTextEdit* text_edit;
};