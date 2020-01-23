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

#include "bios_calls.h"

BIOSCalls::BIOSCalls() : threshold(1),
                         widget(new QWidget(this)),
                         threshold_specifier(new QSpinBox(this)),
                         calls(new QTreeWidget(widget)),
                         threshold_layout(new QFormLayout()),
                         widget_layout(new QHBoxLayout(widget))
{
    calls->setHeaderLabels(QStringList() <<
                           "Calling PC" << "Function" << "Return Value");

    threshold_specifier->setMinimum(1);
    threshold_specifier->setMaximum(100);

    threshold_layout->addRow(tr("Maximum number of calls:"), threshold_specifier);

    connect(threshold_specifier,
            QOverload<int>::of(&QSpinBox::valueChanged),
            [&](const int value)
    {
        threshold = value;
    });

    widget_layout->addWidget(calls);
    widget_layout->addLayout(threshold_layout);

    setCentralWidget(widget);
}

BIOSCalls::~BIOSCalls()
{ }

void BIOSCalls::add(const uint32_t pc, const uint32_t fn)
{
    if (call_list.size() != threshold)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(calls);
        item->setText(0, QString::number(pc, 16));
        item->setText(1, QString::number(fn, 16));

        calls->addTopLevelItem(item);
        call_list.push_back(item);
    }
}