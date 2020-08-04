// For HID (resets the SPI device through touchpad)
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

// General stuff
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
	int fd = open("/dev/hidraw0", O_RDWR);
	if (fd < 0) return -1;

	int size = 0;
	ioctl(fd, HIDIOCGRDESCSIZE, &size);

	hidraw_report_descriptor rpt_desc{0};
	rpt_desc.size = size;

	ioctl(fd, HIDIOCGRDESC, &rpt_desc);

	for (int i = 0; i < rpt_desc.size; ++i) {
		putc(rpt_desc.value[i], stdout);
	}

	return 0;
}
