//
//  DXTEncoder.h
//  VPUCodec
//
//  Created by Tom on 30/09/2012.
//
//

#ifndef VPUCodec_DXTEncoder_h
#define VPUCodec_DXTEncoder_h

#include <MacTypes.h>

/*
 An encoder is simply a pointer to a struct which provides functions for the codec to call
 as needed.
 
 The encode function will be called in parallel so must be thread-safe.
 */

typedef struct VPUPCodecDXTEncoder *VPUPCodecDXTEncoderRef;

typedef void(*VPUPCodecDXTEncoder_DestroyFunction)(VPUPCodecDXTEncoderRef encoder);

typedef OSType(*VPUPCodecDXTEncoder_WantedPixelFormatFunction)(VPUPCodecDXTEncoderRef encoder, OSType sourcePixelFormat);

typedef int(*VPUPCodecDXTEncoder_EncodeFunction)(VPUPCodecDXTEncoderRef encoder,
                                                 const void *src,
                                                 unsigned int src_bytes_per_row,
                                                 OSType src_pixel_format,
                                                 void *dst,
                                                 unsigned int width,
                                                 unsigned int height);

#if defined(DEBUG)
typedef void(*VPUPCodecDXTEncoder_ShowFunction)(VPUPCodecDXTEncoderRef encoder);
#endif

struct VPUPCodecDXTEncoder {
    VPUPCodecDXTEncoder_WantedPixelFormatFunction pixelformat_function;
    VPUPCodecDXTEncoder_EncodeFunction encode_function;
    VPUPCodecDXTEncoder_DestroyFunction destroy_function;
#if defined(DEBUG)
    VPUPCodecDXTEncoder_ShowFunction show_function;
#endif
};


#endif
