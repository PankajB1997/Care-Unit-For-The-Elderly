/**
 * Names: PANKAJ BHOOTRA (A0144919W), ZHAO HONGWEI (A0131838A)
 * EE2024 Assignment 2
 */

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx.h"

#include "led7seg.h"
#include "oled.h"
#include "temp.h"
#include "light.h"
#include "rgb.h"
#include "acc.h"
#include "joystick.h"
#include "rotary.h"
#include "pca9532.h"

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

#define TEMP_HIGH_WARNING 2500
#define LIGHT_LOW_WARNING 50
#define TEMP_HALF_PERIODS 170
#define RGB_RED 0x01
#define RGB_BLUE 0x02
#define RGB_RED_BLUE 0x03
#define RGB_RESET 0x00

uint32_t msTicks = 0;
uint32_t sevenSegTime, sevenSegIntroTime, mainTick, blueRedTicks, blueTicks,
		accTicks, redTicks, blinkTick, invertedTick, introTime, ledTick, tTicks,
		tempLightTick, tempStartTime = 0, tempEndTime = 0, tempIntCount = 0;
uint8_t blink_blue_flag = 0, blink_red_flag = 0, noMovementFlag = 0,
		lightIntFlag = 0, tempIntFlag = 0, logModeFlag = 0;
int8_t acc_x = 0, acc_y = 0, acc_z = 0, x = 0, y = 0, z = 0, xoff, yoff, zoff,
		led_y;

static uint8_t barPos = 2;

static void moveBar(uint8_t steps, uint8_t dir) {
	uint16_t ledOn = 0;

	if (barPos == 0)
		ledOn = (1 << 0) | (3 << 14);
	else if (barPos == 1)
		ledOn = (3 << 0) | (1 << 15);
	else
		ledOn = 0x07 << (barPos - 2);

	barPos += (dir * steps);
	barPos = (barPos % 16);

	pca9532_setLeds(ledOn, 0xffff);
}

static void init_i2c(void) {
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_uart(void) {
	UART_CFG_Type uartCfg;
	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;
	//pin select for uart3
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
	//supply power and setup working parts for uart3
	UART_Init(LPC_UART3, &uartCfg);
	//enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);
}

uint32_t getTicks() {
	return msTicks;
}

static void init_GPIO(void) {
	// Initialize button SW4
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1 << 31, 0);

	// Initialize button SW3
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1 << 10, 0);
}

static void init_ssp(void) {
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);
}

void SysTick_Handler(void) {
	msTicks++;
}

static void displayValOn7Seg(uint8_t sevenSegVal, uint32_t isInverted) {
	switch (sevenSegVal) {
	case 0:
		led7seg_setChar('0', isInverted);
		break;
	case 1:
		led7seg_setChar('1', isInverted);
		break;
	case 2:
		led7seg_setChar('2', isInverted);
		break;
	case 3:
		led7seg_setChar('3', isInverted);
		break;
	case 4:
		led7seg_setChar('4', isInverted);
		break;
	case 5:
		led7seg_setChar('5', isInverted);
		break;
	case 6:
		led7seg_setChar('6', isInverted);
		break;
	case 7:
		led7seg_setChar('7', isInverted);
		break;
	case 8:
		led7seg_setChar('8', isInverted);
		break;
	case 9:
		led7seg_setChar('9', isInverted);
		break;
	case 10:
		led7seg_setChar('A', isInverted);
		break;
	case 11:
		led7seg_setChar('B', isInverted);
		break;
	case 12:
		led7seg_setChar('C', isInverted);
		break;
	case 13:
		led7seg_setChar('D', isInverted);
		break;
	case 14:
		led7seg_setChar('E', isInverted);
		break;
	case 15:
		led7seg_setChar('F', isInverted);
		break;
	}
}

void setRGB(uint8_t ledMask) {
	if ((ledMask & RGB_RED) != 0) {
		GPIO_SetValue(2, (1 << 0));
	} else {
		GPIO_ClearValue(2, (1 << 0));
	}

	if ((ledMask & RGB_BLUE) != 0) {
		GPIO_SetValue(0, (1 << 26));
	} else {
		GPIO_ClearValue(0, (1 << 26));
	}
}

//to be done based on accelerometer reading
int invertedNormally(int8_t x, int8_t y, int8_t z) {
	return y >= -3;
}

//send a message to UART
void uart_sendMessage(char* msg) {
	int len = strlen(msg);
	msg[len] = '\r'; // change \0 to \r
	msg[++len] = '\n'; // append \n behind
	msg[++len] = '\n'; // append \n behind
	UART_Send(LPC_UART3, (uint8_t*) msg, (uint32_t) len, BLOCKING);
}

int movementDetected() {
	int diffx, diffy, diffz;
	diffx = (acc_x > x) ? acc_x - x : x - acc_x;
	diffy = (acc_y > y) ? acc_y - y : y - acc_y;
	diffz = (acc_z > z) ? acc_z - z : z - acc_z;
	if ((diffx >= 5) || (diffy >= 5) || (diffz >= 5))
		return 1;
	else return 0;
}

// EINT3 Interrupt Handler
void EINT3_IRQHandler(void) {
	// Determine whether GPIO Interrupt P2.10 has occurred - Pushbutton SW3
	if ((LPC_GPIOINT ->IO2IntStatF >> 10) & 0x1) {
		// Clear GPIO Interrupt P2.10
		LPC_GPIOINT ->IO2IntClr = 1 << 10;
		logModeFlag = !logModeFlag;
	}

	//acc_read(&acc_x, &acc_y, &acc_z);
	//acc_x = acc_x + xoff;
	//acc_y = acc_y + yoff;
	//acc_z = acc_z + zoff;
	//if (movementDetected()) {
	// Determine whether GPIO Interrupt P2.5 has occurred - Light Sensor and Accelerometer
	if ((LPC_GPIOINT ->IO2IntStatF >> 5) & 0x1) {
		LPC_GPIOINT ->IO2IntClr = 1 << 5;
		light_clearIrqStatus();
		lightIntFlag = 1;
		//if (movementDetected()) blink_blue_flag = 1;
		//else noMovementFlag = 1;
	}

	// Separate interrupt needed for accelerometer ?????

	// Determine whether GPIO Interrupt P0.2 has occurred - Temperature Sensor
	if ((LPC_GPIOINT ->IO0IntStatF >> 2) & 0x1) {
		if (tempStartTime==0 && tempEndTime==0)
			tempStartTime = msTicks;
		else if (tempStartTime!=0 && tempEndTime==0) {
			tempIntCount++;
			if (tempIntCount == TEMP_HALF_PERIODS) {
				tempEndTime = msTicks;
				tempIntFlag = 1;
			}
		}
		LPC_GPIOINT ->IO0IntClr = 1 << 2;
	}
	//}
}

// UART3 Interrupt Handler
void UART3_IRQHandler(void) {
	UART3_StdIntHandler();
}

int main(void) {

	NVIC_SetPriority(SysTick_IRQn, 0);

	uint8_t btn1 = 1, flag = 0, sevenSegVal = 0, blink_flag = 0, invertedFlag =
			0, sToMFlag = 0, mToSFlag = 1, displayFlag = 0, uartFlag = 0, rgbFlag = 0, ledFlag = 0;
	uint32_t lightSensorVal = 0, tempSensorVal = 0;
	int8_t firstTime = 1, val = 57, dir;
	int uartCounter = 0;
	init_i2c();
	init_GPIO();
	init_ssp();
	init_uart();

	pca9532_init();
	pca9532_setBlink1Period(250);
	pca9532_setBlink1Duty(50);
	joystick_init();
	rotary_init();
	light_enable();
	led7seg_init();
	oled_init();
	acc_init();
	rgb_init();
	temp_init(getTicks);

	// Setup light limit for triggering interrupt
	light_setRange(LIGHT_RANGE_4000);
	light_setLoThreshold(LIGHT_LOW_WARNING);
	light_setIrqInCycles(LIGHT_CYCLE_1);
	light_clearIrqStatus();

	// Enable GPIO Interrupt P2.10 - SW3
	LPC_GPIOINT ->IO2IntEnF |= 1 << 10;
	// Enable GPIO Interrupt P2.5 - Light Sensor
	LPC_GPIOINT ->IO2IntEnF |= 1 << 5;
	// Enable GPIO Interrupt P0.2 - Temperature Sensor
	LPC_GPIOINT ->IO0IntEnF |= 1 << 2;
	// Enable EINT3 interrupt
	NVIC_EnableIRQ(EINT3_IRQn);
	NVIC_SetPriority(EINT3_IRQn, 31);

	// Enable UART interrupts to send/receive
	LPC_UART3->IER |= UART_IER_RBRINT_EN;
	LPC_UART3->IER |= UART_IER_THREINT_EN;
	// Enable UART Rx Interrupt
	UART_IntConfig(LPC_UART3, UART_INTCFG_RBR, ENABLE);
	// Enable Interrupt for UART3
	NVIC_EnableIRQ(UART3_IRQn);
	NVIC_SetPriority(UART3_IRQn, 1);

	oled_clearScreen(OLED_COLOR_BLACK);
	led7seg_setChar('@', 0);
	setRGB(0);
	SysTick_Config(SystemCoreClock / 1000);

	sevenSegIntroTime = msTicks;
	sevenSegTime = msTicks;
	mainTick = msTicks;
	blinkTick = msTicks;
	invertedTick = msTicks;
	introTime = msTicks;
	ledTick = msTicks;
	tempLightTick = msTicks;
	accTicks = msTicks;
	tTicks = msTicks;

	// assume the board is in zero-g position when reading first value
	acc_read(&acc_x, &acc_y, &acc_z);
	x = acc_x;
	y = acc_y;
	z = acc_z;
	xoff = -x;
	yoff = -y;
	zoff = 64 - z;

	while (1) {

		if (msTicks - introTime <= 10000) {
			oled_putString(0, 0, (uint8_t *) "C.U.T.E", OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_line(0, 10, 42, 10, OLED_COLOR_WHITE);
			oled_putString(0, 20, (uint8_t *) "BY", OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 40, (uint8_t *) "1. Pankaj B.", OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 50, (uint8_t *) "2. Zhao Hongwei",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			if (firstTime) {
				led7seg_setChar(val, 1);
				firstTime = 0;
			}
			if (msTicks - sevenSegIntroTime >= 1000) {
				led7seg_setChar(--val, 1);
				sevenSegIntroTime = msTicks;
			}
			continue;
		}

		/* Log Mode */
		if (logModeFlag==1)
		{
			//logModeFlag = 0;

			pca9532_setBlink1Leds(0xFFFF);
			led7seg_setChar('@', 0); // turn off the display
			blink_blue_flag = 0;
			blink_red_flag = 0;
			rgbFlag = 0;
			ledFlag = 1;
			setRGB(RGB_RESET);
			continue;
		}

		btn1 = (GPIO_ReadValue(1) >> 31) & 0x01; //reading from SW4

		if (btn1 == 0 && (msTicks - mainTick >= 1000)) {
			mainTick = msTicks;
			flag = !flag;
		}

		if (flag==1) {
//			if (msTicks - tempLightTick >= 500) {
//				tempSensorVal = temp_read();
			if (ledFlag==1 || sToMFlag==1) {
				pca9532_setLeds(0xFF00, 0xFFFF);
				ledFlag = 0;
			}
			if (sToMFlag==1) {
				oled_clearScreen(OLED_COLOR_BLACK);
				oled_putString(0, 0, (uint8_t *) "MONITOR", OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				unsigned char TempPrint1[40] = "";
				unsigned char LightPrint1[40] = "";
				strcat(TempPrint1, "T :  0.00 deg C");
				oled_putString(0, 10, (uint8_t *) TempPrint1, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				strcat(LightPrint1, "L :     0 lux");
				oled_putString(0, 20, (uint8_t *) LightPrint1, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				char XPrint1[20] = "", YPrint1[20] = "", ZPrint1[20] = "";

				strcat(XPrint1, "AX :    0 ");
				strcat(YPrint1, "AY :    0 ");
				strcat(ZPrint1, "AZ :    0 ");
				oled_putString(0, 30, (uint8_t *) XPrint1, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				oled_putString(0, 40, (uint8_t *) YPrint1, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				oled_putString(0, 50, (uint8_t *) ZPrint1, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				//displaying entering monitor mode on UART
				char uartMessage[255];
				strcpy(uartMessage,
						"Entering MONITOR Mode.                            ");
				uart_sendMessage(uartMessage);
				sevenSegVal = 0;
				sToMFlag = 0;
				lightIntFlag = 0;
			}
//			lightSensorVal = light_read();
//				tempLightTick = msTicks;
//			}
			mToSFlag = 1;
			if (msTicks - accTicks >= 200) {
				accTicks = msTicks;
				x = acc_x;
				y = acc_y;
				z = acc_z;
				acc_read(&acc_x, &acc_y, &acc_z);
				acc_x = acc_x + xoff;
				acc_y = acc_y + yoff;
				acc_z = acc_z + zoff;
//				led_y = acc_y;
//				if (led_y < 0) {
//					dir = 1;
//					led_y = -led_y;
//				} else {
//					dir = -1;
//				}
//				if (led_y > 1 && (msTicks - ledTick) >= (40 / (1 + (led_y / 10)))) {
//								moveBar(1, dir);
//								ledTick = msTicks;
//							}
			}
			if (invertedNormally(acc_x, acc_y, acc_z)==1) { //to be done based on accelerometer reading
				invertedFlag = 1;
			} else {
				invertedFlag = 0;
			}
			if (msTicks - blinkTick >= 333) {
				blinkTick = msTicks;
				blink_flag = !blink_flag;
			}
			if (lightIntFlag==1) {
				lightIntFlag = 0;
				if ((msTicks-tTicks >= 400) && (blink_blue_flag==0) && (movementDetected()==1)) {
					blink_blue_flag = 1;
				}

			}
			if (tempIntFlag==1) {
				tempIntFlag = 0;
				//if (tempEndTime > tempStartTime)
				tempEndTime = tempEndTime - tempStartTime;
				//else tempEndTime = 0xFFFFFFFF - tempStartTime + 1 + tempEndTime;
				//tempSensorVal = ((tempEndTime) / (TEMP_HALF_PERIODS*10)) - 273;
				//tempSensorVal = tempEndTime;
				tempSensorVal = ((2*1000*tempEndTime) / TEMP_HALF_PERIODS) - 2731;
				//tempSensorVal /= 100;
				tempIntCount = 0;
				tempStartTime = 0;
				tempEndTime = 0;
				if ((msTicks-tTicks >= 400) && (blink_red_flag==0) && (movementDetected()==1) && (tempSensorVal >= TEMP_HIGH_WARNING)) {
					blink_red_flag = 1;
				}
			}
			//if (rgbFlag || movementDetected()) {
			if (blink_flag==1)
				setRGB(RGB_RESET);
			else {
				if ((blink_blue_flag==1) && (blink_red_flag==1)) {
					setRGB(RGB_RED_BLUE);
					rgbFlag = 1;
				} else if (blink_blue_flag==1) {
					setRGB(RGB_BLUE);
					rgbFlag = 1;
				} else if (blink_red_flag==1) {
					setRGB(RGB_RED);
					rgbFlag = 1;
				} else
					setRGB(RGB_RESET);
			}
			//}

			if (msTicks - sevenSegTime >= 1000) {
				sevenSegTime = msTicks;
				displayValOn7Seg(sevenSegVal, invertedFlag);
				sevenSegVal = (sevenSegVal + 1) % 16;
			}

			if (sevenSegVal == 8 || sevenSegVal == 13 || sevenSegVal == 2)
				displayFlag = 1;
			if (displayFlag==1
					&& (sevenSegVal == 6 || sevenSegVal == 11
							|| sevenSegVal == 0)) {
				displayFlag = 0;
				unsigned char TempPrint[40] = "";
				unsigned char LightPrint[40] = "";

				sprintf(TempPrint, "T : %.2f ", tempSensorVal/100.0);
				strcat(TempPrint, "deg C");
				oled_putString(0, 10, (uint8_t *) TempPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);

				lightSensorVal = light_read();
				sprintf(LightPrint, "L : %5d ", lightSensorVal);
				strcat(LightPrint, "lux");
				oled_putString(0, 20, (uint8_t *) LightPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);

				char XPrint[20], YPrint[20], ZPrint[20];

				sprintf(XPrint, "AX : %4d ", acc_x);
				sprintf(YPrint, "AY : %4d ", acc_y);
				sprintf(ZPrint, "AZ : %4d ", acc_z);
				oled_putString(0, 30, (uint8_t *) XPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				oled_putString(0, 40, (uint8_t *) YPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
				oled_putString(0, 50, (uint8_t *) ZPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);
			}

			if (sevenSegVal != 0) {

			}
			if (sevenSegVal == 14)
				uartFlag = 1;
			if (uartFlag==1 && sevenSegVal == 0) {
				uartFlag = 0;
				char uartMessage[255] = "";
				if (blink_red_flag==1) {
					strcat(&uartMessage,
							"Fire was Detected.                                         \r\n");
				}
				if (blink_blue_flag==1) {
					strcat(&uartMessage,
							"Movement in darkness was Detected.                                   \r\n");
				}
				char values[255] = "";
				sprintf(&values,
						"%03d_-_T%.2f_L%d_AX%d_AY%d_AZ%d                             ",
						uartCounter, tempSensorVal/100.0, lightSensorVal, acc_x,
						acc_y, acc_z);
				strcat(&uartMessage, &values);
				uart_sendMessage(uartMessage);
				uartCounter++;
			}

		} else {
			//the 3 sensors should not be reading values here
			//UART should not be getting any message
			sToMFlag = 1;
			if (ledFlag==1 || mToSFlag==1) {
				pca9532_setLeds(0x00FF, 0xFFFF);
				ledFlag = 0;
			}
			if (mToSFlag) {
     			oled_clearScreen(OLED_COLOR_BLACK);
     			mToSFlag = 0;
			}
			led7seg_setChar('@', 0); // turn off the display
			blink_blue_flag = 0;
			blink_red_flag = 0;
			rgbFlag = 0;
			setRGB(RGB_RESET);
			//oled_putString(0, 0, (uint8_t *) "STABLE            ",
			//	OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			noMovementFlag = 1;
			tTicks = msTicks;
		}
	}

}
