// Copyright 2020 Michael Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright noticeand this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "bus.h"
#include "cpu.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
// Defines the structure of a Sony PlayStation® system.
struct psemu_system
{
	// System bus ("interconnect")
	struct psemu_bus bus;

	// LSI LR33300 interpreter
	struct psemu_cpu cpu;
};

// Creates a Sony PlayStation® system emulator.
struct psemu_system* psemu_create(uint8_t* const bios_data);

// Destroys a Sony PlayStation® system emulator `ps_emu`.
void psemu_destroy(struct psemu_system* const ps_emu);

// Resets a Sony PlayStation® system emulator `ps_emu` to the startup state.
void psemu_reset(struct psemu_system* const ps_emu);

// Executes one full system step on a Sony PlayStation® system emulator
// `ps_emu`.
void psemu_step(struct psemu_system* const ps_emu);

// Notifies the system that the V-Blank interrupt should be triggered. Call
// this function once per frame.
void psemu_vblank(struct psemu_system* const ps_emu);
#ifdef __cplusplus
}
#endif // __cplusplus