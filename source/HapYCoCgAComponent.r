/*
 HapYCoCgACompressor.r
 Hap Codec
 
 Copyright (c) 2012-2016, Tom Butterworth and Vidvox LLC. All rights reserved.
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

#include "HapResCommon.h"

#define kHapYCoCgACodecFormatType kHapYCoCgACodecSubType
#define kHapYCoCgACodecFormatName "Hap Q Alpha"

// These flags specify information about the capabilities of the component
// Works with 32-bit Pixel Maps
#define kHapYCoCgACompressorFlags ( codecInfoDoes32 )
#define kHapYCoCgADecompressorFlags ( codecInfoDoes32 )

// These flags specify the possible format of compressed data produced by the component
// and the format of compressed files that the component can handle during decompression
// The component can decompress from files at 24 and 32-bit depths
#define kHapYCoCgAFormatFlags ( codecInfoDepth32 )

#define kHapYCoCgANameResID 556

#define kHapYCoCgADecompressorDescriptionResID 558

#define kHapYCoCgACompressorDescriptionResID 559

// Component Description
resource 'cdci' (556) {
    kHapYCoCgACodecFormatName,      // Type
    1,                              // Version
    1,                              // Revision level
    kCodecManufacturerType,         // Manufacturer
    kHapYCoCgADecompressorFlags,    // Decompression Flags
    kHapYCoCgACompressorFlags,      // Compression Flags
    kHapYCoCgAFormatFlags,          // Format Flags
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
resource 'STR ' (kHapYCoCgANameResID) {
    kHapYCoCgACodecFormatName
};

#if DECO_BUILD
resource 'thng' (556) {
    decompressorComponentType,              // Type
    kHapYCoCgACodecFormatType,              // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapYCoCgANameResID,                    // Name ID
    'STR ',                                 // Info Type
    kHapYCoCgADecompressorDescriptionResID, // Info ID
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
        kHapYCoCgADecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapYCoCgADecompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecDecompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapYCoCgADecompressorFlags,
    'dlle',
    kCodecDecompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 556;
};

// Component Information
resource 'STR ' (kHapYCoCgADecompressorDescriptionResID) {
    "Hap Q Alpha Decompressor"
};

#endif // DECO_BUILD

#if COMP_BUILD
resource 'thng' (558) {
    compressorComponentType,                // Type
    kHapYCoCgACodecFormatType,              // SubType
    kCodecManufacturerType,                 // Manufacturer
    0,                                      // - use componentHasMultiplePlatforms
    0,
    0,
    0,
    'STR ',                                 // Name Type
    kHapYCoCgANameResID,                    // Name ID
    'STR ',                                 // Info Type
    kHapYCoCgACompressorDescriptionResID,   // Info ID
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
        kHapYCoCgACompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformPowerPCNativeEntryPoint,
    #endif
    #if TARGET_REZ_MAC_X86
        kHapYCoCgACompressorFlags | cmpThreadSafe,
        'dlle',
        kCodecCompressorEntryPointResID,
        platformIA32NativeEntryPoint,
    #endif
#endif

#if TARGET_OS_WIN32
    kHapYCoCgACompressorFlags,
    'dlle',
    kCodecCompressorEntryPointResID,
    platformWin32,
#endif
    },
    'thnr', 556;
};

// Component Information
resource 'STR ' (kHapYCoCgACompressorDescriptionResID) {
    "Hap Q Alpha Compressor"
};

resource 'thnr' (556) {
    {
        'cdci', 1, 0, 'cdci', 556, cmpResourceNoFlags,
        'cpix', 1, 0, 'cpix', 556, cmpResourceNoFlags,
        'ccop', 1, 0, 'ccop', 556, cmpResourceNoFlags,
    }
};

resource 'ccop' (556) {
    kCodecCompressionNoQuality;
};

// Pixel formats we accept and emit
resource 'cpix' (556) {
    {
        'DYtA',
        'RGBA',
        'BGRA'
    }
};

#endif // COMP_BUILD
