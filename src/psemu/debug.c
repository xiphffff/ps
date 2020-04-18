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

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include "debug.h"

// The function to call when it is time to output a log message.
static void (*log_cb)(const char* const msg) = NULL;

// The function to call when it is time to output a log message. If `NULL` is
// passed, log messages are disabled.
void psemu_log_set_cb(void (*cb)(const char* const msg))
{
    log_cb = cb;
}

// Sends a message `msg` to the log callback function, if enabled.
void psemu_log(const char* const msg, ...)
{
    if (log_cb)
    {
        va_list args;
        char buf[256];
        va_start(args, msg);
        vsnprintf(buf, 256, msg, args);
        va_end(args);

        log_cb(buf);
    }
}