//
//  Utility.m
//  Hap Codec
//
//  Created by Tom Butterworth on 06/01/2015.
//
//

#if defined(__APPLE__)
#include "Utility.h"
#import <Foundation/Foundation.h>

int hapCodecMaxTasks()
{
    /*
     Some Adobe products throw an error if they queue more than 10 buffers
     */
    NSString *name = [[NSProcessInfo processInfo] processName];
    if ([name containsString:@"Adobe"])
    {
        return 10;
    }
    return 20;
}

#endif
