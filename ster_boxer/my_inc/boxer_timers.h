#ifndef MY_INC_BOXER_TIMERS_H_
#define MY_INC_BOXER_TIMERS_H_

#include "stm32f0xx_tim.h"
#include "stm32f0xx_syscfg.h"
#include "boxer_bool.h"

#define PWM_FAN_FREQ 30

#define REG_PWM_PUSH_AIR_FAN 				TIM3->CCR3 // wciagajacy powietrze 	TIM3_CH3 -> PB0
#define REG_PWM_PULL_AIR_FAN 				TIM3->CCR4 // wyciagajacy powietrze TIM3_CH4 -> PB1
#define REG_PWM_PUMP 						TIM2->CCR2 // PWM pompy 			TIM2_CH2 -> PA1

typedef enum
{
	PWM_PUMP,
	PWM_FAN_PULL_AIR,
	PWM_FAN_PUSH_AIR
}pwm_dev_type_t;

typedef enum
{
	PWM_MIN_PERCENT = 0,
	PWM_MAX_PERCENT = 100
}pwm_range_t;

uint8_t lastPullPWM;
uint8_t lastPushPWM;

extern bool_t softStartDone;
extern bool_t initFanPwm;

void PWM_FansInit(void);
void PWM_PumpInit(void);
void SoftStart_Handler(void);
void MainTimer_Handler(void);
uint8_t PWM_IncPercentTo(pwm_dev_type_t xPwmDev, uint8_t xPercent);
uint8_t PWM_DecPercentTo(pwm_dev_type_t xPwmDev, uint8_t xPercent);

#endif /* MY_INC_BOXER_TIMERS_H_ */
