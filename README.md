# ROM Bin Patcher (rombp)

rombp is a utility program to ROM bin patches, typically used for
retro gaming romhacks. I wrote it primarily to target
the
[RG350m gaming handheld.](https://obscurehandhelds.com/2020/07/the-anbernic-rg350m-review/),
which runs a flavor of Linux
called
[OpenDingux](https://wiki.dingoonity.org/index.php?title=OpenDingux:About).

Patch file support:
- [IPS patch format](http://fileformats.archiveteam.org/wiki/IPS_(binary_patch_format)
- [BPS patch format](https://github.com/blakesmith/rombp/blob/master/docs/bps_spec.md)

rombp also runs on desktop Linux (I've only tested it with Ubuntu
20.04), if you want to try it on a desktop before loading it on your
console.

I've only tested with an RG350M, but in theory, it should work with
other OpenDingux MIPS64 based handhelds as well.

![Desktop screenshot](https://raw.github.com/blakesmith/rombp/master/docs/screenshot.jpg)

Here it is running on the RG350M:

![RG350M image](https://raw.github.com/blakesmith/rombp/master/docs/rg350m.jpg)

# Installing

Head over to the
[releases page](https://github.com/blakesmith/rombp/releases) to
download the latest OpenDingux OPK file. You should be able to run the
OPK file just like your other programs.

# Building

You'll need to setup your RG350
buildroot. Checkout
[this GitHub repository](https://github.com/tonyjih/RG350_buildroot)
and follow the setup instructions to build a local toolchain. This
will build the necessary MIPS compiler toolchain, as well as compile
the shared libraries that are present on the system for linking (in
rombp's case, we just link against uclibc and SDL2).

Once you setup the toolchain, configure its location via:

```
$ export RG350_TOOLCHAIN=/path/to/toolchain
```

From there, you should be able to build an OPK file via:

```
$ TARGET=rg350 make clean rombp.opk
```

You'll find the built OPK file in the rombp project directory.

