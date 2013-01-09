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

#define kHapCodecSubtype 'VPUV'
#define kHapYCoCgCodecSubtype 'HapY'

#define kHapCodecSampleDescriptionExtension 'HAPe'

void addNumberToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32 );
void addDoubleToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, double numberDouble );
Boolean dictionaryHasValueForKeyOfTypeID( CFDictionaryRef dictionary, CFStringRef key, CFTypeID expectedCFType );
int roundUpToMultipleOf4( int n );
int roundUpToMultipleOf16( int n );

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
