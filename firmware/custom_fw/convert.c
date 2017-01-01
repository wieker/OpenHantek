/*
 * convert_string/convert.c -- FX2 USB data converter (filter) example. 
 * 
 * Copyright (c) 2006--2008 by Wolfgang Wieser ] wwieser (a) gmx <*> de [ 
 * 
 * This file may be distributed and/or modified under the terms of the 
 * GNU General Public License version 2 as published by the Free Software 
 * Foundation. (See COPYING.GPL for details.)
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 */

#define ALLOCATE_EXTERN
#include "fx2regs.h"



// Read TRM p.15-115 for an explanation on this. 
// A single nop is sufficient for default setup but like that we're on 
// the safe side. 
#define	NOP		__asm nop __endasm
#define	SYNCDELAY	NOP; NOP; NOP; NOP
#define SYNCDELAY3 {SYNCDELAY;SYNCDELAY;SYNCDELAY;}

#define MSB(word)  (BYTE)((((WORD)word) >> 8) & 0xff)
#define LSB(word)  (BYTE)(((WORD)word) & 0xff)

BYTE set_samplerate(BYTE rate);


void main(void)
{
	EP2CFG=0xa2;  // 1010 0010 (bulk OUT, 512 bytes, double-buffered)
	SYNCDELAY;
	FIFORESET = 0x82;  SYNCDELAY;
	EP2FIFOCFG = 0x0;
	SYNCDELAY;
	OUTPKTEND = 0x82;  SYNCDELAY;
	OUTPKTEND = 0x82;  SYNCDELAY;
	OEC = 0xff;
	OEA = 0xff;
	IOA = 0xff;
	
	main_init();
	set_samplerate(1);
	start_sampling();
	IOC = IOC & ((IOC + 1) & 0x3);

	
	for(;;)
	{
		if(!(EP2CS & (1<<2)))
		{
			IOC = (IOC + 1) & 0x3;
			OUTPKTEND = 0x82;  SYNCDELAY;
		}
		SUSPEND = 0x01;
	}
}

#define printf(...)

// change to support as many interfaces as you need
BYTE altiface = 0; // alt interface
WORD ledcounter = 0;



/* This sets three bits for each channel, one channel at a time.
 * For channel 0 we want to set bits 5, 6 & 7
 * For channel 1 we want to set bits 2, 3 & 4
 *
 * We convert the input values that are strange due to original firmware code into the value of the three bits as follows:
 * val -> bits
 * 1  -> 010b
 * 2  -> 001b
 * 5  -> 000b
 * 10 -> 011b
 *
 * The third bit is always zero since there are only four outputs connected in the serial selector chip.
 *
 * The multiplication of the converted value by 0x24 sets the relevant bits in
 * both channels and then we mask it out to only affect the channel currently
 * requested.
 */
BYTE set_voltage(BYTE channel, BYTE val)
{
    BYTE bits, mask;
    switch (val) {
    case 1:
	bits = 0x24 * 2;
	break;
    case 2:
	bits = 0x24 * 1;
	break;
    case 5:
	bits = 0x24 * 0;
	break;
    case 10:
	bits = 0x24 * 3;
	break;
    default:
	return 0;
    }

    mask = channel ? 0xe0 : 0x1c;
    //IOA = (IOA & ~mask) | (bits & mask);
    IOA = 0x80 + (0x00);
    return 1;
}

BYTE set_numchannels(BYTE numchannels)
{
    if (numchannels == 1 || numchannels == 2) {
	BYTE fifocfg = 7 + numchannels;
	EP6FIFOCFG = fifocfg;
	return 1;
    }
    return 0;
}

void clear_fifo()
{
    GPIFABORT = 0xff;
    SYNCDELAY3;
    FIFORESET = 0x80;
    SYNCDELAY3;
    //FIFORESET = 0x82;
    SYNCDELAY3;
    FIFORESET = 0x86;
    SYNCDELAY3;
    FIFORESET = 0;
}

void stop_sampling()
{
    GPIFABORT = 0xff;
    SYNCDELAY3;
    if (altiface == 0) {
	INPKTEND = 6;
    } else {
	INPKTEND = 2;
    }
}

void start_sampling()
{
    int i;
    clear_fifo();

    for (i = 0; i < 1000; i++);
    while (!(GPIFTRIG & 0x80)) {
	;
    }
    SYNCDELAY3;
    GPIFTCB1 = 0x28;
    SYNCDELAY3;
    GPIFTCB0 = 0;
    if (altiface == 0)
	GPIFTRIG = 6;
    else
	GPIFTRIG = 4;

    // set green led
    // don't clear led
    ledcounter = 0;
}

//extern __code BYTE highspd_dscr;
//extern __code BYTE fullspd_dscr;
void select_interface(BYTE alt)
{
    //const BYTE *pPacketSize = (USBCS & bmHSM ? &highspd_dscr : &fullspd_dscr)
	//+ (9 + 16*alt + 9 + 4);
    altiface = alt;
    if (alt == 0) {
	// bulk on port 6
	EP6CFG = 0xe0;
	EP6GPIFFLGSEL = 1;

	EP6AUTOINLENL = 0x00;
	EP6AUTOINLENH = 0x02;
    }
}

const struct samplerate_info {
    BYTE rate;
    BYTE wait0;
    BYTE wait1;
    BYTE opc0;
    BYTE opc1;
    BYTE out0;
    BYTE ifcfg;
} samplerates[] = {
    { 48,0x80,   0, 3, 0, 0x00, 0xea },
    { 30,0x80,   0, 3, 0, 0x00, 0xaa },
    { 24,   1,   0, 2, 1, 0x10, 0xca },
    { 16,   1,   1, 2, 0, 0x10, 0xca },
    { 12,   2,   1, 2, 0, 0x10, 0xca },
    {  8,   3,   2, 2, 0, 0x10, 0xca },
    {  4,   6,   5, 2, 0, 0x10, 0xca },
    {  2,  12,  11, 2, 0, 0x10, 0xca },
    {  1,  24,  23, 2, 0, 0x10, 0xca },
    { 50,  48,  47, 2, 0, 0x10, 0xca },
    { 20, 120, 119, 2, 0, 0x10, 0xca },
    { 10, 240, 239, 2, 0, 0x10, 0xca }
};

BYTE set_samplerate(BYTE rate)
{
    BYTE i = 0;
    while (samplerates[i].rate != rate) {
	i++;
	if (i == sizeof(samplerates)/sizeof(samplerates[0]))
	    return 0;
    }

    IFCONFIG = samplerates[i].ifcfg;

    AUTOPTRSETUP = 7;
    AUTOPTRH2 = 0xE4;
    AUTOPTRL2 = 0x00;

    /* The program for low-speed, e.g. 1 MHz, is
     * wait 24, CTL2=0, FIFO
     * wait 23, CTL2=1
     * jump 0, CTL2=1
     *
     * The program for 24 MHz is
     * wait 1, CTL2=0, FIFO
     * jump 0, CTL2=1
     *
     * The program for 30/48 MHz is:
     * jump 0, CTL2=Z, FIFO, LOOP
     */

    EXTAUTODAT2 = samplerates[i].wait0;
    EXTAUTODAT2 = samplerates[i].wait1;
    EXTAUTODAT2 = 1;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    EXTAUTODAT2 = samplerates[i].opc0;
    EXTAUTODAT2 = samplerates[i].opc1;
    EXTAUTODAT2 = 1;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    EXTAUTODAT2 = samplerates[i].out0;
    EXTAUTODAT2 = 0x11;
    EXTAUTODAT2 = 0x11;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;
    EXTAUTODAT2 = 0x00;

    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;
    EXTAUTODAT2 = 0;

    for (i = 0; i < 96; i++)
	EXTAUTODAT2 = 0;
    return 1;
}

//********************  INIT ***********************

void main_init() {
    EP4CFG = 0;
    EP8CFG = 0;

    // in idle mode tristate all outputs
    GPIFIDLECTL = 0x00;
    GPIFCTLCFG = 0x80;
    GPIFWFSELECT = 0x00;
    GPIFREADYSTAT = 0x00;

    stop_sampling();
    set_voltage(0, 1);
    set_voltage(1, 1);
    set_samplerate(1);
    set_numchannels(2);
    select_interface(0);

    printf ( "Initialization Done.\n" );
}



