//
//  HapDecompressorDispatch.h
//  Hap Codec
//

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
