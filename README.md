Hap QuickTime Codec
==========

Hap is a video codec for fast decompression on modern graphics hardware. This is the home of the Hap QuickTime codec. For general information about Hap, see [the Hap project][1].

The Hap QuickTime codec supports encoding and decoding Hap video. Video can be decoded to RGB(A) formats for playback in any existing application, or if an application explicitly requests it, to compressed texture formats for accelerated playback on graphics hardware.

Note that there is no advantage to using Hap over other codecs if the application you will be using for playback does not support accelerated Hap playback.
 
For information and example code to support accelerated playback via this codec in your application, see the [Hap sample code][2].

Download
====

An installer for the Hap QuickTime component can be downloaded from the [Releases](https://github.com/Vidvox/hap-qt-codec/releases/latest) page.

Requires at minimum MacOS 10.6 Snow Leapord, or Windows Vista with QuickTime 7 installed.

Using the Hap QuickTime Codec
====

After installation, the codec will show up when exporting video in applications which support QuickTime. Four variants of the codec exist:

* **Hap** - Reasonable image quality
* **Hap Alpha** - Reasonable image quality with an alpha channel
* **Hap Q** - Good image quality at a higher data-rate
* **Hap Q Alpha** - Good image quality with an alpha channel at a higher data-rate

Hap and Hap Alpha have a quality setting. Although QuickTime displays a slider, it has only two effective settings: below "High" a fast low-quality encoder is used, and at "High" or above a slower high-quality encoder is used.

Note that the current QuickTime Player on macOS does not support non-Apple codecs. Use [QuickTime Player 7](https://support.apple.com/en-us/HT201288) or a third-party player.

Other software for encoding and decoding Hap
====

* [ffmpeg](http://ffmpeg.org)
* [AVF Batch Converter](https://github.com/Vidvox/hap-in-avfoundation/releases)
* [Hap for DirectShow](http://renderheads.com/product/hap-for-directshow/)
* [TouchDesigner](https://www.derivative.ca)


Open-Source
====

The Hap codec project is open-source, licensed under a [FreeBSD License][3], meaning you can use it in your commercial or non-commercial applications free of charge.

This project was originally written by [Tom Butterworth][4] and commissioned by [VIDVOX][5], 2012.

[1]: http://github.com/vidvox/hap
[2]: http://github.com/vidvox/hap-quicktime-playback-demo
[3]: http://github.com/vidvox/hap-qt-codec/blob/master/LICENSE
[4]: http://kriss.cx/tom
[5]: http://www.vidvox.net
