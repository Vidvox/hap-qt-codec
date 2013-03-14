/*
 HapCompressorDispatch.h
 Hap Codec
 
 Copyright (c) 2012-2013, Tom Butterworth and Vidvox LLC. All rights reserved.
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

	ComponentSelectorOffset (8)

	ComponentRangeCount (1)
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
		ComponentCall (GetCodecInfo)							// 0
		ComponentError (GetCompressionTime)
		ComponentCall (GetMaxCompressionSize)
		ComponentError (PreCompress)
		ComponentError (BandCompress)
		ComponentError (PreDecompress)							// 5
		ComponentError (BandDecompress)
		ComponentError (Busy)
		ComponentError (GetCompressedImageSize)
		ComponentError (GetSimilarity)
		ComponentError (TrimImage)								// 10
		ComponentError (RequestSettings)
		ComponentError (GetSettings)
		ComponentError (SetSettings)
		ComponentError (Flush)
		ComponentError (SetTimeCode)							// 15
		ComponentError (IsImageDescriptionEquivalent)
		ComponentError (NewMemory)
		ComponentError (DisposeMemory)
		ComponentError (HitTestData)
		ComponentError (NewImageBufferMemory)					// 20
		ComponentError (ExtractAndCombineFields)
		ComponentError (GetMaxCompressionSizeWithSources)
		ComponentError (SetTimeBase)
		ComponentError (SourceChanged)
		ComponentError (FlushLastFrame)							// 25
		ComponentError (GetSettingsAsText)
		ComponentError (GetParameterListHandle)
		ComponentError (GetParameterList)
		ComponentError (CreateStandardParameterDialog)
		ComponentError (IsStandardParameterDialogEvent)			// 30
		ComponentError (DismissStandardParameterDialog)
		ComponentError (StandardParameterDialogDoAction)
		ComponentError (NewImageGWorld)
		ComponentError (DisposeImageGWorld)
		ComponentError (HitTestDataWithFlags)					// 35
		ComponentError (ValidateParameters)
		ComponentError (GetBaseMPWorkFunction)
		ComponentError (LockBits)
		ComponentError (UnlockBits)
		ComponentError (RequestGammaLevel)						// 40
		ComponentError (GetSourceDataGammaLevel)
		ComponentError (42)
		ComponentError (GetDecompressLatency)
		ComponentError (MergeFloatingImageOntoWindow)
		ComponentError (RemoveFloatingImage)					// 45
		ComponentError (GetDITLForSize)
		ComponentError (DITLInstall)
		ComponentError (DITLEvent)
		ComponentError (DITLItem)
		ComponentError (DITLRemove)								// 50
		ComponentError (DITLValidateInput)
		ComponentError (52)
		ComponentError (53)
		ComponentError (GetPreferredChunkSizeAndAlignment)
		ComponentCall (PrepareToCompressFrames)					// 55
		ComponentCall (EncodeFrame)
		ComponentCall (CompleteFrame)
    	ComponentError (BeginPass)
    	ComponentError (EndPass)
		ComponentError (ProcessBetweenPasses)					// 60
	ComponentRangeEnd (1)
