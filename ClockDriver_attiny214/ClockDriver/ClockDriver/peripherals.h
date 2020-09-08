#ifndef __PERIPHERALS_H
#define __PERIPHERALS_H

#define CLK_SRC_OSC20M		0x00
#define CLK_SRC_OSCULP32K	0x01
#define CLK_SRC_XOSC32K		0x02
#define CLK_SRC_EXTCLK		0x03

#define CLK_DIV1	0x00
#define CLK_DIV2	0x01
#define CLK_DIV4	0x03
#define CLK_DIV8	0x05
#define CLK_DIV16	0x07
#define CLK_DIV32	0x09
#define CLK_DIV64	0x0b
#define CLK_DIV6	0x11
#define CLK_DIV10	0x13
#define CLK_DIV12	0x15
#define CLK_DIV24	0x17
#define CLK_DIV48	0x19
inline void init_clk(uint8_t source, uint8_t divider)
{
	CCP = 0xd8;
	CLKCTRL_MCLKCTRLA = source;
	CCP = 0xd8;//Unlock protected I/O registers
	CLKCTRL_MCLKCTRLB = divider;
}

inline void init_tca0()
{
	TCA0_SPLIT_CTRLA = 0b00001011;//DIV64 enabled, freq = 20M/64/256=1220Hz
	TCA0_SPLIT_CTRLB = 0b01100000;//Enable high-byte compare 1 & 2 (WO4 & WO5)
	TCA0_SPLIT_CTRLC = 0b00000000;
	TCA0_SPLIT_CTRLD = 0b00000001;//Enable split mode
	TCA0_SPLIT_INTCTRL = 0b00000010;//Enable HUNF interrupt

	TCA0_SPLIT_HPER = 255;//Only use higher half for PWM & periodic interrupt
	TCA0_SPLIT_LPER = 0;

	TCA0_SPLIT_HCMP1 = 100; //Dimming PWM
	TCA0_SPLIT_HCMP2 = 100; //Backlight PWM

}

inline void init_adc()
{
	ADC0_CTRLA = 0b00000101;//Enable ADC, 8bit mode
	ADC0_CTRLB = 0b00000000;//No accumulation
	ADC0_CTRLC = 0b01010100;//Select VDD as reference, CLK=DIV32
	ADC0_CTRLD = 0b00110000;//InitDelay = 16 CLKADC, enable ASDV
	ADC0_CTRLE = 0b00000000;//Disable Window comparator
	ADC0_SAMPCTRL = 0b00000000;//Sample length is 2 CLKADC
	ADC0_EVCTRL = 0b00000000;//No event used
	ADC0_INTCTRL = 0b00000001;//Result ready interrupt

	ADC0_MUXPOS = 0x06;//select CDS as default
}

#endif