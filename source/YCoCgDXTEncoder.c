//
//  YCoCgDXTEncoder.c
//  Hap Codec
//
//  Created by Tom on 01/10/2012.
//
//

#include "YCoCgDXTEncoder.h"
#include "YCoCgDXT.h"
#include "PixelFormats.h"

static void HapCodecYCoCgEncoderDestroy(HapCodecDXTEncoderRef encoder)
{
    free(encoder);
}

static int HapCodecYCoCgDXTEncoderEncode(HapCodecDXTEncoderRef encoder,
                                     const void *src,
                                     unsigned int src_bytes_per_row,
                                     OSType src_pixel_format,
                                     void *dst,
                                     unsigned int width,
                                     unsigned int height)
{
#pragma unused(encoder)
    if (src_pixel_format != kHapCVPixelFormat_CoCgXY) return 1;
    CompressYCoCgDXT5(src, dst, width, height, src_bytes_per_row);
    return 0;
}

static OSType HapCodecYCoCgDXTEncoderWantedPixelFormat(HapCodecDXTEncoderRef encoder, OSType sourceFormat)
{
#pragma unused(encoder, sourceFormat)
    return kHapCVPixelFormat_CoCgXY;
}

#if defined(DEBUG)
static const char *HapCodecYCoCgDXTEncoderDescribe(HapCodecDXTEncoderRef encoder)
{
#pragma unused(encoder)
    return "YCoCg DXT5 Encoder";
}
#endif

HapCodecDXTEncoderRef HapCodecYCoCgDXTEncoderCreate()
{
    HapCodecDXTEncoderRef encoder = malloc(sizeof(struct HapCodecDXTEncoder));
    if (encoder)
    {
        encoder->pixelformat_function = HapCodecYCoCgDXTEncoderWantedPixelFormat;
        encoder->destroy_function = HapCodecYCoCgEncoderDestroy;
        encoder->encode_function = HapCodecYCoCgDXTEncoderEncode;
#if defined(DEBUG)
        encoder->describe_function = HapCodecYCoCgDXTEncoderDescribe;
#endif
        encoder->pad_source_buffers = false;
    }
    return encoder;
}
