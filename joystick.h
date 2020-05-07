/**
 * joystick.h
 * 
 * structures and functions to handle joystick events
 * 
 * Created by Diego Viejo
 * 6/May/2020
 * 
 */

#ifndef __JOYSTICK__C__
#define __JOYSTICK__C__

#include <libopencm3/stm32/rcc.h>
#include <FreeRTOS.h>
#include <queue.h>

#include <stdbool.h>

#define JOYSTICK_DEFAULT_REPORT_ID         0x03
#define JOYSTICK_DEFAULT_BUTTON_COUNT         8
#define JOYSTICK_DEFAULT_AXIS_MINIMUM         0
#define JOYSTICK_DEFAULT_AXIS_MAXIMUM      4095
#define JOYSTICK_AXIS_MINIMUM -32767
#define JOYSTICK_AXIS_MAXIMUM 32767

#define _HIDREPORTSIZE 14

struct Joystick_
{
    //joystick state
	uint8_t buttons;
	int16_t xAxis;
	int16_t yAxis;
	int16_t zAxis;
	int16_t wheel;
	int16_t gas;
	int16_t brake;

    //joystick limits
	int16_t                  _xAxisMinimum;
	int16_t                  _xAxisMaximum;
	int16_t                  _yAxisMinimum;
	int16_t                  _yAxisMaximum;
	int16_t                  _zAxisMinimum;
	int16_t                  _zAxisMaximum;
	int16_t                  _acceleratorMinimum;
	int16_t                  _acceleratorMaximum;
	int16_t                  _brakeMinimum;
	int16_t                  _brakeMaximum;
	int16_t                  _steeringMinimum;
	int16_t                  _steeringMaximum;

	QueueHandle_t *js_txq;

};

void Joystick_start(struct Joystick_ *js, QueueHandle_t *queue);
void Joystick_setXAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);
void Joystick_setYAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);
void Joystick_setZAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);
void Joystick_setAcceleratorRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);
void Joystick_setBrakeRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);
void Joystick_setSteeringRange(struct Joystick_ *js, int16_t minimum, int16_t maximum);

void Joystick_sendState(struct Joystick_ *js);

void Joystick_setXAxis(struct Joystick_ *js, int16_t value);
void Joystick_setYAxis(struct Joystick_ *js, int16_t value);
void Joystick_setZAxis(struct Joystick_ *js, int16_t value);
void Joystick_setAccelerator(struct Joystick_ *js, int16_t value);
void Joystick_setBrake(struct Joystick_ *js, int16_t value);
void Joystick_setSteering(struct Joystick_ *js, int16_t value);

void Joystick_setButton(struct Joystick_ *js, uint8_t button, uint8_t value);
void Joystick_pressButton(struct Joystick_ *js, uint8_t button);
void Joystick_releaseButton(struct Joystick_ *js, uint8_t button);
void Joystick_setButtons(struct Joystick_ *js, uint8_t btns);

#endif