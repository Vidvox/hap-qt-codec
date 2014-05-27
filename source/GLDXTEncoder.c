/*
 GLDXTEncoder.c
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

#include "GLDXTEncoder.h"
#include "HapCodecGL.h"
#include "PixelFormats.h"
#include <CoreVideo/CoreVideo.h>

struct HapCodecGLEncoder {
    struct HapCodecDXTEncoder base;
    HapCodecGLRef encoder;
    dispatch_queue_t queue; // We use a queue to enforce serial access to the GL code
#ifdef DEBUG
    char description[255];
#endif
};

static void HapCodecGLEncoderDestroy(HapCodecDXTEncoderRef encoder)
{
    if (((struct HapCodecGLEncoder *)encoder)->queue)
    {
        dispatch_release(((struct HapCodecGLEncoder *)encoder)->queue);
    }
    HapCodecGLDestroy(((struct HapCodecGLEncoder *)encoder)->encoder);
    free(encoder);
}

static OSType HapCodecGLEncoderWantedPixelFormat(HapCodecDXTEncoderRef encoder, OSType format)
{
#pragma unused(encoder)
    switch (format) {
        case kCVPixelFormatType_32BGRA:
        case kCVPixelFormatType_32RGBA:
        case kCVPixelFormatType_422YpCbCr8:
            return format;
        default:
            return kCVPixelFormatType_32BGRA;
    }
}

static int HapCodecGLEncoderEncode(HapCodecDXTEncoderRef encoder,
                                   const void *src,
                                   unsigned int src_bytes_per_row,
                                   OSType src_pixel_format,
                                   void *dst,
                                   unsigned int width,
                                   unsigned int height)
{
#pragma unused(width, height)
    HapCodecGLPixelFormat encoder_pixel_format;
    switch (src_pixel_format) {
        case kCVPixelFormatType_32BGRA:
            encoder_pixel_format = HapCodecGLPixelFormat_BGRA8;
            break;
        case kCVPixelFormatType_32RGBA:
            encoder_pixel_format = HapCodecGLPixelFormat_RGBA8;
            break;
        case kCVPixelFormatType_422YpCbCr8:
            encoder_pixel_format = HapCodecGLPixelFormat_YCbCr422;
            break;
        default:
            return 1;
    }
    __block int result;
    dispatch_sync(((struct HapCodecGLEncoder *)encoder)->queue, ^{
        result = HapCodecGLEncode(((struct HapCodecGLEncoder *)encoder)->encoder,
                                  src_bytes_per_row,
                                  encoder_pixel_format,
                                  src,
                                  dst);
    });
    return result;
}

#if defined(DEBUG)
static const char *HapCodecGLEncoderDescribe(HapCodecDXTEncoderRef encoder)
{
    return ((struct HapCodecGLEncoder *)encoder)->description;
}
#endif

HapCodecDXTEncoderRef HapCodecGLEncoderCreate(unsigned int width, unsigned int height, OSType pixelFormat)
{
    unsigned int encoder_format;
    
    switch (pixelFormat) {
        case kHapCVPixelFormat_RGB_DXT1:
            encoder_format = HapCodecGLCompressedFormat_RGB_DXT1;
            break;
        case kHapCVPixelFormat_RGBA_DXT5:
            encoder_format = HapCodecGLCompressedFormat_RGBA_DXT5;
            break;
        default:
            return NULL;
    }
    
    struct HapCodecGLEncoder *encoder = malloc(sizeof(struct HapCodecGLEncoder));
    if (encoder)
    {
        encoder->base.destroy_function = HapCodecGLEncoderDestroy;
        encoder->base.pixelformat_function = HapCodecGLEncoderWantedPixelFormat;
        encoder->base.encode_function = HapCodecGLEncoderEncode;
        encoder->base.pad_source_buffers = true;
        
        encoder->queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
        encoder->encoder = HapCodecGLCreateEncoder(width, height, encoder_format);
        
#if defined(DEBUG)
        char *format_str;
        switch (encoder_format) {
            case HapCodecGLCompressedFormat_RGB_DXT1:
                format_str = "RGB DXT1";
                break;
            case HapCodecGLCompressedFormat_RGBA_DXT5:
            default:
                format_str = "RGBA DXT5";
                break;
        }
        
        snprintf(encoder->description, sizeof(encoder->description), "GL %s Encoder", format_str);
        
        encoder->base.describe_function = HapCodecGLEncoderDescribe;
#endif
        
        if (encoder->encoder == NULL || encoder->queue == NULL)
        {
            HapCodecGLEncoderDestroy((HapCodecDXTEncoderRef)encoder);
            encoder = NULL;
        }
    }
    
    return (HapCodecDXTEncoderRef)encoder;
}
