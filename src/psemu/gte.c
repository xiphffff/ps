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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include "gte.h"
#include "utility/math.h"

// Unsigned GTE limiters.
#define limA1U(gte, value) unsigned_lim(gte, value, 24, 32767)
#define limA2U(gte, value) unsigned_lim(gte, value, 23, 32767)
#define limA3U(gte, value) unsigned_lim(gte, value, 22, 32767)
#define limB1(gte, value) unsigned_lim(gte, value, 21, 255)
#define limB2(gte, value) unsigned_lim(gte, value, 20, 255)
#define limB3(gte, value) unsigned_lim(gte, value, 19, 255)
#define limC(gte, value) unsigned_lim(gte, value, 18, 65535)
#define limE(gte, value) unsigned_lim(gte, value, 12, 4095)

// Signed GTE limiters.
#define limA1S(gte, value) signed_lim(gte, value, 24, -32768, 32767)
#define limA2S(gte, value) signed_lim(gte, value, 23, -32768, 32767)
#define limA3S(gte, value) signed_lim(gte, value, 22, -32768, 32767)
#define limD1(gte, value) signed_lim(gte, value, 14, -1024, 1023)
#define limD2(gte, value) signed_lim(gte, value, 13, -1024, 1023)

// GTE limiters dependent on the `lim` argument.
#define limA1C(gte, value) lim(gte, value, 24)
#define limA2C(gte, value) lim(gte, value, 23)
#define limA3C(gte, value) lim(gte, value, 22)

struct gte_vector
{
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t* sz;
};

// Clamps `value` to `limit` if `value` has exceeded `limit`, and sets FLAG bit
// `bit` if so, and returns `value`.
static unsigned int unsigned_lim(struct psemu_cpu_gte* const gte,
                                 unsigned int value,
                                 const unsigned int bit,
                                 const unsigned int limit)
{
    assert(gte != NULL);
    assert(bit <= 31U);

    if (value > limit)
    {
        value = limit;
        gte->FLAG |= (1 << bit);
    }
    return value;
}

// Clamps `value` to `lower_limit` or `upper_limit` if `value` exceeds or is
// under either, and sets FLAG bit `bit`. Returns `value.`
static signed int signed_lim(struct psemu_cpu_gte* const gte,
                             signed int value,
                             const unsigned int bit,
                             const signed int lower_limit,
                             const unsigned upper_limit)
{
    assert(gte != NULL);
    assert(bit <= 31U);

    if (value < lower_limit)
    {
        value = lower_limit;
        gte->FLAG |= (1 << bit);
    }
    else if (value > upper_limit)
    {
        value = upper_limit;
        gte->FLAG |= (1 << bit);
    }
    return value;
}

// Clamps `value` in an unsigned or signed manner depending on the value of the
// `lm` argument in the instruction.
static int32_t lim(struct psemu_cpu_gte* const gte,
                   int32_t value,
                   const unsigned int bit)
{
    assert(gte != NULL);
    assert(bit <= 31U);

    return signed_lim(gte, value, bit, -32768, 32767);
}

// Handles perspective transformation.
static void rtp(struct psemu_cpu_gte* const gte,
                const struct gte_vector* const vec,
                const bool last)
{
    assert(gte != NULL);
    assert(vec != NULL);

    int64_t SSX = ((gte->R11R12.r11 * vec->x) +
                   (gte->R11R12.r12 * vec->y) +
                   (gte->R13R21.r13 * vec->z) + gte->TRX);
 
    int64_t SSY = ((gte->R13R21.r21 * vec->x) +
                   (gte->R22R23.r22 * vec->y) +
                   (gte->R22R23.r23 * vec->z) + gte->TRY);

    int64_t SSZ = ((gte->R31R32.r31 * vec->x) +
                   (gte->R31R32.r32 * vec->y) +
                   (gte->R33        * vec->z) + gte->TRZ);

    gte->SZ0 = gte->SZ1;
    gte->SZ1 = gte->SZ2;
    gte->SZ2 = gte->SZ3;
    gte->SZ3 = limC(gte, SSZ);

    // Unsigned Newton-Raphson (UNR) division table
    static const uint8_t division_table[257] =
    {
        0xFF,0xFD,0xFB,0xF9,0xF7,0xF5,0xF3,0xF1,0xEF,0xEE,0xEC,0xEA,0xE8,0xE6,
        0xE4,0xE3,0xE1,0xDF,0xDD,0xDC,0xDA,0xD8,0xD6,0xD5,0xD3,0xD1,0xD0,0xCE,
        0xCD,0xCB,0xC9,0xC8,0xC6,0xC5,0xC3,0xC1,0xC0,0xBE,0xBD,0xBB,0xBA,0xB8,
        0xB7,0xB5,0xB4,0xB2,0xB1,0xB0,0xAE,0xAD,0xAB,0xAA,0xA9,0xA7,0xA6,0xA4,
        0xA3,0xA2,0xA0,0x9F,0x9E,0x9C,0x9B,0x9A,0x99,0x97,0x96,0x95,0x94,0x92,
        0x91,0x90,0x8F,0x8D,0x8C,0x8B,0x8A,0x89,0x87,0x86,0x85,0x84,0x83,0x82,
        0x81,0x7F,0x7E,0x7D,0x7C,0x7B,0x7A,0x79,0x78,0x77,0x75,0x74,0x73,0x72,
        0x71,0x70,0x6F,0x6E,0x6D,0x6C,0x6B,0x6A,0x69,0x68,0x67,0x66,0x65,0x64,
        0x63,0x62,0x61,0x60,0x5F,0x5E,0x5D,0x5D,0x5C,0x5B,0x5A,0x59,0x58,0x57,
        0x56,0x55,0x54,0x53,0x53,0x52,0x51,0x50,0x4F,0x4E,0x4D,0x4D,0x4C,0x4B,
        0x4A,0x49,0x48,0x48,0x47,0x46,0x45,0x44,0x43,0x43,0x42,0x41,0x40,0x3F,
        0x3F,0x3E,0x3D,0x3C,0x3C,0x3B,0x3A,0x39,0x39,0x38,0x37,0x36,0x36,0x35,
        0x34,0x33,0x33,0x32,0x31,0x31,0x30,0x2F,0x2E,0x2E,0x2D,0x2C,0x2C,0x2B,
        0x2A,0x2A,0x29,0x28,0x28,0x27,0x26,0x26,0x25,0x24,0x24,0x23,0x22,0x22,
        0x21,0x20,0x20,0x1F,0x1E,0x1E,0x1D,0x1D,0x1C,0x1B,0x1B,0x1A,0x19,0x19,
        0x18,0x18,0x17,0x16,0x16,0x15,0x15,0x14,0x14,0x13,0x12,0x12,0x11,0x11,
        0x10,0x0F,0x0F,0x0E,0x0E,0x0D,0x0D,0x0C,0x0C,0x0B,0x0A,0x0A,0x09,0x09,
        0x08,0x08,0x07,0x07,0x06,0x06,0x05,0x05,0x04,0x04,0x03,0x03,0x02,0x02,
        0x01,0x01,0x00,0x00,0x00
    };

    int div_result;

    if (gte->H < (gte->SZ3 * 2))
    {
        int z = __builtin_clz(gte->SZ3);
        div_result = (gte->H << z);
        int d = (gte->SZ3 << z);
        uint8_t u = division_table[(d - 0x7FC0) >> 7] + 0x101;
        d = ((0x2000080 - (d * u)) >> 8);
        d = ((0x0000080 + (d * u)) >> 8);
        div_result = psemu_min(0x1FFFF, (((div_result * d) + 0x8000) >> 16));
    }
    else
    {
        div_result = 0x1FFFF;
    }

    int64_t SX = gte->OFX + (gte->IR1 * div_result);
    int64_t SY = gte->OFY + (gte->IR2 * div_result);
    int64_t P  = gte->DQB + (gte->DQA * div_result);

    gte->IR0 = limE(gte, P);

    gte->SXY0.x = gte->SXY1.x;
    gte->SXY1.x = gte->SXY2.x;
    gte->SXY2.x = limD1(gte, SX);

    gte->SXY0.y = gte->SXY1.y;
    gte->SXY1.y = gte->SXY2.y;
    gte->SXY2.y = limD2(gte, SY);

    if (last)
    {
        gte->IR1 = limA1S(gte, SSX);
        gte->IR2 = limA2S(gte, SSY);
        gte->IR3 = limA3S(gte, SSZ);

        gte->MAC0 = P;
        gte->MAC1 = SSX;
        gte->MAC2 = SSY;
        gte->MAC3 = SSZ;
    }
}

// Handles light source calculation.
static void ncd(struct psemu_cpu_gte* const gte,
                const struct gte_vector* const vec,
                const bool last)
{
    assert(gte != NULL);
    assert(vec != NULL);

    const uint16_t LLM1 = limA1U(gte, (gte->L11L12.l11 * vec->x) +
                                      (gte->L11L12.l12 * vec->y) +
                                      (gte->L13L21.l13 * vec->z));

    const uint16_t LLM2 = limA2U(gte, (gte->L13L21.l21 * vec->x) +
                                      (gte->L22L23.l22 * vec->y) +
                                      (gte->L22L23.l23 * vec->z));

    const uint16_t LLM3 = limA3U(gte, (gte->L31L32.l31 * vec->x) +
                                      (gte->L31L32.l32 * vec->y) +
                                      (gte->L33        * vec->z));

    const uint16_t RRLT = limA1U(gte, (gte->LR1LR2.lr1 * LLM1) +
                                      (gte->LR1LR2.lr2 * LLM2) +
                                      (gte->LR3LG1.lr3 * LLM3) + gte->RBK) *
                                      gte->RGB.r;

    const uint16_t GGLT = limA2U(gte, (gte->LR3LG1.lg1 * LLM1) +
                                      (gte->LG2LG3.lg2 * LLM2) +
                                      (gte->LG2LG3.lg3 * LLM3) + gte->GBK) *
                                      gte->RGB.g;

    const uint16_t BBLT = limA3U(gte, (gte->LB1LB2.lb1 * LLM1) +
                                      (gte->LB1LB2.lb2 * LLM2) +
                                      (gte->LB3        * LLM3) + gte->BBK) *
                                      gte->RGB.b;

    const int32_t RR0 = RRLT + (gte->IR0 * limA1S(gte, (gte->RFC - RRLT)));
    const int32_t GG0 = GGLT + (gte->IR0 * limA1S(gte, (gte->GFC - GGLT)));
    const int32_t BB0 = BBLT + (gte->IR0 * limA1S(gte, (gte->BFC - BBLT)));

    gte->RGB0.cd0 = gte->RGB1.cd1;
    gte->RGB1.cd1 = gte->RGB2.cd2;
    gte->RGB2.cd2 = gte->RGB.code;

    gte->RGB0.r0 = gte->RGB1.r1;
    gte->RGB1.r1 = gte->RGB2.r2;
    gte->RGB2.r2 = limB1(gte, RR0);

    gte->RGB0.g0 = gte->RGB1.g1;
    gte->RGB1.g1 = gte->RGB2.g2;
    gte->RGB2.g2 = limB2(gte, GG0);

    gte->RGB0.b0 = gte->RGB1.b1;
    gte->RGB1.b1 = gte->RGB2.b2;
    gte->RGB2.b2 = limB3(gte, BB0);

    if (last)
    {
        gte->IR1 = limA1U(gte, RR0);
        gte->IR2 = limA2U(gte, GG0);
        gte->IR3 = limA3U(gte, BB0);

        gte->MAC1 = RR0;
        gte->MAC2 = GG0;
        gte->MAC3 = BB0;
    }
}

// Handles the `nclip` instruction.
void psemu_cpu_gte_nclip(struct psemu_cpu_gte* const gte)
{
    assert(gte != NULL);

    gte->MAC0 = ((gte->SXY0.x * gte->SXY1.y) +
                 (gte->SXY1.x * gte->SXY2.y) +
                 (gte->SXY2.x * gte->SXY0.y) -
                 (gte->SXY0.x * gte->SXY2.y) -
                 (gte->SXY1.x * gte->SXY0.y) -
                 (gte->SXY2.x * gte->SXY1.y));
}

// Handles the `ncds` instruction.
void psemu_cpu_gte_ncds(struct psemu_cpu_gte* const gte)
{
    assert(gte != NULL);

    const struct gte_vector vec0 =
    {
        .x  = gte->VXY0.x,
        .y  = gte->VXY0.y,
        .z  = gte->VZ0
    };
    ncd(gte, &vec0, true);
}

// Handles the `avsz3` instruction.
void psemu_cpu_gte_avsz3(struct psemu_cpu_gte* const gte)
{
    assert(gte != NULL);

    gte->MAC0 = gte->ZSF3 * (gte->SZ1 + gte->SZ2 + gte->SZ3);
    gte->OTZ  = limC(gte, gte->MAC0);
}

// Handles the `rtpt` instruction.
void psemu_cpu_gte_rtpt(struct psemu_cpu_gte* const gte)
{
    assert(gte != NULL);

    const struct gte_vector vec0 =
    {
        .x  = gte->VXY0.x,
        .y  = gte->VXY0.y,
        .z  = gte->VZ0,
        .sz = &gte->SZ0
    };

    const struct gte_vector vec1 =
    {
        .x  = gte->VXY1.x,
        .y  = gte->VXY1.y,
        .z  = gte->VZ1,
        .sz = &gte->SZ1
    };

    const struct gte_vector vec2 =
    {
        .x  = gte->VXY2.x,
        .y  = gte->VXY2.y,
        .z  = gte->VZ2,
        .sz = &gte->SZ2
    };

    rtp(gte, &vec0, false);
    rtp(gte, &vec1, false);
    rtp(gte, &vec2, true);
}