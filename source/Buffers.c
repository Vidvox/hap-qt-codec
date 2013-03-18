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
    #ifdef __APPLE__
    #include <libkern/OSAtomic.h>
    #else
    #include <Windows.h>
    #include <malloc.h>
    #endif

    typedef struct HapCodecBufferPool {
    #if defined(__APPLE__)
        OSQueueHead             queue;
    #elif defined(_WIN32) 
        PSLIST_HEADER           queue;
    #endif
        long                    size;
    } HapCodecBufferPool;

    typedef struct HapCodecBuffer {
    #if defined(__APPLE__)
        void                    *next;
    #elif defined(_WIN32) 
        SLIST_ENTRY             itemEntry;
    #endif
        void                    *buffer;
        HapCodecBufferPoolRef   pool;
    } HapCodecBuffer;

    HapCodecBufferPoolRef HapCodecBufferPoolCreate(long size)
    {
        HapCodecBufferPoolRef pool = (HapCodecBufferPoolRef)malloc(sizeof(HapCodecBufferPool));
        if (pool)
        {
    #if defined(__APPLE__)
            pool->queue.opaque1 = NULL; // OS_ATOMIC_QUEUE_INIT
            pool->queue.opaque2 = 0; // OS_ATOMIC_QUEUE_INIT
    #elif defined(_WIN32)
            pool->queue = (PSLIST_HEADER)_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
            if(pool->queue == NULL)
            {
                free(pool);
                pool = NULL;
            }
            else
            {
                InitializeSListHead(pool->queue);
            }
    #endif
            pool->size = size;
        }
        return pool;
    }

    static HapCodecBufferRef HapCodecBufferPoolTryCopyBuffer(HapCodecBufferPoolRef pool)
    {
    #if defined(__APPLE__)
        return OSAtomicDequeue(&pool->queue, offsetof(HapCodecBuffer, next));
    #elif defined(_WIN32)
        return (HapCodecBufferRef)InterlockedPopEntrySList(pool->queue);
    #endif
    }

    static void HapCodecBufferDestroy(HapCodecBufferRef buffer)
    {
        if (buffer->buffer) free(buffer->buffer);
    #if defined(__APPLE__)
        free(buffer);
    #elif defined(_WIN32)
        _aligned_free(buffer);
    #endif
    }

    void HapCodecBufferPoolDestroy(HapCodecBufferPoolRef pool)
    {
        if (pool)
        {
            HapCodecBufferRef buffer;
            do
            {
                buffer = HapCodecBufferPoolTryCopyBuffer(pool);
                if (buffer)
                {
                    HapCodecBufferDestroy(buffer);
                }
            } while (buffer != NULL);
    #if defined(_WIN32)
            _aligned_free(pool->queue);
    #endif
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
            HapCodecBuffer *buffer = HapCodecBufferPoolTryCopyBuffer(pool);
            if (buffer == NULL)
            {
    #if defined(__APPLE__)
                buffer = malloc(sizeof(HapCodecBuffer));
    #elif defined(_WIN32)
                buffer = (HapCodecBufferRef)_aligned_malloc(sizeof(HapCodecBuffer), MEMORY_ALLOCATION_ALIGNMENT);
    #endif
                if (buffer)
                {
    #if defined(__APPLE__)
                    buffer->next = NULL;
    #endif
                    buffer->buffer = malloc(pool->size);
                    buffer->pool = pool;
                    if (buffer->buffer == NULL)
                    {
                        HapCodecBufferDestroy(buffer);
                        buffer = NULL;
                    }
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
    #if defined(__APPLE__)
            OSAtomicEnqueue(&buffer->pool->queue, buffer, offsetof(HapCodecBuffer, next));
    #elif defined(_WIN32)
            InterlockedPushEntrySList(buffer->pool->queue, &(buffer->itemEntry));
    #endif
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
