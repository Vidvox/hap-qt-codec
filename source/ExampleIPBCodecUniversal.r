/*

File: ExampleIPBCodecUniversal.r

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

// Mac OS X Mach-O Component: Set TARGET_REZ_CARBON_MACHO to 1
//
// In the target settings of your Xcode project, add one or both of the
// following defines to your OTHER_REZFLAGS depending on the type of component
// you want to build:
//
//      PPC only: -d ppc_$(ppc)
//      x86 only: -d i386_$(i386)
//      Universal Binary: -d ppc_$(ppc) -d i386_$(i386)
//
// Windows Component: Set TARGET_REZ_CARBON_MACHO to 0
// ---------------------------------------------------

// Set to 1 == building Mac OS X
#define TARGET_REZ_CARBON_MACHO 1

#if TARGET_REZ_CARBON_MACHO

    #if defined(ppc_YES)
        // PPC architecture
        #define TARGET_REZ_MAC_PPC 1
    #else
        #define TARGET_REZ_MAC_PPC 0
    #endif

    #if defined(i386_YES)
        // x86 architecture
        #define TARGET_REZ_MAC_X86 1
    #else
        #define TARGET_REZ_MAC_X86 0
    #endif

    #define TARGET_REZ_WIN32 0
#else
    // Must be building on Windows
    #define TARGET_REZ_WIN32 1
#endif

// TODO: re-enable decompressor here
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

#define	kExampleIPBCodecFormatType	'VPUV'
#define	kExampleIPBCodecFormatName	"VPU"
#define kCodecManufacturerType 'VDVX'

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kExampleIPBCompressorFlags ( codecInfoDoes32 )
#define kExampleIPBDecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24 and 32-bit depths
#define kIPBFormatFlags	( codecInfoDepth24 | codecInfoDepth32 )

#define kVPUNameResID 256
#define kVPUDecompressorDescriptionResID 258
#define kVPUCompressorDescriptionResID 259

// Component Description
resource 'cdci' (256) {
	kExampleIPBCodecFormatName,		// Type
	1,								// Version
	1,								// Revision level
	kCodecManufacturerType,			// Manufacturer
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
resource 'STR ' (kVPUNameResID) {
	"VPU"
};

#if DECO_BUILD
resource 'thng' (256) {
	decompressorComponentType,				// Type			
	kExampleIPBCodecFormatType,				// SubType
	kCodecManufacturerType,					// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	kVPUNameResID,							// Name ID
	'STR ',									// Info Type
	kVPUDecompressorDescriptionResID,		// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kExampleIPBDecompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{    
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC
        kExampleIPBDecompressorFlags | cmpThreadSafe, 
        'dlle',
        256,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kExampleIPBDecompressorFlags | cmpThreadSafe, 
        'dlle',
        256,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
	kExampleIPBDecompressorFlags, 
	'dlle',
	256,
	platformWin32,
#endif
	},
	'thnr', 256;
};

// Component Information
resource 'STR ' (kVPUDecompressorDescriptionResID) {
	"VPU Decompressor."
};

// Code Entry Point for Mach-O and Windows
// TODO: change this
resource 'dlle' (256) {
	"ExampleIPB_DComponentDispatch"
};
#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (258) {
	compressorComponentType,				// Type			
	kExampleIPBCodecFormatType,				// SubType
	kCodecManufacturerType,					// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	kVPUNameResID,							// Name ID
	'STR ',									// Info Type
	kVPUCompressorDescriptionResID,			// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kExampleIPBCompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC    
        kExampleIPBCompressorFlags | cmpThreadSafe, 
        'dlle',
        258,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kExampleIPBCompressorFlags | cmpThreadSafe, 
        'dlle',
        258,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
	kExampleIPBCompressorFlags, 
	'dlle',
	258,
	platformWin32,
#endif
	},
	'thnr', 256;
};

// Component Information
resource 'STR ' (kVPUCompressorDescriptionResID) {
	"VPU Compressor."
};

// Code Entry Point for Mach-O and Windows
// TODO: change this
resource 'dlle' (258) {
	"ExampleIPB_CComponentDispatch"
};

resource 'thnr' (256) {
    {
        'cdci', 1, 0, 'cdci', 256, cmpResourceNoFlags,
        'cpix', 1, 0, 'cpix', 256, cmpResourceNoFlags
    }
};

// TODO: if we only take RGBA we probably don't need this
// TODO: This is RGB DXT1 and DXT5 (RGBA)
resource 'cpix' (256) {
    {
        'DXT1',
        'DXT5',
        'RGBA',
        'BGRA'
    }
};

// Settings controls for dialog
#define kMyCodecDITLResID 129
#define kMyCodecPopupCNTLResID 129
#define kMyCodecPopupMENUResID 129
 
#define POPUP_CONTROL_HEIGHT 22
 
resource 'DITL' (kMyCodecDITLResID, "Compressor Options") {
{
 {0, 0,
  POPUP_CONTROL_HEIGHT, 185},
 Control { enabled, kMyCodecPopupCNTLResID },
 };
};
 
resource 'CNTL' (kMyCodecPopupCNTLResID, "Compressor Popup") {
 {0, 0, 20, 185},
 0,
 visible,
 84,        /* title width */
 kMyCodecPopupMENUResID,
 popupMenuCDEFProc+popupFixedWidth,
 0,
 "Compressor:"
};
 
resource 'MENU' (kMyCodecPopupMENUResID, "Compressor Popup") {
 kMyCodecPopupMENUResID,
 textMenuProc,
 allEnabled,       /* Enable flags */
 enabled,
 "Compressor",
 { /* array: 3 elements */
  /* [1] */
  "Snappy", noIcon, noKey, noMark, plain,
  /* [2] */
  "LZF", noIcon, noKey, noMark, plain,
  /* [3] */
  "ZLIB", noIcon, noKey, noMark, plain
 }
};
#endif // COMP_BUILD
