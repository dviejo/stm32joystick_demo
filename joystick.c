/**
 * joystick.c
 * 
 * Created by Diego Viejo
 * 06/May/2020
 * 
 */

#include "joystick.h"
#include "usbhid.h"



int buildAndSet16BitValue(int16_t value, int16_t valueMinimum, int16_t valueMaximum, int16_t actualMinimum, int16_t actualMaximum, uint8_t dataLocation[]);
int buildAndSetAxisValue(int16_t axisValue, int16_t axisMinimum, int16_t axisMaximum, uint8_t dataLocation[]);

/**
 * Joystick_ start
 * 
 * Gives initial values for the joystick
 * 
 */
void Joystick_start(struct Joystick_ *js, QueueHandle_t *queue)
{
    //joystick state
	js->buttons = 0;
	js->xAxis = 0;
	js->yAxis = 0;
	js->zAxis = 0;
	js->wheel = 0;
	js->gas = 0;
	js->brake = 0;

    //joystick limits
	js->_xAxisMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_xAxisMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;
	js->_yAxisMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_yAxisMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;
	js->_zAxisMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_zAxisMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;
	js->_acceleratorMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_acceleratorMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;
	js->_brakeMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_brakeMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;
	js->_steeringMinimum = JOYSTICK_DEFAULT_AXIS_MINIMUM;
	js->_steeringMaximum = JOYSTICK_DEFAULT_AXIS_MAXIMUM;

	js->js_txq = queue;

}

int buildAndSet16BitValue(int16_t value, int16_t valueMinimum, int16_t valueMaximum, int16_t actualMinimum, int16_t actualMaximum, uint8_t dataLocation[])
{
	int16_t convertedValue;
	uint8_t highByte;
	uint8_t lowByte;
	int16_t realMinimum = valueMinimum<=valueMaximum?valueMinimum:valueMaximum; // min(valueMinimum, valueMaximum);
	int16_t realMaximum = valueMinimum<=valueMaximum?valueMaximum:valueMinimum; //max(valueMinimum, valueMaximum);


	if (value < realMinimum) {
		value = realMinimum;
	}
	if (value > realMaximum) {
		value = realMaximum;
	}

	if (valueMinimum > valueMaximum) {
		// Values go from a larger number to a smaller number (e.g. 1024 to 0)
		value = realMaximum - value + realMinimum;
	}

	convertedValue = (value-realMinimum) * (actualMaximum - actualMinimum +1) / (realMaximum-realMinimum+1) + actualMinimum; //map(value, realMinimum, realMaximum, actualMinimum, actualMaximum);

	highByte = (uint8_t)(convertedValue >> 8);
	lowByte = (uint8_t)(convertedValue & 0x00FF);
	
	dataLocation[0] = lowByte;
	dataLocation[1] = highByte;
	
	return 2;
}

int buildAndSetAxisValue(int16_t axisValue, int16_t axisMinimum, int16_t axisMaximum, uint8_t dataLocation[])
{
    return buildAndSet16BitValue(axisValue, axisMinimum, axisMaximum, JOYSTICK_AXIS_MINIMUM, JOYSTICK_AXIS_MAXIMUM, dataLocation);
}


void Joystick_setXAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_xAxisMinimum = minimum;
	js->_xAxisMaximum = maximum;
}

void Joystick_setYAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_yAxisMinimum = minimum;
	js->_yAxisMaximum = maximum;
}

void Joystick_setZAxisRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_zAxisMinimum = minimum;
	js->_zAxisMaximum = maximum;
}

void Joystick_setAcceleratorRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_acceleratorMinimum = minimum;
	js->_acceleratorMaximum = maximum;
}

void Joystick_setBrakeRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_brakeMinimum = minimum;
	js->_brakeMaximum = maximum;

}

void Joystick_setSteeringRange(struct Joystick_ *js, int16_t minimum, int16_t maximum)
{
	js->_steeringMinimum = minimum;
	js->_steeringMaximum = maximum;
}

void Joystick_sendState(struct Joystick_ *js)
{
	uint8_t data[PACKET_SIZE];
	int index = 0;
    int x;
	
    data[index] = JOYSTICK_DEFAULT_REPORT_ID;
    index++;
	// Load Button State
	data[index] = js->buttons;
    index++;

	// Set Axis Values
	index += buildAndSetAxisValue(js->xAxis, js->_xAxisMinimum, js->_xAxisMaximum, &(data[index]));
	index += buildAndSetAxisValue(js->yAxis, js->_yAxisMinimum, js->_yAxisMaximum, &(data[index]));
	index += buildAndSetAxisValue(js->zAxis, js->_zAxisMinimum, js->_zAxisMaximum, &(data[index]));
	
	// Set Simulation Values
	index += buildAndSetAxisValue(js->gas, js->_acceleratorMinimum, js->_acceleratorMaximum, &(data[index]));
	index += buildAndSetAxisValue(js->brake, js->_brakeMinimum, js->_brakeMaximum, &(data[index]));
	index += buildAndSetAxisValue(js->wheel, js->_steeringMinimum, js->_steeringMaximum, &(data[index]));

    if(usbhid_ready() && uxQueueSpacesAvailable(*(js->js_txq)) >= PACKET_SIZE)
	    for ( x=0; x<index; ++x ) 
        {
		    // Send data to the tx queue
		    xQueueSend(*(js->js_txq),&data[x],0);
	    }

}

void Joystick_setXAxis(struct Joystick_ *js, int16_t value)
{
	js->xAxis = value;
	Joystick_sendState(js);
}

void Joystick_setYAxis(struct Joystick_ *js, int16_t value)
{
	js->yAxis = value;
	Joystick_sendState(js);
}

void Joystick_setZAxis(struct Joystick_ *js, int16_t value)
{
	js->zAxis = value;
	Joystick_sendState(js);
}

void Joystick_setAccelerator(struct Joystick_ *js, int16_t value)
{
	js->gas = value;
	Joystick_sendState(js);
}

void Joystick_setBrake(struct Joystick_ *js, int16_t value)
{
	js->brake = value;
	Joystick_sendState(js);
}

void Joystick_setSteering(struct Joystick_ *js, int16_t value)
{
    js->wheel = value;
	Joystick_sendState(js);
}

void Joystick_setButton(struct Joystick_ *js, uint8_t button, uint8_t value)
{
	if (value == 0)
	{
		Joystick_releaseButton(js, button);
	}
	else
	{
		Joystick_pressButton(js, button);
	}
}

void Joystick_pressButton(struct Joystick_ *js, uint8_t button)
{
    if (button >= JOYSTICK_DEFAULT_BUTTON_COUNT) return;

    int bit = button % 8;

	js->buttons |= (0x01<<bit);
	Joystick_sendState(js);
}

void Joystick_releaseButton(struct Joystick_ *js, uint8_t button)
{
    if (button >= JOYSTICK_DEFAULT_BUTTON_COUNT) return;

    int bit = button % 8;

    js->buttons &= ~(0x01<<bit);

	Joystick_sendState(js);
}

void Joystick_setButtons(struct Joystick_ *js, uint8_t btns)
{
    js->buttons = btns;
    Joystick_sendState(js);
}

