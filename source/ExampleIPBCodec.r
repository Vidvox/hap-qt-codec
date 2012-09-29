/*

File: ExampleIPBCodec.r

Abstract: Resource file for ExampleIPBCodec

Version: 1.0

© Copyright 2005 Apple Computer, Inc. All rights reserved.

IMPORTANT:  This Apple software is supplied to 
you by Apple Computer, Inc. ("Apple") in 
consideration of your agreement to the following 
terms, and your use, installation, modification 
or redistribution of this Apple software 
constitutes acceptance of these terms.  If you do 
not agree with these terms, please do not use, 
install, modify or redistribute this Apple 
software.

In consideration of your agreement to abide by 
the following terms, and subject to these terms, 
Apple grants you a personal, non-exclusive 
license, under Apple's copyrights in this 
original Apple software (the "Apple Software"), 
to use, reproduce, modify and redistribute the 
Apple Software, with or without modifications, in 
source and/or binary forms; provided that if you 
redistribute the Apple Software in its entirety 
and without modifications, you must retain this 
notice and the following text and disclaimers in 
all such redistributions of the Apple Software. 
Neither the name, trademarks, service marks or 
logos of Apple Computer, Inc. may be used to 
endorse or promote products derived from the 
Apple Software without specific prior written 
permission from Apple.  Except as expressly 
stated in this notice, no other rights or 
licenses, express or implied, are granted by 
Apple herein, including but not limited to any 
patent rights that may be infringed by your 
derivative works or by other works in which the 
Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS 
IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR 
IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED 
WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING 
THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
OR IN COMBINATION WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY 
SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF 
THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER 
UNDER THEORY OF CONTRACT, TORT (INCLUDING 
NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN 
IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF 
SUCH DAMAGE.

*/


#define TARGET_REZ_CARBON_MACHO 1
#define TARGET_REZ_WIN32 0

#define DECO_BUILD 1
#define COMP_BUILD 1

#define thng_RezTemplateVersion 2

#if TARGET_REZ_CARBON_MACHO
    #include <Carbon/Carbon.r>
    #include <QuickTime/QuickTime.r>
#else
    #include "ConditionalMacros.r"
    #include "MacTypes.r"
    #include "Components.r"
    #include "ImageCodec.r"
#endif

#include "ExampleIPBCodecVersion.h"

#define	kExampleIPBCodecFormatType	'EIPB'
#define	kExampleIPBCodecFormatName	"ExampleIPB"

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kExampleIPBCompressorFlags ( codecInfoDoes32 | codecInfoDoesTemporal | codecInfoDoesReorder | codecInfoDoesRateConstrain )
#define kExampleIPBDecompressorFlags ( codecInfoDoes32 | codecInfoDoesTemporal )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24-bit depths
#define kIPBFormatFlags	( codecInfoDepth24 )

// Component Description
resource 'cdci' (256) {
	kExampleIPBCodecFormatName,		// Type
	1,								// Version
	1,								// Revision level
	'appl',							// Manufacturer
	kExampleIPBDecompressorFlags,	// Decompression Flags
	kExampleIPBCompressorFlags,		// Compression Flags
	kIPBFormatFlags,				// Format Flags
	128,							// Compression Accuracy
	128,							// Decomression Accuracy
	200,							// Compression Speed
	200,							// Decompression Speed
	128,							// Compression Level
	0,								// Reserved
	1,								// Minimum Height
	1,								// Minimum Width
	0,								// Decompression Pipeline Latency
	0,								// Compression Pipeline Latency
	0								// Private Data
};

// Component Name
resource 'STR ' (256) {
	"ExampleIPB"
};

#if DECO_BUILD
resource 'thng' (256) {
	decompressorComponentType,				// Type			
	kExampleIPBCodecFormatType,				// SubType
	'appl',									// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	256,									// Name ID
	'STR ',									// Info Type
	257,									// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kExampleIPBDecompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_REZ_CARBON_MACHO
	kExampleIPBDecompressorFlags | cmpThreadSafe, 
	'dlle',
	256,
	platformPowerPCNativeEntryPoint,
#endif
#if TARGET_OS_WIN32
	kExampleIPBDecompressorFlags, 
	'dlle',
	256,
	platformWin32,
#endif
	},
	0, 0;
};

// Component Information
resource 'STR ' (257) {
	"Demonstration ExampleIPB Decompressor."
};

// Code Entry Point for Mach-O and Windows
resource 'dlle' (256) {
	"ExampleIPB_DComponentDispatch"
};
#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (258) {
	compressorComponentType,				// Type			
	kExampleIPBCodecFormatType,				// SubType
	'appl',									// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	256,									// Name ID
	'STR ',									// Info Type
	258,									// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kExampleIPBCompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_REZ_CARBON_MACHO
	kExampleIPBCompressorFlags | cmpThreadSafe, 
	'dlle',
	258,
	platformPowerPCNativeEntryPoint,
#endif
#if TARGET_OS_WIN32
	kExampleIPBCompressorFlags, 
	'dlle',
	258,
	platformWin32,
#endif
	},
	0, 0;
};

// Component Information
resource 'STR ' (258) {
	"Demonstration ExampleIPB Compressor."
};

// Code Entry Point for Mach-O and Windows
resource 'dlle' (258) {
	"ExampleIPB_CComponentDispatch"
};
#endif // COMP_BUILD

