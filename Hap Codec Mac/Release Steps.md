
##Making an Installer

Check version number in

    Hap Codec Windows/Installer/HapQuickTimeSetup.wxs
    source/HapCodecVersion.h b/source/HapCodecVersion.h
    source/Info.plist
    Hap Codec Mac/Distribution.xml

Product > Archive in Xcode (do not use the product of a debug build, performance will be terrible).

Export the archive and cd into archive directory

Delete usr directory from archive root (Products directory)

    pkgbuild --root Products --version 8 --identifier com.Vidvox.pkg.Hap Hap.pkg

copy LICENSE from the repo to Resources/License.txt

    productbuild --resources Resources/ --distribution ~/Development/Hap/hap-qt-codec/Hap\ Codec\ Mac/Distribution.xml Hap\ Codec\ Installer-unsigned.pkg

    productsign --sign "Developer ID Installer" Hap\ Codec\ Installer-unsigned.pkg Hap\ Codec\ Installer.pkg
