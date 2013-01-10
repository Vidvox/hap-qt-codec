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

#if COMP_BUILD

// Settings controls for dialog

#define kHapCodecPopupCNTLResID 129
#define kHapCodecPopupMENUResID 129

#define POPUP_H 22

resource 'DITL' (kHapCodecDITLResID) {
    {
        {0, 0, POPUP_H, 185}, Control { enabled, kHapCodecPopupCNTLResID },
    };
};

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

#endif
