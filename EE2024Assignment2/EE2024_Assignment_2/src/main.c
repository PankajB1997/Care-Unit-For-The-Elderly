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

#define TEMP_HIGH_WARNING 450
#define LIGHT_LOW_WARNING 50
#define RGB_RED 0x01
#define RGB_BLUE 0x02
#define RGB_RED_BLUE 0x03
#define RGB_RESET 0x00

uint32_t msTicks = 0;
uint32_t sevenSegTime, mainTick, blueRedTicks, blueTicks, redTicks, blinkTick,
		invertedTick, introTime;

// Interval in us
static uint32_t notes[] = { 2272, // A - 440 Hz
		2024, // B - 494 Hz
		3816, // C - 262 Hz
		3401, // D - 294 Hz
		3030, // E - 330 Hz
		2865, // F - 349 Hz
		2551, // G - 392 Hz
		1136, // a - 880 Hz
		1012, // b - 988 Hz
		1912, // c - 523 Hz
		1703, // d - 587 Hz
		1517, // e - 659 Hz
		1432, // f - 698 Hz
		1275, // g - 784 Hz
		};

static void playNote(uint32_t note, uint32_t durationMs) {
	uint32_t t = 0;

	if (note > 0) {
		while (t < (durationMs * 1000)) {
			NOTE_PIN_HIGH()
			;
			Timer0_us_Wait(note / 2); // us timer

			NOTE_PIN_LOW()
			;
			Timer0_us_Wait(note / 2);

			t += note;
		}
	} else {
		Timer0_Wait(durationMs); // ms timer
	}
}

static uint32_t getNote(uint8_t ch) {
	if (ch >= 'A' && ch <= 'G')
		return notes[ch - 'A'];

	if (ch >= 'a' && ch <= 'g')
		return notes[ch - 'a' + 7];

	return 0;
}

static uint32_t getDuration(uint8_t ch) {
	if (ch < '0' || ch > '9')
		return 400;
	/* number of ms */
	return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch) {
	switch (ch) {
	case '+':
		return 0;
	case ',':
		return 5;
	case '.':
		return 20;
	case '_':
		return 30;
	default:
		return 5;
	}
}

static void playSong(uint8_t *song) {
	uint32_t note = 0;
	uint32_t dur = 0;
	uint32_t pause = 0;

	/*
	 * A song is a collection of tones where each tone is
	 * a note, duration and pause, e.g.
	 *
	 * "E2,F4,"
	 */

	while (*song != '\0') {
		note = getNote(*song++);
		if (*song == '\0')
			break;
		dur = getDuration(*song++);
		if (*song == '\0')
			break;
		pause = getPause(*song++);

		playNote(note, dur);
		Timer0_Wait(pause);
	}
}

static uint8_t * song = (uint8_t*) "C1.C1,D2,C2,F2,E4,";

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

	/* ---- Speaker ------> */
//    GPIO_SetDir(2, 1<<0, 1);
//    GPIO_SetDir(2, 1<<1, 1);
	/*
	 GPIO_SetDir(0, 1<<27, 1);
	 GPIO_SetDir(0, 1<<28, 1);
	 GPIO_SetDir(2, 1<<13, 1);
	 // Main tone signal : P0.26
	 GPIO_SetDir(0, 1<<26, 1);
	 GPIO_ClearValue(0, 1<<27); //LM4811-clk
	 GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
	 GPIO_ClearValue(2, 1<<13); //LM4811-shutdn */
	/* <---- Speaker ------ */
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
	return y >= 0;
}

//send a message to UART
void uart_sendMessage(char* msg) {
	int len = strlen(msg);
	msg[len] = '\r'; // change \0 to \r
	msg[++len] = '\n'; // append \n behind
	UART_Send(LPC_UART3, (uint8_t*) msg, (uint32_t) len, BLOCKING);
}

// EINT3 Interrupt Handler
void EINT3_IRQHandler(void) {
	// Determine whether GPIO Interrupt P2.10 has occurred - Pushbutton SW3
	if ((LPC_GPIOINT ->IO2IntStatF >> 10) & 0x1) {
		// printf("GPIO Interrupt 2.10\n");
		// for (i=0;i<9999999;i++);
		// Clear GPIO Interrupt P2.10
		LPC_GPIOINT ->IO2IntClr = 1 << 10;
	}
	// Determine whether GPIO Interrupt P2.5 has occurred - Light Sensor and Accelerometer
	if ((LPC_GPIOINT ->IO2IntStatF >> 5) & 0x1) {
		LPC_GPIOINT ->IO2IntClr = 1 << 5;
	}
	// Determine whether GPIO Interrupt P0.6 has occurred - Temp Sensor
	if ((LPC_GPIOINT ->IO0IntStatF >> 6) & 0x1) {
		LPC_GPIOINT ->IO0IntClr = 1 << 6;
	}
}

int main(void) {

	// Setup light limit for triggering interrupt
//		light_setRange(LIGHT_RANGE_4000);
//		light_setLoThreshold(LIGHT_LOW_WARNING);
//		light_setHiThreshold(1500);
//		light_setIrqInCycles(LIGHT_CYCLE_1);
//		light_clearIrqStatus();

	// Enable GPIO Interrupt P2.10 - SW3
	LPC_GPIOINT ->IO2IntEnF |= 1 << 10;
	// Enable GPIO Interrupt P2.5 - Light Sensor
	LPC_GPIOINT ->IO2IntEnF |= 1 << 5;
	// Enable GPIO Interrupt P0.6 - Temperature Sensor
	LPC_GPIOINT ->IO0IntEnF |= 1 << 6;
	// Enable EINT3 interrupt
	NVIC_EnableIRQ(EINT3_IRQn);

	uint8_t btn1 = 1, flag = 0, sevenSegVal = 0, blink_blue_flag = 0,
			blink_red_flag = 0, blink_flag = 0, invertedFlag = 0, sToMFlag = 0,
			displayFlag = 0, uartFlag = 0;
	uint32_t lightSensorVal = 0;
	int32_t tempSensorVal = 0;
	int8_t acc_x, acc_y, acc_z, x = 0, y = 0, z = 0, xoff, yoff, zoff;
	int uartCounter = 0;
	init_i2c();
	init_GPIO();
	init_ssp();
	init_uart();

	pca9532_init();
	joystick_init();
	light_enable();
	led7seg_init();
	oled_init();
	acc_init();
	rgb_init();
	temp_init(getTicks);

	oled_clearScreen(OLED_COLOR_BLACK);
	setRGB(0);
	SysTick_Config(SystemCoreClock / 1000);

	sevenSegTime = msTicks;
	mainTick = msTicks;
	blinkTick = msTicks;
	invertedTick = msTicks;
//    sevenSegTime = 3000;
//        mainTick = 3000;
//        blinkTick = 3000;
//        invertedTick = 3000;
	introTime = msTicks;
	acc_read(&x, &y, &z);
	xoff = -x;
	yoff = -y;
	zoff = -z;

	while (1) {

		if (msTicks - introTime < 3000) {
			oled_putString(0, 0, (uint8_t *) "EE2024 Assignment 2",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 20, (uint8_t *) "Students :-", OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 40, (uint8_t *) "1. Pankaj Bhootra",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 50, (uint8_t *) "2. Zhao Hongwei",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			continue;
		}

		btn1 = (GPIO_ReadValue(1) >> 31) & 0x01; //reading from SW4

		if (btn1 == 0 && (msTicks - mainTick > 1000)) {
			mainTick = msTicks;
			flag = !flag;
		}

		if (flag) {
			tempSensorVal = temp_read();
			lightSensorVal = light_read();
			acc_read(&acc_x, &acc_y, &acc_z);
			acc_x = acc_x + xoff;
			acc_y = acc_y + yoff;
			acc_z = acc_z + zoff;
			if (invertedNormally(acc_x, acc_y, acc_z)) { //to be done based on accelerometer reading
				invertedFlag = 1;
			} else
				invertedFlag = 0;

			if (sToMFlag) {
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
				//displaying entering monitor mode on uart
				char uartMessage[255];
				strcpy(uartMessage,
						"Entering MONITOR Mode.                            ");
				uart_sendMessage(uartMessage);
				sevenSegVal = 0;
				sToMFlag = 0;
			}

			if (msTicks - blinkTick > 333) {
				blinkTick = msTicks;
				blink_flag = !blink_flag;
			}
			if (blink_flag)
				setRGB(RGB_RESET);
			else {
				if (blink_blue_flag && blink_red_flag)
					setRGB(RGB_RED_BLUE);
				else if (blink_blue_flag)
					setRGB(RGB_BLUE);
				else if (blink_red_flag)
					setRGB(RGB_RED);
				else
					setRGB(RGB_RESET);
			}

			if (msTicks - sevenSegTime > 1000) {
				sevenSegTime = msTicks;
				displayValOn7Seg(sevenSegVal, invertedFlag);
				sevenSegVal = (sevenSegVal + 1) % 16;
			}
			// oled_line(10, 0, 10, 10, OLED_COLOR_WHITE); //coordinates may need to be changed

			if (sevenSegVal == 8 || sevenSegVal == 13 || sevenSegVal == 2)
				displayFlag = 1;
			if (displayFlag
					&& (sevenSegVal == 6 || sevenSegVal == 11
							|| sevenSegVal == 0)) {
				displayFlag = 0;
				//oled_clearScreen(OLED_COLOR_BLACK);
				//oled_putString(0, 0, (uint8_t *) "Monitor Mode", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
				unsigned char TempPrint[40] = "";
				unsigned char LightPrint[40] = "";

				sprintf(TempPrint, "T : %.2f ", tempSensorVal / 10.0);
				strcat(TempPrint, "deg C");
				oled_putString(0, 10, (uint8_t *) TempPrint, OLED_COLOR_WHITE,
						OLED_COLOR_BLACK);

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
				//x = acc_x;
				//          		y = acc_y;
				//        		z = acc_z;

			}

			if (sevenSegVal != 0) //probably change 15 to 0
					{
				if (acc_x != x || acc_y != y || acc_z != z) {
					if (lightSensorVal < LIGHT_LOW_WARNING) {
						blink_blue_flag = 1;
					} else {
						blink_blue_flag = 0;
					}
					if (tempSensorVal > TEMP_HIGH_WARNING) {
						blink_red_flag = 1;
					} else {
						blink_red_flag = 0;
					}
					x = acc_x;
					y = acc_y;
					z = acc_z;
				}
			}
			if (sevenSegVal == 14)
				uartFlag = 1;
			if (uartFlag && sevenSegVal == 0) //probably change 15 to 0
					{
				uartFlag = 0;
				char uartMessage[255] = "";
				if (blink_red_flag) {
					strcat(&uartMessage,
							"Fire was Detected.                                         \r\n");
				}
				if (blink_blue_flag) {
					strcat(&uartMessage,
							"Movement in darkness was Detected.                                   \r\n");
				}
				char values[255] = "";
				sprintf(&values,
						"%03d_-_T%.2f_L%d_AX%d_AY%d_AZ%d                             ",
						uartCounter, tempSensorVal / 10.0, lightSensorVal,
						acc_x, acc_y, acc_z);
				strcat(&uartMessage, &values);
				uart_sendMessage(uartMessage);
				uartCounter++;
			}

		} else if (msTicks > 3000) {
			//the 3 sensors should not be reading values here
			//UART should not be getting any message
			sToMFlag = 1;
			led7seg_setChar('@', 0);
			blink_blue_flag = 0;
			blink_red_flag = 0;
			setRGB(RGB_RESET);
			//oled_clearScreen(OLED_COLOR_BLACK);
			oled_putString(0, 0, (uint8_t *) "STABLE            ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 10, (uint8_t *) "               ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 20, (uint8_t *) "               ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 30, (uint8_t *) "               ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 40, (uint8_t *) "               ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 50, (uint8_t *) "               ",
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);

			//oled_line(10, 0, 10, 10, OLED_COLOR_WHITE); //coordinates may need to be changed
		}
	}

}