//
//  Tasks.h
//  VPUCodec
//
//  Created by Tom Butterworth on 09/05/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

typedef void (*VPUCodecTaskWorkFunction)(void *context);

void VPUCodecWillStartTasks();
void VPUCodecWillStopTasks();
unsigned int VPUCodecNewTaskGroup();
void VPUCodecTask(VPUCodecTaskWorkFunction task, unsigned int group, void *context);
void VPUCodecWaitForTasksToComplete(unsigned int group);