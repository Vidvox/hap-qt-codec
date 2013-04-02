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

#if defined(__APPLE__)
struct HapCodecTaskGroup {
    HapCodecTaskWorkFunction    task;
    dispatch_group_t            group;
    dispatch_queue_t            queue;
    dispatch_semaphore_t        semaphore;
};

void HapCodecTasksAddTask(HapCodecTaskGroupRef group, void *context)
{
    if (group && group->group && group->queue && group->task && group->semaphore)
    {
        dispatch_semaphore_wait(group->semaphore, DISPATCH_TIME_FOREVER);
        // Copy values in case group is released before execution
        HapCodecTaskWorkFunction task = group->task;
        dispatch_semaphore_t semaphore = group->semaphore;
        // Retain semaphore for block
        dispatch_retain(semaphore);
        dispatch_group_async(group->group, group->queue, ^{
            task(context);
            dispatch_semaphore_signal(semaphore);
            dispatch_release(semaphore);
        });
    }
}

void HapCodecTasksWaitForGroupToComplete(HapCodecTaskGroupRef group)
{
    if (group && group->group) dispatch_group_wait(group->group, DISPATCH_TIME_FOREVER);
}

HapCodecTaskGroupRef HapCodecTasksCreateGroup(HapCodecTaskWorkFunction task, unsigned int maxTasks)
{
    HapCodecTaskGroupRef group = NULL;
    if (task)
    {
        group = malloc(sizeof(struct HapCodecTaskGroup));
        if (group)
        {
            group->task = task;
            group->group = dispatch_group_create();
            group->queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
            group->semaphore = dispatch_semaphore_create(maxTasks);
            if (group->group == NULL || group->queue == NULL || group->semaphore == NULL)
            {
                HapCodecTasksDestroyGroup(group);
                group = NULL;
            }
        }
    }
    return group;
}

void HapCodecTasksDestroyGroup(HapCodecTaskGroupRef group)
{
    if (group)
    {
        if (group->group) dispatch_release(group->group);
        if (group->queue) dispatch_release(group->queue);
        if (group->semaphore) dispatch_release(group->semaphore);
        free(group);
    }
}

#else

#include <Windows.h>
#include <malloc.h>

struct HapCodecTaskGroup {
    HapCodecTaskWorkFunction    task;
    unsigned int                maxTasks;
    PTP_POOL                    pool;
    PTP_WORK                    work;
    PTP_CLEANUP_GROUP           cleanup;
    /*
    Below this line members are shared between threads
    */ 
    HANDLE                      sharedSemaphore;
    CRITICAL_SECTION            sharedCritical;
    void **                     sharedContexts;
};

static void NTAPI HapCodecTasksWorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work)
{
    struct HapCodecTaskGroup *group = (struct HapCodecTaskGroup *)context;
    if (group)
    {
        void *userContext = NULL;
        unsigned int i;

        EnterCriticalSection(&(group->sharedCritical));

        for (i = 0; i < group->maxTasks; i++)
        {
            if (group->sharedContexts[i] != NULL)
            {
                userContext = group->sharedContexts[i];
                group->sharedContexts[i] = NULL;
                break;
            }
        }

        LeaveCriticalSection(&(group->sharedCritical));

        group->task(userContext);

        ReleaseSemaphoreWhenCallbackReturns(instance, group->sharedSemaphore, 1);
    }
}

static void NTAPI HapCodecTasksCleanupCallback(PVOID ObjectContext, PVOID CleanupContext)
{
    // For now we have no cleanup to perform here
}

void HapCodecTasksAddTask(HapCodecTaskGroupRef group, void *context)
{
    if (group && group->work && group->sharedSemaphore)
    {
        unsigned int i;
        // TODO: If we can coordinate waiting for ALL threads to finish
        // we can make this much simpler using
        // TrySubmitThreadpoolCallback(....) and 
        WaitForSingleObject(group->sharedSemaphore, INFINITE);
        
        EnterCriticalSection(&(group->sharedCritical));
        for (i = 0; i < group->maxTasks; i++)
        {
            if (group->sharedContexts[i] == NULL)
            {
                group->sharedContexts[i] = context;
                break;
            }
        }
        LeaveCriticalSection(&(group->sharedCritical));
        
        SubmitThreadpoolWork(group->work);
    }
}

void HapCodecTasksWaitForGroupToComplete(HapCodecTaskGroupRef group)
{
    if (group && group->work) WaitForThreadpoolWorkCallbacks(group->work, FALSE);
}

HapCodecTaskGroupRef HapCodecTasksCreateGroup(HapCodecTaskWorkFunction task, unsigned int maxTasks)
{
    HapCodecTaskGroupRef group = NULL;
    if (task)
    {
        group = (HapCodecTaskGroupRef)malloc(sizeof(struct HapCodecTaskGroup));
        if (group)
        {
            BOOL success = FALSE;
            TP_CALLBACK_ENVIRON         callbackEnviron;
            SYSTEM_INFO system;
            
            GetSystemInfo(&system);

            InitializeThreadpoolEnvironment(&callbackEnviron);

            InitializeCriticalSection(&group->sharedCritical);

            group->task = task;
            group->maxTasks = maxTasks;
            group->pool = CreateThreadpool(NULL);
            group->work = NULL;
            group->cleanup = NULL;
            group->sharedSemaphore = NULL;
            group->sharedContexts = NULL;

            if (group->pool)
            {
                SetThreadpoolThreadMaximum(group->pool, system.dwNumberOfProcessors);
                success = SetThreadpoolThreadMinimum(group->pool, 1);
            }
            if (success)
            {
                SetThreadpoolCallbackPool(&callbackEnviron, group->pool);
            }
            if (success)
            {
                group->cleanup = CreateThreadpoolCleanupGroup();
                if (group->cleanup == NULL) success = FALSE;
            }
            if (success)
            {
                SetThreadpoolCallbackCleanupGroup(&callbackEnviron, group->cleanup, (PTP_CLEANUP_GROUP_CANCEL_CALLBACK)HapCodecTasksCleanupCallback);
            }
            if (success)
            {
                group->work = CreateThreadpoolWork((PTP_WORK_CALLBACK)HapCodecTasksWorkCallback, group, &callbackEnviron);
                if (group->work == NULL) success = FALSE;
            }
            if (success)
            {
                group->sharedSemaphore = CreateSemaphore(NULL, maxTasks, maxTasks, NULL);
                if (group->sharedSemaphore == NULL) success = FALSE;
            }
            if (success)
            {
                unsigned int i;
                group->sharedContexts = (void **)malloc(sizeof(void *) * maxTasks);
                if (group->sharedContexts)
                {
                    for (i = 0; i < maxTasks; i++)
                        group->sharedContexts[i] = NULL;
                }
                else
                {
                    success = FALSE;
                }
            }
            if (success == FALSE)
            {
                HapCodecTasksDestroyGroup(group);
                group = NULL;
            }
            DestroyThreadpoolEnvironment(&callbackEnviron);
        }
    }
    return group;
}

void HapCodecTasksDestroyGroup(HapCodecTaskGroupRef group)
{
    if (group)
    {
        DeleteCriticalSection(&(group->sharedCritical));
        if (group->cleanup)
        {
            CloseThreadpoolCleanupGroupMembers(group->cleanup, FALSE, NULL);
            CloseThreadpoolCleanupGroup(group->cleanup);
        }
        if (group->pool)
        {
            CloseThreadpool(group->pool);
        }
        if (group->sharedSemaphore)
        {
            CloseHandle(group->sharedSemaphore);
        }
        if (group->sharedContexts)
        {
            free(group->sharedContexts);
        }
        free(group);
    }
}
#endif
