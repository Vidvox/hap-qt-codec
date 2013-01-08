//
//  Buffers.h
//  Hap Codec
//
//  Created by Tom on 08/05/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//


typedef struct HapCodecBufferPool *HapCodecBufferPoolRef;

typedef struct HapCodecBuffer *HapCodecBufferRef;


HapCodecBufferPoolRef HapCodecBufferPoolCreate(long size);
void HapCodecBufferPoolDestroy(HapCodecBufferPoolRef pool);
long HapCodecBufferPoolGetBufferSize(HapCodecBufferPoolRef pool);

HapCodecBufferRef HapCodecBufferCreate(HapCodecBufferPoolRef pool);
void HapCodecBufferReturn(HapCodecBufferRef buffer);
void *HapCodecBufferGetBaseAddress(HapCodecBufferRef buffer);
long HapCodecBufferGetSize(HapCodecBufferRef buffer);
