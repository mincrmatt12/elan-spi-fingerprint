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
#include <algorithm>
#include <fstream>
#include <numeric>
#include <iostream>
#include <cmath>
#include <vector>

#include <assert.h>

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

	void PageSelect(int fd, uint8_t page) {
		uint8_t cmd[2] = {0x7, page};
		write(fd, cmd, 2);
		printf("set device register page to %d\n", page);
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

	void SetOTPParameters(int spi_fd) {
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

		// Set VCM mode (vcom apparently)
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

	void WriteRegtable(int fd, RegTable table) {
		for (int i = 0; i < table.length; ++i) {
			printf("Regtable[%d] sets %02x --> %02x\n", i, table.values[i].address, table.values[i].value);
			elan::WriteRegister(fd, table.values[i].address, table.values[i].value);
		}
	}

	//
	// Capturing on the 0xe type sensor is also weird, there are two modes in the driver:
	// - non x571: (case i'm implementing rn)
	//   send a 0x10; read in the entire image + a bunch of optional junk, filter for 0xffs, populate normally. (apparently 0xff is an invalid reading?)
	// - x571:
	//   send a 0x10, read in the entire image with exactly 2 bytes stride between rows.
	
	void CaptureRawImageHV(int fd, int width, int height, uint16_t *raw_data_out) {
		assert(width == 80 && height == 80);

		uint8_t hardcoded_unclean_rx_buf[0x4102];
		memset(hardcoded_unclean_rx_buf, 0xff, sizeof hardcoded_unclean_rx_buf);
		uint8_t output_buf[0x4102];
		memset(output_buf, 0, sizeof output_buf);

		// Send sensor command 0x1
		{
			uint8_t cmd = 0x1;
			write(fd, &cmd, 1);
		}

		// Wait for image to be ready.
		while (true) {
			uint8_t status = ReadSPIStatus(fd);
			if (status & 4) break;
			// wait for bit 3
			puts("waiting for image...");
		}

		// Receieve entire image + pad
		output_buf[0] = 0x10;
		SpiFullDuplex(fd, hardcoded_unclean_rx_buf, output_buf, 0x4102);

		uint16_t value = 0;
		for (int i = 2, outptr = 0; i < 0x4102 && outptr < (width*height*2); ++i) {
			if (hardcoded_unclean_rx_buf[i] != 0xff) {
				if (outptr % 2) {
					value <<= 8;
					value |= hardcoded_unclean_rx_buf[i];
					raw_data_out[outptr / 2] = value;
				}
				else {
					value = hardcoded_unclean_rx_buf[i];
				}
				++outptr;
			}
		}
	}

	// raw_data_out is laid out row major and has correct endianness
	// (row increases slowest)
	void CaptureRawImage(int fd, int width, int height, uint16_t *raw_data_out) {
		// TODO: SetRegisterInitialValuesForSensing & WOE Mode (unsupported on tyler's system)

		uint8_t rx_buf[2 + width*2];
		memset(rx_buf, 0, sizeof rx_buf);
		uint8_t tx_buf[2 + width*2];
		memset(tx_buf, 0, sizeof tx_buf);

		// Send sensor command 0x1
		{
			uint8_t cmd = 0x1;
			write(fd, &cmd, 1);
		}

		// Todo: TIMEOUTS
		for (int line = 0; line < height; ++line) {
			// Wait for status ready
			while (true) {
				uint8_t status = ReadSPIStatus(fd);
				if (status & 4) break;
				// wait for bit 3
			}
			
			tx_buf[0] = 0x10;
			tx_buf[1] = 0x00;

			// Send out command and receieve one line of image data

			SpiFullDuplex(fd, rx_buf, tx_buf, 2 + width*2);

			// Populate data in buffer

			for (int col = 0; col < width; ++col) {
				uint8_t low = rx_buf[2 + col*2 + 1];
				uint8_t high = rx_buf[2 + col*2];

				raw_data_out[width * line + col] = low + high * 0x100;
			}
		}
	}

	enum struct GuessResult {
		FP,
		EMPTY,
		UNKNOWN
	};

	// Try to guess whether or not this image has a fingerprint in it or not
	GuessResult GuessFingerprint(const uint16_t *data, int width, int height) {
		// First, compute the standard deviation
		
		// To do this, we first find the mean
		int mean = std::accumulate(data, data+(width*height), 0) / (width*height);
		int stddev = std::sqrt(std::accumulate(data, data+(width*height), 0., [&](double a, uint16_t b){
			return a + std::abs((double)b - (double)mean)*std::abs((double)b - (double)mean);
		}) / (width * height));
		printf("GuessFingerprint mean=%d stddev=%d\n", mean, stddev);

		// Next, we segment the data

		// this'd be so much easier with c++20 lol
		std::vector<uint16_t> low_half(data, data+(width*height));
		std::sort(low_half.begin(), low_half.end());
		low_half.erase(std::unique(low_half.begin(), low_half.end()), low_half.end());

		std::vector<uint16_t> high_half(low_half);

		// this could really be a binary search esque thing with lower_bound
		low_half.erase(std::remove_if(low_half.begin(), low_half.end(), [&](auto a){return a > mean;}), low_half.end());
		high_half.erase(std::remove_if(high_half.begin(), high_half.end(), [&](auto a){return a < mean;}), high_half.end());
		
		// Compute the meandist
		int distavg = ((mean - low_half.back()) + (high_half.front() - mean)) / 2;
		printf("GuessFingerprint distavg=%d\n", distavg);

		int is_real = 0, is_empty = 0;
		if (stddev < 850) ++is_real;
		if (stddev > 2200) ++is_empty;

		if (distavg < 200) ++is_real;
		if (distavg > 1100) ++ is_empty;

		printf("GuessFingerprint real=%d empty=%d\n", is_real, is_empty);
		if (is_real > is_empty) return GuessResult::FP;
		else if (is_empty > is_real) return GuessResult::EMPTY;
		else return GuessResult::UNKNOWN;
	}

	struct RegisterGuard {
		RegisterGuard(int fd, uint8_t addr, uint8_t enter, uint8_t exit) {
			this->fd = fd;
			this->addr = addr;
			this->exit = exit;

			elan::WriteRegister(fd, addr, enter);
		}

		~RegisterGuard() {
			printf("RegisterGuard clearing %d to %d", this->addr, this->exit);
			elan::WriteRegister(this->fd, this->addr, this->exit);
		}

	private:
		int fd;
		uint8_t addr, exit;
	};

	bool CalibrateHV(int fd, int sensorId, int width, int height) {
		/*
		 * Algorithm:
		 *  There is a DAC value (which is probably a constant gain / offset for the DAC) which is a 10 bit integer, stored in
		 *  registers 6 and 7 (GDAC_H (upper 8 bits), GDAC_L (lower 2 bits)).
		 *
		 *  We start with this value at 0x100. We then perform a binary search with this value to try and get the mean of a blank
		 *  image as close as possible to some target V. Due to _reasons_, we also apparently keep track of historical values.
		 *
		 * 	uint BestDAC = 0, BestMeanDiff = UINT_MAX;
		 *  uint DAC = 0x100, step = 0x100;
		 *  for (int i = 0; i < 10; ++i) {
		 *  	WriteGDAC(DAC);
		 *  	int mean = GrabAndGetMean();
		 *  	int diff = abs(mean - Target);
		 *  	if (diff < 100) {
		 *  		return DAC;
		 *  	}
		 *  	if (diff < BestMeanDiff) {
		 *  		BestMeanDiff = diff;
		 *  		BestDAC = DAC;
		 *  	}
		 *  	step /= 2;
		 *  	if (step == 0) break;
		 *  	if (mean < Target) DAC -= step; // yes, opposite
		 *  	else               DAC += step;
		 *  }
		 *  return BestDAC;
		 */

		// set register to page 0
		PageSelect(fd, 0);
		// set calibration flag
		{
			uint8_t cmd = 0x4;
			write(fd, &cmd, 1);
			usleep(1000);
		}
		// set 0 to 5a
		RegisterGuard guard(fd, 0, 0x5a, 0);
		puts("Sending table 0");
		switch (sensorId) {
			case 0xe:
			default:
				WriteRegtable(fd, calib::HV::Page0);
				break;
		}
		PageSelect(fd, 1);
		puts("Sending table 1");
		switch (sensorId) {
			case 0xe:
			default:
				WriteRegtable(fd, calib::HV::Page1);
				break;
		}
		PageSelect(fd, 0);

		auto WriteGDAC = [&](uint16_t DAC){
			WriteRegister(fd, 0x6 /* GDAC_H */, (DAC >> 2) & 0xff);
			WriteRegister(fd, 0x7 /* GDAC_H */, (DAC & 0b11));
		};

		const int TargetMean = 3000;

		uint16_t raw_image[width*height];
		uint16_t BestDAC = 0, BestMeanDiff = UINT16_MAX;
		uint16_t DAC = 0x100, step = 0x100;
		for (int i = 0; i < 10; ++i) {
			printf("Calibration loop %d; with DAC = %d.\nBestDAC = %d, BestMeanDiff = %d\n", i, DAC, BestDAC, BestMeanDiff);
			WriteGDAC(DAC);
			CaptureRawImageHV(fd, width, height, raw_image);
			int mean = std::accumulate(raw_image, raw_image + (width * height), 0) / (width * height);
			printf("Image mean = %d\n", mean);
			uint16_t diff = abs(mean - TargetMean);
			if (diff < 100) {
				puts("Mean diff < 100; exiting early.");
				return true;
			}
			if (diff < BestMeanDiff) {
				BestMeanDiff = diff;
				BestDAC = DAC;
				puts("New best");
			}
			step /= 2;
			if (step == 0) break;
			if (mean < TargetMean) DAC -= step;
			else                   DAC += step;
		}

		puts("Exited loop, using best values.");
		WriteGDAC(BestDAC);

		return true;
	}

	bool Calibrate(int fd, int sensorId, int width, int height) {
		// Start by writing 0x5a to register 0, which seems to be a prerequisite for changing most of the registers or something.

		if (sensorId == 0xe) return CalibrateHV(fd, sensorId, width, height);

		puts("Starting calibration");
		RegisterGuard guard(fd, 0, 0x5a, 0x00);

		// Send command 0x4, which probably sets the "calibrated" bit.

		puts("Sending cmd 0x4");
		{
			uint8_t cmd = 0x4;
			write(fd, &cmd, 1);
			usleep(1000);
		}

		puts("Sending regtable");

		// Based on the sensor ID, write a regtable.
		switch (sensorId) {
			case 0:
				WriteRegtable(fd, calib::CalibId0);
				break;
			case 0x5:
				WriteRegtable(fd, calib::CalibId5);
				break;
			case 0x6:
				WriteRegtable(fd, calib::CalibId6);
				break;
			case 0x7:
				WriteRegtable(fd, calib::CalibId7);
				break;
			default:
				WriteRegtable(fd, calib::CalibIdDefault);
				break;
		}
		
		// Take a raw image to set gain
		
		puts("Capturing image");

		uint16_t raw_image[width * height];
		memset(raw_image, 0, sizeof raw_image);

		CaptureRawImage(fd, width, height, raw_image);

		// Compute mean value
		uint8_t calibration_dac_value = (((std::accumulate(raw_image, raw_image + (width * height), 0) / (width*height)) & 0xffff) + 0x80) >> 8;

		printf("Got calibration value %02x\n", calibration_dac_value);

		if (0x3f < calibration_dac_value) calibration_dac_value = 0x3f;

		// Write that to register 6

		WriteRegister(fd, 0x6, calibration_dac_value - 0x40);

		// Take another image
		CaptureRawImage(fd, width, height, raw_image);

		int mean_value = std::accumulate(raw_image, raw_image + (width * height), 0) / (width*height);

		printf("New mean image == %d\n", mean_value);

		if (mean_value >= 1000) {
			puts("======================");
			puts("======================");
			puts("The mean value is abnormally high, the real driver would reject this. The prototype doesn't care, but you probably have your fingerprint on the sensor or something.");
			puts("======================");
			puts("======================");
		}

		puts("Increasing gain to 0x6f");

		WriteRegister(fd, 0x5, 0x6f);

		// Adjust DAC

		for (int i = 0; i < 2; ++i) {
			puts("Taking calibration image 3 for iterative");
			CaptureRawImage(fd, width, height, raw_image);
			mean_value = std::accumulate(raw_image, raw_image + (width * height), 0) / (width*height);
			printf("CalibLoop[%d] mean_value == %d\n", i, mean_value);
			if ((mean_value - 3000) < 0x1389) {
				printf("Calibration success, DAC=%02x\n", calibration_dac_value);
				return true;
			}
			if (mean_value < 0x1f41) calibration_dac_value -= 1;
			else 					 calibration_dac_value += 1;
			printf("CalibLoop[%d] nudging DAC to %02x\n", i, calibration_dac_value);
			WriteRegister(fd, 0x6, calibration_dac_value - 0x40);
		}

		puts("Calibration failed to settle!");
		return false;
	}

	void CorrectWithBg(int width, int height, uint16_t *correct, const uint16_t* bg) {
		int countMin = 0;
		puts("CorrectWithBg");

		for (int i = 0; i < width*height; ++i) {
			if (correct[i] < bg[i]) {
				countMin++;
			}
			else {
				correct[i] -= bg[i];
			}
		}

		printf("ImgCorrection BG reported %d low pixels from %d total (%d percent)\n", countMin, width*height, (countMin * 100)/(width*height));
	}
}

int main(int argc, char **argv) {
	puts("Prototype starting...");
	printf("Compiled with HKEY values : TP_VID %x; TP_PID %x; ACPI_ID %s\n", elan::TP_VID, elan::TP_PID, elan::ACPI_HID);

	if (argc < 2) {
		puts("Usage: prototype /dev/spi /dev/hidraw");
		puts("    or prototype udev");
		return 1;
	}

	std::string located_spi_path = "";
	std::string located_hid_path = "";

	if (argc == 2) {
		udev* udev = udev_new();

		{
			udev_enumerate *e = udev_enumerate_new(udev);
			udev_enumerate_add_match_subsystem(e, "spidev");

			udev_enumerate_scan_devices(e);
			udev_list_entry *devices, *dev_entry;
			devices = udev_enumerate_get_list_entry(e);
			udev_list_entry_foreach(dev_entry, devices) {
				const char* syspath = udev_list_entry_get_name(dev_entry);
				printf("Got SPI entry %s\n", syspath);
				if (strstr(syspath, elan::ACPI_HID)) {
					puts("Found ACPI id!");

					udev_device *dev = udev_device_new_from_syspath(udev, syspath);
					located_spi_path.assign(udev_device_get_devnode(dev));
					udev_device_unref(dev);

					break;
				}
			}

			udev_enumerate_unref(e);
		}

		{
			udev_enumerate *e = udev_enumerate_new(udev);
			udev_enumerate_add_match_subsystem(e, "hidraw");

			udev_enumerate_scan_devices(e);
			udev_list_entry *devices, *dev_entry;
			devices = udev_enumerate_get_list_entry(e);

			udev_list_entry_foreach(dev_entry, devices) {
				const char* syspath = udev_list_entry_get_name(dev_entry);
				printf("Got HID entry %s\n", syspath);
				udev_device *dev = udev_device_new_from_syspath(udev, syspath);
				const char* devpath = udev_device_get_devnode(dev);
				if (!devpath) {
					puts("skipping because no devpath");
					udev_device_unref(dev);
					continue;
				}
				
				int temp_hid = open(devpath, O_RDWR);
				if (temp_hid < 0) {
					puts("skipping because failed to open");
					udev_device_unref(dev);
					continue;
				}
				hidraw_devinfo info;

				int res = ioctl(temp_hid, HIDIOCGRAWINFO, &info);
				close(temp_hid);
				if (res < 0) {
					puts("skipping because failed to get info");
					udev_device_unref(dev);
					continue;
				}

				if (info.vendor == elan::TP_VID && info.product == elan::TP_PID) {
					puts("Found TP ID!");
					located_hid_path.assign(devpath);
					udev_device_unref(dev);
					break;
				}
				udev_device_unref(dev);
			}

			udev_enumerate_unref(e);
		}

		udev_unref(udev);
	}
	else {
		located_spi_path = argv[1];
		located_hid_path = argv[2];
	}

	if (located_hid_path.empty() || located_spi_path.empty()) {
		puts("Failed to detect SPI or HID!");
		return 1;
	}

	// Ok, we now have a /dev/hidraw and /dev/spidev
	printf("Got SPI = %s and HID = %s, opening.\n", located_spi_path.c_str(), located_hid_path.c_str());

	// Open the SPI first
	int spi_fd = open(located_spi_path.c_str(), O_RDWR);
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
	elan::DoHidReset(located_hid_path.c_str());

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
	//
	// Based on the obscene nonsense here I have a sneaking suspicion 
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
			sensIcVersion = (elan::ReadRegister(spi_fd, 0x17) & 0x80) ? 1 : 0;
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
					sensIcVersion = (elan::ReadSensorVersion(spi_fd) & 0x70) >> 4;
				}
			}
			else {
				sensIcVersion = 1;
			}
		}
	}

	printf("After hardcoded lookup: (%d x %d) Version = %d\n", sensWidth, sensHeight, sensIcVersion);

	bool sensIsOtp;
	int sensId = -1;

	for (int i = 0; i < elan::SensorTableLength; ++i) {
		if (elan::SensorDataTable[i].width == sensWidth && elan::SensorDataTable[i].height == sensHeight && elan::SensorDataTable[i].ic_version == sensIcVersion) {
			sensId = i;
			sensIsOtp = elan::SensorDataTable[i].is_otp_model;
			break;
		}
	}

	if (sensId == -1) {
		puts("Unknown sensor type!");
		return 1;
	}

	printf("Found sensor ID %d => [%s] (%d X %d) Version = %d; OTP = %d\n", sensId, elan::SensorNameTable[sensId], sensWidth, sensHeight, sensIcVersion, sensIsOtp);

	if (sensId == 0xe) {
		puts("====\nyou have a 80SC, if you haven't setup spidev with a large enough bufsiz _this will not work properly_\n===");
	}

	puts("SoftwareReset...");
	elan::SoftwareReset(spi_fd);

	// DEVIATION: Out of curiosity, do another regdump + status
	spistatus = elan::ReadSPIStatus(spi_fd);
	printf("SPIStatus after reset = 0x%.2X\n", spistatus);
	DumpReg();

	// Set OTP params
	if (sensIsOtp) elan::SetOTPParameters(spi_fd);

	// Do sensor calibration
	if (!elan::Calibrate(spi_fd, sensId, sensWidth, sensHeight)) {
		puts("Sensor didn't calibrate!");
		return 1;
	}

	DumpReg();

	puts("Taking background image");

	uint16_t bg_data[sensWidth * sensHeight];
	(sensId == 0xe ? elan::CaptureRawImageHV : elan::CaptureRawImage)(spi_fd, sensWidth, sensHeight, bg_data);

	std::string fname;
	
	puts("I will wait for a finger to exist for a few frames before saving it. Where should i save it?");
	puts("Where to save dump (enter)");
	std::cin >> fname;

	uint16_t data[sensWidth * sensHeight];

	int downcounter = 0;
	while (true) {
		(sensId == 0xe ? elan::CaptureRawImageHV : elan::CaptureRawImage)(spi_fd, sensWidth, sensHeight, data);
		elan::CorrectWithBg(sensWidth, sensHeight, data, bg_data);
		// Check if it's a real fingerprint
		switch(elan::GuessFingerprint(data, sensWidth, sensHeight)) {
			case elan::GuessResult::UNKNOWN:
				puts("UNKNOWN");
				break;
			case elan::GuessResult::FP:
				puts("FP exist");
				++downcounter;
				printf("downcount %d\n", downcounter);
				if (downcounter >= 10) goto done_loop;
				break;
			default:
				puts("EMPTY");
				downcounter = 0;
				break;
		}
	}
	
done_loop:
	std::ofstream out_fd(fname, std::ios::out | std::ios::binary | std::ios::trunc);
	out_fd.write((char*)data, sensWidth * sensHeight * 2);

	puts("Wrote out!");
	puts("Done..");

	// Close fds
	close(spi_fd);

	return 0;
}
