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

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>

// Geometry Transformation Engine (GTE/COP2) instructions
#define PSEMU_CPU_OP_NCLIP 0x06
#define PSEMU_CPU_OP_NCDS 0x13
#define PSEMU_CPU_OP_AVSZ3 0x2D
#define PSEMU_CPU_OP_RTPT 0x30

// Geometry Transformation Engine (GTE/COP2) data registers
#define PSEMU_CPU_COP2_VXY0 0
#define PSEMU_CPU_COP2_VZ0 1
#define PSEMU_CPU_COP2_VXY1 2
#define PSEMU_CPU_COP2_VZ1 3
#define PSEMU_CPU_COP2_VXY2 4
#define PSEMU_CPU_COP2_VZ2 5
#define PSEMU_CPU_COP2_RGB 6
#define PSEMU_CPU_COP2_OTZ 7
#define PSEMU_CPU_COP2_IR0 8
#define PSEMU_CPU_COP2_IR1 9
#define PSEMU_CPU_COP2_IR2 10
#define PSEMU_CPU_COP2_IR3 11
#define PSEMU_CPU_COP2_SXY0 12
#define PSEMU_CPU_COP2_SXY1 13
#define PSEMU_CPU_COP2_SXY2 14
#define PSEMU_CPU_COP2_SXYP 15
#define PSEMU_CPU_COP2_SZ0 16
#define PSEMU_CPU_COP2_SZ1 17
#define PSEMU_CPU_COP2_SZ2 18
#define PSEMU_CPU_COP2_SZ3 19
#define PSEMU_CPU_COP2_RGB0 20
#define PSEMU_CPU_COP2_RGB1 21
#define PSEMU_CPU_COP2_RGB2 22
#define PSEMU_CPU_COP2_MAC0 24
#define PSEMU_CPU_COP2_MAC1 25
#define PSEMU_CPU_COP2_MAC2 26
#define PSEMU_CPU_COP2_MAC3 27
#define PSEMU_CPU_COP2_IRGB 28
#define PSEMU_CPU_COP2_ORGB 29
#define PSEMU_CPU_COP2_LZCS 30
#define PSEMU_CPU_COP2_LZCR 31

// Geometry Transformation Engine (GTE/COP2) control registers
#define PSEMU_CPU_COP2_R11R12 0
#define PSEMU_CPU_COP2_R13R21 1
#define PSEMU_CPU_COP2_R22R23 2
#define PSEMU_CPU_COP2_R31R32 3
#define PSEMU_CPU_COP2_R33 4
#define PSEMU_CPU_COP2_TRX 5
#define PSEMU_CPU_COP2_TRY 6
#define PSEMU_CPU_COP2_TRZ 7
#define PSEMU_CPU_COP2_L11L12 8
#define PSEMU_CPU_COP2_L13L21 9
#define PSEMU_CPU_COP2_L22L23 10
#define PSEMU_CPU_COP2_L31L32 11
#define PSEMU_CPU_COP2_L33 12
#define PSEMU_CPU_COP2_RBK 13
#define PSEMU_CPU_COP2_GBK 14
#define PSEMU_CPU_COP2_BBK 15
#define PSEMU_CPU_COP2_LR1LR2 16
#define PSEMU_CPU_COP2_LR3LG1 17
#define PSEMU_CPU_COP2_LG2LG3 18
#define PSEMU_CPU_COP2_LB1LB2 19
#define PSEMU_CPU_COP2_LB3 20
#define PSEMU_CPU_COP2_RFC 21
#define PSEMU_CPU_COP2_GFC 22
#define PSEMU_CPU_COP2_BFC 23
#define PSEMU_CPU_COP2_OFX 24
#define PSEMU_CPU_COP2_OFY 25
#define PSEMU_CPU_COP2_H 26
#define PSEMU_CPU_COP2_DQA 27
#define PSEMU_CPU_COP2_DQB 28
#define PSEMU_CPU_COP2_ZSF3 29
#define PSEMU_CPU_COP2_ZSF4 30
#define PSEMU_CPU_COP2_FLAG 31

// Geometry Transformation Engine (GTE/COP2) registers
struct psemu_cpu_gte
{
#define DEFINE_UNSIGNED_8BIT_PAIR(name, b0, b1, b2, b3) \
    union \
    { \
        struct \
        { \
            unsigned int b0 : 8; \
            unsigned int b1 : 8; \
            unsigned int b2 : 8; \
            unsigned int b3 : 8; \
        }; \
        uint32_t word; \
    } name; \

#define DEFINE_UNSIGNED_5BIT_PAIR(name, b0, b1, b2) \
    union \
    { \
        struct \
        { \
            unsigned int b0 : 5; \
            unsigned int b1 : 5; \
            unsigned int b2 : 5; \
            unsigned int    : 17; \
        }; \
        uint32_t word; \
    } name; \

#define DEFINE_SIGNED_32BIT_PAIR(name, lo, hi) \
    union \
    { \
        struct \
        { \
            signed int lo : 16; \
            signed int hi : 16; \
        }; \
        int32_t word; \
    } name; \

    // Data registers
    DEFINE_SIGNED_32BIT_PAIR(VXY0, x, y);             // 0
    int16_t VZ0;                                      // 1
    DEFINE_SIGNED_32BIT_PAIR(VXY1, x, y);             // 2
    int16_t VZ1;                                      // 3
    DEFINE_SIGNED_32BIT_PAIR(VXY2, x, y);             // 4
    int16_t VZ2;                                      // 5
    DEFINE_UNSIGNED_8BIT_PAIR(RGB, r, g, b, code);    // 6
    uint16_t OTZ;                                     // 7
    int16_t IR0;                                      // 8
    int16_t IR1;                                      // 9
    int16_t IR2;                                      // 10
    int16_t IR3;                                      // 11
    DEFINE_SIGNED_32BIT_PAIR(SXY0, x, y);             // 12
    DEFINE_SIGNED_32BIT_PAIR(SXY1, x, y);             // 13
    DEFINE_SIGNED_32BIT_PAIR(SXY2, x, y);             // 14
    int16_t SXYP;                                     // 15
    uint16_t SZ0;                                     // 16
    uint16_t SZ1;                                     // 17
    uint16_t SZ2;                                     // 18
    uint16_t SZ3;                                     // 19
    DEFINE_UNSIGNED_8BIT_PAIR(RGB0, r0, g0, b0, cd0); // 20
    DEFINE_UNSIGNED_8BIT_PAIR(RGB1, r1, g1, b1, cd1); // 21
    DEFINE_UNSIGNED_8BIT_PAIR(RGB2, r2, g2, b2, cd2); // 22
    int32_t MAC0;                                     // 24
    int32_t MAC1;                                     // 25
    int32_t MAC2;                                     // 26
    int32_t MAC3;                                     // 27
    DEFINE_UNSIGNED_5BIT_PAIR(IRGB, r, g, b);         // 28
    DEFINE_UNSIGNED_5BIT_PAIR(ORGB, r, g, b);         // 29
    int32_t LZCS;                                     // 30

    union
    {
        struct
        {
            unsigned int LZCR : 6;
            unsigned int : 26;
        };
        uint32_t word;
    } LZCR; // 31

    // Control registers
    DEFINE_SIGNED_32BIT_PAIR(R11R12, r12, r11); // 0
    DEFINE_SIGNED_32BIT_PAIR(R13R21, r21, r13); // 1
    DEFINE_SIGNED_32BIT_PAIR(R22R23, r23, r22); // 2
    DEFINE_SIGNED_32BIT_PAIR(R31R32, r32, r31); // 3
    int16_t R33;                                // 4
    int32_t TRX;                                // 5
    int32_t TRY;                                // 6
    int32_t TRZ;                                // 7
    DEFINE_SIGNED_32BIT_PAIR(L11L12, l12, l11); // 8
    DEFINE_SIGNED_32BIT_PAIR(L13L21, l21, l13); // 9
    DEFINE_SIGNED_32BIT_PAIR(L22L23, l23, l22); // 10
    DEFINE_SIGNED_32BIT_PAIR(L31L32, l32, l31); // 11
    int16_t L33;                                // 12
    int32_t RBK;                                // 13
    int32_t GBK;                                // 14
    int32_t BBK;                                // 15
    DEFINE_SIGNED_32BIT_PAIR(LR1LR2, lr2, lr1); // 16
    DEFINE_SIGNED_32BIT_PAIR(LR3LG1, lg1, lr3); // 17
    DEFINE_SIGNED_32BIT_PAIR(LG2LG3, lg3, lg2); // 18
    DEFINE_SIGNED_32BIT_PAIR(LB1LB2, lb2, lb1); // 19
    int16_t LB3;                                // 20
    int32_t RFC;                                // 21
    int32_t GFC;                                // 22
    int32_t BFC;                                // 23
    int32_t OFX;                                // 24
    int32_t OFY;                                // 25
    uint16_t H;                                 // 26
    int16_t DQA;                                // 27
    int16_t DQB;                                // 28
    int16_t ZSF3;                               // 29
    int16_t ZSF4;                               // 30
    uint32_t FLAG;                              // 31

    // Current instruction (required for parameters to some GTE operations)
    uint32_t instruction;
};

// Handles the `nclip` instruction.
void psemu_cpu_gte_nclip(struct psemu_cpu_gte* const gte);

// Handles the `ncds` instruction.
void psemu_cpu_gte_ncds(struct psemu_cpu_gte* const gte);

// Handles the `avsz3` instruction.
void psemu_cpu_gte_avsz3(struct psemu_cpu_gte* const gte);

// Handles the `rtpt` instruction.
void psemu_cpu_gte_rtpt(struct psemu_cpu_gte* const gte);

#ifdef __cplusplus
}
#endif // __cplusplus