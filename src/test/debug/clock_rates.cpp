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

#include "clock_rates.h"

ClockRates::ClockRates() : master_clock(new QDoubleSpinBox(this)),
                           cpu_clock(new QLabel(this)),
                           gpu_clock(new QLabel(this)),
                           spu_clock(new QLabel(this)),
                           frame_rate(new QLabel(this)),
                           widget(new QWidget(this)),
                           main_layout(new QFormLayout(widget))
{
    master_clock->setDecimals(4);
    master_clock->setMaximum(INT_MAX);
    master_clock->setSuffix(" MHz");

    connect(master_clock, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double value)
    {
        emit master_clock_changed(value);
    });

    main_layout->addRow(tr("Master clock:"), master_clock);
    main_layout->addRow(tr("CPU clock: "),   cpu_clock);
    main_layout->addRow(tr("GPU clock: "),   gpu_clock);
    main_layout->addRow(tr("SPU clock: "),   spu_clock);
    main_layout->addRow(tr("Frame rate: "),  frame_rate);

    setCentralWidget(widget);
}

ClockRates::~ClockRates()
{ }