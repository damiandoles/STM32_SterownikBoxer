/*
 * boxer_climate.c
 *
 *  Created on: 29 lip 2015
 *      Author: Doles
 */

#include "boxer_climate.h"
#include "boxer_timers.h"
#include "boxer_display.h"
#include "boxer_light.h"
#include "hardware/TSL2561/tsl2561.h"

static systime_t measureOwireTimer 	= 0;
static systime_t i2cMeasTimer 		= 0;
static bool_t oneWireResetDone 		= FALSE;

ErrorStatus errorSht = SUCCESS;
ErrorStatus errorTsl = SUCCESS;
uint8_t errorDsUp 	 = 1; 		//presence
uint8_t errorDsDown  = 1;		//presence

float lastTempUp 		= 0;
float lastTempDown 		= 0;
float lastTempMiddle 	= 0;
float lastHumidity 		= 0;
uint32_t lastLux 		= 0;
/////////////////////////////////////////////////////////////////////////////
void Climate_SensorsHandler(void)
{
	if (oneWireResetDone == FALSE)
	{
	#ifndef OWIRE_OFF_MODE
		errorDsUp   = initializeConversion(&sensorTempUp);
		errorDsDown = initializeConversion(&sensorTempDown);
	#endif
		oneWireResetDone = TRUE;

#ifdef ONE_WIRE_LOGS
		if (errorDsUp == 0)
		{
			_error("ds up init error");
		}
		else
		{
			_printString("ds up init ok\r\n");
		}

		if (errorDsDown == 0)
		{
			_error("ds down init error");
		}
		else
		{
			_printString("ds dpwn init ok\r\n");
		}
#endif
	}

	if (oneWireResetDone == TRUE)
	{
		if (systimeTimeoutControl(&measureOwireTimer, 1000))
		{
#ifndef OWIRE_OFF_MODE
			errorDsUp = readTemperature(&sensorTempUp);
			lastTempUp = displayData.temp_up_t;
			displayData.temp_up_t = sensorTempUp.fTemp;

			errorDsDown = readTemperature(&sensorTempDown);
			lastTempDown = displayData.temp_down_t;
			displayData.temp_down_t = sensorTempDown.fTemp;
#endif

#ifdef ONE_WIRE_LOGS
			char tempString[5] = {0};
			if (errorDsUp == 0)
			{
				_error("ds up measure error");
			}
			else
			{
				ftoa(displayData.temp_up_t, tempString, 1);
				_printString(tempString);
				_printString("\r\n");
			}

			if (errorDsDown == 0)
			{
				_error("ds down measure error");
			}
			else
			{
				ftoa(displayData.temp_down_t, tempString, 1);
				_printString(tempString);
				_printString("\r\n");
			}
#endif
			oneWireResetDone = FALSE;
		}
	}
////////////////////////////////////////////////////////////////////////////////////////////////////
	if (systimeTimeoutControl(&i2cMeasTimer, 5000))
	{
#ifndef I2C_OFF_MODE
		errorSht = SHT21_SoftReset(I2C2, SHT21_ADDR);
#ifdef I2C2_LOGS
		if (errorSht == ERROR)
		{
			_error("SHT21 reset error (loop)");
		}
		else
		{
			_printString("SHT21 reset ok (loop)\r\n");
		}
#endif
		uint16_t tempWord = 0;
		uint16_t humWord = 0;

		systimeDelayMs(25); //datasheet
		tempWord = SHT21_MeasureTempCommand(I2C2, SHT21_ADDR, &errorSht);
#ifdef I2C2_LOGS
		if (errorSht == ERROR)
		{
			_error("SHT21 meas temp error (loop)");
		}
		else
		{
			_printString("SHT21 meas temp ok (loop)\r\n");
		}
#endif
		humWord = SHT21_MeasureHumCommand(I2C2, SHT21_ADDR, &errorSht);
#ifdef I2C2_LOGS
		if (errorSht == ERROR)
		{
			_error("SHT21 meas hum error (loop)");
		}
		else
		{
			_printString("SHT21 meas hum ok (loop)\r\n");
		}
#endif
		humWord = ((uint16_t)(SHT_HumData.msb_lsb[0]) << 8) | SHT_HumData.msb_lsb[1];
		tempWord = ((uint16_t)(SHT_TempData.msb_lsb[0]) << 8) | SHT_TempData.msb_lsb[1];

		lastTempMiddle = displayData.temp_middle_t;
		displayData.temp_middle_t = SHT21_CalcTemp(tempWord);

		lastHumidity = displayData.humiditySHT2x;
		displayData.humiditySHT2x = SHT21_CalcRH(humWord);
///////////////////////////////////////////////////////////////////////////////////////////////
		lastLux = displayData.lux;
		errorTsl = TSL2561_ReadLux(&displayData.lux);
#ifdef I2C2_LOGS
		if (errorTsl == ERROR)
		{
			_error("TSL2561 read lux error (loop)");
		}
		else
		{
			_printString("TSL2561 read lux ok (loop)\r\n");
		}
#endif
#endif
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Climate_TempCtrl_Handler(void)
{
	if (systickIrq)
	{
		if (softStartDone == TRUE)
		{
			switch (xTempControl.tempCtrlMode)
			{
			case TEMP_AUTO:
				switch (xLightControl.lightingState)
				{
				case LIGHT_ON:
					if (sensorTempUp.fTemp > (float)xTempControl.userTemp)
					{
						PWM_IncPercentTo(PWM_FAN_PULL_AIR, 100);//wyciagajacy
						lastPullPWM = 100;
						PWM_IncPercentTo(PWM_FAN_PUSH_AIR, 70);	//wciagajacy
						lastPushPWM = 70;
					}
					else
					{
						PWM_DecPercentTo(PWM_FAN_PULL_AIR, 60);
						lastPullPWM = 60;
						PWM_DecPercentTo(PWM_FAN_PUSH_AIR, 30);
						lastPushPWM = 30;
					}
					break;

				case LIGHT_OFF:
					if (lastPullPWM > 40)
					{
						if (PWM_DecPercentTo(PWM_FAN_PULL_AIR, 40))
						{
							lastPullPWM = 40;
						}
					}
					else
					{
						if (PWM_IncPercentTo(PWM_FAN_PULL_AIR, 40))
						{
							lastPullPWM = 40;
						}
					}

					if (lastPushPWM > 30)
					{
						if (PWM_DecPercentTo(PWM_FAN_PUSH_AIR, 30))
						{
							lastPushPWM = 30;
						}
					}
					else
					{
						if (PWM_IncPercentTo(PWM_FAN_PUSH_AIR, 30))
						{
							lastPushPWM = 30;
						}
					}
					break;

				default:
					break;
				}
				break;

			case TEMP_MANUAL:
				if (lastPullPWM != xTempControl.fanPull)
				{
					if (lastPullPWM > xTempControl.fanPull)
					{
						if (PWM_DecPercentTo(PWM_FAN_PULL_AIR, xTempControl.fanPull))
						{
							lastPullPWM = xTempControl.fanPull;
						}
					}
					else
					{
						if (PWM_IncPercentTo(PWM_FAN_PULL_AIR, xTempControl.fanPull))
						{
							lastPullPWM = xTempControl.fanPull;
						}
					}
				}

				if (lastPushPWM != xTempControl.fanPush)
				{
					if (lastPushPWM > xTempControl.fanPush)
					{
						if (PWM_DecPercentTo(PWM_FAN_PUSH_AIR, xTempControl.fanPush))
						{
							lastPushPWM = xTempControl.fanPush;
						}
					}
					else
					{
						if (PWM_IncPercentTo(PWM_FAN_PUSH_AIR, xTempControl.fanPush))
						{
							lastPushPWM = xTempControl.fanPush;
						}
					}
				}
				break;

			default:
				break;
			}
		}

		systickIrq = 0;
	}
}
