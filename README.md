Hap QuickTime Codec
==========

Hap is a video codec for fast decompression on modern graphics hardware. This is the home of the Hap QuickTime codec. For general information about Hap, see [the Hap project][1].

The Hap QuickTime codec supports encoding and decoding Hap video. Video can be decoded to RGB(A) formats for playback in any existing application, or if an application explicitly requests it, to S3TC/DXT frames for accelerated playback on graphics hardware.

Note that there is no advantage to using Hap over other codecs if the application you will be using for playback does not support accelerated Hap playback.
 
For information and example code to support accelerated playback via this codec in your application, see the [Hap sample code][2].

Download
====

An installer for MacOS X can be downloaded here XXXXXXX

Using the Hap QuickTime Codec
====

After installation, the codec will show up when exporting video in applications which support QuickTime. Three variants of the codec exist:

* **Hap** - Reasonable image quality
* **Hap Alpha** - Reasonable image quality with an alpha channel
* **Hap Q** - Good image quality at a higher data-rate

Hap and Hap Alpha have a quality setting. At any setting below "High", fast hardware-based compression will be performed. Above "High", slower but higher-quality software compression will be performed.

Open-Source
====

The Hap codec project is open-source, licensed under a [New BSD License][3], meaning you can use it in your commercial or non-commercial applications free of charge.

This project was originally written by [Tom Butterworth][4] and commissioned by [VIDVOX][5], 2012.

[1]: http://github.com/vidvox/hap
[2]: http://github.com/vidvox/hap-quicktime-playback-demo
[3]: http://github.com/vidvox/hap-qt-codec/blob/master/LICENSE
[4]: http://kriss.cx/tom
[5]: http://www.vidvox.net
