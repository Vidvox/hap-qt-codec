//
//  VPUCodecGL.h
//  VPUCodec
//
//  Created by Tom Butterworth on 28/09/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//

#ifndef VPUCodec_VPUCodecGL_h
#define VPUCodec_VPUCodecGL_h

enum VPUCGLPixelFormat {
    VPUCGLPixelFormat_BGRA8,
    VPUCGLPixelFormat_RGBA8,
    VPUCGLPixelFormat_YCbCr422
};

typedef unsigned int VPUCGLPixelFormat;

enum VPUCGLCompressedFormat {
    VPUCGLCompressedFormat_RGB_DXT1 = 0x83F0,
    VPUCGLCompressedFormat_RGBA_DXT1 = 0x83F1,
    VPUCGLCompressedFormat_RGBA_DXT3 = 0x83F2,
    VPUCGLCompressedFormat_RGBA_DXT5 = 0x83F3
};

typedef unsigned int VPUCGLCompressedFormat;

typedef struct VPUCGL *VPUCGLRef;

/*
 Note input buffers must be padded to a multiple of 4 bytes, as some hardware (eg Intel HD 4000) strongly dislikes
 non multiple-of-4 dimensions and produces corrupt frames. You can pass non-rounded values to the create functions,
 but the buffers provided to VPUCGLEncode and VPUCGLDecode must be padded appropriately.
 */

VPUCGLRef VPUCGLCreateEncoder(unsigned int width, unsigned int height, unsigned int compressed_format);
VPUCGLRef VPUCGLCreateDecoder(unsigned int width, unsigned int height, unsigned int compressed_format);
unsigned int VPUCGLGetWidth(VPUCGLRef coder);
unsigned int VPUCGLGetHeight(VPUCGLRef coder);
unsigned int VPUCGLGetCompressedFormat(VPUCGLRef coder);
void VPUCGLDestroy(VPUCGLRef coder);
void VPUCGLEncode(VPUCGLRef coder, unsigned int bytes_per_row, VPUCGLPixelFormat pixel_format, const void *source, void *destination);
void VPUCGLDecode(VPUCGLRef coder, unsigned int bytes_per_row, VPUCGLPixelFormat pixel_format, const void *source, void *destination);

#endif
