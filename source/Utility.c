//
//  Utility.c
//  Hap Codec
//
//  Created by Tom Butterworth on 04/10/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include "Utility.h"
#include <QuickTime/QuickTime.h>
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

size_t dxtBytesForDimensions(int width, int height, OSType codecSubType)
{
    size_t length = roundUpToMultipleOf4(width) * roundUpToMultipleOf4(height);
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
