//
//  Buffers.c
//  VPUCodec
//
//  Created by Tom on 08/05/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//

#include "Buffers.h"
#include <libkern/OSAtomic.h>

typedef struct VPUCodecBufferPool {
    OSQueueHead             queue;
    long                    size;
} VPUCodecBufferPool;

typedef struct VPUCodecBuffer {
    void                    *buffer;
    void                    *next;
    VPUCodecBufferPoolRef   pool;
} VPUCodecBuffer;

VPUCodecBufferPoolRef VPUCodecCreateBufferPool(long size)
{
    VPUCodecBufferPoolRef pool = malloc(sizeof(VPUCodecBufferPool));
    if (pool)
    {
        pool->queue.opaque1 = NULL; // OS_ATOMIC_QUEUE_INIT
        pool->queue.opaque2 = 0; // OS_ATOMIC_QUEUE_INIT
        pool->size = size;
    }
    return pool;
}

void VPUCodecDestroyBufferPool(VPUCodecBufferPoolRef pool)
{
    if (pool)
    {
        VPUCodecBufferRef buffer;
        do
        {
            buffer = OSAtomicDequeue(&pool->queue, offsetof(VPUCodecBuffer, next));
            if (buffer)
            {
                free(buffer->buffer);
                free(buffer);
            }
        } while (buffer != NULL);
        free(pool);
    }
}

long VPUCodecGetBufferPoolBufferSize(VPUCodecBufferPoolRef pool)
{
    if (pool)
    {
        return pool->size;
    }
    else
    {
        return 0;
    }
}

VPUCodecBufferRef VPUCodecGetBuffer(VPUCodecBufferPoolRef pool)
{
    if (pool)
    {
        VPUCodecBuffer *buffer = OSAtomicDequeue(&pool->queue, offsetof(VPUCodecBuffer, next));
        if (buffer == NULL)
        {
            buffer = malloc(sizeof(VPUCodecBuffer));
            if (buffer)
            {
                buffer->next = NULL;
                buffer->buffer = malloc(pool->size);
                buffer->pool = pool;
            }
        }
        return buffer;
    }
    else
    {
        return NULL;
    }
}

void VPUCodecReturnBuffer(VPUCodecBufferRef buffer)
{
    if (buffer && buffer->pool)
    {
        OSAtomicEnqueue(&buffer->pool->queue, buffer, offsetof(VPUCodecBuffer, next));
    }
}

void *VPUCodecGetBufferBaseAddress(VPUCodecBufferRef buffer)
{
    if (buffer)
    {
        return buffer->buffer;
    }
    else
    {
        return NULL;
    }
}

long VPUCodecGetBufferSize(VPUCodecBufferRef buffer)
{
    if (buffer && buffer->pool)
    {
        return buffer->pool->size;
    }
    else
    {
        return 0;
    }
}
