/* USB Serial interface
 * This module inspired by: 
 * 		*libopencm3/libopencm3-examples usbcdc.c
 * 			by Gareth McMullin <gareth@blacksphere.co.nz>
 * 			Warren W. Gay VE3WWG
 *
 * GPIO
 * ----
 * PA11		USB_DM
 * PA12		USB_DP
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>


#include "usbhid.h"



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

const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &hid_iface,
}};

const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0xC0,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char * usb_strings[] = {
	"usbHID DViejo",
	"stm32 joystick",
	"dvh-000-001",
};

// Buffer to be used for control requests.
static uint8_t usbd_control_buffer[128];

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



static void hid_set_config(usbd_device *dev, uint16_t wValue __attribute((unused)))
{
//	(void)dev;

	usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, PACKET_SIZE, NULL);

	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				hid_control_request);

    initialized = true;
}

/*
 * USB Driver task:
 * Listen to joystick queue. When data is ready, send it to the host
 */
static void
usb_task(void *arg) {
	QueueHandle_t *js_txq = (QueueHandle_t *)arg;
	char txbuf[PACKET_SIZE];
	unsigned txlen = 0;

	for (;;) {
		usbd_poll(usbd_dev);			/* Allow driver to do it's thing */
		if ( initialized ) {
			while ( txlen < PACKET_SIZE //sizeof txbuf
			   && xQueueReceive(*js_txq, &txbuf[txlen],0) == pdPASS ) //joystick_txq
				++txlen;		/* Read data to be sent */
			
			if ( txlen > 0 ) {
				if ( usbd_ep_write_packet(usbd_dev,0x81,txbuf,txlen) != 0 ) //0x82
					txlen = 0;	/* Reset if data sent ok */
			} else	{
				taskYIELD();		/* Then give up CPU */
			}
		}
	}
}


/*
 * Start USB driver:
 */
void
usbhid_start(QueueHandle_t *joystick_txq) {


	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USB);
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

	// PA11=USB_DM, PA12=USB_DP
	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver,&dev_descr,&config,
		usb_strings,3,
		usbd_control_buffer,sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usbd_dev,hid_set_config);


	xTaskCreate(usb_task,"USB",200,joystick_txq,configMAX_PRIORITIES-1,NULL);
}

/*
 * Return True if the USB connection + driver initialized and ready.
 */
bool
usbhid_ready(void) {
	return initialized;
}

// End usbcdc.c 
