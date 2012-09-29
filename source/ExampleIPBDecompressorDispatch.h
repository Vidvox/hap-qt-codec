/*

File: ExampleIPBDecompressorDispatch.h, part of ExampleIPBCodec

Abstract: Dispatch header file for ExampleIPBDecompressor.c.

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

	ComponentSelectorOffset (8)

	ComponentRangeCount (3)
	ComponentRangeShift (8)
	ComponentRangeMask	(FF)

	ComponentRangeBegin (0)
		ComponentError	 (GetMPWorkFunction)
		ComponentError	 (Unregister)
		StdComponentCall (Target)
		ComponentError   (Register)
		StdComponentCall (Version)
		StdComponentCall (CanDo)
		StdComponentCall (Close)
		StdComponentCall (Open)
	ComponentRangeEnd (0)
		
	ComponentRangeBegin (1)
		ComponentCall (GetCodecInfo)								// 0
		ComponentDelegate (GetCompressionTime)
		ComponentDelegate (GetMaxCompressionSize)
		ComponentDelegate (PreCompress)
		ComponentDelegate (BandCompress)
		ComponentDelegate (PreDecompress)							// 5
		ComponentDelegate (BandDecompress)
		ComponentDelegate (Busy)
		ComponentCall (GetCompressedImageSize)
		ComponentDelegate (GetSimilarity)
		ComponentDelegate (TrimImage)								// 10
		ComponentDelegate (RequestSettings)
		ComponentDelegate (GetSettings)
		ComponentDelegate (SetSettings)
		ComponentDelegate (Flush)
		ComponentDelegate (SetTimeCode)								// 15
		ComponentDelegate (IsImageDescriptionEquivalent)
		ComponentDelegate (NewMemory)
		ComponentDelegate (DisposeMemory)
		ComponentDelegate (HitTestData)
		ComponentDelegate (NewImageBufferMemory)					// 20
		ComponentDelegate (ExtractAndCombineFields)
		ComponentDelegate (GetMaxCompressionSizeWithSources)
		ComponentDelegate (SetTimeBase)
		ComponentDelegate (SourceChanged)
		ComponentDelegate (FlushLastFrame)							// 25
		ComponentDelegate (GetSettingsAsText)
		ComponentDelegate (GetParameterListHandle)
		ComponentDelegate (GetParameterList)
		ComponentDelegate (CreateStandardParameterDialog)
		ComponentDelegate (IsStandardParameterDialogEvent)			// 30
		ComponentDelegate (DismissStandardParameterDialog)
		ComponentDelegate (StandardParameterDialogDoAction)
		ComponentDelegate (NewImageGWorld)
		ComponentDelegate (DisposeImageGWorld)
		ComponentDelegate (HitTestDataWithFlags)					// 35
		ComponentDelegate (ValidateParameters)
		ComponentDelegate (GetBaseMPWorkFunction)
		ComponentDelegate (LockBits)
		ComponentDelegate (UnlockBits)
		ComponentDelegate (RequestGammaLevel)						// 40
		ComponentDelegate (GetSourceDataGammaLevel)
		ComponentDelegate (42)
		ComponentDelegate (GetDecompressLatency)
		ComponentDelegate (MergeFloatingImageOntoWindow)
		ComponentDelegate (RemoveFloatingImage)						// 45
		ComponentDelegate (GetDITLForSize)
		ComponentDelegate (DITLInstall)
		ComponentDelegate (DITLEvent)
		ComponentDelegate (DITLItem)
		ComponentDelegate (DITLRemove)								// 50
		ComponentDelegate (DITLValidateInput)
		ComponentDelegate (52)
		ComponentDelegate (53)
		ComponentDelegate (GetPreferredChunkSizeAndAlignment)
		ComponentDelegate (PrepareToCompressFrames)					// 55
		ComponentDelegate (EncodeFrame)
		ComponentDelegate (CompleteFrame)
    	ComponentDelegate (BeginPass)
    	ComponentDelegate (EndPass)
		ComponentDelegate (ProcessBetweenPasses)					// 60
	ComponentRangeEnd (1)

	ComponentRangeUnused (2)

	ComponentRangeBegin (3)
		ComponentCall (Preflight)
		ComponentCall (Initialize)
		ComponentCall (BeginBand)
		ComponentCall (DrawBand)
		ComponentCall (EndBand)
		ComponentCall (QueueStarting)
		ComponentCall (QueueStopping)
		ComponentDelegate (DroppingFrame)
		ComponentDelegate (ScheduleFrame)
		ComponentDelegate (CancelTrigger)
		ComponentDelegate (10)
		ComponentDelegate (11)
		ComponentDelegate (12)
		ComponentDelegate (13)
		ComponentDelegate (14)
		ComponentCall (DecodeBand)
	ComponentRangeEnd (3)
