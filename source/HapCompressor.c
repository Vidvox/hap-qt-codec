/*
 
HapCompressor.c
Hap Codec
 
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
	
	long							width;
	long							height;
	size_t							maxEncodedDataSize;
	
    int                             lastFrameOut;
    HapCodecBufferRef               *finishedFrames;
    int                             finishedFramesCapacity;
    OSSpinLock                      lock;
    
    Boolean                         endTasksPending;
    HapCodecBufferPoolRef           compressTaskPool;
    
    HapCodecDXTEncoderRef           dxtEncoder;
    
    HapCodecBufferPoolRef           formatConvertPool;
    size_t                          formatConvertBufferBytesPerRow;
    
    HapCodecBufferPoolRef           dxtBufferPool;
    
    Boolean                         allowAsyncCompletion;
    int32_t                         backgroundError;
    unsigned int                    dxtFormat;
    unsigned int                    hapCompressor;
    unsigned int                    taskGroup;
    
    CFMutableDictionaryRef          settings; // Settings from our settings dialog if applicable
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

/*
 kSettingsSecondaryCompressorKey
 
 value is one of the following CFStrings
 */
#define kSettingsSecondaryCompressorKey CFSTR("com.vidvox.hap.settings.secondary-compressor")

#define kSettingsSecondaryCompressorSnappy CFSTR("snappy")
#define kSettingsSecondaryCompressorLZF CFSTR("lzf")
#define kSettingsSecondaryCompressorZLIB CFSTR("zlib")

/*
 kSettingsPreserveAlphaKey
 
 value is a CFBoolean
 */
#define kSettingsPreserveAlphaKey CFSTR("com.vidvox.hap.settings.preserve-alpha")

/*
 kSettingsQualityKey
 
 value is a CFNumber representing a CodecQ
 */
#define kSettingsQualityKey CFSTR("com.vidvox.hap.settings.quality")

static ComponentResult finishFrame(HapCodecBufferRef buffer, bool onBackgroundThread);
static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame);
static HapCodecBufferRef dequeueFrameNumber(HapCompressorGlobals glob, int number);
static void emitBackgroundError(HapCompressorGlobals glob, ComponentResult error);
static ComponentResult backgroundError(HapCompressorGlobals glob);

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
	    
	glob = calloc( sizeof( HapCompressorGlobalsRecord ), 1 );
	if( ! glob ) {
		err = memFullErr;
		goto bail;
	}
	SetComponentInstanceStorage( self, (Handle)glob );
    
	glob->self = self;
	glob->target = self;
    glob->session = NULL;
    glob->width = 0;
    glob->height = 0;
	glob->maxEncodedDataSize = 0;
    glob->lastFrameOut = 0;
    glob->finishedFrames = NULL;
    glob->finishedFramesCapacity = 0;
    glob->lock = OS_SPINLOCK_INIT;
    glob->endTasksPending = false;
    glob->compressTaskPool = NULL;
    glob->dxtEncoder = NULL;
    glob->formatConvertPool = NULL;
    glob->formatConvertBufferBytesPerRow = 0;
    glob->dxtBufferPool = NULL;
    glob->allowAsyncCompletion = false;
    glob->backgroundError = noErr;
    glob->dxtFormat = 0;
    glob->hapCompressor = HapCompressorSnappy;
    glob->taskGroup = 0;
    glob->settings = NULL;
    
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

            printf("HAP CODEC: %u frames over %.1f seconds %.1f FPS. ", glob->debugFrameCount, time, glob->debugFrameCount / time);
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
        
        HapCodecBufferPoolDestroy(glob->compressTaskPool);
        glob->compressTaskPool = NULL;
        
        free(glob->finishedFrames);
        glob->finishedFrames = NULL;
        
        if (glob->endTasksPending)
        {
            HapCodecTasksWillStop();
        }
        
        if (glob->settings)
        {
            CFRelease(glob->settings);
            glob->settings = NULL;
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

		err = GetComponentResource((Component)glob->self, codecInfoResourceType, 256, (Handle *)&tempCodecInfo);
		if (err == noErr)
        {
			*info = **tempCodecInfo;
			DisposeHandle((Handle)tempCodecInfo);
            
            // We suppress this from the resource to avoid having the user-confusing Millions+ menu
            // but we can hand it out to any other interested parties
            info->formatFlags |= codecInfoDepth32;
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
Hap_CGetMaxCompressionSize(
	HapCompressorGlobals glob,
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
    // TODO: other pixel formats? non-A formats
    OSType pixelFormatList[] = { k32BGRAPixelFormat, k32RGBAPixelFormat };
	Fixed gammaLevel;
	
	// Record the compressor session for later calls to the ICM.
	// Note: this is NOT a CF type and should NOT be CFRetained or CFReleased.
	glob->session = session;
	
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
	
    bool alpha;
    
    if (dictionaryHasValueForKeyOfTypeID(glob->settings, kSettingsPreserveAlphaKey, CFBooleanGetTypeID()))
    {
        CFBooleanRef alphaFromSettings = CFDictionaryGetValue(glob->settings, kSettingsPreserveAlphaKey);
        alpha = CFBooleanGetValue(alphaFromSettings);
    }
    else
    {
        UInt32 depth = 0;
        err = ICMCompressionSessionOptionsGetProperty(sessionOptions,
                                                      kQTPropertyClass_ICMCompressionSessionOptions,
                                                      kICMCompressionSessionOptionsPropertyID_Depth,
                                                      sizeof( depth ),
                                                      &depth,
                                                      NULL );
        if( err )
            goto bail;
        
        
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
    }
    
    if (alpha)
        (*imageDescription)->depth = 32;
    else
        (*imageDescription)->depth = 24;
    
    CodecQ quality;
    
    if (dictionaryHasValueForKeyOfTypeID(glob->settings, kSettingsQualityKey, CFNumberGetTypeID()))
    {
        CFNumberRef qualityFromSettings = CFDictionaryGetValue(glob->settings, kSettingsQualityKey);
        SInt32 qualitySInt;
        if (CFNumberGetValue(qualityFromSettings, kCFNumberSInt32Type, &qualitySInt))
        {
            quality = qualitySInt;
        }
        else
        {
            err = internalComponentErr;
            goto bail;
        }
    }
    else
    {
        if(sessionOptions) {
            
            err = ICMCompressionSessionOptionsGetProperty(sessionOptions,
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
    }
    
    if (quality <= codecLowQuality)
    {
        glob->dxtEncoder = HapCodecGLEncoderCreate(glob->width, glob->height, alpha ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1);
        glob->dxtFormat = alpha ? HapTextureFormat_RGBA_DXT5 : HapTextureFormat_RGB_DXT1;
    }
    else if (quality < codecHighQuality || alpha)
    {
        glob->dxtEncoder = HapCodecSquishEncoderCreate(HapCodecSquishEncoderMediumQuality, alpha ? kHapCVPixelFormat_RGBA_DXT5 : kHapCVPixelFormat_RGB_DXT1);
        glob->dxtFormat = alpha ? HapTextureFormat_RGBA_DXT5 : HapTextureFormat_RGB_DXT1;
    }
    else
    {
        glob->dxtEncoder = HapCodecYCoCgDXTEncoderCreate();
        glob->dxtFormat = HapTextureFormat_YCoCg_DXT5;
    }
    if (glob->dxtEncoder == NULL)
    {
        err = internalComponentErr;
        goto bail;
    }
    
    // Create a pixel buffer attributes dictionary.
	err = createPixelBufferAttributesDictionary( glob->width, glob->height,
                                                pixelFormatList, sizeof(pixelFormatList) / sizeof(OSType),
                                                glob->dxtEncoder->pad_source_buffers,
                                                &compressorPixelBufferAttributes );
	if( err )
		goto bail;
    
	*compressorPixelBufferAttributesOut = compressorPixelBufferAttributes;
	compressorPixelBufferAttributes = NULL;
    
    // Currently we store the Hap frame type in the image description as discovering it otherwise
    // would require reading a frame
    
    Handle stored = NewHandleClear(sizeof(UInt32));
    if (stored == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    **(UInt32 **)stored = OSSwapHostToBigInt32(glob->dxtFormat);
    err = AddImageDescriptionExtension(imageDescription, stored, kHapCodecSampleDescriptionExtension);
    DisposeHandle(stored);
    if (err)
    {
        goto bail;
    }
    
    // If we're allowed to, we output frames on a background thread
    err = ICMCompressionSessionOptionsGetProperty(sessionOptions,
                                                  kQTPropertyClass_ICMCompressionSessionOptions,
                                                  kICMCompressionSessionOptionsPropertyID_AllowAsyncCompletion,
                                                  sizeof( Boolean ),
                                                  &glob->allowAsyncCompletion,
                                                  NULL );
    
	// Work out the upper bound on encoded frame data size -- we'll allocate buffers of this size.
    long wantedDXTSize = roundUpToMultipleOf4(glob->width) * roundUpToMultipleOf4(glob->height);
    if (glob->dxtFormat == HapTextureFormat_RGB_DXT1) wantedDXTSize /= 2;
    
    if (HapCodecBufferPoolGetBufferSize(glob->dxtBufferPool) != wantedDXTSize)
    {
        HapCodecBufferPoolDestroy(glob->dxtBufferPool);
        glob->dxtBufferPool = HapCodecBufferPoolCreate(wantedDXTSize);
    }
    if (glob->dxtBufferPool == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    glob->maxEncodedDataSize = HapMaxEncodedLength(wantedDXTSize);
    
    // TODO: right now all the pixel formats we accept have the same number of bits per pixel
    // but we will accept DXT etc buffers in, so this will have to be more flexible
    size_t wantedConvertBufferBytesPerRow = roundUpToMultipleOf16(glob->width * 4);
    size_t wantedBufferSize = wantedConvertBufferBytesPerRow * glob->height;
    if (HapCodecBufferPoolGetBufferSize(glob->formatConvertPool) != wantedBufferSize)
    {
        HapCodecBufferPoolDestroy(glob->formatConvertPool);
        glob->formatConvertPool = HapCodecBufferPoolCreate(wantedBufferSize);
        glob->formatConvertBufferBytesPerRow = wantedConvertBufferBytesPerRow;
    }
    
    if (dictionaryHasValueForKeyOfTypeID(glob->settings, kSettingsSecondaryCompressorKey, CFStringGetTypeID()))
    {
        CFStringRef compressorFromSettings = CFDictionaryGetValue(glob->settings, kSettingsSecondaryCompressorKey);
        if (CFEqual(compressorFromSettings, kSettingsSecondaryCompressorZLIB))
        {
            glob->hapCompressor = HapCompressorZLIB;
        }
        else if (CFEqual(compressorFromSettings, kSettingsSecondaryCompressorLZF))
        {
            glob->hapCompressor = HapCompressorLZF;
        }
        else
        {
            glob->hapCompressor = HapCompressorSnappy;
        }
    }
    else
    {
        glob->hapCompressor = HapCompressorSnappy;
    }
    
#ifdef DEBUG
    char *compressor_str;
    switch (glob->hapCompressor) {
        case HapCompressorSnappy:
            compressor_str = "snappy";
            break;
        case HapCompressorLZF:
            compressor_str = "LZF";
            break;
        case HapCompressorZLIB:
            compressor_str = "ZLIB";
            break;
        default:
            err = internalComponentErr;
            goto bail;
    }
    printf("HAP CODEC: start / %s / ", compressor_str);
    if (glob->dxtEncoder->describe_function)
    {
        printf("%s", glob->dxtEncoder->describe_function(glob->dxtEncoder));
    }
    printf("\n");
#endif // DEBUG
    
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
    
    // TODO: once we support DXT in, be prepared to skip the DXT encoding stage entirely
    
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
    }
    
    // Encode the DXT frame
    const void *encode_src;
    unsigned int encode_src_bytes_per_row;
    
    dxtBuffer = HapCodecBufferCreate(glob->dxtBufferPool);
    if (dxtBuffer == NULL)
    {
        err = memFullErr;
        goto bail;
    }
    
    if (formatConvertBuffer)
    {
        encode_src = HapCodecBufferGetBaseAddress(formatConvertBuffer);
        encode_src_bytes_per_row = glob->formatConvertBufferBytesPerRow;
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
    
    const void *codec_src = HapCodecBufferGetBaseAddress(dxtBuffer);
    unsigned int codec_src_length = HapCodecBufferGetSize(dxtBuffer);
    
    unsigned long actualEncodedDataSize = 0;
    unsigned int hapResult = HapEncode(codec_src,
                                       codec_src_length,
                                       glob->dxtFormat,
                                       glob->hapCompressor,
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
    
    // Output the encoded frame - or pass the error on
    task->error = err;
    if (glob->allowAsyncCompletion)
    {
        err = finishFrame((HapCodecBufferRef)info, true);
        if (err != noErr)
        {
            emitBackgroundError(glob, err);
        }
    }
    else
    {
        queueEncodedFrame(glob, (HapCodecBufferRef)info);
    }
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
    
    HapCodecTasksAddTask(Background_Encode, glob->taskGroup, buffer);
    
    if (glob->allowAsyncCompletion == false)
    {
        do
        {
            buffer = dequeueFrameNumber(glob, glob->lastFrameOut + 1); // TODO: this function could just look up the next frame number from glob, ah well
            err = finishFrame(buffer, false);
            if (err != noErr)
            {
                goto bail;
            }
        } while (buffer != NULL);
    }
    else
    {
        err = backgroundError(glob);
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
Hap_CCompleteFrame( 
  HapCompressorGlobals glob,
  ICMCompressorSourceFrameRef sourceFrame,
  UInt32 flags )
{
#pragma unused (sourceFrame, flags)
    ComponentResult err = noErr;
    if (glob->taskGroup)
    {
        HapCodecTasksWaitForGroupToComplete(glob->taskGroup); // TODO: this waits for all pending frames rather than the particular frame
    }
    HapCodecBufferRef buffer;
    
    if (glob->allowAsyncCompletion == false)
    {
        do
        {
            buffer = dequeueFrameNumber(glob, glob->lastFrameOut + 1); // TODO: this function could just look up the next frame number from glob, ah well
            
            err = finishFrame(buffer, false);
            if (err != noErr)
            {
                goto bail;
            }

        } while (buffer != NULL);
    }
    else
    {
        err = backgroundError(glob);
    }
bail:
	return err;
}

static ComponentResult finishFrame(HapCodecBufferRef buffer, bool onBackgroundThread)
{
    ComponentResult err = noErr;
    if (buffer != NULL)
    {
        HapCodecCompressTask *task = HapCodecBufferGetBaseAddress(buffer);
        
        err = task->error;
        
        if (onBackgroundThread)
        {
            OSSpinLockLock(&task->glob->lock);
        }
        task->glob->lastFrameOut = ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame);
        if (onBackgroundThread)
        {
            OSSpinLockUnlock(&task->glob->lock);
        }
        if (task->error == noErr)
        {
            ICMCompressorSessionEmitEncodedFrame(task->glob->session, task->encodedFrame, 1, &task->sourceFrame);
        }
        else
        {
            ICMCompressorSessionDropFrame(task->glob->session, task->sourceFrame);
        }
        ICMCompressorSourceFrameRelease(task->sourceFrame);
        ICMEncodedFrameRelease(task->encodedFrame);
        HapCodecBufferReturn(buffer);
    }
    return err;
}

static void queueEncodedFrame(HapCompressorGlobals glob, HapCodecBufferRef frame)
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
            HapCodecBufferRef *newFinishedFrames = malloc(sizeof(HapCodecBufferRef) * glob->finishedFramesCapacity);
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

static HapCodecBufferRef dequeueFrameNumber(HapCompressorGlobals glob, int number)
{
    HapCodecBufferRef found = NULL;
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
                    HapCodecCompressTask *task = (HapCodecCompressTask *)HapCodecBufferGetBaseAddress(glob->finishedFrames[i]);
                    if (ICMCompressorSourceFrameGetDisplayNumber(task->sourceFrame) == number)
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

static void emitBackgroundError(HapCompressorGlobals glob, ComponentResult error)
{
    // We ignore failure here - if we failed then there is already an error pending delivery
    OSAtomicCompareAndSwap32(noErr, error, &glob->backgroundError);
}

static ComponentResult backgroundError(HapCompressorGlobals glob)
{
    ComponentResult err;
    do {
        err = glob->backgroundError;
    } while (OSAtomicCompareAndSwap32(err, noErr, &glob->backgroundError) == false);
    return err;
}

/*
 Compressor Settings
 
 http://developer.apple.com/library/mac/#technotes/tn2081/_index.html
 
 */

ComponentResult Hap_CGetSettings(HapCompressorGlobals glob, Handle settings)
{
    ComponentResult err = noErr;
    
    if (!settings)
    {
        err = paramErr;
    }
    else
    {
        if (glob->settings)
        {
            CFDataRef data = CFPropertyListCreateData(kCFAllocatorDefault,
                                                      glob->settings,
                                                      kCFPropertyListXMLFormat_v1_0,
                                                      0,
                                                      NULL);
            if (data)
            {
                CFIndex length = CFDataGetLength(data);
                SetHandleSize(settings, length);
                CFDataGetBytes(data, CFRangeMake(0, length), (UInt8 *)*settings);
                CFRelease(data);
            }
            else
            {
                err = internalComponentErr;
            }
        }
        else
        {
            SetHandleSize(settings, 0);
        }
    }
    
    debug_print_err(glob, err);
    return err;
}

ComponentResult Hap_CSetSettings(HapCompressorGlobals glob, Handle settings)
{
    ComponentResult err = noErr;
    
    if (glob->settings) CFRelease(glob->settings);
    glob->settings = NULL;
    
    Size settingsSize;
    
    if (settings) settingsSize = GetHandleSize(settings);
    else settingsSize = 0;
    
    if (settingsSize == 5 && ((UInt32 *) *settings)[0] == 'VPUV')
    {
        // this was our old style of settings, never in a distributed build, we ignore them
    }
    else if (settingsSize != 0)
    {
        CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *)*settings, GetHandleSize(settings), kCFAllocatorNull);
        CFPropertyListRef propertyList = CFPropertyListCreateWithData(kCFAllocatorDefault,
                                                                      data,
                                                                      kCFPropertyListMutableContainers,
                                                                      NULL,
                                                                      NULL);
        
        if (data) CFRelease(data);
        
        if (propertyList)
        {
            if (CFGetTypeID(propertyList) == CFDictionaryGetTypeID())
            {
                glob->settings  = (CFMutableDictionaryRef)propertyList;
            }
            else
            {
                CFRelease(propertyList);
            }
        }
        
        if (glob->settings == NULL) err = internalComponentErr;
    }
    
    debug_print_err(glob, err);
    return err;
}

#define kMyCodecDITLResID 129

ComponentResult Hap_CGetDITLForSize(HapCompressorGlobals glob,
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

#define kItemSlider 2
#define kItemCheckbox 3
#define kItemPopup 4
#define kItemText 5

ComponentResult Hap_CDITLInstall(HapCompressorGlobals glob,
                                        DialogRef d,
                                        short itemOffset)
{
#pragma unused(glob)
    ControlRef cRef;
    
    CFIndex compressorPopupIndex = 1;
    unsigned long alphaCheckboxValue = 0;
    unsigned long qualitySliderValue = 0;
    
    if (glob->settings)
    {
        CFStringRef compressor = CFDictionaryGetValue(glob->settings, kSettingsSecondaryCompressorKey);
        
        if (compressor && CFEqual(compressor, kSettingsSecondaryCompressorLZF)) compressorPopupIndex = 2;
        else if (compressor && CFEqual(compressor, kSettingsSecondaryCompressorZLIB)) compressorPopupIndex = 3;
        
        CFBooleanRef preserveAlpha = CFDictionaryGetValue(glob->settings, kSettingsPreserveAlphaKey);
        if (preserveAlpha && CFEqual(preserveAlpha, kCFBooleanTrue))
        {
            alphaCheckboxValue = 1;
        }
        
        if (dictionaryHasValueForKeyOfTypeID(glob->settings, kSettingsQualityKey, CFNumberGetTypeID()))
        {
            CFNumberRef quality = CFDictionaryGetValue(glob->settings, kSettingsQualityKey);
            SInt32 value = 0;
            if (CFNumberGetValue(quality, kCFNumberSInt32Type, &value))
            {
                if (value > codecNormalQuality)
                {
                    qualitySliderValue = 2;
                }
                else if (value > codecLowQuality)
                {
                    qualitySliderValue = 1;
                }
            }
        }
    }
    
    GetDialogItemAsControl(d, kItemPopup + itemOffset, &cRef);
    SetControl32BitValue(cRef, compressorPopupIndex);
    
    GetDialogItemAsControl(d, kItemCheckbox + itemOffset, &cRef);
    SetControl32BitValue(cRef, alphaCheckboxValue);
    
    GetDialogItemAsControl(d, kItemSlider + itemOffset, &cRef);
    SetControl32BitValue(cRef, qualitySliderValue);
    
    GetDialogItemAsControl(d, kItemText + itemOffset, &cRef);
    
    CFStringRef sizeString;
    
    if (alphaCheckboxValue == 1 || qualitySliderValue == 2)
        sizeString = CFSTR("File Size: Large");
    else
        sizeString = CFSTR("File Size: Normal");
    
    HIViewSetText(cRef, sizeString);
    HIViewSetNeedsDisplay(cRef, true);
    HIViewRender(cRef);
    
    return noErr;
}

ComponentResult Hap_CDITLEvent(HapCompressorGlobals glob,
                                      DialogRef d,
                                      short itemOffset,
                                      const EventRecord *theEvent,
                                      short *itemHit,
                                      Boolean *handled)
{
#pragma unused(d, theEvent)
    if (glob->settings == NULL && theEvent->what == nullEvent)
    {
        /*
         We use QuickTime's mechanism to determine alpha and quality rather than our own
         when no settings have been set. This allows our compressor to work with apps which
         use their own (or no) UI.
         
         To make sure we always use our own settings once the settings dialog has been shown, we
         need to force QuickTime to get and apply our settings, so we mark the first nullEvent in the
         dialog as having been handled by us, so QuickTime thinks our settings have changed.
         */
        *itemHit = itemOffset + kItemText;
        *handled = true;
    }
    else
    {
        *handled = false;
    }
    return noErr;
}

ComponentResult Hap_CDITLItem(HapCompressorGlobals glob,
                                     DialogRef d,
                                     short itemOffset,
                                     short itemNum)
{
#pragma unused(glob)
    ControlRef cRef;
    
    switch (itemNum - itemOffset) {
        case kItemCheckbox:
            GetDialogItemAsControl(d, itemOffset + kItemCheckbox, &cRef);
            SetControl32BitValue(cRef, !GetControl32BitValue(cRef));
            break;
    }
    return noErr;
}

ComponentResult Hap_CDITLRemove(HapCompressorGlobals glob,
                                       DialogRef d,
                                       short itemOffset)
{
    ControlRef cRef;
    unsigned long popupValue;
    unsigned long alphaCheckboxValue;
    unsigned long qualitySliderValue;
    
    GetDialogItemAsControl(d, kItemPopup + itemOffset, &cRef);
    popupValue = GetControl32BitValue(cRef);
    
    GetDialogItemAsControl(d, kItemCheckbox + itemOffset, &cRef);
    alphaCheckboxValue = GetControl32BitValue(cRef);
    
    GetDialogItemAsControl(d, kItemSlider + itemOffset, &cRef);
    qualitySliderValue = GetControl32BitValue(cRef);
    
    if (glob->settings == NULL)
    {
        glob->settings = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    if (glob->settings)
    {
        CFStringRef compressor;
        
        switch (popupValue) {
            case 2:
                compressor = kSettingsSecondaryCompressorLZF;
                break;
            case 3:
                compressor = kSettingsSecondaryCompressorZLIB;
                break;
            default:
                compressor = kSettingsSecondaryCompressorSnappy;
        }
        
        CFDictionarySetValue(glob->settings, kSettingsSecondaryCompressorKey, compressor);
        
        CFBooleanRef alpha = alphaCheckboxValue ? kCFBooleanTrue : kCFBooleanFalse;
        
        CFDictionarySetValue(glob->settings, kSettingsPreserveAlphaKey, alpha);
        
        SInt32 value;
        switch (qualitySliderValue) {
            case 2:
                value = codecHighQuality;
                break;
            case 1:
                value = codecNormalQuality;
                break;
            default:
                value = codecLowQuality;
        }
        
        CFNumberRef quality = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
        if (quality)
        {
            CFDictionarySetValue(glob->settings, kSettingsQualityKey, quality);
            CFRelease(quality);
        }
        else
        {
            CFDictionaryRemoveValue(glob->settings, kSettingsQualityKey);
        }
    }
    
    return noErr;
}

ComponentResult Hap_CDITLValidateInput(HapCompressorGlobals storage,
                                              Boolean *ok)
{
#pragma unused(storage)
    if (ok)
        *ok = true;
    return noErr;
}
