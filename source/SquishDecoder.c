/*
 SquishDecoder.c
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

#include "SquishDecoder.h"
#include "HapPlatform.h"
#include "DXTBlocks.h"
#include <stdint.h>
#include <string.h>
#include <tmmintrin.h>
#include "squish-c.h"

/*
    This file only used by the Windows codec
*/

static HAP_INLINE void HapCodecSquishWriteBlockRGBA(uint8_t *block, uint8_t *dst, unsigned int dst_bytes_per_row)
{
    int py;
    for (py = 0; py < 4; py++)
    {
        memcpy(dst, block, 16);
        dst += dst_bytes_per_row;
        block += 16;
    }
}

static HAP_INLINE void HapCodecSquishWriteBlockBGRAScalar(uint8_t *block, uint8_t *dst, unsigned int dst_bytes_per_row)
{
    int y, x;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            dst[0] = block[2];
            dst[1] = block[1];
            dst[2] = block[0];
            dst[3] = block[3];
            dst += 4;
            block += 4;
        }
        dst += dst_bytes_per_row - 16;
    }
}

static HAP_INLINE void HapCodecSquishWriteBlockBGRASSE3(uint8_t *block, uint8_t *dst, unsigned int dst_bytes_per_row)
{
    int y;
    __m128i a;
    const __m128i mask = _mm_set_epi8(0x0F, 0x0C, 0x0D, 0x0E, 0x0B, 0x08, 0x09, 0x0A, 0x07, 0x04, 0x05, 0x06, 0x03, 0x00, 0x01, 0x02);

    for (y = 0; y < 4; y++)
    {
        a = _mm_load_si128((__m128i *)block);
        _mm_store_si128((__m128i *)dst, _mm_shuffle_epi8(a, mask));
        dst += dst_bytes_per_row;
        block += 16;
    }
}

void HapCodecSquishDecode(const void *src,
                          unsigned int src_pixel_format,
                          void *dst,
                          unsigned int dst_pixel_format,
                          unsigned int dst_bytes_per_row,
                          unsigned int width,
                          unsigned int height)
{
    unsigned int y,x;
    int bytes_per_block = (src_pixel_format == kHapCVPixelFormat_RGB_DXT1 ? 8 : 16);
    uint8_t *src_block = (uint8_t *)src;
    int flags = (src_pixel_format == kHapCVPixelFormat_RGB_DXT1 ? kDxt1 : kDxt5);
    int hasSSSE3 = 0;
    if (dst_pixel_format != 'RGBA')
    {
        hasSSSE3 = HapCodecHasSSSE3();
    }
    for (y = 0; y < height; y+= 4)
    {
        for (x = 0; x < width; x += 4)
        {
            HAP_ALIGN_16 uint8_t block_rgba[16*4];
            uint8_t *dst_base = ((uint8_t *)dst) + (dst_bytes_per_row * y) + (4 * x);
            SquishDecompress(block_rgba, src_block, flags);
            if (dst_pixel_format == 'RGBA')
            {
                HapCodecSquishWriteBlockRGBA(block_rgba, dst_base, dst_bytes_per_row);
            }
            else if (hasSSSE3)
            {
                HapCodecSquishWriteBlockBGRASSE3(block_rgba, dst_base, dst_bytes_per_row);
            }
            else
            {
                HapCodecSquishWriteBlockBGRAScalar(block_rgba, dst_base, dst_bytes_per_row);
            }
            src_block += bytes_per_block;
        }
    }
}
