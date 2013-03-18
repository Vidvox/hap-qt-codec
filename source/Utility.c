/*
 Utility.c
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

#if defined(__APPLE__)
#include <QuickTime/QuickTime.h>
#elif defined(_WIN32)
//#include <QuickTime.h>
#include <ConditionalMacros.h>
#include <ImageCodec.h>
#endif

#include "Utility.h"
#include "HapCodecSubTypes.h"

// Utility to add an SInt32 to a CFMutableDictionary.
void addNumberToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32 )
{
	CFNumberRef number = CFNumberCreate( NULL, kCFNumberSInt32Type, &numberSInt32 );
	if( ! number ) 
		return;
	CFDictionaryAddValue( dictionary, key, number );
	CFRelease( number );
}

// Utility to add a double to a CFMutableDictionary.
void addDoubleToDictionary( CFMutableDictionaryRef dictionary, CFStringRef key, double numberDouble )
{
	CFNumberRef number = CFNumberCreate( NULL, kCFNumberDoubleType, &numberDouble );
	if( ! number ) 
		return;
	CFDictionaryAddValue( dictionary, key, number );
	CFRelease( number );
}

// Utility to round up to a multiple of 16.
int roundUpToMultipleOf16( int n )
{
	if( 0 != ( n & 15 ) )
		n = ( n + 15 ) & ~15;
	return n;
}

// Utility to round up to a multiple of 4.
int roundUpToMultipleOf4( int n )
{
	if( 0 != ( n & 3 ) )
		n = ( n + 3 ) & ~3;
	return n;
}

unsigned long dxtBytesForDimensions(int width, int height, OSType codecSubType)
{
    unsigned long length = roundUpToMultipleOf4(width) * roundUpToMultipleOf4(height);
    if (codecSubType == kHapCodecSubType) length /= 2;
    return length;
}

SInt16 resourceIDForComponentType(OSType componentType, OSType resourceType)
{    
    if (resourceType == codecInfoResourceType)
    {
        switch (componentType) {
            case kHapCodecSubType:
                return 256;
            case kHapAlphaCodecSubType:
                return 456;
            case kHapYCoCgCodecSubType:
                return 356;
            default:
                return 0;
        }
    }
    return 0;
}
