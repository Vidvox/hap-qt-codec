/*

 HapCompressorDispatch.h
 Hap Compressor

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
