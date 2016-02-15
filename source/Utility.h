/*
 Utility.h
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

#ifndef HapCodec_Utility_h
#define HapCodec_Utility_h

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <CoreFoundation.h>
#include <stdio.h>
#endif
#include "HapPlatform.h"

void addNumberToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32 );
void addDoubleToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, double numberDouble );
int roundUpToMultipleOf4( int n );
int roundDownToMultipleOf4( int n );
int roundUpToMultipleOf16( int n );

unsigned long dxtBytesForDimensions(int width, int height, OSType codecSubType);

/*
 Boolean isDXTPixelFormat(OSType fmt)
 
 For our purposes we treat YCoCg DXT as a DXT format
 as this function is used to differentiate cases where
 a DXT encode or decode stage is required.
 */
#define isDXTPixelFormat(fmt) (((fmt) == kHapCVPixelFormat_RGB_DXT1 \
                                || (fmt) == kHapCVPixelFormat_RGBA_DXT5 \
                                || (fmt) == kHapCVPixelFormat_YCoCg_DXT5 \
                                || (fmt) == kHapCVPixelFormat_YCoCg_DXT5_A_RGTC1) ? true : false)

SInt16 resourceIDForComponentType(OSType componentType, OSType resourceType);

int hapCodecMaxTasks();

#ifdef DEBUG
#if defined(_WIN32)
#define debug_print_function_call(glob) debug_print((glob), NULL)
#define debug_print(glob, s) debug_print_s((glob), HAP_FUNC, (s))
#define debug_print_err(glob, e) { if ((e) != noErr) debug_print_i(glob, HAP_FUNC, (e)); }
void debug_print_s(void *glob, const char *func, const char *s);
void debug_print_i(void *glob, const char *func, int e);
#else
#define debug_print_function_call(glob) fprintf(stdout, "%p %s\n", (glob), HAP_FUNC)
#define debug_print(glob, s) fprintf(stdout, "%p %s %s\n", (glob), HAP_FUNC, s)
#define debug_print_err(glob, e) if ((e) != noErr) fprintf(stdout, "%p %s error code %d\n", (glob), HAP_FUNC, (int)(e))
#endif
#else
#define debug_print_function_call(glob)
#define debug_print(glob, s)
#define debug_print_err(glob, e)
#endif
#endif
