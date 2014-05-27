/*
 DXTEncoder.h
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

#ifndef HapCodec_DXTEncoder_h
#define HapCodec_DXTEncoder_h

#include <MacTypes.h>

/*
 An encoder is simply a pointer to a struct which provides functions for the codec to call
 as needed, and a boolean value which if true requires input buffers be padded to multiple-of-4
 pixel dimensions.
 
 The encode function will be called in parallel so must be thread-safe.
 */

typedef struct HapCodecDXTEncoder *HapCodecDXTEncoderRef;

typedef void(*HapCodecDXTEncoder_DestroyFunction)(HapCodecDXTEncoderRef encoder);

typedef OSType(*HapCodecDXTEncoder_WantedPixelFormatFunction)(HapCodecDXTEncoderRef encoder, OSType sourcePixelFormat);

typedef int(*HapCodecDXTEncoder_EncodeFunction)(HapCodecDXTEncoderRef encoder,
                                                 const void *src,
                                                 unsigned int src_bytes_per_row,
                                                 OSType src_pixel_format,
                                                 void *dst,
                                                 unsigned int width,
                                                 unsigned int height);

#if defined(DEBUG)
/*
 Returns a C-string description, valid for the lifetime of the encoder.
 */
typedef const char *(*HapCodecDXTEncoder_DescribeFunction)(HapCodecDXTEncoderRef encoder);
#endif

struct HapCodecDXTEncoder {
    HapCodecDXTEncoder_WantedPixelFormatFunction pixelformat_function;
    HapCodecDXTEncoder_EncodeFunction encode_function;
    HapCodecDXTEncoder_DestroyFunction destroy_function;
#if defined(DEBUG)
    HapCodecDXTEncoder_DescribeFunction describe_function;
#endif
    Boolean pad_source_buffers;
};

#define HapCodecDXTEncoderDestroy(x) if ((x) && (x)->destroy_function) (x)->destroy_function((x))

#endif
