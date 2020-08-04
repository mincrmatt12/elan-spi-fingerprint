#pragma once

// This prototype could theoretically be expanded to work with the GPIO reset type, but I don't have any hardware to test that with
// so we're assuming it uses the touchpad.
//
// These values come from HKLM\System\CurrentControlSet\Control\ElanFP\OtherSetting\TP_VID,TP_PID

#include <stdint.h>

namespace elan {

	const static int TP_VID = 0x04F3;
	const static int TP_PID = 0x3057;

	// This is both from the DSDT table and the .inf file with the driver
	const static char *ACPI_HID = "ELAN7001";

};
