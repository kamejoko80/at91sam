/*
 *  Copyright (C) 2005 SAN People
 *  Copyright (C) 2008 Atmel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/at73c213.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/consumer.h>

#include <mach/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/at91sam9_smc.h>

#include "sam9_smc.h"
#include "generic.h"

#include <linux/usb/android_composite.h>
/*
 * board revision encoding
 * bit 0:
 * 	0 => 1 sd/mmc slot
 * 	1 => 2 sd/mmc slots connectors (board from revision C)
 */
#define HAVE_2MMC	(1 << 0)
static int inline ek_have_2mmc(void)
{
	return machine_is_at91sam9g20ek_2mmc() || (system_rev & HAVE_2MMC);
}


static void __init ek_map_io(void)
{
	/* Initialize processor: 18.432 MHz crystal */
	at91sam9260_initialize(18432000);

	/* DBGU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* USART0 on ttyS1. (Rx, Tx, CTS, RTS, DTR, DSR, DCD, RI) */
	at91_register_uart(AT91SAM9260_ID_US0, 1, ATMEL_UART_CTS | ATMEL_UART_RTS
			   | ATMEL_UART_DTR | ATMEL_UART_DSR | ATMEL_UART_DCD
			   | ATMEL_UART_RI);

	/* USART1 on ttyS2. (Rx, Tx, RTS, CTS) */
	at91_register_uart(AT91SAM9260_ID_US1, 2, ATMEL_UART_CTS | ATMEL_UART_RTS);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init ek_init_irq(void)
{
	at91sam9260_init_interrupts(NULL);
}


/*
 * USB Host port
 */
static struct at91_usbh_data __initdata ek_usbh_data = {
	.ports		= 2,
};

/*
 * USB Device port
 */
static struct at91_udc_data __initdata ek_udc_data = {
	.vbus_pin	= AT91_PIN_PC5,
	.pullup_pin	= 0,		/* pull-up driven by UDC */
};


/*
 * SPI devices.
 */
static struct spi_board_info ek_spi_devices[] = {
#if !(defined(CONFIG_MMC_ATMELMCI) || defined(CONFIG_MMC_AT91))
	{	/* DataFlash chip */
		.modalias	= "mtd_dataflash",
		.chip_select	= 1,
		.max_speed_hz	= 15 * 1000 * 1000,
		.bus_num	= 0,
	},
#if defined(CONFIG_MTD_AT91_DATAFLASH_CARD)
	{	/* DataFlash card */
		.modalias	= "mtd_dataflash",
		.chip_select	= 0,
		.max_speed_hz	= 15 * 1000 * 1000,
		.bus_num	= 0,
	},
#endif
#endif
};

static struct usba_platform_data __initdata ek_usba_udc_data;
/*
 * MACB Ethernet device
 */
static struct at91_eth_data __initdata ek_macb_data = {
	.phy_irq_pin	= AT91_PIN_PA7,
	.is_rmii	= 1,
};

static void __init ek_add_device_macb(void)
{
	if (ek_have_2mmc())
		ek_macb_data.phy_irq_pin = AT91_PIN_PB0;

	at91_add_device_eth(&ek_macb_data);
};

/*
 * NAND flash
 */
static struct mtd_partition __initdata ek_nand_partition[] = {
     {
         .name   = "Bootstrap",
         .offset = 0,
         .size   = 5 * 1024 * 1024,
     },
 
     {
         .name   = "system",
         .offset = 5 * 1024 * 1024,
         .size   = 95 * 1024 * 1024,
     },

     {
         .name   ="userdata",        /*for mtd@userdata*/
         .offset = 100 * 1024 * 1024,
         .size   = MTDPART_SIZ_FULL,

     },

};

static struct mtd_partition * __init nand_partitions(int size, int *num_partitions)
{
	*num_partitions = ARRAY_SIZE(ek_nand_partition);
	return ek_nand_partition;
}

/* det_pin is not connected */
static struct atmel_nand_data __initdata ek_nand_data = {
	.ale		= 21,
	.cle		= 22,
	.rdy_pin	= AT91_PIN_PC13,
	.enable_pin	= AT91_PIN_PC14,
	.partition_info	= nand_partitions,
#if defined(CONFIG_MTD_NAND_ATMEL_BUSWIDTH_16)
	.bus_width_16	= 1,
#else
	.bus_width_16	= 0,
#endif
};

static struct sam9_smc_config __initdata ek_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 2,
	.ncs_write_setup	= 0,
	.nwe_setup		= 2,

	.ncs_read_pulse		= 4,
	.nrd_pulse		= 4,
	.ncs_write_pulse	= 4,
	.nwe_pulse		= 4,

	.read_cycle		= 7,
	.write_cycle		= 7,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 3,
};

static void __init ek_add_device_nand(void)
{
	/* setup bus-width (8 or 16) */
	if (ek_nand_data.bus_width_16)
		ek_nand_smc_config.mode |= AT91_SMC_DBW_16;
	else
		ek_nand_smc_config.mode |= AT91_SMC_DBW_8;

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(3, &ek_nand_smc_config);

	at91_add_device_nand(&ek_nand_data);
}


/*
 * MCI (SD/MMC)
 * wp_pin and vcc_pin are not connected
 */
#if defined(CONFIG_MMC_ATMELMCI) || defined(CONFIG_MMC_ATMELMCI_MODULE)
static struct mci_platform_data __initdata ek_mmc_data = {
	.slot[1] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PC9,
	},

};
#else
static struct at91_mmc_data __initdata ek_mmc_data = {
	.slot_b		= 1,	/* Only one slot so use slot B */
	.wire4		= 1,
	.det_pin	= AT91_PIN_PC9,
};
#endif

static void __init ek_add_device_mmc(void)
{
#if defined(CONFIG_MMC_ATMELMCI) || defined(CONFIG_MMC_ATMELMCI_MODULE)
	if (ek_have_2mmc()) {
		ek_mmc_data.slot[0].bus_width = 4;
		ek_mmc_data.slot[0].detect_pin = AT91_PIN_PC2;
	}
	at91_add_device_mci(0, &ek_mmc_data);
#else
	at91_add_device_mmc(0, &ek_mmc_data);
#endif
}

/*
 *   *  Android ADB
 *     */
static char *usb_functions_ums[] = {
		"usb_mass_storage",
};

static char *usb_functions_ums_adb[] = {
		"usb_mass_storage",
		"adb",
};

static char *usb_functions_rndis[] = {
		"rndis",
};

static char *usb_functions_rndis_adb[] = {
		"rndis",
		"adb",
};

#ifdef CONFIG_USB_ANDROID_DIAG
static char *usb_functions_adb_diag[] = {
		"usb_mass_storage",
		"adb",
		"diag",
};
#endif

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
		"rndis",
#endif
		"usb_mass_storage",
		"adb",
#ifdef CONFIG_USB_ANDROID_ACM
		"acm",
#endif
#ifdef CONFIG_USB_ANDROID_DIAG
		"diag",
#endif
};

static struct android_usb_product usb_products[] = {
		{
			.product_id	= 0x6129,
			.num_functions	= ARRAY_SIZE(usb_functions_ums),
			.functions	= usb_functions_ums,
	    },
		{
			.product_id	= 0x6155,
			.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
			.functions	= usb_functions_ums_adb,
		},
    	{
			.product_id	= 0x6156,
			.num_functions	= ARRAY_SIZE(usb_functions_rndis),
			.functions	= usb_functions_rndis,
		},
		{
	    	.product_id	= 0x6157,
			.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
			.functions	= usb_functions_rndis_adb,
		},
#ifdef CONFIG_USB_ANDROID_DIAG
		{
			.product_id	= 0x6158,
			.num_functions	= ARRAY_SIZE(usb_functions_adb_diag),
			.functions	= usb_functions_adb_diag,
		},
#endif
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
		.nluns		= 1,
		.vendor		= "ATMEL",
		.product	= "SAM9G20",
		.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
		.name	= "usb_mass_storage",
		.id	= -1,
		.dev	= {
					.platform_data = &mass_storage_pdata,
				},
};

#ifdef CONFIG_USB_ANDROID_RNDIS
static struct usb_ether_platform_data rndis_pdata = {
		/* ethaddr is filled by board_serialno_setup */
		.vendorID	= 0x03EB,
		.vendorDescr	= "ATMEL",
};

static struct platform_device rndis_device = {
		.name	= "rndis",
	    .id	= -1,
		.dev	= {
		         	.platform_data = &rndis_pdata,
				},
};
#endif

static struct android_usb_platform_data android_usb_pdata = {
		.vendor_id	= 0x03EB,
		.product_id	= 0x6129,
		.version	= 0x0100,
		.product_name		= "SAM9X5",
		.manufacturer_name	= "ATMEL",
		.num_products = ARRAY_SIZE(usb_products),
		.products = usb_products,
		.num_functions = ARRAY_SIZE(usb_functions_all),
		.functions = usb_functions_all,
};

static struct platform_device android_usb_device = {
		.name	= "android_usb",
		.id		= -1,
		.dev		= {
				.platform_data = &android_usb_pdata,
				},
};

static void __init at91_usb_adb_init(void){
		platform_device_register(&android_usb_device);
		platform_device_register(&usb_mass_storage_device);
}

/*
 * LEDs
 */
static struct gpio_led ek_leds[] = {
	{	/* "bottom" led, green, userled1 to be defined */
		.name			= "ds5",
		.gpio			= AT91_PIN_PA6,
		.active_low		= 1,
		.default_trigger	= "none",
	},
	{	/* "power" led, yellow */
		.name			= "ds1",
		.gpio			= AT91_PIN_PA9,
		.default_trigger	= "heartbeat",
	}
};

static void __init ek_add_device_gpio_leds(void)
{
	if (ek_have_2mmc()) {
		ek_leds[0].gpio = AT91_PIN_PB8;
		ek_leds[1].gpio = AT91_PIN_PB9;
	}

	at91_gpio_leds(ek_leds, ARRAY_SIZE(ek_leds));
}

/*
 * GPIO Buttons
 */
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button ek_buttons[] = {
	{
		.gpio		= AT91_PIN_PA30,
		.code		= BTN_3,
		.desc		= "Button 3",
		.active_low	= 1,
		.wakeup		= 1,
	},
	{
		.gpio		= AT91_PIN_PA31,
		.code		= BTN_4,
		.desc		= "Button 4",
		.active_low	= 1,
		.wakeup		= 1,
	}
};

static struct gpio_keys_platform_data ek_button_data = {
	.buttons	= ek_buttons,
	.nbuttons	= ARRAY_SIZE(ek_buttons),
};

static struct platform_device ek_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &ek_button_data,
	}
};

static void __init ek_add_device_buttons(void)
{
	at91_set_gpio_input(AT91_PIN_PA30, 1);	/* btn3 */
	at91_set_deglitch(AT91_PIN_PA30, 1);
	at91_set_gpio_input(AT91_PIN_PA31, 1);	/* btn4 */
	at91_set_deglitch(AT91_PIN_PA31, 1);

	platform_device_register(&ek_button_device);
}
#else
static void __init ek_add_device_buttons(void) {}
#endif

#if defined(CONFIG_REGULATOR_FIXED_VOLTAGE) || defined(CONFIG_REGULATOR_FIXED_VOLTAGE_MODULE)
static struct regulator_consumer_supply ek_audio_consumer_supplies[] = {
	REGULATOR_SUPPLY("AVDD", "0-001b"),
	REGULATOR_SUPPLY("HPVDD", "0-001b"),
	REGULATOR_SUPPLY("DBVDD", "0-001b"),
	REGULATOR_SUPPLY("DCVDD", "0-001b"),
};

static struct regulator_init_data ek_avdd_reg_init_data = {
	.constraints	= {
		.name	= "3V3",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.consumer_supplies = ek_audio_consumer_supplies,
	.num_consumer_supplies = ARRAY_SIZE(ek_audio_consumer_supplies),
};

static struct fixed_voltage_config ek_vdd_pdata = {
	.supply_name	= "board-3V3",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.enabled_at_boot = 0,
	.init_data	= &ek_avdd_reg_init_data,
};
static struct platform_device ek_voltage_regulator = {
	.name		= "reg-fixed-voltage",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &ek_vdd_pdata,
	},
};
static void __init ek_add_regulators(void)
{
	platform_device_register(&ek_voltage_regulator);
}
#else
static void __init ek_add_regulators(void) {}
#endif


static struct i2c_board_info __initdata ek_i2c_devices[] = {
        {
                I2C_BOARD_INFO("24c512", 0x50)
        },
        {
                I2C_BOARD_INFO("wm8731", 0x1b)
        },
};


static void __init ek_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* USB Host */
	at91_add_device_usbh(&ek_usbh_data);
	/* USB Device */
	at91_add_device_udc(&ek_udc_data);
	/* SPI */
	at91_add_device_spi(ek_spi_devices, ARRAY_SIZE(ek_spi_devices));
	/* NAND */
	ek_add_device_nand();
	/* Ethernet */
	ek_add_device_macb();
	/* Regulators */
	ek_add_regulators();
	/* MMC */
	ek_add_device_mmc();
	/* I2C */
	at91_add_device_i2c(ek_i2c_devices, ARRAY_SIZE(ek_i2c_devices));
	/* LEDs */
	ek_add_device_gpio_leds();
	/* Push Buttons */
	ek_add_device_buttons();
	/* PCK0 provides MCLK to the WM8731 */
	at91_set_B_periph(AT91_PIN_PC1, 0);
	/* SSC (for WM8731) */
	at91_add_device_ssc(AT91SAM9260_ID_SSC, ATMEL_SSC_TX);
		/*usb adb*/
	at91_usb_adb_init();
}

MACHINE_START(AT91SAM9G20EK, "Atmel AT91SAM9G20-EK")
	/* Maintainer: Atmel */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= ek_map_io,
	.init_irq	= ek_init_irq,
	.init_machine	= ek_board_init,
MACHINE_END

MACHINE_START(AT91SAM9G20EK_2MMC, "Atmel AT91SAM9G20-EK 2 MMC Slot Mod")
	/* Maintainer: Atmel */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= ek_map_io,
	.init_irq	= ek_init_irq,
	.init_machine	= ek_board_init,
MACHINE_END
