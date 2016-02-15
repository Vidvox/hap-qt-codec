/*
 SquishRGTC1Decoder.c
 Hap Codec

 Copyright (c) 2016, Tom Butterworth and Vidvox LLC. All rights reserved. 
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

#include "SquishDecoder.h"
#include "HapPlatform.h"
#include "DXTBlocks.h"
#include <stdint.h>
#include <string.h>
#include "squish-c.h"

static HAP_INLINE void HapCodecDXTWriteAlphaBlockXXXA(const uint8_t *copy_src, uint8_t *copy_dst, unsigned int dst_bytes_per_row)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            copy_dst[3] = copy_src[3];
            copy_dst += 4;
            copy_src += 4;
        }
        copy_dst += dst_bytes_per_row - 16;
    }
}

void HapCodecSquishRGTC1Decode(const void *src,
                               void *dst,
                               unsigned int dst_bytes_per_row,
                               unsigned int width,
                               unsigned int height)
{
    unsigned int y,x;
    uint8_t *src_block = (uint8_t *)src;

    // TODO: SSE3 version if needed

    for (y = 0; y < height; y+= 4)
    {
        for (x = 0; x < width; x += 4)
        {
            HAP_ALIGN_16 uint8_t block_rgba[16*4];
            uint8_t *dst_base = ((uint8_t *)dst) + (dst_bytes_per_row * y) + (4 * x);
            SquishDecompress(block_rgba, src_block, kRgtc1A);
            HapCodecDXTWriteAlphaBlockXXXA(block_rgba, dst_base, dst_bytes_per_row);
            src_block += 8;
        }
    }
}
