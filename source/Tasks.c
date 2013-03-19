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
