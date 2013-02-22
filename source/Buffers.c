/*
 Buffers.c
 Hap Codec
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 * Neither the name of Hap nor the name of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
