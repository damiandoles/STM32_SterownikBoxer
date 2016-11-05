/*
 * boxer_uart.c
 *
 *  Created on: 29 lip 2015
 *      Author: Doles
 */

#include "boxer_communication.h"
#include "boxer_struct.h"
#include "misc.h"

static void AtnelResetModule(void);

//This initializes the FIFO structure with the given buffer and size
void fifo_init(fifo_t * f, char * buf, int size){
     f->head = 0;
     f->tail = 0;
     f->size = size;
     f->buf = buf;
}

//This reads nbytes bytes from the FIFO
//The number of bytes read is returned
int fifo_read(fifo_t * f, void * buf, int nbytes)
{
     int i;
     char * p;
     p = buf;
     for(i=0; i < nbytes; i++){
          if( f->tail != f->head ){ //see if any data is available
               *p++ = f->buf[f->tail];  //grab a byte from the buffer
               f->tail++;  //increment the tail
               if( f->tail == f->size ){  //check for wrap-around
                    f->tail = 0;
               }
          } else {
               return i; //number of bytes read
          }
     }
     return nbytes;
}

//This writes up to nbytes bytes to the FIFO
//If the head runs in to the tail, not all bytes are written
//The number of bytes written is returned
int fifo_write(fifo_t * f, const void * buf, int nbytes){
     int i;
     const char * p;
     p = buf;
     for(i=0; i < nbytes; i++){
           //first check to see if there is space in the buffer
           if( (f->head + 1 == f->tail) ||
                ( (f->head + 1 == f->size) && (f->tail == 0) )){
                 return i; //no more room
           } else {
               f->buf[f->head] = *p++;
               f->head++;  //increment the head
               if( f->head == f->size ){  //check for wrap-around
                    f->head = 0;
               }
           }
     }
     return nbytes;
}

//**************************************************************************************



void UART2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    fifo_init(RxFifo, (void *)RxBuffer, sizeof(RxBuffer));
    fifo_init(TxFifo, (void *)TxBuffer, sizeof(TxBuffer));

#ifdef DEBUG_TERMINAL_USART
    USART_InitStructure.USART_BaudRate = 115200;//115200; 230400
#else
    USART_InitStructure.USART_BaudRate = 230400;//115200; 230400
#endif
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    /* Enable GPIO clock */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    /* Enable USART clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* Connect PXx to USARTx_Tx */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);

    /* Connect PXx to USARTx_Rx */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);

    /* Configure USART Tx, Rx as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART configuration */
    USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	NVIC_SetPriority(USART2_IRQn, 3);
	NVIC_EnableIRQ(USART2_IRQn);

    /* Enable USART */
    USART_Cmd(USART2, ENABLE);
}

void USART_SendString(USART_TypeDef *USARTx, char *string)
{
	while (*string != 0) // wysy�am tyle razy ile znak�w jest w stringu
	{
		USART_SendData(USARTx, (uint8_t)(*string)); // wysy�am aktualny znak ze stringa
		string++; // inkrementuj� po kolei ka�dy znak 'char' z kt�rych sk�ada si� string
	}
}

void USART2_IRQHandler(void)
{
  if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) // Received characters added to fifo
  {
	  USART_ClearITPendingBit(USART2, USART_IT_RXNE);

	  if (RxBuffIndex < RXSIZE)
	  {
		  RxBuffer[RxBuffIndex] = (char)USART_ReceiveData(USART2); // Receive the character
		  RxBuffIndex++;
	  }
	  else
	  {
		  flagsGlobal.bufferSizeOverflow = 1;
	  }

	  flagsGlobal.udpReceiveByte = TRUE;
  }

	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
	{
		USART_ClearITPendingBit(USART2, USART_IT_TXE);
		uint8_t tx;

		if (fifo_read(TxFifo, &tx, 1) == 1) // Anything to send?
			USART_SendData(USART2, (uint16_t)tx); // Transmit the character
		else
			USART_ITConfig(USART2, USART_IT_TXE, DISABLE); // Suppress interrupt when empty
	}
}

void PcCommunication_Handler(void)
{
	/************************************************************************/
	/*							UART/UDP COMMUNICATION						*/
	/************************************************************************/
	// RAMKA RESET STEROWNIKA
	if (flagsGlobal.udpReceiveByte == TRUE) //TODO przerobic na callbacki !!!!
	{
		if (RxBuffer[0] == 'S' &&
			RxBuffer[1] == 'T' &&
			RxBuffer[2] == 'A' &&
			RxBuffer[3] == 'R' &&
			RxBuffer[4] == 'E' &&
			RxBuffer[5] == 'N' &&
			RxBuffer[6] == 'D')
		{
			memset(RxBuffer, 0, RXSIZE);
			RxBuffIndex = 0;

			MISC_ResetARM();
		}
		// RAMKA TEMPERATURY
		else if (RxBuffer[0] == 'S' &&
				 RxBuffer[1] == 'T' &&
				 RxBuffer[2] == 'A' &&
				 RxBuffer[3] == 'S' &&
				 RxBuffer[4] == 'T' &&
				 RxBuffer[7] == 'E' &&
				 RxBuffer[8] == 'N' &&
				 RxBuffer[9] == 'D')
		{

			tempControl.userTemp = RxBuffer[5];
			tempControl.tempControl = RxBuffer[6];

			SaveConfiguration();
			memset(RxBuffer, 0, RXSIZE);
			RxBuffIndex = 0;
		}
		// RAMKA OSWIETLENIA
		else if (RxBuffer[0]  == 'S' &&
				 RxBuffer[1]  == 'T' &&
				 RxBuffer[2]  == 'A' &&
				 RxBuffer[3]  == 'S' &&
				 RxBuffer[4]  == 'L' &&
				 RxBuffer[8]  == 'E' &&
				 RxBuffer[9]  == 'N' &&
				 RxBuffer[10] == 'D')
		{

			lastTimeOnHour = lightControl.timeOnHours;
			lastTimeOffHour = lightControl.timeOffHours;
			lightControl.timeOnHours = RxBuffer[5];
			lightControl.timeOffHours = RxBuffer[6];

			light_state_t tempLightState = lightControl.lightingState;
			lightControl.lightingState = RxBuffer[7];

			// jesli nowy stan lampy jest inny od poprzedniego to skasuj liczniki (nowy stan)
			if (tempLightState != lightControl.lightingState)
			{
				lightCounters.counterHours = 0;
				lightCounters.counterSeconds = 0;
			}

			SaveConfiguration();
			memset(RxBuffer, 0, RXSIZE);
			RxBuffIndex = 0;
		}
		// RAMKA NAWADNIANIA
		else if (RxBuffer[0]  == 'S' &&
				 RxBuffer[1]  == 'T' &&
				 RxBuffer[2]  == 'A' &&
				 RxBuffer[3]  == 'S' &&
				 RxBuffer[4]  == 'I' &&
				 RxBuffer[8]  == 'E' &&
				 RxBuffer[9]  == 'N' &&
				 RxBuffer[10] == 'D')
		{

			irrigationControl.mode = RxBuffer[5];
			irrigationControl.frequency = RxBuffer[6];
			irrigationControl.water = RxBuffer[7];

			SaveConfiguration();
			memset(RxBuffer, 0, RXSIZE);
			RxBuffIndex = 0;
		}
		// RAMKA KALIBRACJI SOND PH
		else if (RxBuffer[0] == 'S' &&
				 RxBuffer[1] == 'T' &&
				 RxBuffer[2] == 'A' &&
				 RxBuffer[3] == 'C' &&
				 RxBuffer[4] == 'P' &&
				 RxBuffer[6] == 'E' &&
				 RxBuffer[7] == 'N' &&
				 RxBuffer[8] == 'D')
		{

			calibrateFlags.probeType = RxBuffer[5];
			calibrateFlags.processActive = 1;
			calibrateFlags.turnOnBuzzer = 1;
			calibrateFlags.toggleBuzzerState = 1;

			memset(RxBuffer, 0, RXSIZE);
			RxBuffIndex = 0;
		}

		flagsGlobal.udpReceiveByte = FALSE;
	}

	if (flagsGlobal.bufferSizeOverflow == 1)
	{
		memset(RxBuffer, 0, RXSIZE);
		RxBuffIndex = 0;
		flagsGlobal.bufferSizeOverflow = 0;
	}
}

static void AtnelResetModule(void)
{
	GPIOx_ResetPin(WIFI_RST_PORT, WIFI_RST_PIN);
	systimeDelayMs(3500);
	GPIOx_SetPin(WIFI_RST_PORT, WIFI_RST_PIN);
}
//void USART2_IRQHandler(void)
//{
////	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
////	{
////	   /* Write one byte to the transmit data register */
////		if (TxBuffer[TxCount] != '\0')
////		{
////		  USART_SendData(EVAL_COM1, TxBuffer[TxCount++]);
////		}
////		//else{
////		//  USART_ITConfig(EVAL_COM1, USART_IT_TXE, DISABLE);
////		//}
////	}
//
//	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) // Received uint8_tacters modify string
//	{
//		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
//		RxBuffer[RxBuffIndex] = USART2->RDR; //USART_ReceiveData(USART2);
//		RxBuffIndex++;
//		byteReceivedFlag = TRUE;
//
//		//////////////////////////////////////////////////////////////////////////
//		if (RxBuffIndex == 1)
//		{
//			if (DataFromPC[0] == 0x21)
//			{
//				findFirstStartByte = 1;
//			}
//		}
//
//		if (RxBuffIndex == 2)
//		{
//			if (DataFromPC[1] == 0x67)
//			{
//				findSecondStartByte = 1;
//			}
//		}
//
//		if (RxBuffIndex == 100)
//		{
//			RxBuffIndex = 0;
//			if (DataFromPC[99] == 0x55) // end of frame
//			{
////				SaveParametersFlag = 1;
////				calibrateFlags.saveFactors = 1;
//				flag.saveParameters = 1;
//				findFirstStartByte = 0;
//				findSecondStartByte = 0;
//			}
//			//////////////////////////////////////////////////////////////////////////
//			if (DataFromPC[9] == RSTCommand)
//			{
//	//				RESETuC;
//			}
//			//////////////////////////////////////////////////////////////////////////
//			if (DataFromPC[15] == CalibrationCommand)
//			{
//				DataFromPC[15] = 0;
//				calibrateFlags.processActive = 1;
//				calibrateFlags.turnOnBuzzer = 1;
//				calibrateFlags.toggleBuzzerState = 1;
//				calibrateFlags.processDone = 0;
//				if (DataFromPC[17] == 1) // calibrate soil probe
//				{
//					calibrateFlags.probe = 1;
//					DataFromPC[17] = 0;
//				}
//				else if (DataFromPC[17] == 2) // calibrate water probe
//				{
//					calibrateFlags.probe = 2;
//					DataFromPC[17] = 0;
//				}
//			}
//			//////////////////////////////////////////////////////////////////////////
//			if (DataFromPC[16] == SaveFactorsCommand) // save factors flag from C# App
//			{
//				calibrateFlags.saveFactors = 1;
//				flag.saveParameters = 1;
////				SaveParametersFlag = 1; // ????
//
//				if (calibrateFlags.probe == 1)
//				{
//					FactorASoil.factorAbyte[0] = DataFromPC[18];
//					FactorASoil.factorAbyte[1] = DataFromPC[19];
//					FactorASoil.factorAbyte[2] = DataFromPC[20];
//					FactorASoil.factorAbyte[3] = DataFromPC[21];
//					FactorBSoil.factorBbyte[0] = DataFromPC[22];
//					FactorBSoil.factorBbyte[1] = DataFromPC[23];
//					FactorBSoil.factorBbyte[2] = DataFromPC[24];
//					FactorBSoil.factorBbyte[3] = DataFromPC[25];
//				}
//				else if (calibrateFlags.probe == 2)
//				{
//					FactorAWater.factorAbyte[0] = DataFromPC[18];
//					FactorAWater.factorAbyte[1] = DataFromPC[19];
//					FactorAWater.factorAbyte[2] = DataFromPC[20];
//					FactorAWater.factorAbyte[3] = DataFromPC[21];
//					FactorBWater.factorBbyte[0] = DataFromPC[22];
//					FactorBWater.factorBbyte[1] = DataFromPC[23];
//					FactorBWater.factorBbyte[2] = DataFromPC[24];
//					FactorBWater.factorBbyte[3] = DataFromPC[25];
//				}
//			}
//		}
//	}
//}
