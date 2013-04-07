fraunhofer - Far-Field Lens Diffraction
=======================================

<p align="center">
<img
src="https://raw.github.com/TomCrypto/fraunhofer/master/renders/screenshot.png"
alt="Fraunhofer Diffraction Pattern"/>
</p>

This is a little program which generates Fraunhofer (far-field) diffraction
patterns from aperture transmission functions, with OpenCL acceleration.

Requirements
------------

- A C++ compiler (C++98 should do, C++11 will of course work)
- An OpenCL 1.1 device and implementation

Compatibility
-------------

Compatible with GCC 4.7.2, MinGW 4.7.2 (Linux x64 and Windows x64).

Usage
-----

The program takes three command-line arguments:
- A path to a PPM file encoding the aperture transmission function
- A path to a location to write the resulting pattern (HDRI)
- A number of passes (more is better, but slower)

The aperture should have power of two dimensions, as the FFT implementation
only supports these dimensions. The width and height can be different (todo
: actually they cannot at the moment...). Some demonstration apertures are
provided in the `apertures` folder.

There are also a few additional options, set outside the command line - the
`config.xml` file contains a few program settings:

- OpenCL Platform: set to the (zero-based) index of the desired platform.
- OpenCL Device: set to the (zero-based) index of the desired device.
- FFT Threshold: this is used to apply a black and white threshold to the
                 aperture transmission function. If it is set to 1, no
                 threshold is applied and the transmission function
                 takes values between 0 and 1. If it is less than 0,
                 points on the aperture transmission function less
                 than the threshold become 0, and become 1 above.
- FFT Lens Distance: this is the distance between the aperture and the
                     observation plane in which the diffraction pattern
                     is to be observed. Generally, values between 1mm
                     (0.001) and 1cm (0.01) are best.

Finally, there are some parameters in the `cl/def.cl` file, as follows:
- RINGING: controls the blade ringing, this is an aesthetic parameter,
           between 0 and infinity. small values make the aperture
           diffraction blades very thin, large values make them
           larger and more spread out.
- ROTATE: controls how much spread there is in the diffraction pattern
          rotation (in real life, the aperture and observation plane i.e.
          sensor do not always stay immobile relative to each other, so
          we account for this by introducing random differences in
          rotation). This value is in degrees, less than 15 is best.
- BLUR: controls the amount of blurring taking place in the observation
        plane (see above for an explanation of why we do this). A small
        value will make the pattern very sharp, a high value will blur
        it out. Best is subjective, but less than 0.01 is nice.

Additional notes
----------------

You can get cool transparent lens flare overlays from the resulting HDRI
images by alphablending with black, by definition of wave superposition.

Todo
----

- Support non-square apertures (not sure if worth it or even possible)
- Add support for arbitrary incident light wave spectrums (right now
  only pure white light is used, perhaps reading a 1D spectral image
  as a command-line argument?)
