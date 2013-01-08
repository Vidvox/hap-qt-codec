//
//  SquishEncoder.h
//  Hap Codec
//
//  Created by Tom on 01/10/2012.
//
//

#ifndef HapCodec_SquishEncoder_h
#define HapCodec_SquishEncoder_h

#include "DXTEncoder.h"

enum HapCodecSquishEncoderQuality {
    HapCodecSquishEncoderWorstQuality = 0,
    HapCodecSquishEncoderMediumQuality = 1,
    HapCodecSquishEncoderBestQuality = 2
};

typedef int HapCodecSquishEncoderQuality;

// pixelFormat must be one of the vanilla DXT formats

HapCodecDXTEncoderRef HapCodecSquishEncoderCreate(HapCodecSquishEncoderQuality quality, OSType pixelFormat);

#endif
