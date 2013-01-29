//
//  HaResCommon.r
//  Hap Codec
//
//  Resources common to all components
//

#include "HapResCommon.h"

// Code Entry Points for Mach-O and Windows

#if COMP_BUILD
resource 'dlle' (kCodecCompressorEntryPointResID) {
    "Hap_CComponentDispatch"
};
#endif

#if DECO_BUILD
resource 'dlle' (kCodecDecompressorEntryPointResID) {
    "Hap_DComponentDispatch"
};
#endif
