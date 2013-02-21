//
//  PixelFormats.c
//  Hap Codec
//
//  Created by Tom on 20/02/2013.
//
//

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
    BlockZero(&pixelInfo, sizeof(pixelInfo));
    pixelInfo.size  = sizeof(ICMPixelFormatInfo);
    pixelInfo.formatFlags = (has_alpha ? kICMPixelFormatHasAlphaChannel : 0);
    pixelInfo.bitsPerPixel[0] = bits_per_pixel;
    pixelInfo.cmpCount = 4;
    pixelInfo.cmpSize = bits_per_pixel / 4;
    
    // Ignore any errors here as this could be a duplicate registration
    ICMSetPixelFormatInfo(fmt, &pixelInfo);
    
    addNumberToDictionary(dict, kCVPixelFormatConstant, fmt);
    addNumberToDictionary(dict, kCVPixelFormatBlockWidth, 4);
    addNumberToDictionary(dict, kCVPixelFormatBlockHeight, 4);
    
    // CV has a bug where it disregards kCVPixelFormatBlockHeight, so the following line is a lie to
    // produce correctly-sized buffers
    addNumberToDictionary(dict, kCVPixelFormatBitsPerBlock, bits_per_pixel * 4);
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
    BlockZero(&pixelInfo, sizeof(pixelInfo));
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
