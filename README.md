fraunhofer - Far-Field Lens Diffraction
=======================================

<p align="center">
<img
src="https://raw.github.com/TomCrypto/fraunhofer/master/renders/circle.png"
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

You can get cool transparent lens flare overlays from the resulting HDR
images by alphablending with black, by definition of wave superposition.

In fact you can use this program to generate the white-light starburst
pattern and then superimpose it on every pixel of an existing render,
scaling its size and intensity based on the pixel's brightness and
optionally colorizing it based on the pixel's color. The idea is that
only very bright pixels (such as light sources or direct reflections
thereof) actually get a big enough superimposed copy to be noticeable.

This will effectively "augment" the render adding starburst effects.
The secondary "ghost" effects are generally only visible for the
brightest object and this tool does not help render them.

There are a couple template apertures I cobbled up myself using my
pathetic computer drawing skills:
- "pupil" is a small aperture roughly corresponding to a human
  pupil, with a few specks of dirt here and there. Not really
  accurate but if you look at the screenshot, it looks good.
- "circle_noise" is a circular aperture with some random noise
  thrown in. It looks pretty spectacular although not particularly
  realistic.
- "pentagon_noise" is what it sounds like, a pentagon with noise. Note
  that it produces 10 streaks instead of 5, because of how wave
  superposition works (this is real and expected).

This program wasn't designed to be extensible, so it's pretty quickly
written. There are very few function calls, but it's quite readable.

There is a decent OpenCL FFT implementation in `cl/fft.cl`, as well.

Todo
----

- Support non-square apertures (not sure if worth it or even possible)
- Add support for arbitrary incident light wave spectrums (right now
  only pure white light is used, perhaps reading a 1D spectral image
  as a command-line argument?)
