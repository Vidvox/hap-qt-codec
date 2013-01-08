//
//  HapCodecUniversal.r
//  Hap Codec
//

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

#include "HapCodecVersion.h"

#define	kHapCodecFormatType	'VPUV'
#define	kHapCodecFormatName	"Hap"
#define kCodecManufacturerType 'VDVX'

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kHapCompressorFlags ( codecInfoDoes32 )
#define kHapDecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24 and 32-bit depths
//#define kIPBFormatFlags	( codecInfoDepth24 | codecInfoDepth32 )
// We suppress codecInfoDepth32 to keep the depth selection out of the UI

#define kIPBFormatFlags	( codecInfoDepth24 )
#define kHapNameResID 256
#define kHapDecompressorDescriptionResID 258
#define kHapCompressorDescriptionResID 259

// Component Description
resource 'cdci' (256) {
	kHapCodecFormatName,            // Type
	1,								// Version
	1,								// Revision level
	kCodecManufacturerType,			// Manufacturer
	kHapDecompressorFlags,          // Decompression Flags
	kHapCompressorFlags,            // Compression Flags
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
resource 'STR ' (kHapNameResID) {
	"Hap"
};

#if DECO_BUILD
resource 'thng' (256) {
	decompressorComponentType,				// Type			
	kHapCodecFormatType,                    // SubType
	kCodecManufacturerType,					// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	kHapNameResID,							// Name ID
	'STR ',									// Info Type
	kHapDecompressorDescriptionResID,		// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kHapDecompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{    
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC
        kHapDecompressorFlags | cmpThreadSafe, 
        'dlle',
        256,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapDecompressorFlags | cmpThreadSafe, 
        'dlle',
        256,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
	kHapDecompressorFlags, 
	'dlle',
	256,
	platformWin32,
#endif
	},
	'thnr', 256;
};

// Component Information
resource 'STR ' (kHapDecompressorDescriptionResID) {
	"Hap Decompressor."
};

// Code Entry Point for Mach-O and Windows
// TODO: change this
resource 'dlle' (256) {
	"Hap_DComponentDispatch"
};
#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (258) {
	compressorComponentType,				// Type			
	kHapCodecFormatType,				// SubType
	kCodecManufacturerType,					// Manufacturer
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
	'STR ',									// Name Type
	kHapNameResID,							// Name ID
	'STR ',									// Info Type
	kHapCompressorDescriptionResID,			// Info ID
	0,										// Icon Type
	0,										// Icon ID
	kHapCompressorVersion,
	componentHasMultiplePlatforms +			// Registration Flags 
		componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC    
        kHapCompressorFlags | cmpThreadSafe, 
        'dlle',
        258,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapCompressorFlags | cmpThreadSafe, 
        'dlle',
        258,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
	kHapCompressorFlags, 
	'dlle',
	258,
	platformWin32,
#endif
	},
	'thnr', 256;
};

// Component Information
resource 'STR ' (kHapCompressorDescriptionResID) {
	"Hap Compressor."
};

// Code Entry Point for Mach-O and Windows
// TODO: change this
resource 'dlle' (258) {
	"Hap_CComponentDispatch"
};

resource 'thnr' (256) {
    {
        'cdci', 1, 0, 'cdci', 256, cmpResourceNoFlags,
        'cpix', 1, 0, 'cpix', 256, cmpResourceNoFlags,
        'ccop', 1, 0, 'ccop', 256, cmpResourceNoFlags,
    }
};

resource 'ccop' (256) {
    kCodecCompressionNoQuality;
};

// TODO: if we only take RGBA we probably don't need this
// TODO: This is RGB DXT1 and DXT5 (RGBA)
resource 'cpix' (256) {
    {
        'DXt1',
        'DXT1',
        'DXT5',
        'DYt5',
        'RGBA',
        'BGRA'
    }
};

// Settings controls for dialog
#define kMyCodecDITLResID 129
#define kMyCodecPopupCNTLResID 129
#define kMyCodecPopupMENUResID 129
#define kMyCodecSliderCNTLResID 130

#define POPUP_H 22
#define SPACING 16
#define TEXT_H 16
#define SLIDER_H 26

resource 'DITL' (kMyCodecDITLResID, "Compressor Options") {
{
    {2, 0, 2 + TEXT_H, 50}, StaticText { disabled, "Quality:" },

    {0, 50 + 8, SLIDER_H, 300}, Control { enabled, kMyCodecSliderCNTLResID },
    
    {SLIDER_H + SPACING, 0, SLIDER_H + SPACING + TEXT_H, 185}, CheckBox { enabled, "Preserve Alpha" },

    {TEXT_H + SPACING + SLIDER_H + SPACING, 0, TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H, 185}, Control { enabled, kMyCodecPopupCNTLResID },
    
    {TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H + SPACING, 0, TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H + SPACING + TEXT_H, 185}, StaticText { disabled, "" },
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

resource 'CNTL' (kMyCodecSliderCNTLResID, "Quality Slider"){
	{0,0,26,185},
    3/*value:initially, # of ticks*/,
    visible,
    /*max*/0x2,
    /*min*/0,
    kControlSliderProc|kControlSliderHasTickMarks,
    0,
    ""
};


/*
resource 'CNTL' (kMyCodecBoxCNTLResID, "Quality Box"){
	{0,0,100,185},
    0,
    visible,
    0,
    0,
    kControlGroupBoxTextTitleProc,
    0,
    "Quality"
};
*/

#endif // COMP_BUILD
