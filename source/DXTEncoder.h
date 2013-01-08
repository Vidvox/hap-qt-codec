//
//  DXTEncoder.h
//  Hap Codec
//
//  Created by Tom on 30/09/2012.
//
//

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
    bool pad_source_buffers;
};

#define HapCodecDXTEncoderDestroy(x) if ((x) && (x)->destroy_function) (x)->destroy_function((x))

#endif
