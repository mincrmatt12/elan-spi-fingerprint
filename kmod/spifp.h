#pragma once
#include <linux/ioctl.h>

/*
 * This module is responsible for sending extremely basic commands to and from the sensor.
 *
 * Specifically, no sensor logic is performed here, only basic SPI transfers.
 *
 * We create a character device that supports only ioctls:
 *
 *   - ESPIFP_CONFIG_READ: Use the two byte "config read" command form to obtain a 1-byte parameter:
 *   TX  0xff <param> 0xff
 *   RX  ------------ value  
 *
 *   - ESPIFP_REGISTER_READ: Read a 1-byte register.
 *   - ESPIFP_REGISTER_WRITE: Write a 1-byte register
 *
 *   - ESPIFP_SW_RESET: Send the software reset command
 */

