/*
 DXTBlocksSSSE3.c
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
#include <tmmintrin.h>

void HapCodecDXTReadBlockBGRASSSE3(const uint8_t *copy_src, uint8_t *copy_dst, unsigned int src_bytes_per_row)
{
    int y;
    __m128i a;
    const __m128i mask = _mm_set_epi8(0x0F, 0x0C, 0x0D, 0x0E, 0x0B, 0x08, 0x09, 0x0A, 0x07, 0x04, 0x05, 0x06, 0x03, 0x00, 0x01, 0x02);
    
    for (y = 0; y < 4; y++)
    {
        a = _mm_load_si128((__m128i *)copy_src);
        _mm_store_si128((__m128i *)copy_dst, _mm_shuffle_epi8(a, mask));
        copy_src += src_bytes_per_row;
        copy_dst += 16;
    }
}
