/*
 SquishEncoder.c
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

#include "SquishEncoder.h"
#include "PixelFormats.h"
#include "HapPlatform.h"
#include "DXTBlocks.h"
#include "squish-c.h"
#include "Buffers.h"
#include <stdlib.h>
#include <stdint.h>
#if defined(DEBUG)
#include <stdio.h>
#endif

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

struct HapCodecSquishEncoder {
    struct HapCodecDXTEncoder base;
    int flags;
#if defined(DEBUG)
    char description[255];
#endif
};

static HAP_INLINE void HapCodecSquishCopyPixelRGBA(uint8_t *copy_src, uint8_t *copy_dst)
{
    int i;
    for(i = 0; i < 4; ++i)
        *copy_dst++ = *copy_src++;
}

static HAP_INLINE void HapCodecSquishCopyPixelBGRA(uint8_t *copy_src, uint8_t *copy_dst)
{
    
    copy_dst[0] = copy_src[2];
    copy_dst[1] = copy_src[1];
    copy_dst[2] = copy_src[0];
    copy_dst[3] = copy_src[3];
}

static void HapCodecSquishEncoderDestroy(HapCodecDXTEncoderRef encoder)
{
    if (encoder)
    {
        free(encoder);
    }
}

static int HapCodecSquishEncoderEncode(HapCodecDXTEncoderRef encoder,
                                   const void *src,
                                   unsigned int src_bytes_per_row,
                                   OSType src_pixel_format,
                                   void *dst,
                                   unsigned int width,
                                   unsigned int height)
{
    // We feed Squish block by block to handle extra bytes at the ends of rows
    
	uint8_t* dst_block = (uint8_t *)dst;
	int bytes_per_block = ( ( ((struct HapCodecSquishEncoder *)encoder)->flags & ( kDxt1 | kRgtc1A ) ) != 0 ) ? 8 : 16;
    unsigned int y, x, py, px;
    
#if !defined(HAP_SSSE3_ALWAYS_AVAILABLE)
    int hasSSSE3 = HapCodecHasSSSE3();
#endif
    
    if (src_pixel_format != 'RGBA' && src_pixel_format != 'BGRA') return 1;

	for( y = 0; y < height; y += 4 )
	{
        int remaining_height = height - y;
        
		for( x = 0; x < width; x += 4 )
		{
            int remaining_width = width - x;
            
			// Pack the 4x4 pixel block
			HAP_ALIGN_16 uint8_t block_rgba[16*4];
            int mask;
            
            uint8_t *copy_src = (uint8_t *)src + (y * src_bytes_per_row) + (x * 4);
            uint8_t *copy_dst = block_rgba;
            
            if ((remaining_height < 4) || (remaining_width < 4) )
            {
                // If the source has dimensions which aren't a multiple of 4 we only copy the existing
                // pixels and use the mask argument to tell Squish to ignore the extras in the block
                mask = 0;
                for( py = 0; py < 4; ++py )
                {
                    for( px = 0; px < 4; ++px )
                    {
                        unsigned int sx = x + px;
                        unsigned int sy = y + py;

                        if( sx < width && sy < height )
                        {
                            if (src_pixel_format == 'BGRA') HapCodecSquishCopyPixelBGRA(copy_src, copy_dst);
                            else HapCodecSquishCopyPixelRGBA(copy_src, copy_dst);
                            
                            mask |= ( 1 << ( 4*py + px ) );

                            copy_src += 4;
                        }
                        copy_dst += 4;
                    }
                    copy_src += src_bytes_per_row - (MIN(remaining_width, 4) * 4);
                }
            }
            else
            {
                // If all the pixels are in the frame, we can copy them in 4 x 4
                if (src_pixel_format == 'BGRA')
                {
                    
#if defined(HAP_SSSE3_ALWAYS_AVAILABLE)
                    HapCodecDXTReadBlockBGRASSSE3(copy_src, copy_dst, src_bytes_per_row);
#else
                    if (hasSSSE3) HapCodecDXTReadBlockBGRASSSE3(copy_src, copy_dst, src_bytes_per_row);
                    else HapCodecDXTReadBlockBGRAScalar(copy_src, copy_dst, src_bytes_per_row);
#endif
                    
                }
                else HapCodecDXTReadBlockRGBA(copy_src, copy_dst, src_bytes_per_row);
                
                mask = 0xFFFF;
            }
			
			// Compress the block
			SquishCompressMasked( block_rgba, mask, dst_block, ((struct HapCodecSquishEncoder *)encoder)->flags, NULL );
			
			dst_block += bytes_per_block;
		}
	}
    return 0;
}

static OSType HapCodecSquishEncoderWantedPixelFormat(HapCodecDXTEncoderRef encoder HAP_ATTR_UNUSED, OSType sourceFormat)
{
    switch (sourceFormat) {
        case 'RGBA':
        case 'BGRA':
            return sourceFormat;
        default:
            return 'RGBA';
    }
}

#if defined(DEBUG)
static const char *HapCodecSquishEncoderDescribe(HapCodecDXTEncoderRef encoder)
{
    return ((struct HapCodecSquishEncoder *)encoder)->description;
}
#endif

HapCodecDXTEncoderRef HapCodecSquishEncoderCreate(HapCodecSquishEncoderQuality quality, OSType pixelFormat)
{
    struct HapCodecSquishEncoder *encoder = (struct HapCodecSquishEncoder *)malloc(sizeof(struct HapCodecSquishEncoder));
    if (encoder)
    {
        encoder->base.pixelformat_function = HapCodecSquishEncoderWantedPixelFormat;
        encoder->base.encode_function = HapCodecSquishEncoderEncode;
        encoder->base.destroy_function = HapCodecSquishEncoderDestroy;
        encoder->base.pad_source_buffers = false;
        encoder->base.can_slice = true;
        
        switch (quality) {
            case HapCodecSquishEncoderWorstQuality:
                encoder->flags = kColourRangeFit;
                break;
            case HapCodecSquishEncoderBestQuality:
                encoder->flags = kColourIterativeClusterFit;
                break;
            default:
                encoder->flags = kColourClusterFit;
                break;
        }
        switch (pixelFormat) {
            case kHapCVPixelFormat_RGB_DXT1:
                encoder->flags |= kDxt1;
                break;
            case kHapCVPixelFormat_RGBA_DXT5:
                encoder->flags |= kDxt5;
                break;
            case kHapCVPixelFormat_A_RGTC1:
                encoder->flags |= kRgtc1A;
                break;
            default:
                HapCodecSquishEncoderDestroy((HapCodecDXTEncoderRef)encoder);
                encoder = NULL;
                break;
        }
        
#if defined(DEBUG)
        if (encoder)
        {
            char *format = encoder->flags & kDxt1 ? "RGB DXT1" : encoder->flags & kDxt5 ? "RGBA DXT5" : "A RGTC1";
            
            char *qualityString;
            if (encoder->flags & kColourRangeFit)
                qualityString = "ColourRangeFit";
            else if (encoder->flags & kColourClusterFit)
                qualityString = "ColourClusterFit";
            else
                qualityString = "ColourIterativeClusterFit";
#if defined(__APPLE__)
            snprintf(encoder->description, sizeof(encoder->description), "Squish %s %s Encoder", format, qualityString);
#else
            _snprintf_s(encoder->description, sizeof(encoder->description), _TRUNCATE, "Squish %s %s Encoder", format, qualityString);
#endif
            encoder->base.describe_function = HapCodecSquishEncoderDescribe;
        }
#endif
    }
    return (HapCodecDXTEncoderRef)encoder;
}
