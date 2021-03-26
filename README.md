# elan-spi-fingerprint

Reverse engineering the SPI elantech fingerprint sensor drivers. These seem to appear a lot in asus laptops, especially those with the fingerprint in the touchpad.

## Device support

This driver is specifically for elantech's _SPI_ based sensors. If the fingerprint sensor shows up in `lsusb`, you can probably use the
preexisting `libfprint` USB elantech driver, which is already merged into mainline `libfprint` (or see [here](https://github.com/iafilatov/libfprint))

### Laptop status

| Laptop | ACPI ID | Touchpad HID PID | Sensor name | Status | Notes |
| :------- | ---- | ---- | ------- | :--------: | :----------- |
| ASUS VivoBook S15 S510UA (x510uar) | `ELAN7001` | `3057` | `eFSA96SA` (`0x6`) | Working (prototype+libfprint on `mincrmatt12/elan-spi`) | |
| ASUS VivoBook S15 S510UN (x510un) | `ELAN7001` | unknown, probably `3057` | unknown, probably `eFSA96SA` (`0x6`) | Potentially working (prototype+libfprint on `mincrmatt12/elan-spi`) | See [#1](https://github.com/mincrmatt12/elan-spi-fingerprint/issues/1#issuecomment-748479266) |
| ASUS VivoBook S15 S530FN (x530fn) | `ELAN7001` | `3087` | unknown | Potentially working (prototype+libfprint on `mincrmatt12/elan-spi-s530fn`) | See [#1](https://github.com/mincrmatt12/elan-spi-fingerprint/issues/1#issue-703963799) |
| ASUS ExpertBook B9400CEA | `ELAN70A1` | `3134` | `eFSA80SC` (`0xe`) | In progress (prototype only) | See [#2](https://github.com/mincrmatt12/elan-spi-fingerprint/issues/2) |

### Specific sensor status

| Sensor Name/ID | Prototype status | Libfprint status |
| :------- | -------- | --------------- |
| `eFSA120S` (`0x0`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA120SA` (`0x1`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA160S` (`0x2`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA820R` (`0x3`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA519R` (`0x4`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA96S` (`0x5`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA96SA` (`0x6`) | Working | Working (on branch `mincrmatt12/elan-spi`) |
| `eFSA96SB` (`0x7`) | Not tested, probably not working (version 2) | Not started |
| `eFSA816RA` (`0x8`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA614RA` (`0x9`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA614RB` (`0xa`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA688RA` (`0xb`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA80SA` (`0xc`) | Not tested, probably working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA712RA` (`0xd`) | Not tested, probably not working | Not tested (try `mincrmatt12/elan-spi`) |
| `eFSA80SC` (`0xe`) | In progress | Not started |

Note, for devices marked "not tested" for libfprint but which _do_ have a branch listed, you will probably need to modify the PID constants in `elanspi.h` based on which touchpad you have to get it to detect (and potentially work with) your
sensor.

## Testing the driver

First, you should try out the prototype. It depends on `libudev` (installable on debian with `libudev-dev`) and is built with CMake.
You should determine your _ACPI ID_ and _Touchpad PID_. You can find these by searching in `/sys`.

Specifically, you can find the ACPI ID by finding a device like `spi-ELAN<some 4 digit hex number>` somewhere under `/sys/bus/spi/devices`. The ACPI id
is then `ELAN<that 4 digit hex>`.

The touchpad PID is the product ID for your touchpad's HID device (you can usually find references to this in `dmesg` output. You're looking for the second half of a `04f3:<4 digit hex number>` pair.)

You can then put these as `ACPI_HID` and `TP_PID` in `proto/hkeyvalue.h`. If you're really stuck, you might be able
to find them in the Windows registry and the `.inf` file for your fingerprint's windows driver, respectively.

Once you've got these setup, you can try compiling the prototype (`cd proto; mkdir build; cd build; cmake ..; make`) and running it as `./proto udev`. In theory
it'll spit out an image which you can try converting to a png with `tool/printdump.py`.

If the driver complains it can't find an `spidev` device, you either don't have the right ACPI id set, or need to install the udev rules in `udev/99-elan-spi.rules`.

If the prototype works, you can try using the libfprint driver. Make sure you build it with `-D drivers=all`.

## Prototype

The `proto/` subfolder contains a prototype that tries to connect to a fingerprint sensor, calibrate it, and take an image. It then dumps the corrected 16-bit ADC data to a file. The `printdump.py` file in the `tool` subdirectory
can format this back into a png.

Currently the only "mode" of operation supported by this prototype is the SPI-only setup, where there is no GPIO interrupt nor GPIO reset attached to the sensor. According to the driver's code, it would appear that this base configuration
is supported by all sensors.

These configuration settings can be found in the windows registry at `HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\ElanFP\OtherSetting`, specifically `SPIResetFound`, `WOEModeSupported`, `GPIOInterruptFound`, `WaitFingerPressType`.
See the driver's .inf file for the meanings of these values.

The prototype currently talks to the sensor using the linux `spidev` driver. If you're running kernel version 4.20 or higher, there's an easy way to get this to load with udev rules, see the `udev` subfolder. If you can't run a newer
kernel version, the current recommended technique is to compile a custom version of the `spidev` driver and add `ELAN7001` to the list of ACPI ids it loads for.

The prototype uses the HID method to reset the sensor. To identify which HID to use, we currently test all HIDS and match on VID:PID combo. This can also be found in the windows registry.

In order to find these devices automatically, we currently use `udev`, and the technique is fairly naive. Anyone with more experience in using `libudev` is welcome to write a PR.

## libfprint fork

There is also work on a proper `libfprint` driver for these sensors.

The current development one uses much the same logic as the prototype, only supporting the bare minimum communication system. 
We currently add a bus type `UDEV` to `libfprint` which lets drivers completely control what system devices they want (this driver uses a hid device for hw reset and an spidev)

In order to determine whether or not a finger is on the sensor, the windows driver either waits for a GPIO interrupt / power state change (which we currently don't have any hardware to test with since the only machine we have doesn't use this)
or continuously takes images and tries to guess whether or not a finger is present.

The logic for doing this has proved difficult to reverse except for the first step, which is to compute the standard deviation of the image. From analyzing logs it appears that this is probably enough to get a basic implementation going.
Ideas for other methods or help reversing the windows driver is welcome.

This standard deviation is kind of weird, it isn't over the entire image but rather it's done row-wise, presumably because the image seems to have noticeable "bands" of background, and then an average of all the standard devs
is taken. (my pitiful math skills might mean that this is actually the same as an average over the entire image)

The current implementation uses two techniques to determine if an image is empty, which works well enough right now:

- stddev
- number of pixels below the background image (currently implemented with a massive hack because I made a silly error when implementing the background correction function, but functionally does this)

We also treat the sensors as swipe-style ones, since the libfprint image matching algorithm is not designed to deal with such small sensors.

When building the libfprint fork, please _don't_ use the version in this repo, rather pull the latest one from [mincrmatt12/libfprint](https://github.com/mincrmatt12/libfprint). Make sure you build it with `-D drivers=all`.
Right now it's a little hard-coded and might need some convincing to use your specific sensor.

## Protocol info

Currently no real documentation is written, although I've tried to comment the `elanfp.gar` file well, and reading the prototype or `notes.txt` file should get you started if you want to write your own driver to talk to these devices.
