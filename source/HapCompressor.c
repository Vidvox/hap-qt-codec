/*
 HapCompressor.c
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

#if __APPLE_CC__
    #include <QuickTime/QuickTime.h>
#else
    #include <ConditionalMacros.h>
    #include <Endian.h>
    #include <ImageCodec.h>
#endif

#include "HapCodecVersion.h"
#include "Utility.h"
#include "PixelFormats.h"
#include "HapCodecSubTypes.h"
#include "hap.h"
#include "Tasks.h"
#include "Buffers.h"
#include "DXTEncoder.h"
#include <Accelerate/Accelerate.h>

/*
 Defines determining the method of DXT compression.
 The GPU is very fast but produces low quality results
 Squish produces nicer results but takes longer.
 YCoCg encodes YCoCg in DXT and requires a shader to draw, and produces very high quality results
 If both are available, the GPU is selected at the lowest quality setting.
 */

#include "GLDXTEncoder.h"
#include "SquishEncoder.h"
#include "YCoCg.h"
#include "YCoCgDXTEncoder.h"

#include <libkern/OSAtomic.h>

#ifdef DEBUG
    #include <mach/mach_time.h>
#endif

typedef struct HapCodecCompressTask HapCodecCompressTask;

typedef struct {
	ComponentInstance				self;
	ComponentInstance				target;
	
	ICMCompressorSessionRef 		session; // NOTE: we do not need to retain or release this
	
    OSType                          type;
    
	long							width;
	long							height;
	size_t							maxEncodedDataSize;
	
    CodecQ                          quality;
    
    int                             lastFrameOut;
    HapCodecBufferRef               finishedFrames;
    OSSpinLock                      lock;
    
    Boolean                         endTasksPending;
    HapCodecBufferPoolRef           compressTaskPool;
    
    HapCodecDXTEncoderRef           dxtEncoder;
    
    HapCodecBufferPoolRef           formatConvertPool;
    size_t                          formatConvertBufferBytesPerRow;
    
    HapCodecBufferPoolRef           dxtBufferPool;
    
    unsigned int                    dxtFormat;
    unsigned int                    taskGroup;
    
#ifdef DEBUG
    unsigned int                    debugFrameCount;
    uint64_t                        debugStartTime;
    uint64_t                        debugLastFrameTime;
    unsigned long                   debugTotalFrameBytes;
    unsigned long                   debugLargestFrameBytes;
    unsigned long                   debugSmallestFrameBytes;    
#endif
} HapCompressorGlobalsRecord, *HapCompressorGlobals;

struct HapCodecCompressTask
{
    HapCompressorGlobals            glob;
    ICMCompressorSourceFrameRef     sourceFrame;
    ICMMutableEncodedFrameRef       encodedFrame;
    ComponentResult                 error;
    HapCodecBufferRef               next; // Used to queue finished tasks
};

// Setup required for ComponentDispatchHelper.c
#define IMAGECODEC_BASENAME() 		Hap_C
#define IMAGECODEC_GLOBALS() 		HapCompressorGlobals storage

#define CALLCOMPONENT_BASENAME()	IMAGECODEC_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		IMAGECODEC_GLOBALS()

#define QT_BASENAME()				CALLCOMPONENT_BASENAME()
#define QT_GLOBALS()				CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppImageCodec
#define COMPONENT_DISPATCH_FILE		"HapCompressorDispatch.h"
#define COMPONENT_SELECT_PREFIX()  	kImageCodec

#if __APPLE_CC__
#include <CoreServices/Components.k.h>
#include <QuickTime/ImageCodec.k.h>
#include <QuickTime/ImageCompression.k.h>
#include <QuickTime/ComponentDispatchHelper.c>
#else
#include <Components.k.h>
#include <ImageCodec.k.h>
#include <ImageCompression.k.h>
#include <ComponentDispatchHelper.c>
#endif

static void releaseTaskFrames(HapCodecCompressTask *task);
static ComponentResult finishFrame(HapCodecBufferRef buffer);
static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame);
static HapCodecBufferRef dequeueNextFrameOut(HapCompressorGlobals glob);

// Open a new instance of the component.
// Allocate component instance storage ("globals") and associate it with the new instance so that other
// calls will receive it.  
// Note that "one-shot" component calls like CallComponentVersion and ImageCodecGetCodecInfo work by opening
// an instance, making that call and then closing the instance, so you should avoid performing very expensive
// initialization operations in a component's Open function.
ComponentResult 
Hap_COpen(
  HapCompressorGlobals glob,
  ComponentInstance self )
{    
	ComponentResult err = noErr;
    
#ifndef __clang_analyzer__
    glob = calloc( sizeof( HapCompressorGlobalsRecord ), 1 );
#endif
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}
	SetComponentInstanceStorage( self, (Handle)glob );
    
    // Store the type of compressor we are (Hap or Hap YCoCg)
    ComponentDescription componentDescription;
    GetComponentInfo((Component)self, &componentDescription, 0, 0, 0);
    
	glob->self = self;
	glob->target = self;
    glob->session = NULL;
    glob->type = componentDescription.componentSubType;
    glob->width = 0;
    glob->height = 0;
	glob->maxEncodedDataSize = 0;
    glob->quality = codecNormalQuality;
    glob->lastFrameOut = 0;
    glob->finishedFrames = NULL;
    glob->lock = OS_SPINLOCK_INIT;
    glob->endTasksPending = false;
    glob->compressTaskPool = NULL;
    glob->dxtEncoder = NULL;
    glob->formatConvertPool = NULL;
    glob->formatConvertBufferBytesPerRow = 0;
    glob->dxtBufferPool = NULL;
    glob->dxtFormat = 0;
    glob->taskGroup = 0;
    
bail:
    debug_print_err(glob, err);
	return err;
}

// Closes the instance of the component.
// Release all storage associated with this instance.
// Note that if a component's Open function fails with an error, its Close function will still be called.
ComponentResult 
Hap_CClose(
  HapCompressorGlobals glob,
  ComponentInstance self )
{
#pragma unused (self)
	if( glob )
    {
#ifdef DEBUG
        if (glob->debugFrameCount)
        {
            uint64_t elapsed = glob->debugLastFrameTime - glob->debugStartTime;
            
            // Convert to nanoseconds.
            
            AbsoluteTime absoluteElapsed = UInt64ToUnsignedWide(elapsed);
            Nanoseconds elapsedNano = AbsoluteToNanoseconds(absoluteElapsed);
            
            // Convert to seconds
            
            double time = (double)UnsignedWideToUInt64(elapsedNano) / (double)NSEC_PER_SEC;
            printf("HAP CODEC: ");
            if (glob->dxtEncoder == NULL)
            {
                printf("DXT frames in");
            }
            else if (glob->dxtEncoder->describe_function)
            {
                printf("%s ", glob->dxtEncoder->describe_function(glob->dxtEncoder));
            }
            printf("%u frames over %.1f seconds %.1f FPS. ", glob->debugFrameCount, time, glob->debugFrameCount / time);
            printf("Largest frame bytes: %lu smallest: %lu average: %lu ",
                   glob->debugLargestFrameBytes, glob->debugSmallestFrameBytes, glob->debugTotalFrameBytes/ glob->debugFrameCount);
            unsigned int uncompressed = glob->width * glob->height;
            if (glob->dxtFormat == HapTextureFormat_RGB_DXT1) uncompressed /= 2;
            uncompressed += 4U; // Hap uses 4 extra bytes
            printf("uncompressed: %d\n", uncompressed);
        }
#endif
        HapCodecBufferPoolDestroy(glob->dxtBufferPool);
        glob->dxtBufferPool = NULL;
        
        HapCodecDXTEncoderDestroy(glob->dxtEncoder);
        glob->dxtEncoder = NULL;
        
        HapCodecBufferPoolDestroy(glob->formatConvertPool);
        glob->formatConvertPool = NULL;
        
        if (glob->endTasksPending)
        {
            HapCodecTasksWillStop();
        }
        
        // We should never have queued frames when we are closed
        // but we check and properly release the memory if we do
        
        HapCodecBufferRef this = glob->finishedFrames;
        HapCodecBufferRef next = NULL;
        while (this) {
            HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(this);
            if (task)
            {
                releaseTaskFrames(task);
                next = task->next;
            }
            else
            {
                next = NULL;
            }
            HapCodecBufferReturn(this);
            this = next;
        }
        
        HapCodecBufferPoolDestroy(glob->compressTaskPool);
        glob->compressTaskPool = NULL;
        
		free( glob );
	}
	
	return noErr;
}

// Return the version of the component.
// This does not need to correspond in any way to the human-readable version numbers found in
// Get Info windows, etc.  
// The principal use of component version numbers is to choose between multiple installed versions
// of the same component: if the component manager sees two components with the same type, subtype
// and manufacturer codes and either has the componentDoAutoVersion registration flag set,
// it will deregister the one with the older version.  (If componentAutoVersionIncludeFlags is also 
// set, it only does this when the component flags also match.)
// By convention, the high short of the component version is the interface version, which Apple
// bumps when there is a major change in the interface.  
// We recommend bumping the low short of the component version every time you ship a release of a component.
// The version number in the 'thng' resource should be the same as the number returned by this function.
ComponentResult 
Hap_CVersion(HapCompressorGlobals glob)
{
#pragma unused (glob)
	return kHapCompressorVersion;
}

// Sets the target for a component instance.
// When a component wants to make a component call on itself, it should make that call on its target.
// This allows other components to delegate to the component.
// By default, a component's target is itself -- see the Open function.
ComponentResult 
Hap_CTarget(HapCompressorGlobals glob, ComponentInstance target)
{
	glob->target = target;
	return noErr;
}

// Your component receives the ImageCodecGetCodecInfo request whenever an application calls the Image Compression Manager's GetCodecInfo function.
// Your component should return a formatted compressor information structure defining its capabilities.
// Both compressors and decompressors may receive this request.
ComponentResult 
Hap_CGetCodecInfo(HapCompressorGlobals glob, CodecInfo *info)
{
	OSErr err = noErr;

	if (info == NULL)
    {
		err = paramErr;
	}
	else
    {
		CodecInfo **tempCodecInfo;
        SInt16 resourceID = resourceIDForComponentType(glob->type, codecInfoResourceType);
        if (resourceID == 0)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        err = GetComponentResource((Component)glob->self, codecInfoResourceType, resourceID, (Handle *)&tempCodecInfo);
        if (err == noErr)
        {
            *info = **tempCodecInfo;
            DisposeHandle((Handle)tempCodecInfo);
        }
    }
bail:
    debug_print_err(glob, err);
	return err;
}

// Return the maximum size of compressed data for the image in bytes.
// Note that this function is only used when the ICM client is using a compression sequence
// (created with CompressSequenceBegin, not ICMCompressionSessionCreate).
// Nevertheless, it's important to implement it because such clients need to know how much 
// memory to allocate for compressed frame buffers.
ComponentResult 
Hap_CGetMaxCompressionSize(
	HapCompressorGlobals glob,
	PixMapHandle        src,
	const Rect *        srcRect,
	short               depth,
	CodecQ              quality,
	long *              size)
{
#pragma unused (glob, src, depth, quality)
	
	if( ! size )
		return paramErr;
    int dxtSize = dxtBytesForDimensions(srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, glob->type);
	*size = HapMaxEncodedLength(dxtSize);
    
	return noErr;
}

// Create a dictionary that describes the kinds of pixel buffers that we want to receive.
// The important keys to add are kCVPixelBufferPixelFormatTypeKey, 
// kCVPixelBufferWidthKey and kCVPixelBufferHeightKey.
// Many compressors will also want to set kCVPixelBufferExtendedPixels, 
// kCVPixelBufferBytesPerRowAlignmentKey, kCVImageBufferGammaLevelKey and kCVImageBufferYCbCrMatrixKey.
static OSStatus 
createPixelBufferAttributesDictionary( SInt32 width, SInt32 height,
                                      const OSType *pixelFormatList, int pixelFormatCount,
                                      Boolean padBuffers,
                                      CFMutableDictionaryRef *pixelBufferAttributesOut )
{
	OSStatus err = memFullErr;
	int i;
	CFMutableDictionaryRef pixelBufferAttributes = NULL;
	CFNumberRef number = NULL;
	CFMutableArrayRef array = NULL;
    SInt32 extendRight, extendBottom;
	
	pixelBufferAttributes = CFDictionaryCreateMutable( 
                                                      NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if( ! pixelBufferAttributes ) goto bail;
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	if( ! array ) goto bail;
	
	// Under kCVPixelBufferPixelFormatTypeKey, add the list of source pixel formats.
	// This can be a CFNumber or a CFArray of CFNumbers.
	for( i = 0; i < pixelFormatCount; i++ ) {
		number = CFNumberCreate( NULL, kCFNumberSInt32Type, &pixelFormatList[i] );
		if( ! number ) goto bail;
		
		CFArrayAppendValue( array, number );
		
		CFRelease( number );
		number = NULL;
	}
	
	CFDictionaryAddValue( pixelBufferAttributes, kCVPixelBufferPixelFormatTypeKey, array );
	CFRelease( array );
	array = NULL;
	
	// Add kCVPixelBufferWidthKey and kCVPixelBufferHeightKey to specify the dimensions
	// of the source pixel buffers.  Normally this is the same as the compression target dimensions.
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferWidthKey, width );
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferHeightKey, height );
	
	// If we will use vImage it prefers 16-byte alignment - we set it always, but we could only set it if
    // the encoder needs it, however most buffers are 16-byte aligned anyway even without asking.
	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferBytesPerRowAlignmentKey, 16 );
	
    // If you want to require that extra scratch pixels be allocated on the edges of source pixel buffers,
	// add the kCVPixelBufferExtendedPixels{Left,Top,Right,Bottom}Keys to indicate how much.
	// (Note that if your compressor needs to copy the pixels anyhow (eg, in order to convert to a different
	// format) you may get better performance if your copy routine does not require extended pixels.)
    if (padBuffers)
    {
        extendRight = roundUpToMultipleOf4(width) - width;
        extendBottom = roundUpToMultipleOf4(height) - height;
        if(extendRight)
        {
            addNumberToDictionary(pixelBufferAttributes, kCVPixelBufferExtendedPixelsRightKey, extendRight);
        }
        if (extendBottom)
        {
            addNumberToDictionary(pixelBufferAttributes, kCVPixelBufferExtendedPixelsBottomKey, extendBottom);
        }
    }
    
	addDoubleToDictionary( pixelBufferAttributes, kCVImageBufferGammaLevelKey, 2.2 );
	
	err = noErr;
	*pixelBufferAttributesOut = pixelBufferAttributes;
	pixelBufferAttributes = NULL;
	
bail:
	if( pixelBufferAttributes ) CFRelease( pixelBufferAttributes );
	if( number ) CFRelease( number );
	if( array ) CFRelease( array );
	return err;
}

// Prepare to compress frames.
// Compressor should record session and sessionOptions for use in later calls.
// Compressor may modify imageDescription at this point.
// Compressor may create and return pixel buffer options.
ComponentResult 
Hap_CPrepareToCompressFrames(
  HapCompressorGlobals glob,
  ICMCompressorSessionRef session,
  ICMCompressionSessionOptionsRef sessionOptions,
  ImageDescriptionHandle imageDescription,
  void *reserved,
  CFDictionaryRef *compressorPixelBufferAttributesOut)
{
#pragma unused (reserved)
	ComponentResult err = noErr;
	CFMutableDictionaryRef compressorPixelBufferAttributes = NULL;
    OSType pixelFormatList[] = { k32BGRAPixelFormat, k32RGBAPixelFormat, 0, 0 };
    int pixelFormatCount;
	Fixed gammaLevel;
	
    switch (glob->type) {
        case kHapCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_RGB_DXT1;
            pixelFormatCount = 3;
            break;
        case kHapAlphaCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_RGBA_DXT5;
            pixelFormatCount = 3;
            break;
        case kHapYCoCgCodecSubType:
            pixelFormatList[2] = kHapCVPixelFormat_YCoCg_DXT5;
            pixelFormatList[3] = kHapCVPixelFormat_CoCgXY;
            pixelFormatCount = 4;
            break;
        default:
            err = internalComponentErr;
            goto bail;
    }
    
	// Record the compressor session for later calls to the ICM.
	// Note: this is NOT a CF type and should NOT be CFRetained or CFReleased.
	glob->session = session;
	
	// Modify imageDescription
    
	gammaLevel = kQTCCIR601VideoGammaLevel;
	err = ICMImageDescriptionSetProperty( imageDescription,
			kQTPropertyClass_ImageDescription,
			kICMImageDescriptionPropertyID_GammaLevel,
			sizeof(gammaLevel),
			&gammaLevel );
	if( err )
		goto bail;
	
	// Record the dimensions from the image description.
	glob->width = (*imageDescription)->width;
	glob->height = (*imageDescription)->height;
    
    if (glob->type == kHapAlphaCodecSubType)
        (*imageDescription)->depth = 32;
    else
        (*imageDescription)->depth = 24;
    
    // Record quality so we can select an appropriate DXT encoder
    if (glob->type == kHapCodecSubType || glob->type == kHapAlphaCodecSubType)
    {
        CodecQ quality;
        
        if(sessionOptions)
        {
            
            err = ICMCompressionSessionOptionsGetProperty(sessionOptions,
                                                          kQTPropertyClass_ICMCompressionSessionOptions,
                                                          kICMCompressionSessionOptionsPropertyID_Quality,
                                                          sizeof( quality ),
                                                          &quality,
                                                          NULL );
            if (err) goto bail;
        }
        else
        {
            quality = codecLosslessQuality;
        }
        
        glob->quality = quality;
        
        glob->dxtFormat = glob->type == kHapAlphaCodecSubType ? HapTextureFormat_RGBA_DXT5 : HapTextureFormat_RGB_DXT1;
    }
    else
    {
        glob->dxtFormat = HapTextureFormat_YCoCg_DXT5;
    }
    
    // Create a pixel buffer attributes dictionary.
    // Only the GL DXT encoder requires padded-to-4 source buffers
	err = createPixelBufferAttributesDictionary( glob->width, glob->height,
                                                pixelFormatList, pixelFormatCount,
                                                ((glob->type != kHapYCoCgCodecSubType && glob->quality < codecHighQuality) ? true : false),
                                                &compressorPixelBufferAttributes );
	if( err )
		goto bail;
    
	*compressorPixelBufferAttributesOut = compressorPixelBufferAttributes;
	compressorPixelBufferAttributes = NULL;
    
    // Work out the upper bound on encoded frame data size -- we'll allocate buffers of this size.
    long wantedDXTSize = dxtBytesForDimensions(glob->width, glob->height, glob->type);
    
    glob->maxEncodedDataSize = HapMaxEncodedLength(wantedDXTSize);
    
    if (glob->compressTaskPool == NULL)
    {
        glob->compressTaskPool = HapCodecBufferPoolCreate(sizeof(HapCodecCompressTask));
    }
    
    if (glob->compressTaskPool == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    HapCodecTasksWillStart();
    glob->endTasksPending = true;
    
    glob->taskGroup = HapCodecTasksNewGroup();
    
#ifdef DEBUG    
    glob->debugStartTime = mach_absolute_time();
#endif
bail:
	if( compressorPixelBufferAttributes ) CFRelease( compressorPixelBufferAttributes );
    debug_print_err(glob, err);
	return err;
}

static void Background_Encode(void *info)
{
    HapCodecBufferRef formatConvertBuffer = NULL;
    HapCodecBufferRef dxtBuffer = NULL;
    CVPixelBufferRef sourceBuffer = NULL;
    HapCodecCompressTask *task = HapCodecBufferGetBaseAddress((HapCodecBufferRef)info);
    HapCompressorGlobals glob = task->glob;
    
    const void *codec_src = NULL;
    unsigned int codec_src_length = 0;
    
    ComponentResult err = noErr;
    
    err = ICMEncodedFrameCreateMutable(glob->session, task->sourceFrame, glob->maxEncodedDataSize, &task->encodedFrame);
    if (err)
        goto bail;
    
    sourceBuffer = ICMCompressorSourceFrameGetPixelBuffer(task->sourceFrame);
    
    if (CVPixelBufferLockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
    {
        sourceBuffer = NULL;
        err = internalComponentErr;
        goto bail;
    }
    
    unsigned int sourceBytesPerRow = CVPixelBufferGetBytesPerRow(sourceBuffer);
    OSType sourcePixelFormat = CVPixelBufferGetPixelFormatType(sourceBuffer);
    void *sourceBaseAddress = CVPixelBufferGetBaseAddress(sourceBuffer);
    
    OSType dxtPixelFormat = (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : (glob->type == kHapCodecSubType ? kHapCVPixelFormat_RGB_DXT1 : kHapCVPixelFormat_YCoCg_DXT5));
    
    if (isDXTPixelFormat(sourcePixelFormat))
    {
        if (sourcePixelFormat != dxtPixelFormat)
        {
            // This shouldn't happen because we don't offer to convert between DXT formats
            err = internalComponentErr;
            goto bail;
        }
        
        codec_src = sourceBaseAddress;
        
        long expectedDXTLength = dxtBytesForDimensions(glob->width, glob->height, glob->type);
        
        if (CVPixelBufferGetDataSize(sourceBuffer) < expectedDXTLength)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        codec_src_length = expectedDXTLength;
    }
    else
    {
        const void *encode_src;
        unsigned int encode_src_bytes_per_row;
        
        OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourcePixelFormat);
        
        // If necessary, convert the pixels to a format the encoder can ingest
        if (wantedPixelFormat != sourcePixelFormat)
        {
            formatConvertBuffer = HapCodecBufferCreate(glob->formatConvertPool);
            
            if (formatConvertBuffer == NULL)
            {
                err = memFullErr;
                goto bail;
            }
            
            switch (wantedPixelFormat)
            {
                case kHapCVPixelFormat_CoCgXY:
                    if (sourcePixelFormat == k32BGRAPixelFormat)
                    {
                        ConvertBGR_ToCoCg_Y8888(sourceBaseAddress,
                                                HapCodecBufferGetBaseAddress(formatConvertBuffer),
                                                glob->width,
                                                glob->height,
                                                sourceBytesPerRow,
                                                glob->formatConvertBufferBytesPerRow);
                    }
                    else
                    {
                        ConvertRGB_ToCoCg_Y8888(sourceBaseAddress,
                                                HapCodecBufferGetBaseAddress(formatConvertBuffer),
                                                glob->width,
                                                glob->height,
                                                sourceBytesPerRow,
                                                glob->formatConvertBufferBytesPerRow);
                    }
                    break;
                case k32RGBAPixelFormat:
                    if (sourcePixelFormat == k32BGRAPixelFormat)
                    {
                        vImage_Buffer src = {
                            sourceBaseAddress,
                            glob->height,
                            glob->width,
                            sourceBytesPerRow
                        };
                        
                        vImage_Buffer dst = {
                            HapCodecBufferGetBaseAddress(formatConvertBuffer),
                            glob->height,
                            glob->width,
                            glob->formatConvertBufferBytesPerRow
                        };
                        
                        uint8_t permuteMap[] = {2, 1, 0, 3};
                        vImage_Error permuteError = vImagePermuteChannels_ARGB8888(&src, &dst, permuteMap, kvImageNoFlags);
                        
                        if (permuteError != kvImageNoError)
                        {
                            err = internalComponentErr;
                            goto bail;
                        }
                    }
                    else
                    {
                        err = internalComponentErr;
                        goto bail;
                    }
                    break;
                default:
                    err = internalComponentErr;
                    goto bail;
                    break;
            }
            
            encode_src = HapCodecBufferGetBaseAddress(formatConvertBuffer);
            encode_src_bytes_per_row = glob->formatConvertBufferBytesPerRow;
        }
        else // wantedPixelFormat == sourcePixelFormat
        {
            encode_src = sourceBaseAddress;
            encode_src_bytes_per_row = sourceBytesPerRow;
        }
        
        // Encode the DXT frame
        
        dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
        if (dxtBuffer == NULL)
        {
            err = memFullErr;
            goto bail;
        }
        
        int result = glob->dxtEncoder->encode_function(glob->dxtEncoder,
                                                       encode_src,
                                                       encode_src_bytes_per_row,
                                                       wantedPixelFormat,
                                                       HapCodecBufferGetBaseAddress(dxtBuffer),
                                                       glob->width,
                                                       glob->height);
        
        if (result != 0)
        {
            err = internalComponentErr;
            goto bail;
        }
        
        // Unlock the source buffer as soon as we are finished with it
        // It has to be before we emit the frame because we lose our reference to it then
        CVPixelBufferUnlockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly);
        sourceBuffer = NULL;
        
        // Detach the pixel buffer so it can be recycled
        ICMCompressorSourceFrameDetachPixelBuffer(task->sourceFrame);
        
        // Return the format conversion buffer as soon as we can to minimise the number created
        HapCodecBufferReturn(formatConvertBuffer);
        formatConvertBuffer = NULL;
        
        codec_src = HapCodecBufferGetBaseAddress(dxtBuffer);
        codec_src_length = HapCodecBufferGetSize(dxtBuffer);
    }
    
    unsigned long actualEncodedDataSize = 0;
    unsigned int hapResult = HapEncode(codec_src,
                                       codec_src_length,
                                       glob->dxtFormat,
                                       HapCompressorSnappy,
                                       ICMEncodedFrameGetDataPtr(task->encodedFrame),
                                       glob->maxEncodedDataSize,
                                       &actualEncodedDataSize);
    if (hapResult != HapResult_No_Error)
    {
        err = internalComponentErr;
        goto bail;
    }
    
    ICMEncodedFrameSetDataSize(task->encodedFrame, actualEncodedDataSize);
    
    MediaSampleFlags mediaSampleFlags = 0;
    
    mediaSampleFlags |= mediaSampleDroppable;
    mediaSampleFlags |= mediaSampleDoesNotDependOnOthers;
	
	err = ICMEncodedFrameSetMediaSampleFlags(task->encodedFrame, mediaSampleFlags );
	if( err )
		goto bail;
	
	err = ICMEncodedFrameSetFrameType( task->encodedFrame, kICMFrameType_I );
	if (err)
        goto bail;
    
#ifdef DEBUG
    OSSpinLockLock(&glob->lock);
    glob->debugFrameCount++;
    glob->debugLastFrameTime = mach_absolute_time();
    glob->debugTotalFrameBytes += actualEncodedDataSize;
    if (actualEncodedDataSize > glob->debugLargestFrameBytes) glob->debugLargestFrameBytes = actualEncodedDataSize;
    if (actualEncodedDataSize < glob->debugSmallestFrameBytes || glob->debugSmallestFrameBytes == 0) glob->debugSmallestFrameBytes = actualEncodedDataSize;
    OSSpinLockUnlock(&glob->lock);
#endif
bail:
    debug_print_err(glob, err);
    if (sourceBuffer) CVPixelBufferUnlockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly);
    HapCodecBufferReturn(formatConvertBuffer);
    HapCodecBufferReturn(dxtBuffer);
    
    task->error = err;
    
    // Queue the encoded frame for output
    queueEncodedFrame(glob, (HapCodecBufferRef)info);
}

// Presents the compressor with a frame to encode.
// The compressor may encode the frame immediately or queue it for later encoding.
// If the compressor queues the frame for later decode, it must retain it (by calling ICMCompressorSourceFrameRetain)
// and release it when it is done with it (by calling ICMCompressorSourceFrameRelease).
// Pixel buffers are guaranteed to conform to the pixel buffer options returned by ImageCodecPrepareToCompressFrames.
ComponentResult 
Hap_CEncodeFrame(
  HapCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags )
{
#pragma unused (flags)
	ComponentResult err = noErr;
    
    CVPixelBufferRef sourcePixelBuffer = ICMCompressorSourceFrameGetPixelBuffer(sourceFrame);
    OSType sourceFormat = CVPixelBufferGetPixelFormatType(sourcePixelBuffer);
    
    if (!isDXTPixelFormat(sourceFormat))
    {
        // Create a DXT encoder if one will be needed by this frame
        if (glob->dxtEncoder == NULL)
        {
            if (glob->type == kHapYCoCgCodecSubType)
            {
                glob->dxtEncoder = HapCodecYCoCgDXTEncoderCreate();
            }
            else if (glob->quality < codecHighQuality)
            {
                glob->dxtEncoder = HapCodecGLEncoderCreate(glob->width,
                                                           glob->height,
                                                           (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1));
            }
            else
            {
                glob->dxtEncoder = HapCodecSquishEncoderCreate(HapCodecSquishEncoderMediumQuality,
                                                               (glob->type == kHapAlphaCodecSubType ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1));
            }
            if (glob->dxtEncoder == NULL)
            {
                err = internalComponentErr;
                goto bail;
            }
        }
        
        // Create a DXT buffer pool if one doesn't already exist
        if (glob->dxtBufferPool == NULL)
        {
            long wantedDXTSize = dxtBytesForDimensions(glob->width, glob->height, glob->type);
            
            glob->dxtBufferPool = HapCodecBufferPoolCreate(wantedDXTSize);
            
            if (glob->dxtBufferPool == NULL)
            {
                err = internalComponentErr;
                goto bail;
            }
        }
        
        if (glob->formatConvertPool == NULL)
        {
            OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourceFormat);
            
            // Create a buffer to convert the RGBA pixel buffer to an ordering the DXT encoder supports
            if (wantedPixelFormat != sourceFormat)
            {
                size_t wantedConvertBufferBytesPerRow = roundUpToMultipleOf16(glob->width * 4);
                size_t wantedBufferSize = wantedConvertBufferBytesPerRow * glob->height;
                
                glob->formatConvertPool = HapCodecBufferPoolCreate(wantedBufferSize);
                
                if (glob->formatConvertPool == NULL)
                {
                    err = internalComponentErr;
                    goto bail;
                }
                glob->formatConvertBufferBytesPerRow = wantedConvertBufferBytesPerRow;
            }

        }
    }
    
    // Create a task and dispatch it
    HapCodecBufferRef buffer = HapCodecBufferCreate(glob->compressTaskPool);
    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(buffer);
    if (task == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    task->sourceFrame = ICMCompressorSourceFrameRetain(sourceFrame);
    task->glob = glob;
    task->error = noErr;
    task->encodedFrame = NULL;
    task->next = NULL;
    
    HapCodecTasksAddTask(Background_Encode, glob->taskGroup, buffer);
    
    // Dequeue and deliver any encoded frames
    do
    {
        buffer = dequeueNextFrameOut(glob);
        err = finishFrame(buffer);
        HapCodecBufferReturn(buffer);
        if (err != noErr)
        {
            goto bail;
        }
    } while (buffer != NULL);
    
bail:
    debug_print_err(glob, err);
	return err;
}

// Directs the compressor to finish with a queued source frame, either emitting or dropping it.
// This frame does not necessarily need to be the first or only source frame emitted or dropped
// during this call, but the compressor must call either ICMCompressorSessionDropFrame or 
// ICMCompressorSessionEmitEncodedFrame with this frame before returning.
// The ICM will call this function to force frames to be encoded for the following reasons:
//   - the maximum frame delay count or maximum frame delay time in the sessionOptions
//     does not permit more frames to be queued
//   - the client has called ICMCompressionSessionCompleteFrames.
ComponentResult 
Hap_CCompleteFrame( 
  HapCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags )
{
#pragma unused (sourceFrame, flags)
    ComponentResult err = noErr;
    if (glob->taskGroup)
    {
        // This waits for all pending frames rather than the particular frame
        // It makes the progress-bar jerky in QuickTime Player but otherwise isn't
        // a problem.
        HapCodecTasksWaitForGroupToComplete(glob->taskGroup);
    }
    HapCodecBufferRef buffer;
    
    do
    {
        buffer = dequeueNextFrameOut(glob);
        
        err = finishFrame(buffer);
        HapCodecBufferReturn(buffer);
        if (err != noErr)
        {
            goto bail;
        }

    } while (buffer != NULL);
    
bail:
	return err;
}

static void releaseTaskFrames(HapCodecCompressTask *task)
{
    if (task)
    {
        ICMCompressorSourceFrameRelease(task->sourceFrame);
        ICMEncodedFrameRelease(task->encodedFrame);
    }
}

static ComponentResult finishFrame(HapCodecBufferRef buffer)
{
    ComponentResult err = noErr;
    if (buffer != NULL)
    {
        HapCodecCompressTask *task = HapCodecBufferGetBaseAddress(buffer);
        
        err = task->error;
        
        task->glob->lastFrameOut = ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame);
        
        if (task->error == noErr)
        {
            ICMCompressorSessionEmitEncodedFrame(task->glob->session, task->encodedFrame, 1, &task->sourceFrame);
        }
        else
        {
            ICMCompressorSessionDropFrame(task->glob->session, task->sourceFrame);
        }
        releaseTaskFrames(task);
    }
    return err;
}

static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame)
{
    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(frame);
    if (glob && task)
    {
        OSSpinLockLock(&glob->lock);
        task->next = glob->finishedFrames;
        glob->finishedFrames = frame;
        OSSpinLockUnlock(&glob->lock);
    }
}

static HapCodecBufferRef dequeueNextFrameOut(HapCompressorGlobals glob)
{
    HapCodecBufferRef found = NULL;
    if (glob)
    {
        OSSpinLockLock(&glob->lock);
        HapCodecBufferRef *prevPtr = &glob->finishedFrames;
        HapCodecBufferRef this = glob->finishedFrames;
        while (this) {
            HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(this);
            if (task)
            {
                long frameNumber = ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame);
                if (frameNumber == glob->lastFrameOut + 1)
                {
                    found = this;
                    *prevPtr = task->next;
                    break;
                }
                this = task->next;
                prevPtr = &task->next;
            }
            else
            {
                this = NULL;
            }
            
        }
        OSSpinLockUnlock(&glob->lock);
    }
    return found;
}
