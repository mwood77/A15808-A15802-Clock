/*
 * ClockDriver.c
 *
 * Created: 2020/3/7 23:49:53
 * Author : lhf
 */

 /****Attiny214 Pinout********
           --------
          |o       |
      VDD-|1     14|-GND
      PA4-|2     13|-PA3/EXTCLK
      PA5-|3     12|-PA2
      PA6-|4     11|-PA1
      PA7-|5     10|-PA0/RESET/UPDI
TOSC1/PB3-|6      9|-PB0
TOSC2/PB2-|7      8|-PB1
          |        |
	       --------
******************************/

/**********Pin mapping*********
PA0		UPDI
PA1		RX8025_SDA
PA2		RX8025_SCL
PA3		32.768KHz input
PA4		PWM for dimming (TCA0_WO4)
PA5		PWM for backlight (TCA0_WO5)
PA6		CDS sensor input (ADC6)
PA7		Potential meter input (ADC7)

PB0		Phase1 output
PB1		Phase2 output
PB2		LED         
PB3		Button      
 ******************************/

 /*********Interrupts**********
 PA3 Rising edge	32.768KHz interrupt
 TCA				1000Hz control interrupt
 ******************************/


#include <avr/io.h>
#include <avr/interrupt.h>
#include "peripherals.h"

#define PH1_ON() PORTB_OUTSET = 0b00000001
#define PH1_OFF() PORTB_OUTCLR = 0b00000001
#define PH2_ON() PORTB_OUTSET = 0b00000010
#define PH2_OFF() PORTB_OUTCLR = 0b00000010

#define LED_ON() PORTB_OUTSET = 0b00000100
#define LED_OFF() PORTB_OUTCLR = 0b00000100
#define LED_TOGGLE() PORTB_OUTTGL = 0b00000100

#define BUTTON (!(PORTB_IN & 0b00001000))


volatile uint8_t light_value = 0;
volatile uint8_t pot_value = 0;
uint16_t light_value_filtered = 0;
uint16_t pot_value_filtered = 0; // 256 times the actual value

uint8_t pwm_backlight = 0;
uint8_t pwm_dimming = 0;
uint16_t pwm_dimming_auto = 0;
uint16_t pwm_backlight_filtered = 0;
uint16_t pwm_dimming_filtered = 0;

uint8_t backlight_enable = 0;
uint8_t auto_dimming_enable = 0;

volatile uint16_t tick_cnt = 0;
volatile uint8_t pps_flag = 0;

uint8_t button_cnt = 0;

void delay_ms(uint16_t ms)
{
	tick_cnt = 0;
	while(tick_cnt < ms)asm("nop");
	tick_cnt = 0;
}

uint8_t read_eeprom(uint8_t address)
{
	return *((uint8_t*)EEPROM_START + address);
}

void write_eeprom(uint8_t address, uint8_t data)
{
	*((uint8_t*)EEPROM_START + address) = data;
	asm("cli");
	CCP = 0x9d;//Unlock self-programming
	NVMCTRL_CTRLA = 0x03;//Erase & write to EEPROM
	asm("sei");
	while(NVMCTRL_STATUS & 0x02)asm("nop");//Wait till finish
}

int main(void)
{
	PORTA_DIR = 0b00110110;
	PORTA_OUT = 0b10000000;

	PORTB_DIR = 0b00000111;
	PORTB_PIN3CTRL = 0b00001000;//Enable pull up on PB3
	PORTB_OUT = 0b00000000;
	PORTA_PIN3CTRL = 0b00000010;//32.768K interrupt, enable rising edge interrupt

	init_clk(CLK_SRC_OSC20M, CLK_DIV1);
	init_adc();
	init_tca0();

	backlight_enable = read_eeprom(0);
	auto_dimming_enable = read_eeprom(1);
	asm("sei");
    while (1) 
    {
		if(tick_cnt >= 12)//~100Hz
		{
			tick_cnt = 0;

			//Read keys
			if(!BUTTON && (button_cnt>=5) && (button_cnt < 200))//Short press, switch backlight
			{
				backlight_enable = !backlight_enable;
				write_eeprom(0, backlight_enable);
			}			
			if(button_cnt==200)//Long press, switch auto-dimming
			{
				auto_dimming_enable = !auto_dimming_enable;
				write_eeprom(1, auto_dimming_enable);
				if(auto_dimming_enable)//Flash twice
				{
					LED_ON();
					delay_ms(250);
					LED_OFF();
					delay_ms(250);
					LED_ON();
					delay_ms(250);
					LED_OFF();
					delay_ms(250);
				}
				else//Flash once
				{
					LED_ON();
					delay_ms(500);
					LED_OFF();
					delay_ms(500);
				}
			}
			button_cnt = BUTTON?((button_cnt < 255)?(button_cnt+1):255):0;

			//Dimming control
			pot_value_filtered = pot_value_filtered - (pot_value_filtered >> 4) + (pot_value << 4);
			light_value_filtered = light_value_filtered - (light_value_filtered >> 8) + light_value;

			if(auto_dimming_enable)
			{
				pwm_dimming_auto = (light_value_filtered >> 8) * (pot_value_filtered >> 8);
				pwm_dimming_auto >>= 7;
				if(pwm_dimming_auto < 30)pwm_dimming = 30;
				else if(pwm_dimming_auto > 255)pwm_dimming = 255;
				else pwm_dimming = (uint8_t)pwm_dimming_auto;
				pwm_backlight = backlight_enable?pwm_dimming:0;				
			}
			else
			{
				pwm_dimming = (uint8_t)(pot_value_filtered >> 8);
				if(pwm_dimming < 30)pwm_dimming = 30;
				pwm_backlight = backlight_enable?pwm_dimming:0;
			}

			pwm_backlight_filtered = pwm_backlight_filtered - (pwm_backlight_filtered >> 5) + (pwm_backlight << 3);
			pwm_dimming_filtered = pwm_dimming_filtered - (pwm_dimming_filtered >> 5) + (pwm_dimming << 3);

			//Flash LED
			if(pps_flag)
			{
				LED_ON();
				pps_flag = 0;
			}
			else
			{
				LED_OFF();
			}

			//Output PWM
			TCA0_SPLIT_HCMP1 =(uint8_t)(pwm_dimming_filtered >> 8);//Dimming
			TCA0_SPLIT_HCMP2 = 255 - (uint8_t)(pwm_backlight_filtered >> 8);//PWM for backlight is inversed

			ADC0_MUXPOS = 0x06;
			ADC0_COMMAND = 0x01;//Start reading sensors
		}
    }
}

uint8_t counter_dropper = 0;
uint8_t counter_divider = 0;
uint8_t  out_state = 0;
uint16_t pps_divider = 0;
//32768Hz interrupt
ISR(PORTA_PORT_vect)
{
	if(PORTA_INTFLAGS & 0b00001000)
	{
		//32768/(60Hz*4)=136...128
		//So we drop 1 clock every 32768/128=256 clocks
		if(counter_dropper == 255)counter_dropper = 0;
		else
		{
			counter_dropper++;
			if(counter_divider>= 135)
			{
				counter_divider = 0;
				out_state = out_state <= 2?(out_state + 1):0;
				switch(out_state)
				{
					case 0:
					PH1_ON();
					PH2_OFF();
					break;
					case 1:
					PH1_ON();
					PH2_ON();
					break;
					case 2:
					PH1_OFF();
					PH2_ON();
					break;
					case 3:
					PH1_OFF();
					PH2_OFF();
					break;
					default:break;
				}
			}
			else
			{
				counter_divider++;
			}
			if(pps_divider < 32767)
				pps_divider++;
			else
			{
				pps_divider = 0;
				pps_flag = 1;
			}
		}
	}
	PORTA_INTFLAGS |= 0xff;
}

//1220Hz period
ISR(TCA0_HUNF_vect)
{
	tick_cnt++;
	TCA0_SPLIT_INTFLAGS |= 0xff;
}

ISR(ADC0_RESRDY_vect)
{
	if(ADC0_MUXPOS == 0x06)//CDS value has been read
	{
		pot_value = 255 - ADC0_RESL;
		ADC0_MUXPOS = 0x07;//Read Pot value in the next cycle
		ADC0_COMMAND = 0x01;//Start next conversion
	}
	else
	{
		light_value = ADC0_RESL;
		ADC0_MUXPOS = 0x06;//Read CDS in the next cycle
	}
}
