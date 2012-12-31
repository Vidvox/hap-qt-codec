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
};

static void VPUPSquishEncoderDestroy(VPUPCodecDXTEncoderRef encoder)
{
    if (encoder)
    {
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
    if (src_pixel_format != k32RGBAPixelFormat) return 1;
    
    // We feed Squish block by block to handle extra bytes at the ends of rows
    
	uint8_t* dst_block = (uint8_t *)dst;
	int bytes_per_block = ( ( ((struct VPUPCodecSquishEncoder *)encoder)->flags & kDxt1 ) != 0 ) ? 8 : 16;
    
	
	for( int y = 0; y < height; y += 4 )
	{
        int remaining_height = height - y;
        
		for( int x = 0; x < width; x += 4 )
		{
            int remaining_width = width - x;
            
			// Pack the 4x4 pixel block
			uint8_t block_rgba[16*4];
            int mask;
            
            uint8_t *copy_src = (uint8_t *)src + (y * src_bytes_per_row) + (x * 4);
            uint8_t *copy_dst = block_rgba;
            
            if ((remaining_height < 4) || (remaining_width < 4) )
            {
                // If the source has dimensions which aren't a multiple of 4 we only copy the existing
                // pixels and use the mask argument to tell Squish to ignore the extras in the block
                mask = 0;
                for( int py = 0; py < 4; ++py )
                {
                    for( int px = 0; px < 4; ++px )
                    {
                        int sx = x + px;
                        int sy = y + py;

                        if( sx < width && sy < height )
                        {
                            for( int i = 0; i < 4; ++i )
                                *copy_dst++ = *copy_src++;
                            mask |= ( 1 << ( 4*py + px ) );
                        }
                        else
                        {
                            copy_dst += 4;
                        }
                    }
                    copy_src += src_bytes_per_row - (MIN(remaining_width, 4) * 4);
                }
            }
            else
            {
                // If all the pixels are in the frame, we can copy them in 4 x 4
                for (int j = 0; j < 4; j++) {
                    memcpy(copy_dst, copy_src, 4 * 4);
                    copy_src += src_bytes_per_row;
                    copy_dst += 4 * 4;
                }
                mask = 0xFFFF;
            }
			
			// Compress the block
			SquishCompressMasked( block_rgba, mask, dst_block, ((struct VPUPCodecSquishEncoder *)encoder)->flags, NULL );
			
			dst_block += bytes_per_block;
		}
	}
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
        encoder->base.pad_source_buffers = false;
        
        switch (quality) {
            case VPUPCodecSquishEncoderWorstQuality:
                encoder->flags = kColourRangeFit;
                break;
            case VPUPCodecSquishEncoderBestQuality:
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
