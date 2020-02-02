// Copyright 2019 Michael Rodriguez
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

// Implements the first controller ever released for the PlayStation
// (SCPH-1010).

#include "scph1010.h"
#include "../utility/memory.h"

// Creates a SCPH-1010.
struct libps_scph1010* libps_scph1010_create(struct libps_system* ps)
{
    struct libps_scph1010* controller =
    libps_safe_malloc(sizeof(struct libps_scph1010));

    return controller;
}

// Destroys the SCPH-1010.
void libps_scph1010_destroy(struct libps_scph1010* controller)
{
    libps_safe_free(controller);
}