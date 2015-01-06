/*
 DXTBlocks.c
 Hap Codec
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DXTBlocks.h"
#include <string.h>

void HapCodecDXTReadBlockRGBA(const uint8_t *copy_src, uint8_t *copy_dst, unsigned int src_bytes_per_row)
{
    int i;
    for (i = 0; i < 4; i++) {
        memcpy(copy_dst, copy_src, 16);
        copy_src += src_bytes_per_row;
        copy_dst += 16;
    }
}

#if !defined(HAP_SSSE3_ALWAYS_AVAILABLE)

#if defined(_WIN32)
#include <immintrin.h>
#define hap_cpuid(i,t)    __cpuid((i),(t))

#else

static void hap_cpuid(int info[4],int infoType){
    __asm__ __volatile__ (
                          "cpuid":
                          "=a" (info[0]),
                          "=b" (info[1]),
                          "=c" (info[2]),
                          "=d" (info[3]) :
                          "a" (infoType)
                          );
}

#endif // !defined(_WIN32)

int HapCodecHasSSSE3(void)
{
    int info[4] = { 0, 0, 0, 0 };
    int hasSSE2, hasSSE3, hasSSSE3;
    
    hap_cpuid(info,0x00000001);
    hasSSE2  = (info[3] & ((int)1 << 26)) != 0;
    hasSSE3  = (info[2] & ((int)1 <<  0)) != 0;
    hasSSSE3 = (info[2] & ((int)1 <<  9)) != 0;
    return (hasSSE2 && hasSSE3 && hasSSSE3);
}

void HapCodecDXTReadBlockBGRAScalar(const uint8_t *copy_src, uint8_t *copy_dst, unsigned int src_bytes_per_row)
{
    int y, x;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            copy_dst[0] = copy_src[2];
            copy_dst[1] = copy_src[1];
            copy_dst[2] = copy_src[0];
            copy_dst[3] = copy_src[3];
            copy_dst += 4;
            copy_src += 4;
        }
        copy_src += src_bytes_per_row - 16;
    }
}

#endif // !defined(HAP_SSSE3_ALWAYS_AVAILABLE)
