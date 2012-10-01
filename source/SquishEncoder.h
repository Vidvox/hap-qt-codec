//
//  SquishEncoder.h
//  VPUCodec
//
//  Created by Tom on 01/10/2012.
//
//

#ifndef VPUCodec_SquishEncoder_h
#define VPUCodec_SquishEncoder_h

#include "DXTEncoder.h"

enum VPUPCodecSquishEncoderQuality {
    VPUPCodecSquishEncoderWorstQuality = 0,
    VPUPCodecSquishEncoderMediumQuality = 1,
    VPUPCodecSquishEncoderBestQuality = 2
};

typedef int VPUPCodecSquishEncoderQuality;

// pixelFormat must be one of the vanilla DXT formats

VPUPCodecDXTEncoderRef VPUPSquishEncoderCreate(VPUPCodecSquishEncoderQuality quality, OSType pixelFormat);

#endif
