/*
 YCoCgDXTEncoder.c
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

#include "YCoCgDXTEncoder.h"
#include "YCoCgDXT.h"
#include "PixelFormats.h"
#include "HapPlatform.h"
#include <stdlib.h>

static void HapCodecYCoCgEncoderDestroy(HapCodecDXTEncoderRef encoder)
{
    free(encoder);
}

static int HapCodecYCoCgDXTEncoderEncode(HapCodecDXTEncoderRef encoder HAP_ATTR_UNUSED,
                                     const void *src,
                                     unsigned int src_bytes_per_row,
                                     OSType src_pixel_format,
                                     void *dst,
                                     unsigned int width,
                                     unsigned int height)
{
    if (src_pixel_format != kHapCVPixelFormat_CoCgXY) return 1;
    CompressYCoCgDXT5((const byte *)src, (byte *)dst, width, height, src_bytes_per_row);
    return 0;
}

static OSType HapCodecYCoCgDXTEncoderWantedPixelFormat(HapCodecDXTEncoderRef encoder HAP_ATTR_UNUSED, OSType sourceFormat HAP_ATTR_UNUSED)
{
    return kHapCVPixelFormat_CoCgXY;
}

#if defined(DEBUG)
static const char *HapCodecYCoCgDXTEncoderDescribe(HapCodecDXTEncoderRef encoder HAP_ATTR_UNUSED)
{
    return "YCoCg DXT5 Encoder";
}
#endif

HapCodecDXTEncoderRef HapCodecYCoCgDXTEncoderCreate(void)
{
    HapCodecDXTEncoderRef encoder = (HapCodecDXTEncoderRef)malloc(sizeof(struct HapCodecDXTEncoder));
    if (encoder)
    {
        encoder->pixelformat_function = HapCodecYCoCgDXTEncoderWantedPixelFormat;
        encoder->destroy_function = HapCodecYCoCgEncoderDestroy;
        encoder->encode_function = HapCodecYCoCgDXTEncoderEncode;
#if defined(DEBUG)
        encoder->describe_function = HapCodecYCoCgDXTEncoderDescribe;
#endif
        encoder->pad_source_buffers = false;
        encoder->can_slice = true;
    }
    return encoder;
}
