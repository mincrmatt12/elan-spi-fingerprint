# elan-spi-fingerprint

Reverse engineering the SPI elantech fingerprint sensor drivers.

A Ghidra archive of the Wbf driver is present as elanfp.gar. Some notes about the protocol are present in notes.txt

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

The current development one uses much the same logic as the prototype, only supporting the bare minimum communication system. We currently add a bus type `SPIDEV` to `libfprint` (the HID device is opened temporarily and on-demand, and shouldn't
need to be claimed exclusively) and since there doesn't seem to be a good way of automatically assigning devices to drivers, we currently let the driver decide based on a device's syspath + udev device what it wants to do with it.

In order to determine whether or not a finger is on the sensor, the windows driver either waits for a GPIO interrupt / power state change (which we currently don't have any hardware to test with since the only machine we have doesn't use this)
or continuously takes images and tries to guess whether or not a finger is present.

The logic for doing this has proved difficult to reverse except for the first step, which is to compute the standard deviation of the image. From analyzing logs it appears that this is probably enough to get a basic implementation going.
Ideas for other methods or help reversing the windows driver is welcome.

This standard deviation is kind of weird, it isn't over the entire image but rather it's done row-wise, presumably because the image seems to have noticeable "bands" of background, and then an average of all the standard devs
is taken. (my pitiful math skills might mean that this is actually the same as an average over the entire image)

## Protocol info

Currently no real documentation is written, although I've tried to comment the `elanfp.gar` file well, and reading the prototype or `notes.txt` file should get you started if you want to write your own driver to talk to these devices.
