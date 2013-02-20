//
//  Utility.h
//  Hap Codec
//
//  Created by Tom Butterworth on 04/10/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#ifndef HapCodec_Utility_h
#define HapCodec_Utility_h

#include <CoreFoundation/CoreFoundation.h>

#define kHapCodecSampleDescriptionExtension 'HAPe'

void addNumberToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32 );
void addDoubleToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, double numberDouble );
int roundUpToMultipleOf4( int n );
int roundUpToMultipleOf16( int n );

size_t dxtBytesForDimensions(int width, int height, OSType codecSubType);

/*
 Boolean isDXTPixelFormat(OSType fmt)
 
 For our purposes we treat YCoCg DXT as a DXT format
 as this function is used to differentiate cases where
 a DXT encode or decode stage is required.
 */
#define isDXTPixelFormat(fmt) (((fmt) == kHapCVPixelFormat_RGB_DXT1 \
                                || (fmt) == kHapCVPixelFormat_RGBA_DXT5 \
                                || (fmt) == kHapCVPixelFormat_YCoCg_DXT5) ? true : false)

SInt16 resourceIDForComponentType(OSType componentType, OSType resourceType);

#ifdef DEBUG
#define debug_print_function_call(glob) fprintf(stdout, "%p %s\n", (glob), __func__)
#define debug_print(glob, s) fprintf(stdout, "%p %s %s\n", (glob), __func__, s)
#define debug_print_err(glob, e) if ((e) != noErr) fprintf(stdout, "%p %s error code %d\n", (glob), __func__, (int)(e))
#else
#define debug_print_function_call(glob)
#define debug_print(glob, s)
#define debug_print_err(glob, e)
#endif
#endif
