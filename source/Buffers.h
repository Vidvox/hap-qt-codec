//
//  Buffers.h
//  VPUCodec
//
//  Created by Tom on 08/05/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//


typedef struct VPUCodecBufferPool *VPUCodecBufferPoolRef;

typedef struct VPUCodecBuffer *VPUCodecBufferRef;


VPUCodecBufferPoolRef VPUCodecCreateBufferPool(long size);
void VPUCodecDestroyBufferPool(VPUCodecBufferPoolRef pool);
long VPUCodecGetBufferPoolBufferSize(VPUCodecBufferPoolRef pool);

VPUCodecBufferRef VPUCodecGetBuffer(VPUCodecBufferPoolRef pool);
void VPUCodecReturnBuffer(VPUCodecBufferRef buffer);
void *VPUCodecGetBufferBaseAddress(VPUCodecBufferRef buffer);
long VPUCodecGetBufferSize(VPUCodecBufferRef buffer);
