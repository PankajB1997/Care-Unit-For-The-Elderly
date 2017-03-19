/**
 * Names: PANKAJ BHOOTRA (A0144919W), ZHAO HONGWEI (A0131838A)
 * EE2024 Assignment 2
 */

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx.h"

#include "led7seg.h"
#include "oled.h"
#include "temp.h"
#include "light.h"
#include "rgb.h"
#include "acc.h"

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

#define TEMP_HIGH_WARNING 450
#define LIGHT_LOW_WARNING 50

uint32_t msTicks = 0;

// Interval in us
static uint32_t notes[] = {
        2272, // A - 440 Hz
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

static void playNote(uint32_t note, uint32_t durationMs)
{
    uint32_t t = 0;

    if (note > 0) {
        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note/2); // us timer

            NOTE_PIN_LOW();
            Timer0_us_Wait(note/2);

            t += note;
        }
    }
    else {
    	Timer0_Wait(durationMs); // ms timer
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;
    /* number of ms */
    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
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
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        Timer0_Wait(pause);
    }
}

static uint8_t * song = (uint8_t*)"C1.C1,D2,C2,F2,E4,";

static void init_GPIO(void)
{
	// Initialize button SW4
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1<<31, 0);

    /* ---- Speaker ------> */
//    GPIO_SetDir(2, 1<<0, 1);
//    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);

    // Main tone signal : P0.26
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
    /* <---- Speaker ------ */
}

static void init_ssp(void)
{
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

void SysTick_Handler(void)
{
	msTicks++;
}

static void displayValOn7Seg(uint8_t sevenSegVal)
{
    switch(sevenSegVal)
    {
    case 0: led7seg_setChar('0', 0); break;
    case 1: led7seg_setChar('1', 0); break;
    case 2: led7seg_setChar('2', 0); break;
    case 3: led7seg_setChar('3', 0); break;
    case 4: led7seg_setChar('4', 0); break;
    case 5: led7seg_setChar('5', 0); break;
    case 6: led7seg_setChar('6', 0); break;
    case 7: led7seg_setChar('7', 0); break;
    case 8: led7seg_setChar('8', 0); break;
    case 9: led7seg_setChar('9', 0); break;
    case 10: led7seg_setChar('A', 0); break;
    case 11: led7seg_setChar('B', 0); break;
    case 12: led7seg_setChar('C', 0); break;
    case 13: led7seg_setChar('D', 0); break;
    case 14: led7seg_setChar('E', 0); break;
    case 15: led7seg_setChar('F', 0); break;
    }
    //Timer0_Wait(1000);
}

void blink_red_rgb()
{
	SysTick_Config(SystemCoreClock/1000);
	uint32_t redTicks = msTicks;
    while(1)
    {
    	rgb_setLeds(1);
    	if (msTicks - redTicks >= 333)
    	{
    		rgb_setLeds(0);
    		redTicks = msTicks;
    	}
    	if (msTicks - redTicks >= 333) break;
    }
}

void blink_blue_rgb()
{
	SysTick_Config(SystemCoreClock/1000);
	uint32_t blueTicks = msTicks;
	while(1)
	{
	 	rgb_setLeds(2);
	   	if (msTicks - blueTicks >= 333)
	   	{
	   		rgb_setLeds(0);
	   		blueTicks = msTicks;
	   	}
	   	if (msTicks - blueTicks >= 333) break;
	}
}

void blink_blue_red_rgb()
{
	SysTick_Config(SystemCoreClock/1000);
	uint32_t blueRedTicks = msTicks;
	while(1)
	{
	 	rgb_setLeds(3);
	   	if (msTicks - blueRedTicks >= 333)
	   	{
	   		rgb_setLeds(0);
	   		blueRedTicks = msTicks;
	   	}
	   	if (msTicks - blueRedTicks >= 333) break;
	}
}

int main (void) {

    uint8_t btn1 = 1, flag = 0, sevenSegVal = 0, blink_blue_flag=0, blink_red_flag=0;
    int8_t lightSensorVal = 0, tempSensorVal = 0, accelerometerVal = 0;
    int8_t acc_x, acc_y, acc_z, x, y, z;

    //init_i2c();
    init_GPIO();
    init_ssp();
    led7seg_init();
    oled_init();
    acc_init();

    oled_clearScreen(OLED_COLOR_BLACK);
    rgb_setLeds(0);
    SysTick_Config(SystemCoreClock/1000);

    uint32_t sevenSegTime = msTicks, mainTick = msTicks;
    acc_read(&x, &y, &z);

    while (1)
    {
    	btn1 = (GPIO_ReadValue(1) >> 31) & 0x01;

    	if (btn1 == 0 && (msTicks - mainTick > 1000)){
    		flag = !flag;
    		mainTick = msTicks;
    	}
    	if (blink_blue_flag && blink_red_flag) blink_blue_red_rgb();
    	else if (blink_blue_flag) blink_blue_rgb();
    	else if (blink_red_flag) blink_red_rgb();
    	if (flag)
    	{
    		if(msTicks - sevenSegTime > 1000)
    		{
    		    displayValOn7Seg(sevenSegVal);
        	    sevenSegVal = (sevenSegVal+1)%16;
        	    sevenSegTime = msTicks;
    	    }
            oled_putString(0, 0, (uint8_t *) "Monitor Mode", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
            /* Write code below to read in value of temperature sensor, light sensor and accelerometer and display it on the OLED screen */
            if (sevenSegVal==5 || sevenSegVal==10 || sevenSegVal==15)
            {

            }
            /* Write above */

            if (sevenSegVal != 15)
            {
            	acc_read(&acc_x, &acc_y, &acc_z);
            	if (acc_x!=x || acc_y!=y || acc_z!=z)
            	{
            		if (lightSensorVal < LIGHT_LOW_WARNING)
            		{
            			blink_blue_flag = 1;
            		}
            		if (tempSensorVal > TEMP_HIGH_WARNING)
            		{
            			blink_red_flag = 1;
            		}
            		x = acc_x;
            		y = acc_y;
            		z = acc_z;
            	}
            }
            else if (sevenSegVal == 15)
            {
            	//transmit data to the UART Terminal
            	//format: NNN_-_T*****_L*****_AX*****_AY*****_AZ*****\r\n
            	//send message for BLINK_RED if it's happening
            	//send message for BLINK_BLUE if it's happening
            }

    	}
    	else
    	{
    		//the 3 sensors should not be reading values here
    		//UART should not be getting any message
            led7seg_setChar('@', 0);
            blink_blue_flag = 0;
            blink_red_flag = 0;
    		oled_putString(0, 0, (uint8_t *) "Stable State", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    	}
    }
}
