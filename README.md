# WebRadio

## Prerequisites

### All

Make sure that you have dev packages for these dependencies before you build from source:

* libsoup-2.4
* gobject-2.0
* glib-2.0
* gio-2.0
* gio-unix-2.0

### Debian

Make sure you have these packages installed at runtime:

* gstreamer1.0-alsa
* gstreamer1.0-tools
* gst-plugins-bad1.0

### Fedora

Make sure you have these packages installed at runtime:

* gstreamer1
* gstreamer1-plugins-bad-freeworld

## Building from releases (.tar.gz distributions)

1. `./configure`
2. `make`
3. `./play.sh [optional-icy-radio-url]`

If you don't specify any URL, you'll hear [.977 - The 80's Channel](http://www.977music.com/).

## Building new release

1. `autoreconf --install`
2. `./configure`
3. `make dist`

The release will be named `webradio-[version].tar.gz`.

