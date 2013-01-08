//
//  GLDXTEncoder.c
//  VPUCodec
//
//  Created by Tom on 01/10/2012.
//
//

#include "GLDXTEncoder.h"
#include "VPUCodecGL.h"
#include "PixelFormats.h"
#include <CoreVideo/CoreVideo.h>

struct VPUPGLEncoder {
    struct VPUPCodecDXTEncoder base;
    VPUCGLRef encoder;
    dispatch_queue_t queue; // We use a queue to enforce serial access to the GL code
#ifdef DEBUG
    char description[255];
#endif
};

static void VPUPGLEncoderDestroy(VPUPCodecDXTEncoderRef encoder)
{
    if (((struct VPUPGLEncoder *)encoder)->queue)
    {
        dispatch_release(((struct VPUPGLEncoder *)encoder)->queue);
    }
    VPUCGLDestroy(((struct VPUPGLEncoder *)encoder)->encoder);
    free(encoder);
}

static OSType VPUPGLEncoderWantedPixelFormat(VPUPCodecDXTEncoderRef encoder, OSType format)
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

static int VPUPGLEncoderEncode(VPUPCodecDXTEncoderRef encoder,
                                   const void *src,
                                   unsigned int src_bytes_per_row,
                                   OSType src_pixel_format,
                                   void *dst,
                                   unsigned int width,
                                   unsigned int height)
{
#pragma unused(width, height)
    VPUCGLPixelFormat encoder_pixel_format;
    switch (src_pixel_format) {
        case kCVPixelFormatType_32BGRA:
            encoder_pixel_format = VPUCGLPixelFormat_BGRA8;
            break;
        case kCVPixelFormatType_32RGBA:
            encoder_pixel_format = VPUCGLPixelFormat_RGBA8;
            break;
        case kCVPixelFormatType_422YpCbCr8:
            encoder_pixel_format = VPUCGLPixelFormat_YCbCr422;
            break;
        default:
            return 1;
    }
    dispatch_sync(((struct VPUPGLEncoder *)encoder)->queue, ^{
        VPUCGLEncode(((struct VPUPGLEncoder *)encoder)->encoder,
                     src_bytes_per_row,
                     encoder_pixel_format,
                     src,
                     dst);
    });
    return 0;
}

#if defined(DEBUG)
static const char *VPUPGLEncoderDescribe(VPUPCodecDXTEncoderRef encoder)
{
    return ((struct VPUPGLEncoder *)encoder)->description;
}
#endif

VPUPCodecDXTEncoderRef VPUPGLEncoderCreate(unsigned int width, unsigned int height, OSType pixelFormat)
{
    unsigned int encoder_format;
    
    switch (pixelFormat) {
        case kVPUCVPixelFormat_RGB_DXT1:
            encoder_format = VPUCGLCompressedFormat_RGB_DXT1;
            break;
        case kVPUCVPixelFormat_RGBA_DXT5:
            encoder_format = VPUCGLCompressedFormat_RGBA_DXT5;
            break;
        default:
            return NULL;
    }
    
    struct VPUPGLEncoder *encoder = malloc(sizeof(struct VPUPGLEncoder));
    if (encoder)
    {
        encoder->base.destroy_function = VPUPGLEncoderDestroy;
        encoder->base.pixelformat_function = VPUPGLEncoderWantedPixelFormat;
        encoder->base.encode_function = VPUPGLEncoderEncode;
        encoder->base.pad_source_buffers = true;
        
        encoder->queue = dispatch_queue_create(NULL, DISPATCH_QUEUE_SERIAL);
        encoder->encoder = VPUCGLCreateEncoder(width, height, encoder_format);
        if (encoder->encoder == NULL || encoder->queue == NULL)
        {
            VPUPGLEncoderDestroy((VPUPCodecDXTEncoderRef)encoder);
            encoder = NULL;
        }
        
#if defined(DEBUG)
        char *format_str;
        switch (encoder_format) {
            case VPUCGLCompressedFormat_RGB_DXT1:
                format_str = "RGB DXT1";
                break;
            case VPUCGLCompressedFormat_RGBA_DXT5:
            default:
                format_str = "RGBA DXT5";
                break;
        }
        
        snprintf(encoder->description, sizeof(encoder->description), "GL %s Encoder", format_str);
        
        encoder->base.describe_function = VPUPGLEncoderDescribe;
#endif
    }
    
    return (VPUPCodecDXTEncoderRef)encoder;
}
