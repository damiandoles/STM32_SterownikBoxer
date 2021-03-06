#ifndef MY_INC_BOXER_DISPLAY_H_
#define MY_INC_BOXER_DISPLAY_H_

#include "stdint.h"
#include "string.h"
#include "string_builder.h"
#include "timestamp.h"
#include "KS0108.h"
#include "glcd_font5x8.h"
#include "graphic.h"

typedef enum
{
	PAGE_1 = 1,
	PAGE_2,
	PAGE_3
}page_t;

typedef struct
{
	float temp_middle_t;
	float temp_up_t;
	float temp_down_t;
	uint32_t lux;
	float humiditySHT2x;
	char time[20];
	float ph1;
	float ph2;
	page_t page;
	uint8_t pageCounter;
}lcdDisplayData_t;

lcdDisplayData_t displayData;

uint8_t xLastTimeOnHour;
uint8_t xLastTimeOffHour;
time_complex_t xRtcFullDate;

char xTimeString[20];
char xDateString[20];
char weekDayString[20];

void Display_Handler(void);
#endif /* MY_INC_BOXER_DISPLAY_H_ */
