//
//  HapCodecGL.h
//  Hap Codec
//
//  Created by Tom Butterworth on 28/09/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//

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
unsigned int HapCodecGLGetWidth(HapCodecGLRef coder);
unsigned int HapCodecGLGetHeight(HapCodecGLRef coder);
unsigned int HapCodecGLGetCompressedFormat(HapCodecGLRef coder);
void HapCodecGLDestroy(HapCodecGLRef coder);
void HapCodecGLEncode(HapCodecGLRef coder, unsigned int bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination);
void HapCodecGLDecode(HapCodecGLRef coder, unsigned int bytes_per_row, HapCodecGLPixelFormat pixel_format, const void *source, void *destination);

#endif
