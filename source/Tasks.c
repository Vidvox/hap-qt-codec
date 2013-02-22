/*
 Tasks.c
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

#include "Tasks.h"
#include <pthread.h>
#include <sys/sysctl.h>
#include <libkern/OSAtomic.h>

typedef struct HapCodecTaskRecord
{
    unsigned int                group;
    HapCodecTaskWorkFunction    func;
    void                        *context;
    unsigned int                running;
} HapCodecTaskRecord;

// TODO: contain these in a struct which we malloc/free reducing our loaded code footprint and
// making init/cleanup faster (we can set the pointer fast then teardown outside the lock)
static OSSpinLock mGlobalLock = OS_SPINLOCK_INIT;
static unsigned int mSenderCount = 0U;
static int mInitted = 0;
static unsigned int mThreadCount = 0U;
static pthread_mutex_t mThreadLock;
static pthread_cond_t mTaskWaitCond;
static pthread_cond_t mFeedWaitCond;
static HapCodecTaskRecord *mTasks;

static int HapCodecTasksGetMaximumThreadCount();

static void *HapCodecTasksThread(void *info)
{
#pragma unused (info)
    int done = 0;
    pthread_mutex_lock(&mThreadLock);
    do
    {
        int i = 0;
        int ran = 0;
        for (i = 0; i < HapCodecTasksGetMaximumThreadCount(); i++)
        {
            if (mTasks[i].func != NULL && mTasks[i].running == 0)
            {
                mTasks[i].running = 1U;
                pthread_mutex_unlock(&mThreadLock);
                mTasks[i].func(mTasks[i].context);
                pthread_mutex_lock(&mThreadLock);
                mTasks[i].func = NULL;
                mTasks[i].running = 0;
                ran = 1;
                break;
            }
        }
        if (ran == 0 && mSenderCount == 0)
        {
            done = 1;
        }
        if (ran == 1)
        {
            pthread_cond_signal(&mFeedWaitCond); // TODO: check we actually need to signal perhaps by wrapping our semaphores into a counting semaphore pseudo-class
        }
        if (done == 0 && ran == 0)
        {
            pthread_cond_wait(&mTaskWaitCond, &mThreadLock);
        }
    }
    while (done == 0);
    mThreadCount--;
    if (mThreadCount == 0)
    {
        pthread_cond_signal(&mFeedWaitCond);
    }
    pthread_mutex_unlock(&mThreadLock);
    return NULL;
}

static int HapCodecTasksGetMaximumThreadCount()
{
    static int mMaxThreadCount = 0;
    if (mMaxThreadCount == 0)
    {
        int mib[2] = {CTL_HW, HW_NCPU};
        size_t len = sizeof(mMaxThreadCount);
        
        // could use something like and watch for changes to eg power state
        // sysctlbyname("hw.activecpu", &ncpu, &len, NULL, 0);
        
        int result = sysctl(mib, 2, &mMaxThreadCount, &len, NULL, 0);
        if (result != 0)
        {
            mMaxThreadCount = 4; // conservative guess if we couldn't get a value
        }
    }
    return mMaxThreadCount;
}

static int HapCodecTasksInit()
{
    if (mInitted == 0)
    {
        mThreadCount = 0U;
        // This isn't ideal doing these longer operations inside a spinlock... once at load using an initializer?
        if (pthread_mutex_init(&mThreadLock, NULL) != 0)
        {
            return 1;
        }
        if (pthread_cond_init(&mTaskWaitCond, NULL) != 0)
        {
            pthread_mutex_destroy(&mThreadLock);
            return 1;
        }
        if (pthread_cond_init(&mFeedWaitCond, NULL) != 0)
        {
            pthread_mutex_destroy(&mThreadLock);
            pthread_cond_destroy(&mTaskWaitCond);
            return 1;
        }
        mTasks = malloc(sizeof(HapCodecTaskRecord) * HapCodecTasksGetMaximumThreadCount());
        int i;
        for (i = 0; i < HapCodecTasksGetMaximumThreadCount(); i++)
        {
            mTasks[i].running = 0;
            mTasks[i].func = NULL;
        }
        mInitted = 1;
    }
    return 0;
}

static void HapCodecTasksCleanup(void)
{
    if (mInitted != 0)
    {
        // TODO: we could do this on the last thread if we are sure we can avoid creating a new instance over the top of it
        pthread_mutex_lock(&mThreadLock);
        pthread_cond_broadcast(&mTaskWaitCond);
        while (mThreadCount > 0)
        {
            pthread_cond_wait(&mFeedWaitCond, &mThreadLock);
        }
        pthread_mutex_unlock(&mThreadLock);
        pthread_mutex_destroy(&mThreadLock);
        pthread_cond_destroy(&mTaskWaitCond);
        pthread_cond_destroy(&mFeedWaitCond);
        free(mTasks);
        mTasks = NULL;
        mInitted = 0;
    }
}

void HapCodecTasksWillStart(void)
{
    OSSpinLockLock(&mGlobalLock);
    mSenderCount++;
    if (mSenderCount == 1U)
    {
        HapCodecTasksInit();
    }
    OSSpinLockUnlock(&mGlobalLock);
}

void HapCodecTasksWillStop(void)
{
    OSSpinLockLock(&mGlobalLock);
    mSenderCount--;
    if (mSenderCount == 0U)
    {
        // clear state and stop our threads outside of the lock
        // TODO:
        HapCodecTasksCleanup();
    }
    OSSpinLockUnlock(&mGlobalLock);
}

void HapCodecTasksAddTask(HapCodecTaskWorkFunction task, unsigned int group, void *context)
{
    pthread_mutex_lock(&mThreadLock);
    // Check to see if we can spawn a new thread for this task
    if (mThreadCount < HapCodecTasksGetMaximumThreadCount())
    {
        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
        if (pthread_create(&thread, &attr, HapCodecTasksThread, NULL) == 0)
        {
            mThreadCount++;
        }
    }
    // setup the task in a free slot, waiting for one if necessary
    int i;
    int found = 0;
    do
    {
        for (i = 0; i < HapCodecTasksGetMaximumThreadCount(); i++)
        {
            if (mTasks[i].func == NULL)
            {
                found = 1;
                mTasks[i].func = task;
                mTasks[i].group = group;
                mTasks[i].context = context;
                break;
            }
        }
        if (found == 0)
        {
            pthread_cond_wait(&mFeedWaitCond, &mThreadLock);
        }
    }
    while (found == 0);
    // signal the task thread to wake
    pthread_cond_signal(&mTaskWaitCond);
    pthread_mutex_unlock(&mThreadLock);

    
}

void HapCodecTasksWaitForGroupToComplete(unsigned int group)
{
    pthread_mutex_lock(&mThreadLock);
    int done = 0;
    do
    {
        int i;
        done = 1;
        for (i = 0; i < HapCodecTasksGetMaximumThreadCount(); i++)
        {
            if (mTasks[i].func != NULL && mTasks[i].group == group)
            {
                done = 0;
            }
        }
        if (done == 0)
        {
            pthread_cond_wait(&mFeedWaitCond, &mThreadLock);
        }
    } while (done == 0);
    pthread_mutex_unlock(&mThreadLock);
}

unsigned int HapCodecTasksNewGroup(void)
{
    static int32_t mGroup = 0;
    return OSAtomicIncrement32(&mGroup);
}
