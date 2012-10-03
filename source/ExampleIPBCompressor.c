/*

File: ExampleIPBCompressor.c, part of ExampleIPBCodec

Abstract: Example Image Compressor using new IPB-capable component interface in QuickTime 7.

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


#if __APPLE_CC__
    #include <QuickTime/QuickTime.h>
#else
    #include <ConditionalMacros.h>
    #include <Endian.h>
    #include <ImageCodec.h>
#endif

#include "ExampleIPBCodecVersion.h"
#include "VPUCodecUtility.h"
#include "PixelFormats.h"
#include "vpu.h"
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

typedef struct VPUCodecCompressTask VPUCodecCompressTask;

typedef struct {
	ComponentInstance				self;
	ComponentInstance				target;
	
	ICMCompressorSessionRef 		session; // NOTE: we do not need to retain or release this
	ICMCompressionSessionOptionsRef	sessionOptions;
	
	long							width;
	long							height;
	size_t							maxEncodedDataSize;
	int								nextDecodeNumber;
	
    int                             lastFrameOut;
    VPUCodecBufferRef               *finishedFrames;
    int                             finishedFramesCapacity;
    OSSpinLock                      lock;
    
    Boolean                         endTasksPending;
    VPUCodecBufferPoolRef           compressTaskPool;
    
    VPUPCodecDXTEncoderRef          dxtEncoder;
    
    VPUCodecBufferPoolRef           formatConvertPool;
    
    VPUCodecBufferPoolRef           dxtBufferPool;
    
    Boolean                         allowAsyncCompletion;
    unsigned int                    dxtFormat;
    unsigned int                    vpuCompressor;
    unsigned int                    taskGroup;
#ifdef DEBUG
    unsigned int                    debugFrameCount;
    uint64_t                        debugStartTime;
    uint64_t                        debugLastFrameTime;
    unsigned long                   debugTotalFrameBytes;
    unsigned long                   debugLargestFrameBytes;
    unsigned long                   debugSmallestFrameBytes;    
#endif
} ExampleIPBCompressorGlobalsRecord, *ExampleIPBCompressorGlobals;

struct VPUCodecCompressTask
{
    ExampleIPBCompressorGlobals glob;
    int frameNumber;
    ICMCompressorSourceFrameRef sourceFrame;
    ICMMutableEncodedFrameRef encodedFrame;
};

// Setup required for ComponentDispatchHelper.c
#define IMAGECODEC_BASENAME() 		ExampleIPB_C
#define IMAGECODEC_GLOBALS() 		ExampleIPBCompressorGlobals storage

#define CALLCOMPONENT_BASENAME()	IMAGECODEC_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		IMAGECODEC_GLOBALS()

#define QT_BASENAME()				CALLCOMPONENT_BASENAME()
#define QT_GLOBALS()				CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppImageCodec
#define COMPONENT_DISPATCH_FILE		"ExampleIPBCompressorDispatch.h"
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

static void emitFrame(VPUCodecBufferRef buffer, bool onBackgroundThread);
static void queueEncodedFrame(ExampleIPBCompressorGlobals glob, VPUCodecBufferRef frame);
static VPUCodecBufferRef dequeueFrameNumber(ExampleIPBCompressorGlobals glob, int number);

// Open a new instance of the component.
// Allocate component instance storage ("globals") and associate it with the new instance so that other
// calls will receive it.  
// Note that "one-shot" component calls like CallComponentVersion and ImageCodecGetCodecInfo work by opening
// an instance, making that call and then closing the instance, so you should avoid performing very expensive
// initialization operations in a component's Open function.
ComponentResult 
ExampleIPB_COpen(
  ExampleIPBCompressorGlobals glob, 
  ComponentInstance self )
{    
	ComponentResult err = noErr;
	    
	glob = calloc( sizeof( ExampleIPBCompressorGlobalsRecord ), 1 );
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}
	SetComponentInstanceStorage( self, (Handle)glob );

	glob->self = self;
	glob->target = self;
	
	glob->nextDecodeNumber = 1;
    
    glob->lock = OS_SPINLOCK_INIT;
    glob->endTasksPending = false;
    
    glob->dxtEncoder = NULL;
    glob->formatConvertPool = NULL;
    glob->dxtBufferPool = NULL;

    glob->vpuCompressor = VPUCompressorSnappy;
    
bail:
    debug_print_err(glob, err);
	return err;
}

// Closes the instance of the component.
// Release all storage associated with this instance.
// Note that if a component's Open function fails with an error, its Close function will still be called.
ComponentResult 
ExampleIPB_CClose(
  ExampleIPBCompressorGlobals glob, 
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

            printf("VPU CODEC: %u frames over %.1f seconds %.1f FPS. ", glob->debugFrameCount, time, glob->debugFrameCount / time);
            printf("Largest frame bytes: %lu smallest: %lu average: %lu ",
                   glob->debugLargestFrameBytes, glob->debugSmallestFrameBytes, glob->debugTotalFrameBytes/ glob->debugFrameCount);
            unsigned int uncompressed = glob->width * glob->height;
            if (glob->dxtFormat == VPUTextureFormat_RGB_DXT1) uncompressed /= 2;
            uncompressed += 4U; // VPU uses 4 extra bytes
            printf("uncompressed: %d\n", uncompressed);
        }
#endif
		ICMCompressionSessionOptionsRelease( glob->sessionOptions );
		glob->sessionOptions = NULL;
        
        VPUCodecDestroyBufferPool(glob->dxtBufferPool);
        
        if (glob->dxtEncoder && glob->dxtEncoder->destroy_function)
        {
            glob->dxtEncoder->destroy_function(glob->dxtEncoder);
        }
        glob->dxtEncoder = NULL;
        
        VPUCodecDestroyBufferPool(glob->formatConvertPool);
        glob->formatConvertPool = NULL;
        
        VPUCodecDestroyBufferPool(glob->compressTaskPool);
        glob->compressTaskPool = NULL;
        
        if (glob->endTasksPending)
        {
            VPUCodecWillStopTasks();
        }
        
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
ExampleIPB_CVersion(ExampleIPBCompressorGlobals glob)
{
#pragma unused (glob)
	return kExampleIPBCompressorVersion;
}

// Sets the target for a component instance.
// When a component wants to make a component call on itself, it should make that call on its target.
// This allows other components to delegate to the component.
// By default, a component's target is itself -- see the Open function.
ComponentResult 
ExampleIPB_CTarget(ExampleIPBCompressorGlobals glob, ComponentInstance target)
{
	glob->target = target;
	return noErr;
}

// Your component receives the ImageCodecGetCodecInfo request whenever an application calls the Image Compression Manager's GetCodecInfo function.
// Your component should return a formatted compressor information structure defining its capabilities.
// Both compressors and decompressors may receive this request.
ComponentResult 
ExampleIPB_CGetCodecInfo(ExampleIPBCompressorGlobals glob, CodecInfo *info)
{
	OSErr err = noErr;

	if (info == NULL)
    {
		err = paramErr;
	}
	else
    {
		CodecInfo **tempCodecInfo;

		err = GetComponentResource((Component)glob->self, codecInfoResourceType, 256, (Handle *)&tempCodecInfo);
		if (err == noErr)
        {
			*info = **tempCodecInfo;
			DisposeHandle((Handle)tempCodecInfo);
		}
	}
    
    debug_print_err(glob, err);
	return err;
}

// Return the maximum size of compressed data for the image in bytes.
// Note that this function is only used when the ICM client is using a compression sequence
// (created with CompressSequenceBegin, not ICMCompressionSessionCreate).
// Nevertheless, it's important to implement it because such clients need to know how much 
// memory to allocate for compressed frame buffers.
ComponentResult 
ExampleIPB_CGetMaxCompressionSize(
	ExampleIPBCompressorGlobals glob, 
	PixMapHandle        src,
	const Rect *        srcRect,
	short               depth,
	CodecQ              quality,
	long *              size)
{
#pragma unused (glob, src, quality)
	
	if( ! size )
		return paramErr;
    int dxtSize = roundUpToMultipleOf4(srcRect->right - srcRect->left) * roundUpToMultipleOf4(srcRect->bottom - srcRect->top);
    if (depth == 24) dxtSize /= 2;
	*size = VPUMaxEncodedLength(dxtSize);
    
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
                                      CFMutableDictionaryRef *pixelBufferAttributesOut )
{
	OSStatus err = memFullErr;
	int i;
	CFMutableDictionaryRef pixelBufferAttributes = NULL;
	CFNumberRef number = NULL;
	CFMutableArrayRef array = NULL;
//	SInt32 widthRoundedUp, heightRoundedUp, extendRight, extendBottom;
	
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
	
	// If you want to require that extra scratch pixels be allocated on the edges of source pixel buffers,
	// add the kCVPixelBufferExtendedPixels{Left,Top,Right,Bottom}Keys to indicate how much.
	// Internally our encoded can only support multiples of 16x16 macroblocks; 
	// we will round the compression dimensions up to a multiple of 16x16 and encode that size.  
	// (Note that if your compressor needs to copy the pixels anyhow (eg, in order to convert to a different 
	// format) you may get better performance if your copy routine does not require extended pixels.)
//	widthRoundedUp = roundUpToMultipleOf16( width );
//	heightRoundedUp = roundUpToMultipleOf16( height );
//	extendRight = widthRoundedUp - width;
//	extendBottom = heightRoundedUp - height;
//	if( extendRight || extendBottom ) {
//		addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferExtendedPixelsRightKey, extendRight );
//		addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferExtendedPixelsBottomKey, extendBottom );
//	}
	
	// Altivec code is most efficient reading data aligned at addresses that are multiples of 16.
	// Pretending that we have some altivec code, we set kCVPixelBufferBytesPerRowAlignmentKey to
	// ensure that each row of pixels starts at a 16-byte-aligned address.
    // TODO: if we only require that we are 4-byte aligned then reduce this
//	addNumberToDictionary( pixelBufferAttributes, kCVPixelBufferBytesPerRowAlignmentKey, 16 );
	
	// This codec accepts YCbCr input in the form of '2vuy' format pixel buffers.
	// We recommend explicitly defining the gamma level and YCbCr matrix that should be used.
    // TODO: fix gamma
//	addDoubleToDictionary( pixelBufferAttributes, kCVImageBufferGammaLevelKey, 2.2 );
//    addDoubleToDictionary( pixelBufferAttributes, kCVImageBufferGammaLevelKey, 1.0 );
//	CFDictionaryAddValue( pixelBufferAttributes, kCVImageBufferYCbCrMatrixKey, kCVImageBufferYCbCrMatrix_ITU_R_601_4 );
	
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
ExampleIPB_CPrepareToCompressFrames(
  ExampleIPBCompressorGlobals glob,
  ICMCompressorSessionRef session,
  ICMCompressionSessionOptionsRef sessionOptions,
  ImageDescriptionHandle imageDescription,
  void *reserved,
  CFDictionaryRef *compressorPixelBufferAttributesOut)
{
#pragma unused (reserved)
	ComponentResult err = noErr;
	CFMutableDictionaryRef compressorPixelBufferAttributes = NULL;
    // TODO: other pixel formats? non-A formats
    OSType pixelFormatList[] = { k32BGRAPixelFormat, k32RGBAPixelFormat };
	Fixed gammaLevel;
//	SInt32 widthRoundedUp, heightRoundedUp;
	
	// Record the compressor session for later calls to the ICM.
	// Note: this is NOT a CF type and should NOT be CFRetained or CFReleased.
	glob->session = session;
	
	// Retain the session options for later use.
	ICMCompressionSessionOptionsRelease( glob->sessionOptions );
	glob->sessionOptions = sessionOptions;
	ICMCompressionSessionOptionsRetain( glob->sessionOptions );
	
	// Modify imageDescription
    
    /*
    Fixed gammaLevelIn = 0;
    err = ICMImageDescriptionGetProperty(imageDescription, kQTPropertyClass_ImageDescription, kICMImageDescriptionPropertyID_GammaLevel, sizeof(gammaLevelIn), &gammaLevelIn, NULL);
    if (err == noErr)
    {
        printf("input gamma level %f\n", FixedToFloat(gammaLevelIn));
    }
    else if (err == codecExtensionNotFoundErr)
    {
        printf("no input gamma level\n");
    }
     */
	// We describe gamma as platform default, as that's what we get in
    // and we don't convert it. More solid solutions welcome...
    
	gammaLevel = kQTUsePlatformDefaultGammaLevel;
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
    
	// Create a pixel buffer attributes dictionary.
	err = createPixelBufferAttributesDictionary( glob->width, glob->height, 
			pixelFormatList, sizeof(pixelFormatList) / sizeof(OSType),
			&compressorPixelBufferAttributes );
	if( err ) 
		goto bail;

	*compressorPixelBufferAttributesOut = compressorPixelBufferAttributes;
	compressorPixelBufferAttributes = NULL;
	
    UInt32 depth = 0;
    err = ICMCompressionSessionOptionsGetProperty( glob->sessionOptions,
                                                  kQTPropertyClass_ICMCompressionSessionOptions,
                                                  kICMCompressionSessionOptionsPropertyID_Depth,
                                                  sizeof( depth ),
                                                  &depth,
                                                  NULL );
    if( err )
        goto bail;
    
    bool alpha;
    // Different apps send different things here...
    switch (depth) {
        case k32BGRAPixelFormat:
        case 32:
            alpha = true;
            break;
        default:
            // Treat any other depth as k24BGRPixelFormat
            alpha = false;;
            break;
    }
    
    if (alpha)
        (*imageDescription)->depth = 32;
    else
        (*imageDescription)->depth = 24;
    
    CodecQ quality;
    if(glob->sessionOptions) {
        
		err = ICMCompressionSessionOptionsGetProperty(glob->sessionOptions,
                                                      kQTPropertyClass_ICMCompressionSessionOptions,
                                                      kICMCompressionSessionOptionsPropertyID_Quality,
                                                      sizeof( quality ),
                                                      &quality,
                                                      NULL );
		if( err )
			goto bail;
    }
    else
    {
        quality = codecLosslessQuality;
    }
    
    // TODO: currently we use quality to enable the faster, lower quality compressor.
    // This isn't a particularly clear way to highlite the speed gain it offers.
    // http://developer.apple.com/library/mac/#technotes/tn2081/_index.html if we want to remove the Quality slider and provide custom UI
    
    if (quality < codecLowQuality)
    {
        glob->dxtEncoder = VPUPGLEncoderCreate(glob->width, glob->height, alpha ? kVPUCVPixelFormat_RGBA_DXT5 : kVPUCVPixelFormat_RGB_DXT1);
        glob->dxtFormat = alpha ? VPUTextureFormat_RGBA_DXT5 : VPUTextureFormat_RGB_DXT1;
    }
    if (glob->dxtEncoder == NULL && (quality < codecMaxQuality || alpha))
    {
        VPUPCodecSquishEncoderQuality encoder_quality;
        if (quality < codecNormalQuality)
            encoder_quality = VPUPCodecSquishEncoderWorstQuality;
        else if (quality < codecHighQuality)
            encoder_quality = VPUPCodecSquishEncoderMediumQuality;
        else
            encoder_quality = VPUPCodecSquishEncoderBestQuality;
        
        glob->dxtEncoder = VPUPSquishEncoderCreate(encoder_quality, alpha ? kVPUCVPixelFormat_RGBA_DXT5 : kVPUCVPixelFormat_RGB_DXT1);
        glob->dxtFormat = alpha ? VPUTextureFormat_RGBA_DXT5 : VPUTextureFormat_RGB_DXT1;
    }
    if (glob->dxtEncoder == NULL)
    {
        glob->dxtEncoder = VPUPYCoCgDXTEncoderCreate();
        glob->dxtFormat = VPUTextureFormat_YCoCg_DXT5;
    }
    if (glob->dxtEncoder == NULL)
    {
        err = internalComponentErr;
        goto bail;
    }
    
    // If we're allowed to, we output frames on a background thread
    err = ICMCompressionSessionOptionsGetProperty(glob->sessionOptions,
                                                  kQTPropertyClass_ICMCompressionSessionOptions,
                                                  kICMCompressionSessionOptionsPropertyID_AllowAsyncCompletion,
                                                  sizeof( Boolean ),
                                                  &glob->allowAsyncCompletion,
                                                  NULL );
    
	// Work out the upper bound on encoded frame data size -- we'll allocate buffers of this size.
    long wantedDXTSize = roundUpToMultipleOf4(glob->width) * roundUpToMultipleOf4(glob->height);
    if (glob->dxtFormat == VPUTextureFormat_RGB_DXT1) wantedDXTSize /= 2;
    
    if (VPUCodecGetBufferPoolBufferSize(glob->dxtBufferPool) != wantedDXTSize)
    {
        VPUCodecDestroyBufferPool(glob->dxtBufferPool);
        glob->dxtBufferPool = VPUCodecCreateBufferPool(wantedDXTSize);
    }
    if (glob->dxtBufferPool == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    glob->maxEncodedDataSize = VPUMaxEncodedLength(wantedDXTSize);
    
#ifdef DEBUG
    char *compressor_str;
    switch (glob->vpuCompressor) {
        case VPUCompressorSnappy:
            compressor_str = "snappy";
            break;
        case VPUCompressorLZF:
            compressor_str = "LZF";
            break;
        case VPUCompressorZLIB:
            compressor_str = "ZLIB";
            break;
        default:
            err = internalComponentErr;
            goto bail;
    }
    printf("VPU CODEC: start / %s / ", compressor_str);
    glob->dxtEncoder->show_function(glob->dxtEncoder);
#endif // DEBUG
    
    if (glob->compressTaskPool == NULL)
    {
        glob->compressTaskPool = VPUCodecCreateBufferPool(sizeof(VPUCodecCompressTask));
    }
    
    if (glob->compressTaskPool == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    VPUCodecWillStartTasks();
    glob->endTasksPending = true;
    
    glob->taskGroup = VPUCodecNewTaskGroup();
    
    glob->finishedFrames = NULL;
    glob->finishedFramesCapacity = 0;
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
    VPUCodecBufferRef formatConvertBuffer = NULL;
    VPUCodecBufferRef dxtBuffer = NULL;
    CVPixelBufferRef sourceBuffer = NULL;
    VPUCodecCompressTask *task = VPUCodecGetBufferBaseAddress((VPUCodecBufferRef)info);
    ExampleIPBCompressorGlobals glob = task->glob;
    
    // TODO: Don't ignore errors here, feed them back to an API thread for reporting and to drop the frame
    ///////// BEGIN QUICK AND DIRTY OUTPUT
    ComponentResult err = noErr;
    
    ICMMutableEncodedFrameRef output = NULL;
    
    err = ICMEncodedFrameCreateMutable(glob->session, task->sourceFrame, glob->maxEncodedDataSize, &output);
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
    
    // TODO: once we support DXT in, be prepared to skip the DXT encoding stage entirely
    
    OSType wantedPixelFormat = glob->dxtEncoder->pixelformat_function(glob->dxtEncoder, sourcePixelFormat);
    
    // If necessary, convert the pixels to a format the encoder can ingest
    if (wantedPixelFormat != sourcePixelFormat)
    {
        // TODO: right now all the pixel formats we accept have the same number of bits per pixel
        // but we will accept DXT etc buffers in, so this will have to be more flexible
        size_t wantedBufferSize = glob->width * glob->height * 4;
        if (VPUCodecGetBufferPoolBufferSize(glob->formatConvertPool) != wantedBufferSize)
        {
            VPUCodecDestroyBufferPool(glob->formatConvertPool);
            glob->formatConvertPool = VPUCodecCreateBufferPool(wantedBufferSize);
        }
        
        formatConvertBuffer = VPUCodecGetBuffer(glob->formatConvertPool);
        
        if (formatConvertBuffer == NULL)
        {
            err = memFullErr;
            goto bail;
        }
        
        switch (wantedPixelFormat)
        {
            case kVPUCVPixelFormat_YCoCgX:
                if (sourcePixelFormat == k32BGRAPixelFormat)
                {
                    ConvertBGR_ToCoCg_Y8888(sourceBaseAddress,
                                            VPUCodecGetBufferBaseAddress(formatConvertBuffer),
                                            glob->width,
                                            glob->height,
                                            sourceBytesPerRow,
                                            glob->width * 4);
                }
                else
                {
                    ConvertRGB_ToCoCg_Y8888(sourceBaseAddress,
                                            VPUCodecGetBufferBaseAddress(formatConvertBuffer),
                                            glob->width,
                                            glob->height,
                                            sourceBytesPerRow,
                                            glob->width * 4);
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
                        VPUCodecGetBufferBaseAddress(formatConvertBuffer),
                        glob->height,
                        glob->width,
                        glob->width * 4
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
    }
    
    // Encode the DXT frame
    const void *encode_src;
    unsigned int encode_src_bytes_per_row;
    
    dxtBuffer = VPUCodecGetBuffer(glob->dxtBufferPool);
    if (dxtBuffer == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    if (formatConvertBuffer)
    {
        encode_src = VPUCodecGetBufferBaseAddress(formatConvertBuffer);
        encode_src_bytes_per_row = glob->width * 4;
    }
    else
    {
        encode_src = sourceBaseAddress;
        encode_src_bytes_per_row = sourceBytesPerRow;
    }
    
    int result = glob->dxtEncoder->encode_function(glob->dxtEncoder,
                                                   encode_src,
                                                   encode_src_bytes_per_row,
                                                   wantedPixelFormat,
                                                   VPUCodecGetBufferBaseAddress(dxtBuffer),
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
    
    // Return the format conversion buffer as soon as we can to minimise the number created
    VPUCodecReturnBuffer(formatConvertBuffer);
    formatConvertBuffer = NULL;
    
    const void *codec_src = VPUCodecGetBufferBaseAddress(dxtBuffer);
    unsigned int codec_src_length = VPUCodecGetBufferSize(dxtBuffer);
    
    unsigned long actualEncodedDataSize = 0;
    unsigned int vpuResult = VPUEncode(codec_src,
                                       codec_src_length,
                                       glob->dxtFormat,
                                       glob->vpuCompressor,
                                       ICMEncodedFrameGetDataPtr(output),
                                       glob->maxEncodedDataSize,
                                       &actualEncodedDataSize);
    if (vpuResult != VPUResult_No_Error)
    {
        err = internalComponentErr;
        goto bail;
    }
    
    ICMEncodedFrameSetDataSize(output, actualEncodedDataSize);
    
    MediaSampleFlags mediaSampleFlags = 0;
    
    mediaSampleFlags |= mediaSampleDroppable;
    mediaSampleFlags |= mediaSampleDoesNotDependOnOthers;
	
	err = ICMEncodedFrameSetMediaSampleFlags(output, mediaSampleFlags );
	if( err )
		goto bail;
	
	err = ICMEncodedFrameSetFrameType( output, kICMFrameType_I );
	if (err)
        goto bail;
    
	// Output the encoded frame.
    task->encodedFrame = output;
    output = NULL;
    // TODO: we could detach the source frame's pixel buffer at this point
    
    if (glob->allowAsyncCompletion)
    {
        emitFrame((VPUCodecBufferRef)info, true);
    }
    else
    {
        queueEncodedFrame(glob, (VPUCodecBufferRef)info);
    }
    
#ifdef DEBUG
    // TODO: do this on the "main" thread to avoid the lock
    OSSpinLockLock(&glob->lock);
    glob->debugFrameCount++;
    glob->debugLastFrameTime = mach_absolute_time();
    glob->debugTotalFrameBytes += actualEncodedDataSize;
    if (actualEncodedDataSize > glob->debugLargestFrameBytes) glob->debugLargestFrameBytes = actualEncodedDataSize;
    if (actualEncodedDataSize < glob->debugSmallestFrameBytes || glob->debugSmallestFrameBytes == 0) glob->debugSmallestFrameBytes = actualEncodedDataSize;
    OSSpinLockUnlock(&glob->lock);
#endif
    ///////// END QUICK AND DIRTY OUTPUT
bail:
    if (sourceBuffer) CVPixelBufferUnlockBaseAddress(sourceBuffer, kCVPixelBufferLock_ReadOnly);
    if (output) ICMEncodedFrameRelease( output );
    VPUCodecReturnBuffer(formatConvertBuffer);
    VPUCodecReturnBuffer(dxtBuffer);
    debug_print_err(glob, err);
    // TODO: do something with err
}

// Presents the compressor with a frame to encode.
// The compressor may encode the frame immediately or queue it for later encoding.
// If the compressor queues the frame for later decode, it must retain it (by calling ICMCompressorSourceFrameRetain)
// and release it when it is done with it (by calling ICMCompressorSourceFrameRelease).
// Pixel buffers are guaranteed to conform to the pixel buffer options returned by ImageCodecPrepareToCompressFrames.
ComponentResult 
ExampleIPB_CEncodeFrame(
  ExampleIPBCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags )
{
#pragma unused (flags)
	ComponentResult err = noErr;
    
    VPUCodecBufferRef buffer = VPUCodecGetBuffer(glob->compressTaskPool);
    VPUCodecCompressTask *task = (VPUCodecCompressTask *)VPUCodecGetBufferBaseAddress(buffer);
    if (task == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    task->sourceFrame = ICMCompressorSourceFrameRetain(sourceFrame);
    task->glob = glob;
    task->frameNumber = ICMCompressorSourceFrameGetDisplayNumber(sourceFrame); // TODO we could just call this whenever we need it and not store it
    
    VPUCodecTask(Background_Encode, glob->taskGroup, buffer);
    
    if (glob->allowAsyncCompletion == false)
    {
        do
        {
            buffer = dequeueFrameNumber(glob, glob->lastFrameOut + 1); // TODO: this function could just look up the next frame number from glob, ah well
            emitFrame(buffer, false);
        } while (buffer != NULL);
    }
    
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
ExampleIPB_CCompleteFrame( 
  ExampleIPBCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags )
{
#pragma unused (sourceFrame, flags)
    VPUCodecWaitForTasksToComplete(glob->taskGroup); // TODO: this waits for all pending frames rather than the particular frame
    VPUCodecBufferRef buffer;
    
    if (glob->allowAsyncCompletion == false)
    {
        do
        {
            buffer = dequeueFrameNumber(glob, glob->lastFrameOut + 1); // TODO: this function could just look up the next frame number from glob, ah well
            
            emitFrame(buffer, false);

        } while (buffer != NULL);
    }
	return noErr;
}

static void emitFrame(VPUCodecBufferRef buffer, bool onBackgroundThread)
{
    if (buffer != NULL)
    {
        VPUCodecCompressTask *task = VPUCodecGetBufferBaseAddress(buffer);
        if (onBackgroundThread)
        {
            OSSpinLockLock(&task->glob->lock);
        }
        task->glob->lastFrameOut = task->frameNumber;
        if (onBackgroundThread)
        {
            OSSpinLockUnlock(&task->glob->lock);
        }
        ICMCompressorSessionEmitEncodedFrame(task->glob->session, task->encodedFrame, 1, &task->sourceFrame); // TODO: we could maybe emit multiple frames at once if we're able
        ICMCompressorSourceFrameRelease(task->sourceFrame);
        ICMEncodedFrameRelease(task->encodedFrame);
        VPUCodecReturnBuffer(buffer);
    }
}

static void queueEncodedFrame(ExampleIPBCompressorGlobals glob, VPUCodecBufferRef frame)
{
    if (glob)
    {
        OSSpinLockLock(&glob->lock);
        int i;
        int insertIndex = -1; // -1 == not found
        // Look for a free slot
        for (i = 0; i < glob->finishedFramesCapacity; i++)
        {
            if (glob->finishedFrames[i] == NULL)
            {
                insertIndex = i;
                break;
            }
        }
        // If no slot available then grow finishedFrames by one
        if (insertIndex == -1)
        {
            glob->finishedFramesCapacity++;
            VPUCodecBufferRef *newFinishedFrames = malloc(sizeof(VPUCodecBufferRef) * glob->finishedFramesCapacity);
            for (i = 0; i < glob->finishedFramesCapacity; i++) {
                if (i < (glob->finishedFramesCapacity - 1) && glob->finishedFrames != NULL)
                {
                    newFinishedFrames[i] = glob->finishedFrames[i];
                }
                else
                {
                    newFinishedFrames[i] = NULL;
                }
            }
            free(glob->finishedFrames);
            glob->finishedFrames = newFinishedFrames;
            insertIndex = glob->finishedFramesCapacity - 1;
        }
        glob->finishedFrames[insertIndex] = frame;
        OSSpinLockUnlock(&glob->lock);
    }
}

static VPUCodecBufferRef dequeueFrameNumber(ExampleIPBCompressorGlobals glob, int number)
{
    VPUCodecBufferRef found = NULL;
    if (glob)
    {
        OSSpinLockLock(&glob->lock);
        if (glob->finishedFrames != NULL)
        {
            int i;
            for (i = 0; i < glob->finishedFramesCapacity; i++)
            {
                if (glob->finishedFrames[i] != NULL)
                {
                    VPUCodecCompressTask *task = (VPUCodecCompressTask *)VPUCodecGetBufferBaseAddress(glob->finishedFrames[i]);
                    if (task->frameNumber == number)
                    {
                        found = glob->finishedFrames[i];
                        glob->finishedFrames[i] = NULL;
                    }
                }
            }
        }
        OSSpinLockUnlock(&glob->lock);
    }
    return found;
}

/*
 These values are stored in users' compression settings, don't change them
 */
#define kVPUCompressorSnappy 1
#define kVPUCompressorLZF 2
#define kVPUCompressorZLIB 3

static UInt8 storedConstantForCompressor(unsigned int compressor)
{
    switch (compressor) {
        case VPUCompressorLZF:
            return kVPUCompressorLZF;
        case VPUCompressorZLIB:
            return kVPUCompressorZLIB;
        case VPUCompressorSnappy:
        default:
            return kVPUCompressorSnappy;
    }
}

static unsigned int compressorForStoredConstant(UInt8 constant)
{
    switch (constant) {
        case kVPUCompressorLZF:
            return VPUCompressorLZF;
        case kVPUCompressorZLIB:
            return VPUCompressorZLIB;
        case kVPUCompressorSnappy:
        default:
            return VPUCompressorSnappy;
    }
}

ComponentResult ExampleIPB_CGetSettings(ExampleIPBCompressorGlobals glob, Handle settings)
{
    ComponentResult err = noErr;
    
    if (!settings)
    {
        err = paramErr;
    }
    else
    {
        SetHandleSize(settings, 5);
        ((UInt32 *) *settings)[0] = 'VPUV';
        ((UInt8 *) *settings)[4] = storedConstantForCompressor(glob->vpuCompressor);
    }
    
    debug_print_err(glob, err);
    return err;
}

ComponentResult ExampleIPB_CSetSettings(ExampleIPBCompressorGlobals glob, Handle settings)
{
    ComponentResult err = noErr;
    
    if (!settings || GetHandleSize(settings) == 0)
    {
        glob->vpuCompressor = VPUCompressorSnappy;
    }
    else if (GetHandleSize(settings) == 5 && ((UInt32 *) *settings)[0] == 'VPUV')
    {
        glob->vpuCompressor = compressorForStoredConstant(((UInt8 *) *settings)[4]);
    }
    else
    {
        err = paramErr;
    }
    
    debug_print_err(glob, err);
    return err;
}

#define kMyCodecDITLResID 129
#define kMyCodecPopupCNTLResID 129
#define kMyCodecPopupMENUResID 129

ComponentResult ExampleIPB_CGetDITLForSize(ExampleIPBCompressorGlobals glob,
                                           Handle *ditl,
                                           Point *requestedSize)
{
    Handle h = NULL;
    ComponentResult err = noErr;
    
    switch (requestedSize->h) {
        case kSGSmallestDITLSize:
            GetComponentResource((Component)(glob->self), FOUR_CHAR_CODE('DITL'),
                                 kMyCodecDITLResID, &h);
            if (NULL != h) *ditl = h;
            else err = resNotFound;
            break;
        default:
            err = badComponentSelector;
            break;
    }
    
    debug_print_err(glob, err);
    return err;
}

#define kItemPopup 1

ComponentResult ExampleIPB_CDITLInstall(ExampleIPBCompressorGlobals glob,
                                        DialogRef d,
                                        short itemOffset)
{
#pragma unused(glob)
    ControlRef cRef;
    
    unsigned long popupValue = storedConstantForCompressor(glob->vpuCompressor);
    
    GetDialogItemAsControl(d, kItemPopup + itemOffset, &cRef);
    SetControl32BitValue(cRef, popupValue);

    return noErr;
}

ComponentResult ExampleIPB_CDITLEvent(ExampleIPBCompressorGlobals glob,
                                      DialogRef d,
                                      short itemOffset,
                                      const EventRecord *theEvent,
                                      short *itemHit,
                                      Boolean *handled)
{
#pragma unused(glob, d, itemOffset, theEvent, itemHit)
    *handled = false;
    return noErr;
}

ComponentResult ExampleIPB_CDITLItem(ExampleIPBCompressorGlobals glob,
                                     DialogRef d,
                                     short itemOffset,
                                     short itemNum)
{
#pragma unused(glob, d, itemOffset, itemNum)
    
    return noErr;
}

ComponentResult ExampleIPB_CDITLRemove(ExampleIPBCompressorGlobals glob,
                                       DialogRef d,
                                       short itemOffset)
{
    ControlRef cRef;
    unsigned long popupValue;
    
    GetDialogItemAsControl(d, kItemPopup + itemOffset, &cRef);
    popupValue = GetControl32BitValue(cRef);
    
    glob->vpuCompressor = compressorForStoredConstant(popupValue);

    return noErr;
}

ComponentResult ExampleIPB_CDITLValidateInput(ExampleIPBCompressorGlobals storage,
                                              Boolean *ok)
{
#pragma unused(storage)
    if (ok)
        *ok = true;
    return noErr;
}
