/*
 * boxer_timers.c
 *
 *  Created on: 3 sie 2015
 *      Author: Doles
 */

#include "boxer_timers.h"
#include "boxer_communication.h"
#include "boxer_display.h"
#include "boxer_light.h"
#include "boxer_climate.h"
#include "boxer_irrigation.h"
#include "boxer_ph.h"
#include "boxer_datastorage.h"
#include "pcf8563.h"

#define PUMP_PIN				GPIO_Pin_1
#define TIMER_PRESCALER			48

static systime_t oneSecTimer 	= 0;
static uint16_t TimerPeriod 	= 0;
bool_t initFanPwm 				= FALSE;
bool_t softStartDone 			= FALSE;

static uint32_t PWM_PercentToRegister(uint8_t xPercent);
static uint8_t PWM_FANSoftStart(void);
#ifdef CALIB_DEBUG
static uint8_t calibDbgCounter = 0;
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t PWM_DecPercentTo(pwm_dev_type_t xPwmDev, uint8_t xPercent)
{
	uint8_t ret = 0;
	uint8_t power = 100 - xPercent;

	switch (xPwmDev)
	{
		case PWM_PUMP:
			if (REG_PWM_PUMP < PWM_PercentToRegister(power))
			{
				REG_PWM_PUMP += 1;
			}
			else
			{
				ret = 1;
			}
			break;

		case PWM_FAN_PULL_AIR:
			if (REG_PWM_PULL_AIR_FAN < PWM_PercentToRegister(power))
			{
				REG_PWM_PULL_AIR_FAN += 1;
			}
			else
			{
				ret = 1;
			}
			break;

		case PWM_FAN_PUSH_AIR:
			if (REG_PWM_PUSH_AIR_FAN < PWM_PercentToRegister(power))
			{
				REG_PWM_PUSH_AIR_FAN += 1;
			}
			else
			{
				ret = 1;
			}
			break;

		default:
			break;
	}

	return ret;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t PWM_IncPercentTo(pwm_dev_type_t xPwmDev, uint8_t xPercent)
{
	uint8_t ret = 0;
	uint8_t power = 100 - xPercent;
	switch (xPwmDev)
	{
		case PWM_PUMP:
			if (REG_PWM_PUMP > PWM_PercentToRegister(power))
			{
				REG_PWM_PUMP -= 1;
			}
			else
			{
				ret = 1;
			}
			break;

		case PWM_FAN_PULL_AIR:
			if (REG_PWM_PULL_AIR_FAN > PWM_PercentToRegister(power))
			{

				REG_PWM_PULL_AIR_FAN -= 1;
			}
			else
			{
				ret = 1;
			}
			break;

		case PWM_FAN_PUSH_AIR:
			if (REG_PWM_PUSH_AIR_FAN > PWM_PercentToRegister(power))
			{
				REG_PWM_PUSH_AIR_FAN -= 1;
			}
			else
			{
				ret = 1;
			}
			break;
		default:
			break;
	}
	return ret;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static uint32_t PWM_PercentToRegister(uint8_t xPercent)
{
	/*
    Aby otrzymac prawidlowa wartosc rejestru CCR nalezy skorzystac z ponizszego wzoru

    pulse_length = ((TIM_Period + 1) * DutyCycle) / 100 - 1

    25% duty cycle:     pulse_length = ((8399 + 1) * 25) / 100 - 1 = 2099
    50% duty cycle:     pulse_length = ((8399 + 1) * 50) / 100 - 1 = 4199
    75% duty cycle:     pulse_length = ((8399 + 1) * 75) / 100 - 1 = 6299
    100% duty cycle:    pulse_length = ((8399 + 1) * 100) / 100 - 1 = 8399
    */

	uint32_t reg = 0;

	if (xPercent >= 0 && xPercent <= 100)
	{
		if (xPercent == 0)
		{
			reg = 0;
		}
		else if (xPercent == 100)
		{
			reg = (TimerPeriod + 1);
		}
		else
		{
			reg = (uint32_t)( ( (TimerPeriod + 1) * xPercent / 100) - 1);
		}
	}

	return reg;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SoftStart_Handler(void)
{
	if (initFanPwm == TRUE)
	{
		if (PWM_FANSoftStart() == TRUE)
		{
			initFanPwm = FALSE;
			softStartDone = TRUE;
		}
	}

//	Irrigation_PumpControll();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainTimer_Handler(void)
{
	if (systimeTimeoutControl(&oneSecTimer, 1000))
	{
		if (calibrateFlags.processActive == FALSE)
		{
			displayData.pageCounter++;
		}

#ifdef CALIB_DEBUG
		if (calibDbgCounter < 5)
		{
			calibDbgCounter++;
			if (calibDbgCounter == 5)
			{
				calibrateFlags.probeType = PROBE_WATER;
				calibrateFlags.processActive = TRUE;
				calibrateFlags.waitForNextBuffer = TRUE;
				GLCD_ClearScreen();
			}
		}
#endif
		RTC_Handler();
		AtnelWiFi_Handler();
		Ntp_Handler();
    	Lightning_Handler();
		ADC_CalibrateProbes_Handler();
//		Irrigation_Core();

		if ((xLightControl.timeOnHours != 0 && xLightControl.timeOffHours != 24) ||
			(xLightControl.timeOnHours != 24 && xLightControl.timeOffHours != 0))
		{
	    	if (xLightControl.counterSeconds % ((uint32_t)1200000) == 0) //co 20 min
	    	{
	    		FLASH_SaveConfiguration();
	    	}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PWM_PumpInit(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Pin = PUMP_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP ;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_2);

	/* Wzor do przeliczenia okresu PWM */
	TimerPeriod = (SystemCoreClock / PWM_FAN_FREQ) - 1;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 , ENABLE);

	TIM_TimeBaseStructure.TIM_Prescaler = TIMER_PRESCALER;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = TimerPeriod;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

	TIM_OCInitStructure.TIM_Pulse = (TimerPeriod + 1); 	// ustawiam moc na 0%
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);

	TIM_Cmd(TIM2, ENABLE);
	TIM_CtrlPWMOutputs(TIM2, ENABLE);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PWM_FansInit(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_1);

	/* Wzor do przeliczenia okresu PWM */
	TimerPeriod = ((SystemCoreClock/TIMER_PRESCALER) / PWM_FAN_FREQ) - 1;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	TIM_TimeBaseStructure.TIM_Prescaler = TIMER_PRESCALER; // aby zmniejszyc F taktowania z 48Mhz na 1Mhz
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = TimerPeriod;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;

	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

	TIM_OCInitStructure.TIM_Pulse = (TimerPeriod + 1); 	// ustawiam moc na 0%
	TIM_OC3Init(TIM3, &TIM_OCInitStructure);

	TIM_OCInitStructure.TIM_Pulse = (TimerPeriod + 1);	// ustawiam moc na 0%
	TIM_OC4Init(TIM3, &TIM_OCInitStructure);

	TIM_ARRPreloadConfig(TIM3,ENABLE);
	TIM_CtrlPWMOutputs(TIM3, ENABLE);
	TIM_Cmd(TIM3, ENABLE);

	initFanPwm = TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static uint8_t PWM_FANSoftStart(void)
{
	uint8_t ret = 0;
	uint8_t retPull = 0;
	uint8_t retPush = 0;

	switch (xTempControl.tempCtrlMode)
	{
	case TEMP_MANUAL:
		retPull = PWM_IncPercentTo(PWM_FAN_PULL_AIR, xTempControl.fanPull);
		retPush = PWM_IncPercentTo(PWM_FAN_PUSH_AIR, xTempControl.fanPush);
		lastPullPWM = xTempControl.fanPull;
		lastPushPWM = xTempControl.fanPush;
		break;

	case TEMP_AUTO:
		retPull = PWM_IncPercentTo(PWM_FAN_PULL_AIR, 60);
		retPush = PWM_IncPercentTo(PWM_FAN_PUSH_AIR, 30);
		lastPullPWM = 60;
		lastPushPWM = 30;
		break;

	default:
		break;
	}

	if (retPull & retPush)
	{
		ret = 1;
	}

	return ret;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
