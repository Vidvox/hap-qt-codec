//
//  PixelFormats.h
//  Hap Codec
//
//  Created by Tom on 01/10/2012.
//
//

#ifndef HapCodec_PixelFormats_h
#define HapCodec_PixelFormats_h

/*
 S3TC RGB DXT1
 */
#define kHapCVPixelFormat_RGB_DXT1 'DXt1'

/*
 S3TC RGBA DXT5
 */
#define kHapCVPixelFormat_RGBA_DXT5 'DXT5'

/*
 Scaled YCoCg in S3TC RGBA DXT5
 */
#define kHapCVPixelFormat_YCoCg_DXT5 'DYt5'

/*
 YCoCg stored Co Cg _ Y
 
 This is only accepted by the compressor, never emitted
 */
#define kHapCVPixelFormat_CoCgXY 'CCXY'

#endif
