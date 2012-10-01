//
//  YCoCgDXTEncoder.c
//  VPUCodec
//
//  Created by Tom on 01/10/2012.
//
//

#include "YCoCgDXTEncoder.h"
#include "YCoCgDXT.h"
#include "PixelFormats.h"

static void VPUPYCoCgEncoderDestroy(VPUPCodecDXTEncoderRef encoder)
{
    free(encoder);
}

static int VPUPYCoCgDXTEncoderEncode(VPUPCodecDXTEncoderRef encoder,
                                     const void *src,
                                     unsigned int src_bytes_per_row,
                                     OSType src_pixel_format,
                                     void *dst,
                                     unsigned int width,
                                     unsigned int height)
{
#pragma unused(encoder)
    if (src_pixel_format != kVPUCVPixelFormat_YCoCgX) return 1;
    CompressYCoCgDXT5(src, dst, width, height, src_bytes_per_row);
    return 0;
}

static OSType VPUPYCoCgDXTEncoderWantedPixelFormat(VPUPCodecDXTEncoderRef encoder, OSType sourceFormat)
{
#pragma unused(encoder, sourceFormat)
    return kVPUCVPixelFormat_YCoCgX;
}

#if defined(DEBUG)
static void VPUPYCoCgDXTEncoderShow(VPUPCodecDXTEncoderRef encoder)
{
#pragma unused(encoder)
    printf("YCoCg DXT Encoder\n");
}
#endif

VPUPCodecDXTEncoderRef VPUPYCoCgDXTEncoderCreate()
{
    VPUPCodecDXTEncoderRef encoder = malloc(sizeof(struct VPUPCodecDXTEncoder));
    if (encoder)
    {
        encoder->pixelformat_function = VPUPYCoCgDXTEncoderWantedPixelFormat;
        encoder->destroy_function = VPUPYCoCgEncoderDestroy;
        encoder->encode_function = VPUPYCoCgDXTEncoderEncode;
#if defined(DEBUG)
        encoder->show_function = VPUPYCoCgDXTEncoderShow;
#endif
    }
    return encoder;
}
