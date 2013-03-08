/*
 PixelFormats.c
 Hap Codec
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 
 * Neither the name of Hap nor the name of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.
 
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

#include "PixelFormats.h"
#include <QuickTime/QuickTime.h>
#include "Utility.h"

static void HapCodecRegisterDXTPixelFormat(OSType fmt, short bits_per_pixel, SInt32 open_gl_internal_format, Boolean has_alpha)
{
    /*
     * See http://developer.apple.com/legacy/mac/library/#qa/qa1401/_index.html
     */
    
    ICMPixelFormatInfo pixelInfo;
    
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    bzero(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = (has_alpha ? kICMPixelFormatHasAlphaChannel : 0);
    pixelInfo.bitsPerPixel[0] = bits_per_pixel;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = bits_per_pixel / 4;
    
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(fmt, &pixelInfo);
    
    addNumberToDictionary(dict, kCVPixelFormatConstant, fmt);
    
    // CV has a bug where it disregards kCVPixelFormatBlockHeight, so we lie about block size
    // (4x1 instead of actual 4x4) and add a vertical block-alignment key instead
    addNumberToDictionary(dict, kCVPixelFormatBitsPerBlock, bits_per_pixel * 4);
    addNumberToDictionary(dict, kCVPixelFormatBlockWidth, 4);
    addNumberToDictionary(dict, kCVPixelFormatBlockVerticalAlignment, 4);
    
    addNumberToDictionary(dict, kCVPixelFormatOpenGLInternalFormat, open_gl_internal_format);
    
    CFDictionarySetValue(dict, kCVPixelFormatOpenGLCompatibility, kCFBooleanTrue);
    
    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), (has_alpha ? kCFBooleanTrue : kCFBooleanFalse));
    
    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, fmt);
    CFRelease(dict);
}

static void HapCodecRegisterYCoCgPixelFormat(void)
{
    ICMPixelFormatInfo pixelInfo;
    
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                            0,
                                                            &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    bzero(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = 0;
    pixelInfo.bitsPerPixel[0] = 32;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = 8;
    
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(kHapCVPixelFormat_CoCgXY, &pixelInfo);
    
    addNumberToDictionary(dict, kCVPixelFormatConstant, kHapCVPixelFormat_CoCgXY);
    addNumberToDictionary(dict, kCVPixelFormatBitsPerBlock, 32);
    
    // kCVPixelFormatContainsAlpha is only defined in the SDK for 10.7 plus
    CFDictionarySetValue(dict, CFSTR("ContainsAlpha"), kCFBooleanFalse);
    
    CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(dict, kHapCVPixelFormat_CoCgXY);
    CFRelease(dict);
}

__attribute__((constructor))
static void HapCodecRegisterPixelFormats(void)
{
    static Boolean registered = false;
    if (!registered)
    {
        // Register our DXT pixel buffer types if they're not already registered
        // arguments are: OSType, OpenGL internalFormat, alpha
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_RGB_DXT1, 4, 0x83F0, false);
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_RGBA_DXT5, 8, 0x83F3, true);
        HapCodecRegisterDXTPixelFormat(kHapCVPixelFormat_YCoCg_DXT5, 8, 0x83F3, false);
        HapCodecRegisterYCoCgPixelFormat();
        registered = true;
    }
}
