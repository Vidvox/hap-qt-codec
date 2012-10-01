//
//  SquishEncoder.c
//  VPUCodec
//
//  Created by Tom on 01/10/2012.
//
//

#include "SquishEncoder.h"
#include "PixelFormats.h"
#include "squish-c.h"
#include "Buffers.h"

struct VPUPCodecSquishEncoder {
    struct VPUPCodecDXTEncoder base;
    int flags;
    VPUCodecBufferPoolRef pool;
};

static void VPUPSquishEncoderDestroy(VPUPCodecDXTEncoderRef encoder)
{
    if (encoder)
    {
        VPUCodecDestroyBufferPool(((struct VPUPCodecSquishEncoder *)encoder)->pool);
        free(encoder);
    }
}

static int VPUPSquishEncoderEncode(VPUPCodecDXTEncoderRef encoder,
                                   const void *src,
                                   unsigned int src_bytes_per_row,
                                   OSType src_pixel_format,
                                   void *dst,
                                   unsigned int width,
                                   unsigned int height)
{
#pragma unused(encoder)
    if (src_pixel_format != k32RGBAPixelFormat || src_bytes_per_row != width * 4) return 1;
    // Squish won't tolerate extra bytes at the ends of rows
    // so we have to copy to a temporary buffer
    // TODO: probably easy to modify squish to take a rowBytes argument
    // or just compress beyond the edge of the image if the extra bytes can pretend to be whole pixels
    unsigned int needed_bytes_per_row = width * 4;
    VPUCodecBufferRef buffer = NULL;
    
    if (src_bytes_per_row != needed_bytes_per_row)
    {
        unsigned int wanted_buffer_size = height * needed_bytes_per_row;
        if (VPUCodecGetBufferPoolBufferSize(((struct VPUPCodecSquishEncoder *)encoder)->pool) != wanted_buffer_size)
        {
            VPUCodecDestroyBufferPool(((struct VPUPCodecSquishEncoder *)encoder)->pool);
            ((struct VPUPCodecSquishEncoder *)encoder)->pool = VPUCodecCreateBufferPool(wanted_buffer_size);
        }
        buffer = VPUCodecGetBuffer(((struct VPUPCodecSquishEncoder *)encoder)->pool);
        if (buffer == NULL) return 1;
        
        void *bufferBaseAddress = VPUCodecGetBufferBaseAddress(buffer);
        
        unsigned int i;
        for (i = 0; i < height; i++) {
            memcpy(bufferBaseAddress + (needed_bytes_per_row * i), src + (src_bytes_per_row * i), needed_bytes_per_row);
        }
        src = bufferBaseAddress;
        src_bytes_per_row = needed_bytes_per_row;
    }
    
    // TODO: it may be faster to split the image into blocks ourself and do them in parallel
    SquishCompressImage(src, width, height, dst, ((struct VPUPCodecSquishEncoder *)encoder)->flags, NULL);
    
    VPUCodecReturnBuffer(buffer);
    
    return 0;
}

static OSType VPUPSquishEncoderWantedPixelFormat(VPUPCodecDXTEncoderRef encoder, OSType sourceFormat)
{
#pragma unused(encoder, sourceFormat)
    return k32RGBAPixelFormat;
}

#if defined(DEBUG)
static void VPUPSquishEncoderShow(VPUPCodecDXTEncoderRef encoder)
{
    int flags = ((struct VPUPCodecSquishEncoder *)encoder)->flags;
    char *format = flags & kDxt1 ? "RGB DXT1" : "RGBA DXT5";
    char *quality;
    if (flags & kColourRangeFit)
        quality = "ColourRangeFit";
    else if (flags & kColourClusterFit)
        quality = "ColourClusterFit";
    else
        quality = "ColourIterativeClusterFit";
    
    printf("Squish %s %s Encoder\n", format, quality);
}
#endif

VPUPCodecDXTEncoderRef VPUPSquishEncoderCreate(VPUPCodecSquishEncoderQuality quality, OSType pixelFormat)
{
    struct VPUPCodecSquishEncoder *encoder = malloc(sizeof(struct VPUPCodecSquishEncoder));
    if (encoder)
    {
        encoder->base.pixelformat_function = VPUPSquishEncoderWantedPixelFormat;
        encoder->base.encode_function = VPUPSquishEncoderEncode;
        encoder->base.destroy_function = VPUPSquishEncoderDestroy;
#if defined(DEBUG)
        encoder->base.show_function = VPUPSquishEncoderShow;
#endif
        encoder->pool = NULL;
        
        switch (quality) {
            case VPUPCodecSquishEncoderWorstQuality:
                encoder->flags = kColourRangeFit;
                break;
            case VPUPCodecSquishEncoderBestQuality:
                // TODO: consider quality versus time cost of kColourIterativeClusterFit
                encoder->flags = kColourIterativeClusterFit;
                break;
            default:
                encoder->flags = kColourClusterFit;
                break;
        }
        switch (pixelFormat) {
            case kVPUCVPixelFormat_RGB_DXT1:
                encoder->flags |= kDxt1;
                break;
            case kVPUCVPixelFormat_RGBA_DXT5:
                encoder->flags |= kDxt5;
                break;
            default:
                VPUPSquishEncoderDestroy((VPUPCodecDXTEncoderRef)encoder);
                encoder = NULL;
                break;
        }
    }
    return (VPUPCodecDXTEncoderRef)encoder;
}
