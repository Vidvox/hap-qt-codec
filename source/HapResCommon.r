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

// UI Parts

#if COMP_BUILD
resource 'CNTL' (kHapCodecPopupCNTLResID, "Compressor Popup") {
     {0, 0, 20, 185},
     0,
     visible,
     84,        /* title width */
     kHapCodecPopupMENUResID,
     popupMenuCDEFProc+popupFixedWidth,
     0,
     "Compressor:"
};
 
resource 'MENU' (kHapCodecPopupMENUResID, "Compressor Popup") {
    kHapCodecPopupMENUResID,
    textMenuProc,
    allEnabled,       /* Enable flags */
    enabled,
    "Compressor",
    { /* array: 3 elements */
        /* [1] */
        "Snappy", noIcon, noKey, noMark, plain,
        /* [2] */
        "LZF", noIcon, noKey, noMark, plain,
        /* [3] */
        "ZLIB", noIcon, noKey, noMark, plain
    }
};

resource 'CNTL' (kHapCodecSliderCNTLResID, "Quality Slider"){
    {0,0,26,185},
    3/*value:initially, # of ticks*/,
    visible,
    /*max*/0x2,
    /*min*/0,
    kControlSliderProc|kControlSliderHasTickMarks,
    0,
    ""
};

/*
resource 'CNTL' (kMyCodecBoxCNTLResID, "Quality Box"){
    {0,0,100,185},
    0,
    visible,
    0,
    0,
    kControlGroupBoxTextTitleProc,
    0,
    "Quality"
};
*/

#endif
