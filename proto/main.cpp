// Main prototype for ELAN SPI sensors
//
// Uses minimal amounts of UDEV to try and find the sensor;
//
// NOTE: only tested against exactly one device, YMMV
//
// Licensed under GPLv2 (c) matthew mirvish 2020
// ...nerds

// For detecting devices
#include <libudev.h>

// For HID (resets the SPI device through touchpad)
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

// For SPI communication
#include <linux/spi/spidev.h>

// General stuff
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>

// Various tables copied directly from the WbfSpiDriver.dll file

// Register tables for setting modes (calib + init)
#include "regtable.h"

// List of supported sensors
#include "sensortable.h"

// Sensor settings from the registry
#include "hkeyvalue.h"

// Helper routine for doing duplex
auto SpiFullDuplex(int fd, uint8_t *rx_buffer, uint8_t *tx_buffer, size_t length) {
	spi_ioc_transfer mesg;
	memset(&mesg, 0, sizeof mesg);
	mesg.len = length;
	mesg.rx_buf = (__u64)rx_buffer;
	mesg.tx_buf = (__u64)tx_buffer;

	return ioctl(fd, SPI_IOC_MESSAGE(1), &mesg);
}

namespace elan {
	// Get the "SPI Status" field
	
	template<uint8_t Idx>
	uint8_t ReadGenericDevInfo(int fd) {
		uint8_t cmd[3] = {
			Idx, 0xff, 0xff // this last byte is probably uninportant
		};
		uint8_t resp[3];
		SpiFullDuplex(fd, resp, cmd, 3);
		return resp[2];
	}

	inline uint8_t ReadSPIStatus(int fd) {return ReadGenericDevInfo<0x3>(fd);}
	inline uint8_t ReadSensorVersion(int fd) {return ReadGenericDevInfo<0xa>(fd);}
	inline uint8_t ReadSensorHeight(int fd) {return ReadGenericDevInfo<0x8>(fd) + 1;}
	inline uint8_t ReadSensorWidth(int fd) {return ReadGenericDevInfo<0x9>(fd) + 1;}

	uint8_t ReadRegister(int fd, uint8_t regId) {
		uint8_t cmd[2] = {
			static_cast<uint8_t>(regId | 0x40), // 0x40 bit == read register, 0x80 bit == write register
			0x0 // padding
		};
		uint8_t resp[2];
		SpiFullDuplex(fd, resp, cmd, 2);
		return resp[1];
	}

	void WriteRegister(int fs, uint8_t regId, uint8_t value) {
		uint8_t cmd[2] = {static_cast<uint8_t>(regId | 0x80), value};
		write(fs, cmd, 2);
	}

	void SoftwareReset(int fd) {
		uint8_t cmd = 0x31;
		write(fd, &cmd, 1);
		usleep(4000);
	}

	auto DoHidReset(const char *hidpath) {
		// Send feature 0xe

		int fd = open(hidpath, O_RDWR);
		// TODO: check ok

		uint8_t buf[5] = {
			0xe,
			0,
			0,
			0,
			0
		};

		// See NotifyDriverViaHIDVendorDefineForReset
		auto result = ioctl(fd, HIDIOCSFEATURE(5), buf);

		printf("Result of ioctl for reset %d\n", result);

		close(fd);
		usleep(20000);

		return result;
	}
}

int main(int argc, char **argv) {
	puts("Prototype starting...");
	printf("Compiled with HKEY values : TP_VID %s; TP_PID %s; ACPI_ID %s\n", elan::TP_VID, elan::TP_PID, elan::ACPI_HID);

	if (argc < 3) {
		puts("Usage: prototype /dev/spi /dev/hidraw");
		return 1;
	}
	
	char *located_spi_path = argv[1];
	char *located_hid_path = argv[2];

	// Ok, we now have a /dev/hidraw and /dev/spidev
	printf("Got SPI = %s and HID = %s, opening.\n", located_spi_path, located_hid_path);

	// Open the SPI first
	int spi_fd = open(located_spi_path, O_RDWR);
	if (spi_fd < 0) {
		puts("Failed to open SPI, check permissions?");
		return 2;
	}

	// ======== CBIOMETRICDEVICE::FPINITIALIZE is copied here
	puts("Beginning initialization");
	
	// Check the SPIStatus (probably for debugging but it's useful for that very reason)
	auto spistatus = elan::ReadSPIStatus(spi_fd);
	printf("SPIStatus = 0x%.2X\n", spistatus);
	// In theory this should have both bit 1 and 7 clear to indicate uncalibrated and needing reset?
	//
	// In practice the driver resets unconditionally
	elan::DoHidReset(located_hid_path);

	// DEVIATION: read the spi status again to see if it changed
	spistatus = elan::ReadSPIStatus(spi_fd);
	printf("SPIStatus after reset = 0x%.2X\n", spistatus);

	// Dump registers
	auto DumpReg = [&](){
		for (int i = 0; i < 0x40; ++i) {
			printf("- Register %02x = %02x\n", i, elan::ReadRegister(spi_fd, i));
		}
	};
	DumpReg();

	puts("Reading raw dimensions");	

	auto rawHeight = elan::ReadSensorHeight(spi_fd);
	auto rawWidth = elan::ReadSensorWidth(spi_fd);

	uint8_t sensWidth = rawWidth;
	uint8_t sensHeight = rawHeight;
	uint8_t sensIcVersion = 0;

	printf("Got %dx%d sensor\n", rawWidth, rawHeight);

	// Do a hardcoded check:
	// It appears that the format changed with the versions, as some sensors report 1+the correct values for these, indicating that the +1 was added later, hence why they are 
	// labelled ICVersion 0
	//
	// As a result, we check for the three dimensions that have this behaviour first
	if ( ((rawHeight == 0xa1) && (rawWidth == 0xa1)) ||
	     ((rawHeight == 0xd1) && (rawWidth == 0x51)) ||
	     ((rawHeight == 0xc1) && (rawWidth == 0x39)) ) {
		sensIcVersion = 0; // Version 0
		sensWidth = rawWidth - 1;
		sensHeight = rawHeight - 1;
	}
	else {
		// If the sensor is exactly 96x96 (0x60 x 0x60), the version is the high bit of register 17
		if (rawWidth == 0x60 && rawHeight == 0x60) {
			if (-1 < static_cast<int8_t>(elan::ReadRegister(spi_fd, 0x17))) {
				sensIcVersion = 0;
			}
			else {
				sensIcVersion = 1;
			}
		}
		else {
			if ( ((rawHeight != 0xa0) || (rawWidth != 0x50)) &&
				 ((rawHeight != 0x90) || (rawWidth != 0x40)) &&
				 ((rawHeight != 0x78) || (rawWidth != 0x78)) ) {
				if ( ((rawHeight != 0x40) || (rawWidth != 0x58)) &&
				     ((rawHeight != 0x50) || (rawWidth != 0x50)) ) {
					// Old sensor hack??
					sensWidth = 0x78;
					sensHeight = 0x78;
					sensIcVersion = 0;
				}
				else {
					// Otherwise, read the version 'normally'
					sensIcVersion = elan::ReadSensorVersion(spi_fd);
					if ((sensIcVersion & 0x70) == 0x10) sensIcVersion = 1;
				}
			}
			else {
				sensIcVersion = 1;
			}
		}
	}

	printf("After hardcoded lookup: (%d x %d) Version = %d\n", sensWidth, sensHeight, sensIcVersion);

	bool sensIsOtp;
	int sensId;

	for (int i = 0; i < elan::SensorTableLength; ++i) {
		if (elan::SensorDataTable[i].width == sensWidth && elan::SensorDataTable[i].height == sensHeight && elan::SensorDataTable[i].ic_version == sensIcVersion) {
			sensId = i;
			sensIsOtp = elan::SensorDataTable[i].is_otp_model;
			break;
		}
	}

	printf("Found sensor ID %d => [%s] (%d X %d) Version = %d; OTP = %d\n", sensId, elan::SensorNameTable[sensId], sensWidth, sensHeight, sensIcVersion, sensIsOtp);

	puts("SoftwareReset...");
	elan::SoftwareReset(spi_fd);

	// Set OTP params
	if (sensIsOtp) {
		puts("SettingOTPParameter");
		// SettingOTPParameter
		uint8_t vref_trim1 = elan::ReadRegister(spi_fd, 0x3d);
		printf("Before CTL_REG_VREF_TRIM1 = 0x%.2x\n", vref_trim1);
		vref_trim1 &= 0x3f; // mask out low bits
		elan::WriteRegister(spi_fd, 0x3d, vref_trim1);
		printf("After CTL_REG_VREF_TRIM1 = 0x%.2x\n", vref_trim1);

		// Set inital value for register 0x28

		elan::WriteRegister(spi_fd, 0x28, 0x78);

		uint8_t VcmMode = 0;

		for (int itercount = 0; itercount < 3; ++itercount) { // totally arbitrary timeout replacement
			// TODO: timeout

			uint8_t regVal = elan::ReadRegister(spi_fd, 0x28);
			if ((regVal & 0x40) == 0) {
				// Do more stuff...
				uint8_t regVal2 = elan::ReadRegister(spi_fd, 0x27);
				if (regVal2 & 0x80) {
					VcmMode = 2;
					break;
				}
				VcmMode = regVal2 & 0x1;
				if ((regVal2 & 6) == 6) {
					uint8_t reg_dac2 = elan::ReadRegister(spi_fd, 7);
					printf("Before CTL_REG_DAC2 = 0x%.2x\n", reg_dac2);
					reg_dac2 |= 0x80;
					printf("After CTL_REG_DAC2 = 0x%.2x\n", reg_dac2);
					// rewrite it back
					elan::WriteRegister(spi_fd, 7, reg_dac2);
					elan::WriteRegister(spi_fd, 10, 0x97);
					break;
				}
			}
			// otherwise continue loop
		}

		// Set VCM mode
		printf("SelectVCM %d\n", VcmMode);

		if (VcmMode == 2) {
			elan::WriteRegister(spi_fd, 0xb, 0x72);
			elan::WriteRegister(spi_fd, 0xc, 0x62);
		}
		else if (VcmMode == 1) {
			elan::WriteRegister(spi_fd, 0xb, 0x71);
			elan::WriteRegister(spi_fd, 0xc, 0x49);
		}

		puts("SetOTPParameters");
	}

	// Close fds
	close(spi_fd);

	return 0;
}
