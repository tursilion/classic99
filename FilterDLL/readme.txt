sms_ntsc 0.2.2: Sega Master System NTSC Video Filter
----------------------------------------------------
Sms_ntsc emulates Sega Master System NTSC video output, allowing an authentic
image in an emulator. It uses a highly optimized algorithm to perform the same
signal processing as an NTSC decoder in a TV, giving very similar pixel
artifacts. The usual TV picture controls can be adjusted: hue (tint),
saturation, contrast, brightness, and sharpness. Additionally, the amount of
NTSC chroma and luma artifacts can be adjusted, allowing an image that
corresponds to composite video (lots of artifacts), S-video (color bleeding
only), RGB (clean pixels), or anywhere inbetween.

The output is scaled to the proper horizontal width, leaving it up the emulator
to simply double the height. Custom blitters can be written using a special
interface, allowing output in other pixel formats and efficient scanline
doubling as a part of blitting.

Blitting a 248x192 source image to a 581x384 pixel 16-bit RGB memory buffer at
60 frames per second uses 7% CPU on a 2.0 GHz Athlon 3500+ and 33% CPU on a
10-year-old 400 MHz G3 PowerMac.

Feedback about the interface and usability would be helpful.

Author  : Shay Green <gblargg@gmail.com>
Website : http://www.slack.net/~ant/
Forum   : http://groups.google.com/group/blargg-sound-libs
License : GNU Lesser General Public License (LGPL)
Language: C or C++


Getting Started
---------------
Build a program consisting of demo.c, sms_ntsc.c, and the SDL multimedia
library (see http://libsdl.org/). Running the program with "sms.raw" in the
same directory should show an image as it would look on a TV, with mouse
control of two picture parameters.

A simple benchmark program is included to allow measuring the frame rate and
how much CPU time the blitter would use running at 60 frames per second. This
is useful for getting an idea of whether this library will be usable in your
emulator, and for improving the performance of a custom blitter (see
notes.txt).

If you're using C++, avoid linking issues by renaming sms_ntsc.c to
sms_ntsc.cpp (this causes compilation in C++ rather than C).

See notes.txt for more information, and respective header (.h) files for
reference. Post to the discussion forum for assistance.


Files
-----
readme.txt              Essential information
sms_ntsc.txt            Library documentation
changes.txt             Changes since previous releases
LGPL.txt                GNU Lesser General Public License

demo.c                  Loads raw image and displays on screen using SDL
sms.raw                 Raw image for demo
benchmark.c             Measures frame rate and CPU usage of blitter

sms_ntsc.h              Library header and source
sms_ntsc.c

-- 
Shay Green <gblargg@gmail.com>
