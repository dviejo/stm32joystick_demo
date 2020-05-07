# stm32f103C8 joystick_demo

## Demo program
  Created by Diego Viejo dviejo@ua.es for educational purposes
  on May the fourth 2020 ;)
 
 The libopencm3 code is released under the terms of the GNU Lesser General Public License (LGPL), 
 	version 3 or later.
 
 Uses libOpenCM3 HAL Library and FreeRTOS
	Thanks to Warren Gay and his "Beginning STM32: Developing with FreeRTOS, libopencm3 and GCC"
 
 I've picked up ideas from these (thanks a lot for sharing your knowledge):
    https://satoshinm.github.io/blog/171227_stm32hid_pill_duck_scriptable_usb_hid_device_using_an_stm32_blue_pill_from_mouse_jigglers_to_rubber_duckies.html
    https://hackaday.io/project/163715-wii-nunchuk-as-a-usb-hid-controller/log/159261-first-steps-with-stm32-usb-firmware
 
 
 The bluepill is configured as a multiaxis joystick with:
    8 buttons
    6 axis: xAxis, yAxis, zAxis, Accelerator, Brake, Steering
  
    Buttons are pressed every 500ms in a sequence, then released in a sequence too, and so on.
    xAxis is moved back and forth
  
  This firmware has been flashed in a stm32f103C8 Bluepill device and tested using linux jstest program.
  
 
