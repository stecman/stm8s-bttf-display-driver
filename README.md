# STM8S 16-segment display driver

This is firmware for an STM8SF003 microcontroller to drive a 16-segment display as an SPI slave device in the project *[Time Circuits Time Piece](https://hackaday.io/project/19865-time-circuits-timepiece)*.

Pre-compiled versions of the firmware are provided on the [releases page](https://github.com/stecman/stm8s-bttf-display-driver/releases) if you don't want to compile it yourself.

## Setup

Grab the code:

```sh
git clone https://github.com/stecman/stm8s-bttf-display-driver.git
cd stm8s-bttf-display-driver

# Pull in SDCC compatible STM8S peripheral library
git submodule init
git submodule update
```

Once the toolchain below is available:

```sh
# Build
scons

# Flash through STLinkV2
scons flash
```

## Linux Toolchain

### SCons (build tool)

This should be available in your distribution's pacakage manager. It can also be installed via Python's `pip` package manager.

### SDCC (compiler)

[SDCC](http://sdcc.sourceforge.net/) may be available in your distro's package manager. I recommend installing from source to get the most recent release, as the Debian and Ubuntu repos can be a few versions behind:

```sh
# with Git
git clone https://github.com/svn2github/sdcc

# or with SVN
svn co http://svn.code.sf.net/p/sdcc/code/trunk sdcc

cd sdcc/sdcc
./configure
make
sudo make install
```

### stm8flash (flashing tool)

[stm8flash](https://github.com/vdudouyt/stm8flash) uses an STLink V1/V2 to program STM8 devices through their SWIM interface.

This needs to be compiled from source currently, which is simple:

```sh
git clone https://github.com/vdudouyt/stm8flash
cd stm8flash
make
sudo make install
```