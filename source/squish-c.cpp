//
//  squish-c.cpp
//  Hap Codec
//
//  Created by Tom Butterworth on 26/04/2011.
//

#include "squish-c.h"
#include "squish.h"

extern "C" {

void SquishCompressMasked( u8 const* rgba, int mask, void* block, int flags, float* metric = 0 )
{
    squish::CompressMasked(rgba, mask, block, flags, metric);
}

void SquishCompress(const u8* rgba, void* block, int flags, float* metric)
{
    squish::CompressMasked( rgba, 0xffff, block, flags, metric );
}
    
void SquishDecompress(u8* rgba, const void* block, int flags )
{
    squish::Decompress(rgba, block, flags);
}

int SquishGetStorageRequirements( int width, int height, int flags )
{
    return squish::GetStorageRequirements(width, height, flags);
}
    
void SquishCompressImage(const u8* rgba, int width, int height, void* blocks, int flags, float* metric)
{
    squish::CompressImage(rgba, width, height, blocks, flags, metric);
}
    
void SquishDecompressImage( u8* rgba, int width, int height, void const* blocks, int flags )
{
    squish::DecompressImage(rgba, width, height, blocks, flags);
}

}
