#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/spi/spi.h>

#include "spifp.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Matthew Mirvish");
MODULE_DESCRIPTION("Elantech SPI fingerprint glue");
MODULE_VERSION("0.01");

// Reference to the actual SPI device: only one will ever exist

static DEFINE_MUTEX(underlying_spi_lock);
struct elan_spifp_data {
	dev_t devt;
	spinlock_t spi_lock;

	struct spi_device *spi;
	unsigned users;
};

static struct elan_spifp_data device_data;

static const struct acpi_device_id elan_spifp_acpi_ids[] = {
	{ "ELAN7001", 1 },
	{}
};

static int elan_spifp_probe(struct spi_device *spi) {
	// Check if there's already a device found
	if (device_data.spi) {
		printk(KERN_WARNING "elan_spifp: detected multiple spi devices");
		return -1;
	}
}

static int elan_spifp_remove(struct spi_device *spi) {
}

static struct spi_driver elan_spifp_spi_driver = {
	.driver = {
		.name = "elan_spifp",
		.acpi_match_table = ACPI_PTR(elan_spifp_acpi_ids)
	},

	.probe = elan_spifp_probe,
	.remove = elan_spifp_remove
};

static int __init elan_spifp_init(void) {
	int status;

	printk(KERN_INFO "elan_spifp: starting\n");

	memset(&device_data, 0, sizeof device_data);

	printk(KERN_INFO "elan_spifp: registering spi\n");

	status = spi_register_driver(&elan_spifp_spi_driver);
	if (status < 0) {
		// recover
	}

	return status;
}

static void __exit elan_spifp_exit(void) {
	printk(KERN_INFO "elan_spifp: exiting\n");

	spi_unregister_driver(&elan_spifp_spi_driver);
}

module_init(elan_spifp_init);
module_exit(elan_spifp_exit);
