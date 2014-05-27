/*
 HapCodecGL.h
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

#ifndef HapCodec_HapCodecGL_h
#define HapCodec_HapCodecGL_h

enum HapCodecGLPixelFormat {
    HapCodecGLPixelFormat_BGRA8,
    HapCodecGLPixelFormat_RGBA8,
    HapCodecGLPixelFormat_YCbCr422
};

typedef unsigned int HapCodecGLPixelFormat;

enum HapCodecGLCompressedFormat {
    HapCodecGLCompressedFormat_RGB_DXT1 = 0x83F0,
    HapCodecGLCompressedFormat_RGBA_DXT1 = 0x83F1,
    HapCodecGLCompressedFormat_RGBA_DXT3 = 0x83F2,
    HapCodecGLCompressedFormat_RGBA_DXT5 = 0x83F3
};

typedef unsigned int HapCodecGLCompressedFormat;

typedef struct HapCodecGL *HapCodecGLRef;

/*
 Note input buffers must be padded to a multiple of 4 bytes, as some hardware (eg Intel HD 4000) strongly dislikes
 non multiple-of-4 dimensions and produces corrupt frames. You can pass non-rounded values to the create functions,
 but the buffers provided to HapCodecGLEncode and HapCodecGLDecode must be padded appropriately.
 */

HapCodecGLRef HapCodecGLCreateEncoder(unsigned int width, unsigned int height, unsigned int compressed_format);
HapCodecGLRef HapCodecGLCreateDecoder(unsigned int width, unsigned int height, unsigned int compressed_format);
unsigned int HapCodecGLGetCompressedFormat(HapCodecGLRef coder);
void HapCodecGLDestroy(HapCodecGLRef coder);
/*
 encode/decode funtions return 0 on success
 */
int HapCodecGLEncode(HapCodecGLRef coder, unsigned int source_bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination);
int HapCodecGLDecode(HapCodecGLRef coder, unsigned int destination_bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination);

#endif
