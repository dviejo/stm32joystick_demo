/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>

/* Define this to include the DFU APP interface. */
//#define INCLUDE_DFU_INTERFACE

#ifdef INCLUDE_DFU_INTERFACE
#include <libopencm3/cm3/scb.h>
#include <libopencm3/usb/dfu.h>
#endif

#define PACKET_SIZE 14

// True when USB configured:
static volatile bool initialized = false;

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5710,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};



static const uint8_t hid_report_descriptor[] = {
  0x05, 0x01, // USAGE_PAGE (Generic Desktop)
  0x09, 0x08, //JOYSTICK_TYPE_MULTI_AXIS, // USAGE (Multi axis)
  0xA1, 0x01, // COLLECTION (Application)
	//================================Input Report======================================//
  	// WheelReport
  	0x85, 0x03, //JOYSTICK_DEFAULT_REPORT_ID, // REPORT_ID (default 3)
  	//0xA1, 0x00, // COLLECTION (Physical)
  	0x05, 0x09, // USAGE_PAGE  (Button)
  	0x19, 0x01, // USAGE_MINIMUM (Button 1)
  	0x29, 0x08, // USAGE_MAXIMUM (Button 8)
  	0x15, 0x00, // LOGICAL_MINIMUM (0)
  	0x25, 0x01, // LOGICAL_MAXIMUM (1)
  	0x75, 0x01, // REPORT_SIZE (1)
  	0x95, 0x08, // REPORT_COUNT (8)
  	0x55, 0x00, // UNIT_EXPONENT (0)
  	0x65, 0x00, // UNIT (None)
  	0x81, 0x02, //INPUT (Data,Var,Abs)

  	//3 Axis: X, Y, Z
  	0x05, 0x01, // USAGE_PAGE (Generic Desktop)
  	0x09, 0x01, // USAGE (Pointer)
  	0x16, 0x01, 0x80, //LOGICAL_MINIMUM (-32768)
  	0x26, 0xFF, 0x7F, //LOGICAL_MAXIMUM (32767)
  	0x75, 0x10, // REPORT_SIZE (16)
  	0x95, 0x03, // REPORT_COUNT (3)
  	0xA1, 0x00, // COLLECTION (Physical)
  		0x09, 0x30, // USAGE (X)
  		0x09, 0x31, // USAGE (Y)
  		0x09, 0x32, // USAGE (Z)
  		0x81, 0x02, // INPUT (Data,Var,Abs)
  	0xc0, // END_COLLECTION (Physical)

  	//simulation 3 Axis
  	0x05, 0x02, // USAGE_PAGE (Simulation Controls)
  	0x16, 0x01, 0x80, //LOGICAL_MINIMUM (-32768)
  	0x26, 0xFF, 0x7F, //LOGICAL_MAXIMUM (32767)
  	0x75, 0x10, // REPORT_SIZE (16)
  	0x95, 0x03, // REPORT_COUNT (3)
  	0xA1, 0x00, // COLLECTION (Physical)
  		0x09, 0xC4, // USAGE (Accelerator)
  		0x09, 0xC5, // USAGE (Brake)
  		0x09, 0xC8, // USAGE (Steering)
  		0x81, 0x02, // INPUT (Data,Var,Abs)
  	0xc0, // END_COLLECTION 

  0xC0, // END COLLECTION ()
};


static const struct {
	struct usb_hid_descriptor hid_descriptor;
	struct {
		uint8_t bReportDescriptorType;
		uint16_t wDescriptorLength;
	} __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function = {
	.hid_descriptor = {
		.bLength = sizeof(hid_function),
		.bDescriptorType = USB_DT_HID,
		.bcdHID = 0x0100,
		.bCountryCode = 0,
		.bNumDescriptors = 1,
	},
	.hid_report = {
		.bReportDescriptorType = USB_DT_REPORT,
		.wDescriptorLength = sizeof(hid_report_descriptor),
	}
};

const struct usb_endpoint_descriptor hid_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x81,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = PACKET_SIZE, //4,
	.bInterval = 0x20,
};

const struct usb_interface_descriptor hid_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = 1, /* boot */
	.bInterfaceProtocol = 2, /* mouse */
	.iInterface = 0,

	.endpoint = &hid_endpoint,

	.extra = &hid_function,
	.extralen = sizeof(hid_function),
};

#ifdef INCLUDE_DFU_INTERFACE
const struct usb_dfu_descriptor dfu_function = {
	.bLength = sizeof(struct usb_dfu_descriptor),
	.bDescriptorType = DFU_FUNCTIONAL,
	.bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
	.wDetachTimeout = 255,
	.wTransferSize = 1024,
	.bcdDFUVersion = 0x011A,
};

const struct usb_interface_descriptor dfu_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = 0xFE,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 1,
	.iInterface = 0,

	.extra = &dfu_function,
	.extralen = sizeof(dfu_function),
};
#endif

const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &hid_iface,
#ifdef INCLUDE_DFU_INTERFACE
}, {
	.num_altsetting = 1,
	.altsetting = &dfu_iface,
#endif
}};

const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
#ifdef INCLUDE_DFU_INTERFACE
	.bNumInterfaces = 2,
#else
	.bNumInterfaces = 1,
#endif
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0xC0,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"Black Sphere Technologies",
	"HID Demo",
	"DEMO",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes hid_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)complete;
	(void)dev;

	if((req->bmRequestType != 0x81) ||
	   (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
	   (req->wValue != 0x2200))
		return USBD_REQ_NOTSUPP;

	/* Handle the HID report descriptor. */
	*buf = (uint8_t *)hid_report_descriptor;
	*len = sizeof(hid_report_descriptor);

	return USBD_REQ_HANDLED;
}

#ifdef INCLUDE_DFU_INTERFACE
static void dfu_detach_complete(usbd_device *dev, struct usb_setup_data *req)
{
	(void)req;
	(void)dev;

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO10);
	gpio_set(GPIOA, GPIO10);
	scb_reset_core();
}

static enum usbd_request_return_codes dfu_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)buf;
	(void)len;
	(void)dev;

	if ((req->bmRequestType != 0x21) || (req->bRequest != DFU_DETACH))
		return USBD_REQ_NOTSUPP; /* Only accept class request. */

	*complete = dfu_detach_complete;

	return USBD_REQ_HANDLED;
}
#endif

static void hid_set_config(usbd_device *dev, uint16_t wValue)
{
	(void)wValue;
	(void)dev;

	usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, PACKET_SIZE, NULL);

	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				hid_control_request);
#ifdef INCLUDE_DFU_INTERFACE
	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				dfu_control_request);
#endif

    initialized = true;

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	/* SysTick interrupt every N clock pulses: set reload to N-1 */
	systick_set_reload(99999);
	systick_interrupt_enable();
	systick_counter_enable();
}

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	rcc_periph_clock_enable(RCC_GPIOA);
	/*
	 * This is a somewhat common cheap hack to trigger device re-enumeration
	 * on startup.  Assuming a fixed external pullup on D+, (For USB-FS)
	 * setting the pin to output, and driving it explicitly low effectively
	 * "removes" the pullup.  The subsequent USB init will "take over" the
	 * pin, and it will appear as a proper pullup to the host.
	 * The magic delay is somewhat arbitrary, no guarantees on USBIF
	 * compliance here, but "it works" in most places.
	 */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	for (unsigned i = 0; i < 800000; i++) {
		__asm__("nop");
	}

	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, hid_set_config);

	while (1)
		usbd_poll(usbd_dev);
}

#define SIZE 14

void sys_tick_handler(void)
{
	static int butt = 1;
    static int dir = 1;
    static int xpos = -500;
	static int zpos = 32767;
	uint8_t buf[SIZE];
    int cont;
    for(cont=0;cont<SIZE;cont++)
    {
        buf[cont] = 0;
    }
    
    buf[0] = 0x03; //joystick report id
	buf[1] = butt;
    butt = butt<<1;
    if(!(butt&0xFF))
        butt = 1;

    buf[3] = xpos>>8;
    buf[2] = xpos & 0x00FF;
    buf[5] = xpos>>8;
    buf[4] = xpos & 0x00FF;
    xpos += dir;
    if(xpos>32767)
    {
        xpos = 32767;
        dir = -1;
    }
    if(xpos<-32767)
    {
        xpos = -32767;
        dir = 1;
    }
	buf[6] = zpos & 0x00FF;
	buf[7] = zpos>>8;
	buf[8] = -zpos & 0x00FF;
	buf[9] = -zpos>>8;
	buf[10] = zpos & 0x00FF;
	buf[11] = zpos>>8;
	buf[12] = -zpos & 0x00FF;
	buf[13] = -zpos>>8;

    if(initialized)
    	usbd_ep_write_packet(usbd_dev, 0x81, buf, SIZE);
}