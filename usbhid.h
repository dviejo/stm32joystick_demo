#ifndef LIBUSBCDC_H
#define LIBUSBCDC_H

#define PACKET_SIZE 14

void usbhid_start(QueueHandle_t *joystick_txq);
bool usbhid_ready(void);


#endif /* LIBUSBCDC_H */
