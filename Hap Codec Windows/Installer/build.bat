MSBuild "..\Hap Codec.sln" /p:Configuration=RELEASE
"C:\Program Files (x86)\WiX Toolset v3.9\bin\candle.exe" HapQuickTimeSetup.wxs
"C:\Program Files (x86)\WiX Toolset v3.9\bin\light.exe" -ext WixUIExtension HapQuickTimeSetup.wixobj