//
//  VPUCodecUtility.c
//  VPUCodec
//
//  Created by Tom Butterworth on 04/10/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

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