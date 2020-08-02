#pragma once

// This prototype could theoretically be expanded to work with the GPIO reset type, but I don't have any hardware to test that with
// so we're assuming it uses the touchpad.
//
// These values come from HKLM\System\CurrentControlSet\Control\ElanFP\OtherSetting\TP_VID,TP_PID

#include <stdint.h>

namespace elan {

	// TODO: confirm with tester
	const static char *TP_VID = "04F3"; // stored as string because UDEV
	const static char *TP_PID = "3057";

	// This is both from the DSDT table and the .inf file with the driver
	const static char *ACPI_HID = "ELAN7001";

};
