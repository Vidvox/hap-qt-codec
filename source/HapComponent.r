//
//  HapComponent.r
//  Hap Codec
//

#include "HapResCommon.h"

#define    kHapCodecFormatType    kHapCodecSubType
#define    kHapCodecFormatName    "Hap"

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kHapCompressorFlags ( codecInfoDoes32 )
#define kHapDecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24 and 32-bit depths
//#define kHapFormatFlags    ( codecInfoDepth24 | codecInfoDepth32 )
// We suppress codecInfoDepth32 to keep the depth selection out of the UI

#define kHapFormatFlags    ( codecInfoDepth24 )

#define kHapNameResID 256

#define kHapDecompressorDescriptionResID 258

#define kHapCompressorDescriptionResID 259

// Component Description
resource 'cdci' (256) {
    kHapCodecFormatName,            // Type
    1,                              // Version
    1,                              // Revision level
    kCodecManufacturerType,         // Manufacturer
    kHapDecompressorFlags,          // Decompression Flags
    kHapCompressorFlags,            // Compression Flags
    kHapFormatFlags,                // Format Flags
    128,                            // Compression Accuracy
    128,                            // Decomression Accuracy
    200,                            // Compression Speed
    200,                            // Decompression Speed
    128,                            // Compression Level
    0,                              // Reserved
    1,                              // Minimum Height
    1,                              // Minimum Width
    0,                              // Decompression Pipeline Latency
    0,                              // Compression Pipeline Latency
    0                               // Private Data
};

// Component Name
resource 'STR ' (kHapNameResID) {
    "Hap"
};

#if DECO_BUILD
resource 'thng' (256) {
    decompressorComponentType,              // Type
    kHapCodecFormatType,                    // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapNameResID,                          // Name ID
    'STR ',                                 // Info Type
    kHapDecompressorDescriptionResID,       // Info ID
    0,                                      // Icon Type
    0,                                      // Icon ID
    kHapDecompressorVersion,
    componentHasMultiplePlatforms +         // Registration Flags
        componentDoAutoVersion,
    0,                                      // Resource ID of Icon Family
    {    
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC
        kHapDecompressorFlags | cmpThreadSafe, 
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapDecompressorFlags | cmpThreadSafe, 
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapDecompressorFlags, 
    'dlle',
    kCodecDecompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 256;
};

// Component Information
resource 'STR ' (kHapDecompressorDescriptionResID) {
    "Hap Decompressor."
};

#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (258) {
    compressorComponentType,                // Type
    kHapCodecFormatType,                    // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapNameResID,                          // Name ID
    'STR ',                                 // Info Type
    kHapCompressorDescriptionResID,         // Info ID
    0,                                      // Icon Type
    0,                                      // Icon ID
    kHapCompressorVersion,
    componentHasMultiplePlatforms +         // Registration Flags
        componentDoAutoVersion,
    0,                                      // Resource ID of Icon Family
    {
#if TARGET_REZ_CARBON_MACHO
    #if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
        #error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
    #endif
    
    #if TARGET_REZ_MAC_PPC    
        kHapCompressorFlags | cmpThreadSafe, 
        'dlle',
        kCodecCompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapCompressorFlags | cmpThreadSafe, 
        'dlle',
        kCodecCompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapCompressorFlags, 
    'dlle',
    kCodecCompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 256;
};

// Component Information
resource 'STR ' (kHapCompressorDescriptionResID) {
    "Hap Compressor."
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
        'DXT5',
        'RGBA',
        'BGRA'
    }
};

// Settings controls for dialog
#define kHapCodecDITLResID 129

resource 'DITL' (kHapCodecDITLResID) {
{
    {2, 0, 2 + TEXT_H, 50}, StaticText { disabled, "Quality:" },

    {0, 50 + 8, SLIDER_H, 300}, Control { enabled, kHapCodecSliderCNTLResID },
    
    {SLIDER_H + SPACING, 0, SLIDER_H + SPACING + TEXT_H, 185}, CheckBox { enabled, "Preserve Alpha" },

    {TEXT_H + SPACING + SLIDER_H + SPACING, 0, TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H, 185}, Control { enabled, kHapCodecPopupCNTLResID },
    
    {TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H + SPACING, 0, TEXT_H + SPACING + SLIDER_H + SPACING + POPUP_H + SPACING + TEXT_H, 185}, StaticText { disabled, "" },
 };
};

#endif // COMP_BUILD
