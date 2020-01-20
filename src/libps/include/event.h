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

#pragma once

#ifdef LIBPS_DEBUG

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

void libps_ev_unknown_word_load(const uint32_t paddr);
void libps_ev_unknown_halfword_load(const uint32_t paddr);
void libps_ev_unknown_byte_load(const uint32_t paddr);

void libps_ev_unknown_word_store(const uint32_t paddr, const uint32_t data);
void libps_ev_unknown_halfword_store(const uint32_t paddr, const uint16_t data);
void libps_ev_unknown_byte_store(const uint32_t paddr, const uint8_t data);

void libps_ev_dma_otc_unknown(const uint32_t chcr);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIBPS_DEBUG