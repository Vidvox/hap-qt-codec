//
//  Buffers.c
//  Hap Codec
//
//  Created by Tom on 08/05/2011.
//  Copyright 2011 Tom Butterworth. All rights reserved.
//

#include "Buffers.h"
#include <libkern/OSAtomic.h>

typedef struct HapCodecBufferPool {
    OSQueueHead             queue;
    long                    size;
} HapCodecBufferPool;

typedef struct HapCodecBuffer {
    void                    *buffer;
    void                    *next;
    HapCodecBufferPoolRef   pool;
} HapCodecBuffer;

HapCodecBufferPoolRef HapCodecBufferPoolCreate(long size)
{
    HapCodecBufferPoolRef pool = malloc(sizeof(HapCodecBufferPool));
    if (pool)
    {
        pool->queue.opaque1 = NULL; // OS_ATOMIC_QUEUE_INIT
        pool->queue.opaque2 = 0; // OS_ATOMIC_QUEUE_INIT
        pool->size = size;
    }
    return pool;
}

void HapCodecBufferPoolDestroy(HapCodecBufferPoolRef pool)
{
    if (pool)
    {
        HapCodecBufferRef buffer;
        do
        {
            buffer = OSAtomicDequeue(&pool->queue, offsetof(HapCodecBuffer, next));
            if (buffer)
            {
                free(buffer->buffer);
                free(buffer);
            }
        } while (buffer != NULL);
        free(pool);
    }
}

long HapCodecBufferPoolGetBufferSize(HapCodecBufferPoolRef pool)
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

HapCodecBufferRef HapCodecBufferCreate(HapCodecBufferPoolRef pool)
{
    if (pool)
    {
        HapCodecBuffer *buffer = OSAtomicDequeue(&pool->queue, offsetof(HapCodecBuffer, next));
        if (buffer == NULL)
        {
            buffer = malloc(sizeof(HapCodecBuffer));
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

void HapCodecBufferReturn(HapCodecBufferRef buffer)
{
    if (buffer && buffer->pool)
    {
        OSAtomicEnqueue(&buffer->pool->queue, buffer, offsetof(HapCodecBuffer, next));
    }
}

void *HapCodecBufferGetBaseAddress(HapCodecBufferRef buffer)
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

long HapCodecBufferGetSize(HapCodecBufferRef buffer)
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
