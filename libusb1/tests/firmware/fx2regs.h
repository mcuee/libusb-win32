/*

Register definitons for the Cypress FX2 processor.

Copyright (c) 2006 Christer Weinigel, Nordnav Technologies AB

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This file is a from scratch rewrite of the register
definitions for the Cypress FX2 processor.  The reason for
doing this is that I was unsure of the copyright of all the
different versions of fx2regs.h floating around on the net,
some where copyrighted by Cypress with no clear license,
others were apparently based on the Cypress files but
distributed under many varying and sometimes unclear licenses.
Most of the files were modified in one way or another by more
or less anonymous authors.  I wanted an unencumbered header
file that I can use for anything, so I decided to rewrite it
from scratch and release it under a X11/MIT license.

I started with the EZ-USB FX2 Technical Reference Manual (FX2
TRM) and wrote a Python script that outputs all the register
definitions in this file.  I then compared the resulting file
to the original fx2regs.h from Cypress and to fx2regs.h from
the GNU Universal Software Radio Peripheral (USRP).  When the
files differed I either changed my definition to match the
others or documented why this file differs.  Compared to the
Cypress header files I've added a lot of symbolic constants.
When I've done this I've usually tried to match the
definitions from the USRP project if they existed there.

Some other differences are:

I have skipped a few definitions found in the Cypress header
file that are not documented in the FX2 TRM.  Some of these
seem to be definitions for old FX2 hardware revisions, others
seem to be 8051 registers that don't even exist in the EZ-USB
family.  The registers I've skipped are:

APTR1[HL], AUTOPTR1[HL]
EXTAUTODAT[12]
EP[2-8]GPIFTCH
SPC_FNC
OUT7BUF
CT[1-4], DBUG, TESTCFG, USBTEST

I have added definition for AUTODAT1 and AUTODAT2, even though
they are not documented in the FX2 TRM.  Definitions for these
exist in the Keil C header files though and I wanted to add
them for completenedd.  Do they exist for real on the FX2 or
should I remove them?

A bunch of invalid sbit definitions from the Cypress header
file have been removed (only SFRs with addresses evenly
divisible by eight can be bit-addressable).

The Cypress header file calls the waveform data
GPIF_WAVE_DATA, I've used the name WAVEDATA which matches the
FX2 TRM instead.  I've skipped the Cypress RES_WAVEDATA_END
since I didn't see any use for it.

For some reason INT4IN in the Cypress header was called just
that, without a bm in front.  I've called it bmINT4SRC to
match the FX2 TRM and to have a bm prefix as all other
bitmasks.

The bit called INT6 in the EICON register has been renamed to
EINT6 to avoid a name clash with the INT6 bit in the PORTECFG
register.

I've renamed all I2C bitmask names to start with bmI2C, I did
this so that the generic names DONE, ID0 and ID1 would not
conflict with all other uses of them in the FX2 TRM.  For
symmetry I added a bmI2C prefix to the rest of the I2C
registers too.

The DONE bit in GPIFTRIG is called bmIDLE the same way the
USRP project does to avoid a conflict with all other uses of
DONE in the FX2 TRM.

The DONE bit is GPIFIDLECS is called bmIDLESTATE to avoid a
conflict with all other uses of DONE in the FX2 TRM.

The defines bmNOAUTOARM and bmSKIPCOMMIT defines from the
cypress header files did not match the FX2 TRM.  Use the
defines bmDYN_OUT and bmENH_PKT that match the FX2 TRM
instead.

I've added sbit registers called for most bit-adressadble
registers that didn't have any before.  For example, IOA1 is
bit 1 of the IOA register.  Is there a better way of doing
this?  Does the SDCC compiler have a syntax for getting to a
bit of a bit-adressable register without having to create a
sbit register?
*/

#ifndef _FX2REGS_H_
#define _FX2REGS_H_ 1

/* Special Function Registers (SFR) */

sfr at 0x80 IOA;                /* I/O Port A (bit-adressable) */
#define bmIOA0 0x01
#define bmIOA1 0x02
#define bmIOA2 0x04
#define bmIOA3 0x08
#define bmIOA4 0x10
#define bmIOA5 0x20
#define bmIOA6 0x40
#define bmIOA7 0x80
sbit at 0x80 + 0 IOA0;
sbit at 0x80 + 1 IOA1;
sbit at 0x80 + 2 IOA2;
sbit at 0x80 + 3 IOA3;
sbit at 0x80 + 4 IOA4;
sbit at 0x80 + 5 IOA5;
sbit at 0x80 + 6 IOA6;
sbit at 0x80 + 7 IOA7;
sfr at 0x81 SP;                 /* Stack Pointer */
sfr at 0x82 DPL;                /* Data Pointer Low */
sfr at 0x83 DPH;                /* Data Pointer High */
sfr at 0x84 DPL1;               /* Data Pointer 1 Low */
sfr at 0x85 DPH1;               /* Data Pointer 1 High */
sfr at 0x86 DPS;                /* Data Pointer Select */
#define bmSEL 0x01          /* Data Pointer Select */
sfr at 0x87 PCON;               /* Power Control */
#define bmPCON_IDLE 0x01
#define bmPCON_STOP 0x02
#define bmPCON_GF0 0x04
#define bmPCON_GF1 0x08
#define bmPCON_SMOD0 0x80
sfr at 0x88 TCON;               /* Timer Control (bit-adressable) */
#define bmIT0 0x01
#define bmIE0 0x02
#define bmIT1 0x04
#define bmIE1 0x08
#define bmTR0 0x10
#define bmTF0 0x20
#define bmTR1 0x40
#define bmTF1 0x80
sbit at 0x88 + 0 IT0;
sbit at 0x88 + 1 IE0;
sbit at 0x88 + 2 IT1;
sbit at 0x88 + 3 IE1;
sbit at 0x88 + 4 TR0;
sbit at 0x88 + 5 TF0;
sbit at 0x88 + 6 TR1;
sbit at 0x88 + 7 TF1;
sfr at 0x89 TMOD;               /* Timer Mode */
#define bmM0 0x01           /* Timer Mode Select Bit 0 */
#define bmM1 0x02           /* Timer Mode Select Bit 1 */
#define bmCT 0x04           /* Timer Counter/Timer Select */
#define bmGATE 0x08         /* Timer Gate Control */
sfr at 0x8a TL0;                /* Timer 0 Low */
sfr at 0x8b TL1;                /* Timer 1 Low */
sfr at 0x8c TH0;                /* Timer 0 High */
sfr at 0x8d TH1;                /* Timer 1 High */
sfr at 0x8e CKCON;              /* Clock Control */
#define bmMD0 0x01
#define bmMD1 0x02
#define bmMD2 0x04
#define bmT0M 0x08
#define bmT1M 0x10
#define bmT2M 0x20
sfr at 0x90 IOB;                /* I/O Port B (bit-adressable) */
#define bmIOB0 0x01
#define bmIOB1 0x02
#define bmIOB2 0x04
#define bmIOB3 0x08
#define bmIOB4 0x10
#define bmIOB5 0x20
#define bmIOB6 0x40
#define bmIOB7 0x80
sbit at 0x90 + 0 IOB0;
sbit at 0x90 + 1 IOB1;
sbit at 0x90 + 2 IOB2;
sbit at 0x90 + 3 IOB3;
sbit at 0x90 + 4 IOB4;
sbit at 0x90 + 5 IOB5;
sbit at 0x90 + 6 IOB6;
sbit at 0x90 + 7 IOB7;
sfr at 0x91 EXIF;               /* External Interrupt Flag */
#define bmEXIF_USBINT 0x10
#define bmEXIF_I2CINT 0x20
#define bmEXIF_IE4 0x40
#define bmEXIF_IE5 0x80
sfr at 0x92 MPAGE;              /* MOVX Page */
sfr at 0x98 SCON0;              /* Serial Control 0 (bit-adressable) */
#define bmRI 0x01
#define bmTI 0x02
#define bmRB8 0x04
#define bmTB8 0x08
#define bmREN 0x10
#define bmSM2 0x20
#define bmSM1 0x40
#define bmSM0 0x80
sbit at 0x98 + 0 RI;
sbit at 0x98 + 1 TI;
sbit at 0x98 + 2 RB8;
sbit at 0x98 + 3 TB8;
sbit at 0x98 + 4 REN;
sbit at 0x98 + 5 SM2;
sbit at 0x98 + 6 SM1;
sbit at 0x98 + 7 SM0;
sfr at 0x99 SBUF0;              /* Serial Buffer 1 */
sfr at 0x9a AUTOPTRH1;          /* Autopointer 1 Address High */
sfr at 0x9b AUTOPTRL1;          /* Autopointer 1 Address Low */
sfr at 0x9c AUTODAT1;           /* Autopointer 1 Data (Warning, this register 
                                   is not documented in the FX2 TRM) */
sfr at 0x9d AUTOPTRH2;          /* Autopointer 2 Address Low */
sfr at 0x9e AUTOPTRL2;          /* Autopointer 2 Address Low */
sfr at 0x9f AUTODAT2;           /* Autopointer 2 Data (Warning, this register 
                                   is not documented in the FX2 TRM) */
sfr at 0xa0 IOC;                /* I/O Port C (bit-adressable) */
#define bmIOC0 0x01
#define bmIOC1 0x02
#define bmIOC2 0x04
#define bmIOC3 0x08
#define bmIOC4 0x10
#define bmIOC5 0x20
#define bmIOC6 0x40
#define bmIOC7 0x80
sbit at 0xa0 + 0 IOC0;
sbit at 0xa0 + 1 IOC1;
sbit at 0xa0 + 2 IOC2;
sbit at 0xa0 + 3 IOC3;
sbit at 0xa0 + 4 IOC4;
sbit at 0xa0 + 5 IOC5;
sbit at 0xa0 + 6 IOC6;
sbit at 0xa0 + 7 IOC7;
sfr at 0xa1 INT2CLR;            /* Interrupt 2 Clear */
sfr at 0xa2 INT4CLR;            /* Interrupt 4 Clear */
sfr at 0xa8 IE;                 /* Interrupt Enable (bit-adressable) */
#define bmEX0 0x01
#define bmET0 0x02
#define bmEX1 0x04
#define bmET1 0x08
#define bmES0 0x10
#define bmET2 0x20
#define bmES1 0x40
#define bmEA 0x80
sbit at 0xa8 + 0 EX0;
sbit at 0xa8 + 1 ET0;
sbit at 0xa8 + 2 EX1;
sbit at 0xa8 + 3 ET1;
sbit at 0xa8 + 4 ES0;
sbit at 0xa8 + 5 ET2;
sbit at 0xa8 + 6 ES1;
sbit at 0xa8 + 7 EA;
sfr at 0xaa EP2468STAT;         /* Endpoint 2,4,6,8 Status Flags */
#define bmEP2EMPTY 0x01
#define bmEP2FULL 0x02
#define bmEP4EMPTY 0x04
#define bmEP4FULL 0x08
#define bmEP6EMPTY 0x10
#define bmEP6FULL 0x20
#define bmEP8EMPTY 0x40
#define bmEP8FULL 0x80
sfr at 0xab EP24FIFOFLGS;       /* Endpoint 2,4 Slave FIFO Status Flags */
#define bmEP2FF 0x01
#define bmEP2EF 0x02
#define bmEP2PF 0x04
#define bmEP4FF 0x10
#define bmEP4EF 0x20
#define bmEP4PF 0x40
sfr at 0xac EP68FIFOFLGS;       /* Endpoint 6,8 Slave FIFO Status Flags */
#define bmEP6FF 0x01
#define bmEP6EF 0x02
#define bmEP6PF 0x04
#define bmEP8FF 0x10
#define bmEP8EF 0x20
#define bmEP8PF 0x40
sfr at 0xaf AUTOPTRSETUP;       /* Autopointer Setup */
#define bmAPTREN 0x01
#define bmAPTR1INC 0x02
#define bmAPTR2INC 0x04
sfr at 0xb0 IOD;                /* I/O Port D (bit-adressable) */
#define bmIOD0 0x01
#define bmIOD1 0x02
#define bmIOD2 0x04
#define bmIOD3 0x08
#define bmIOD4 0x10
#define bmIOD5 0x20
#define bmIOD6 0x40
#define bmIOD7 0x80
sbit at 0xb0 + 0 IOD0;
sbit at 0xb0 + 1 IOD1;
sbit at 0xb0 + 2 IOD2;
sbit at 0xb0 + 3 IOD3;
sbit at 0xb0 + 4 IOD4;
sbit at 0xb0 + 5 IOD5;
sbit at 0xb0 + 6 IOD6;
sbit at 0xb0 + 7 IOD7;
sfr at 0xb1 IOE;                /* I/O Port E */
sfr at 0xb2 OEA;                /* Port A Output Enable */
sfr at 0xb3 OEB;                /* Port B Output Enable */
sfr at 0xb4 OEC;                /* Port C Output Enable */
sfr at 0xb5 OED;                /* Port D Output Enable */
sfr at 0xb6 OEE;                /* Port E Output Enable */
sfr at 0xb8 IP;                 /* Interrupt Priority (bit-adressable) */
#define bmPX0 0x01
#define bmPT0 0x02
#define bmPX1 0x04
#define bmPT1 0x08
#define bmPS0 0x10
#define bmPT2 0x20
#define bmPS1 0x40
sbit at 0xb8 + 0 PX0;
sbit at 0xb8 + 1 PT0;
sbit at 0xb8 + 2 PX1;
sbit at 0xb8 + 3 PT1;
sbit at 0xb8 + 4 PS0;
sbit at 0xb8 + 5 PT2;
sbit at 0xb8 + 6 PS1;
sfr at 0xba EP01STAT;           /* Endpoint 0,1 Status Flags */
#define bmEP0BSY 0x01
#define bmEP1OUTBSY 0x02
#define bmEP1INBSY 0x04
sfr at 0xbb GPIFTRIG;           /* Endpoint 2,4,6,8 GPIF Slave FIFO Trigger */
#define bmGPIF_WRITE 0x00
#define bmGPIF_EP2_START 0x00
#define bmGPIF_EP4_START 0x01
#define bmGPIF_EP6_START 0x02
#define bmGPIF_EP8_START 0x03
#define bmGPIF_READ 0x04
#define bmGPIF_IDLE 0x80    /* Changed to avoid conflicts with all other 
                               DONE bits. */
sfr at 0xbd GPIFSGLDATH;        /* GPIF Data High (16-bit mode only) */
sfr at 0xbe GPIFSGLDATLX;       /* GPIF Data Low w/Trigger */
sfr at 0xbf GPIFSGLDATLNOX;     /* GPIF Data Low w/no Trigger */
sfr at 0xc0 SCON1;              /* Serial Control 1 (bit-adressable) */
#define bmRI1 0x01
#define bmTI1 0x02
#define bmRB81 0x04
#define bmTB81 0x08
#define bmREN1 0x10
#define bmSM21 0x20
#define bmSM11 0x40
#define bmSM01 0x80
sbit at 0xc0 + 0 RI1;
sbit at 0xc0 + 1 TI1;
sbit at 0xc0 + 2 RB81;
sbit at 0xc0 + 3 TB81;
sbit at 0xc0 + 4 REN1;
sbit at 0xc0 + 5 SM21;
sbit at 0xc0 + 6 SM11;
sbit at 0xc0 + 7 SM01;
sfr at 0xc1 SBUF1;              /* Serial Buffer 1 */
sfr at 0xc8 T2CON;              /* Timer 2 Control (bit-adressable) */
#define bmCP_RL2 0x01
#define bmC_T2 0x02
#define bmTR2 0x04
#define bmEXEN2 0x08
#define bmTCLK 0x10
#define bmRCLK 0x20
#define bmEXF2 0x40
#define bmTF2 0x80
sbit at 0xc8 + 0 CP_RL2;
sbit at 0xc8 + 1 C_T2;
sbit at 0xc8 + 2 TR2;
sbit at 0xc8 + 3 EXEN2;
sbit at 0xc8 + 4 TCLK;
sbit at 0xc8 + 5 RCLK;
sbit at 0xc8 + 6 EXF2;
sbit at 0xc8 + 7 TF2;
sfr at 0xca RCAP2L;             /* Timer 2 Reload/Capture Low */
sfr at 0xcb RCAP2H;             /* Timer 2 Reload/Capture High */
sfr at 0xcc TL2;                /* Timer 2 Count Low */
sfr at 0xcd TH2;                /* Timer 2 Count High */
sfr at 0xd0 PSW;                /* Program Status Word (bit-adressable) */
#define bmP 0x01            /* Parity Flag */
#define bmF1 0x02           /* User Flag 1 */
#define bmOV 0x04           /* Overflow Flag */
#define bmRS0 0x08          /* Register Bank Select Bit 0 */
#define bmRS1 0x10          /* Register Bank Select bit 1 */
#define bmF0 0x20           /* User Flag 0 */
#define bmAC 0x40           /* Auxiliary Carry Flag */
#define bmCY 0x80           /* Carry Flag */
sbit at 0xd0 + 0 P;
sbit at 0xd0 + 1 F1;
sbit at 0xd0 + 2 OV;
sbit at 0xd0 + 3 RS0;
sbit at 0xd0 + 4 RS1;
sbit at 0xd0 + 5 F0;
sbit at 0xd0 + 6 AC;
sbit at 0xd0 + 7 CY;
sfr at 0xd8 EICON;              /* External Interrupt Control (bit-adressable) 
                                 */
#define bmEINT6 0x08        /* Called INT6 in the FX2 TRM.  Renamed to 
                               EINT6 because of a name clash with INT6 in the PORTECFG register. */
#define bmRESI 0x10
#define bmERESI 0x20
#define bmSMOD1 0x80
sbit at 0xd8 + 3 EINT6;
sbit at 0xd8 + 4 RESI;
sbit at 0xd8 + 5 ERESI;
sbit at 0xd8 + 7 SMOD1;
sfr at 0xe0 ACC;                /* Accumulator Register (bit-adressable) */
sfr at 0xe8 EIE;                /* External Interrupt Enable (bit-adressable) */
#define bmEUSB 0x01
#define bmEI2C 0x02
#define bmEIEX4 0x04
#define bmEIEX5 0x08
#define bmEIEX6 0x10
sbit at 0xe8 + 0 EUSB;
sbit at 0xe8 + 1 EI2C;
sbit at 0xe8 + 2 EIEX4;
sbit at 0xe8 + 3 EIEX5;
sbit at 0xe8 + 4 EIEX6;
sfr at 0xf0 B;                  /* B Register (bit-adressable) */
sfr at 0xf8 EIP;                /* External Interrupt Priority (bit-adressable) 
                                 */
#define bmPUSB 0x01
#define bmPI2C 0x02
#define bmEIPX4 0x04
#define bmEIPX5 0x08
#define bmEIPX6 0x10
sbit at 0xf8 + 0 PUSB;
sbit at 0xf8 + 1 PI2C;
sbit at 0xf8 + 2 EIPX4;
sbit at 0xf8 + 3 EIPX5;
sbit at 0xf8 + 4 EIPX6;

/* FX2 Registers */

volatile xdata at 0xe400 unsigned char WAVEDATA[128]; /* GPIF Waveform 
                                                         Descriptor 0,1,2,3 Data.  Note, this area is called GPIF_WAVE_DATA in the 
                                                         Cypress header.). */
volatile xdata at 0xe600 unsigned char CPUCS; /* CPU Control & Status */
#define bmCLKSPD12MHZ 0x00
#define bm8051RES 0x01
#define bmCLKOE 0x02
#define bmCLKINV 0x04
#define bmCLKSPD0 0x08
#define bmCLKSPD24MHZ 0x08
#define bmCLKSPD1 0x10
#define bmCLKSPD48MHZ 0x10
#define bmCLKSPD 0x18
#define bmPRTCSTB 0x20      /* Called PORTCSTB in the FX2 TRM */
volatile xdata at 0xe601 unsigned char IFCONFIG; /* Interface Configuration 
                                                    (Ports, GPIF, slave FIFOs) */
#define bmIFPORT 0x00
#define bmIFCFG0 0x01
#define bmIFCFG1 0x02
#define bmIFGPIF 0x02
#define bmIFCFGMASK 0x03
#define bmIFSLAVE 0x03
#define bmGSTATE 0x04
#define bmASYNC 0x08
#define bmIFCLKPOL 0x10
#define bmIFCLKOE 0x20
#define bm3048MHZ 0x40
#define bmIFCLKSRC 0x80
volatile xdata at 0xe602 unsigned char PINFLAGSAB; /* Slave FIFO FLAGA and 
                                                      FLAGB Pin Configuration */
#define bmFLAGA_INDEXED 0x00
#define bmFLAGB_INDEXED 0x00
#define bmFLAGA_EP2PF 0x04
#define bmFLAGA_EP4PF 0x05
#define bmFLAGA_EP6PF 0x06
#define bmFLAGA_EP8PF 0x07
#define bmFLAGA_EP2EF 0x08
#define bmFLAGA_EP4EF 0x09
#define bmFLAGA_EP6EF 0x0a
#define bmFLAGA_EP8EF 0x0b
#define bmFLAGA_EP2FF 0x0c
#define bmFLAGA_EP4FF 0x0d
#define bmFLAGA_EP6FF 0x0e
#define bmFLAGA_EP8FF 0x0f
#define bmFLAGB_EP2PF 0x40
#define bmFLAGB_EP4PF 0x50
#define bmFLAGB_EP6PF 0x60
#define bmFLAGB_EP8PF 0x70
#define bmFLAGB_EP2EF 0x80
#define bmFLAGB_EP4EF 0x90
#define bmFLAGB_EP6EF 0xa0
#define bmFLAGB_EP8EF 0xb0
#define bmFLAGB_EP2FF 0xc0
#define bmFLAGB_EP4FF 0xd0
#define bmFLAGB_EP6FF 0xe0
#define bmFLAGB_EP8FF 0xf0
volatile xdata at 0xe603 unsigned char PINFLAGSCD; /* Slave FIFO FLAGC and 
                                                      FLAGD Pin Configuration */
#define bmFLAGC_INDEXED 0x00
#define bmFLAGD_INDEXED 0x00
#define bmFLAGC_EP2PF 0x04
#define bmFLAGC_EP4PF 0x05
#define bmFLAGC_EP6PF 0x06
#define bmFLAGC_EP8PF 0x07
#define bmFLAGC_EP2EF 0x08
#define bmFLAGC_EP4EF 0x09
#define bmFLAGC_EP6EF 0x0a
#define bmFLAGC_EP8EF 0x0b
#define bmFLAGC_EP2FF 0x0c
#define bmFLAGC_EP4FF 0x0d
#define bmFLAGC_EP6FF 0x0e
#define bmFLAGC_EP8FF 0x0f
#define bmFLAGD_EP2PF 0x40
#define bmFLAGD_EP4PF 0x50
#define bmFLAGD_EP6PF 0x60
#define bmFLAGD_EP8PF 0x70
#define bmFLAGD_EP2EF 0x80
#define bmFLAGD_EP4EF 0x90
#define bmFLAGD_EP6EF 0xa0
#define bmFLAGD_EP8EF 0xb0
#define bmFLAGD_EP2FF 0xc0
#define bmFLAGD_EP4FF 0xd0
#define bmFLAGD_EP6FF 0xe0
#define bmFLAGD_EP8FF 0xf0
volatile xdata at 0xe604 unsigned char FIFORESET; /* Restore FIFOS to default 
                                                     state */
#define bmNAKALL 0x80
volatile xdata at 0xe605 unsigned char BREAKPT; /* Breakpoint Control */
#define bmBPEN 0x02
#define bmBPPULSE 0x04
#define bmBREAK 0x08
volatile xdata at 0xe606 unsigned char BPADDRH; /* Breakpoint Address High */
volatile xdata at 0xe607 unsigned char BPADDRL; /* Breakpoint Address Low */
volatile xdata at 0xe608 unsigned char UART230; /* 230 kBaud Internally 
                                                   Generated Reference Clock */
#define bm230UART0 0x01
#define bm230UART1 0x02
volatile xdata at 0xe609 unsigned char FIFOPINPOLAR; /* Slave FIFO Interface 
                                                        Pins Polarity  */
#define bmFF 0x01
#define bmEF 0x02
#define bmSLWR 0x04
#define bmSLRD 0x08
#define bmSLOE 0x10
#define bmPKTEND 0x20
volatile xdata at 0xe60a unsigned char REVID; /* Chip Revision */
volatile xdata at 0xe60b unsigned char REVCTL; /* Chip Revision Control */
#define bmENH_PKT 0x01      /* Called bmSKIPCOMMIT in the Cypress header */
#define bmDYN_OUT 0x02      /* Called bmNOAUTOARM in the Cypress header. */
volatile xdata at 0xe60c unsigned char GPIFHOLDTIME; /* MSTB Hold Time (for 
                                                        UDMA).  Note, this register is called GPIFHOLDAMOUNT in the Cypress header. */
#define bmHOLDTIME0 0x01
#define bmHOLDTIME1 0x02
volatile xdata at 0xe610 unsigned char EP1OUTCFG; /* Endpoint 1 OUT 
                                                     Configuration */
volatile xdata at 0xe611 unsigned char EP1INCFG; /* Endpoint 1 IN Configuration 
                                                  */
volatile xdata at 0xe612 unsigned char EP2CFG; /* Endpoint 2 Configuration */
#define bmQUADBUF 0x00
#define bmOUT 0x00
#define bmBUF0 0x01
#define bmISOCHRONOUS 0x01
#define bmBUF1 0x02
#define bmBULK 0x02
#define bmDOUBLEBUF 0x02
#define bmINTERRUPT 0x03
#define bmTRIPLEBUF 0x03
#define bmSIZE 0x08
#define bm1KBUF 0x08
#define bmTYPE0 0x10
#define bmTYPE1 0x20
#define bmDIR 0x40
#define bmIN 0x40
#define bmVALID 0x80
volatile xdata at 0xe613 unsigned char EP4CFG; /* Endpoint 4 Configuration */
volatile xdata at 0xe614 unsigned char EP6CFG; /* Endpoint 6 Configuration */
volatile xdata at 0xe615 unsigned char EP8CFG; /* Endpoint 8 Configuration */
volatile xdata at 0xe618 unsigned char EP2FIFOCFG; /* Endpoint 2 FIFO 
                                                      Configuration */
#define bmWORDWIDE 0x01
#define bmZEROLENIN 0x04
#define bmAUTOIN 0x08
#define bmAUTOOUT 0x10
#define bmOEP 0x20          /* Called OEP1 in the FX2 TRM */
#define bmINFM 0x40         /* Called INFM1 in the FX2 TRM */
volatile xdata at 0xe619 unsigned char EP4FIFOCFG; /* Endpoint 4 FIFO 
                                                      Configuration */
volatile xdata at 0xe61a unsigned char EP6FIFOCFG; /* Endpoint 6 FIFO 
                                                      Configuration */
volatile xdata at 0xe61b unsigned char EP8FIFOCFG; /* Endpoint 8 FIFO 
                                                      Configuration */
volatile xdata at 0xe620 unsigned char EP2AUTOINLENH; /* Endpoint 2 AUTOIN 
                                                         Packet Length High */
volatile xdata at 0xe621 unsigned char EP2AUTOINLENL; /* Endpoint 2 AUTOIN 
                                                         Packet Length Low */
volatile xdata at 0xe622 unsigned char EP4AUTOINLENH; /* Endpoint 4 AUTOIN 
                                                         Packet Length High */
volatile xdata at 0xe623 unsigned char EP4AUTOINLENL; /* Endpoint 4 AUTOIN 
                                                         Packet Length Low */
volatile xdata at 0xe624 unsigned char EP6AUTOINLENH; /* Endpoint 6 AUTOIN 
                                                         Packet Length High */
volatile xdata at 0xe625 unsigned char EP6AUTOINLENL; /* Endpoint 6 AUTOIN 
                                                         Packet Length Low */
volatile xdata at 0xe626 unsigned char EP8AUTOINLENH; /* Endpoint 8 AUTOIN 
                                                         Packet Length High */
volatile xdata at 0xe627 unsigned char EP8AUTOINLENL; /* Endpoint 8 AUTOIN 
                                                         Packet Length Low */
volatile xdata at 0xe630 unsigned char EP2FIFOPFH; /* Endpoint 2 FIFO 
                                                      Programmable-Level Flag High */
#define bmPKTSTAT 0x40
#define bmDECIS 0x80
volatile xdata at 0xe631 unsigned char EP2FIFOPFL; /* Endpoint 2 FIFO 
                                                      Programmable-Level Flag Low */
volatile xdata at 0xe632 unsigned char EP4FIFOPFH; /* Endpoint 4 FIFO 
                                                      Programmable-Level Flag High */
volatile xdata at 0xe633 unsigned char EP4FIFOPFL; /* Endpoint 4 FIFO 
                                                      Programmable-Level Flag Low */
volatile xdata at 0xe634 unsigned char EP6FIFOPFH; /* Endpoint 6 FIFO 
                                                      Programmable-Level Flag High */
volatile xdata at 0xe635 unsigned char EP6FIFOPFL; /* Endpoint 6 FIFO 
                                                      Programmable-Level Flag Low */
volatile xdata at 0xe636 unsigned char EP8FIFOPFH; /* Endpoint 8 FIFO 
                                                      Programmable-Level Flag High */
volatile xdata at 0xe637 unsigned char EP8FIFOPFL; /* Endpoint 8 FIFO 
                                                      Programmable-Level Flag Low */
volatile xdata at 0xe640 unsigned char EP2ISOINPKTS; /* Endpoint 2 (if ISO) IN 
                                                        Packets Per Frame */
volatile xdata at 0xe641 unsigned char EP4ISOINPKTS; /* Endpoint 4 (if ISO) IN 
                                                        Packets Per Frame */
volatile xdata at 0xe642 unsigned char EP6ISOINPKTS; /* Endpoint 6 (if ISO) IN 
                                                        Packets Per Frame */
volatile xdata at 0xe643 unsigned char EP8ISOINPKTS; /* Endpoint 8 (if ISO) IN 
                                                        Packets Per Frame */
volatile xdata at 0xe648 unsigned char INPKTEND; /* Force IN Packet End */
#define bmSKIP 0x80
volatile xdata at 0xe649 unsigned char OUTPKTEND; /* Force OUT Packet End */
volatile xdata at 0xe650 unsigned char EP2FIFOIE; /* EP2 FIFO Flag Interrupt 
                                                     Enable (INT4) */
#define bmINT_FF 0x01
#define bmINT_EF 0x02
#define bmINT_PF 0x04
#define bmINT_EDGEPF 0x08
volatile xdata at 0xe651 unsigned char EP2FIFOIRQ; /* EP2 FIFO Interrupt 
                                                      Request (INT4) */
volatile xdata at 0xe652 unsigned char EP4FIFOIE; /* EP4 FIFO Flag Interrupt 
                                                     Enable (INT4) */
volatile xdata at 0xe653 unsigned char EP4FIFOIRQ; /* EP4 FIFO Interrupt 
                                                      Request (INT4) */
volatile xdata at 0xe654 unsigned char EP6FIFOIE; /* EP6 FIFO Flag Interrupt 
                                                     Enable (INT4) */
volatile xdata at 0xe655 unsigned char EP6FIFOIRQ; /* EP6 FIFO Interrupt 
                                                      Request (INT4) */
volatile xdata at 0xe656 unsigned char EP8FIFOIE; /* EP8 FIFO Flag Interrupt 
                                                     Enable (INT4) */
volatile xdata at 0xe657 unsigned char EP8FIFOIRQ; /* EP8 FIFO Interrupt 
                                                      Request (INT4) */
volatile xdata at 0xe658 unsigned char IBNIE; /* IN-BULK-NAK Interrupt Enable 
                                                 (INT2) */
#define bmEP0IBN 0x01
#define bmEP1IBN 0x02
#define bmEP2IBN 0x04
#define bmEP4IBN 0x08
#define bmEP6IBN 0x10
#define bmEP8IBN 0x20
volatile xdata at 0xe659 unsigned char IBNIRQ; /* IN-BULK-NAK Interrupt Request 
                                                  (INT2) */
volatile xdata at 0xe65a unsigned char NAKIE; /* Endpoint Ping-NAK/IBN 
                                                 Interrupt Enable (INT2) */
#define bmIBN 0x01
#define bmEP0PING 0x04
#define bmEP1PING 0x08
#define bmEP2PING 0x10
#define bmEP4PING 0x20
#define bmEP6PING 0x40
#define bmEP8PING 0x80
volatile xdata at 0xe65b unsigned char NAKIRQ; /* Endpoint Ping-NAK/IBN 
                                                  Interrupt Request (INT2) */
volatile xdata at 0xe65c unsigned char USBIE; /* USB Interrupt Enable (INT2) */
#define bmSUDAV 0x01
#define bmSOF 0x02
#define bmSUTOK 0x04
#define bmSUSP 0x08
#define bmURES 0x10
#define bmHSGRANT 0x20
#define bmEP0ACK 0x40
volatile xdata at 0xe65d unsigned char USBIRQ; /* USB Interrupt Request (INT2) 
                                                */
volatile xdata at 0xe65e unsigned char EPIE; /* Endpoint Interrupt Enable 
                                                (INT2) */
#define bmEPIE_EP0IN 0x01
#define bmEPIE_EP0OUT 0x02
#define bmEPIE_EP1IN 0x04
#define bmEPIE_EP1OUT 0x08
#define bmEPIE_EP2 0x10
#define bmEPIE_EP4 0x20
#define bmEPIE_EP6 0x40
#define bmEPIE_EP8 0x80
volatile xdata at 0xe65f unsigned char EPIRQ; /* Endpoint Interrupt Request 
                                                 (INT2) */
volatile xdata at 0xe660 unsigned char GPIFIE; /* GPIF Interrupt Enable (INT4) 
                                                */
#define bmGPIFDONE 0x01
#define bmGPIFWF 0x02
volatile xdata at 0xe661 unsigned char GPIFIRQ; /* GPIF Interrupt Request 
                                                   (INT4) */
volatile xdata at 0xe662 unsigned char USBERRIE; /* USB Error Interrupt Enable 
                                                    (INT2) */
#define bmERRLIMIT 0x01
#define bmISOEP2 0x10
#define bmISOEP4 0x20
#define bmISOEP6 0x40
#define bmISOEP8 0x80
volatile xdata at 0xe663 unsigned char USBERRIRQ; /* USB Error Request (INT4) */
volatile xdata at 0xe664 unsigned char ERRCNTLIM; /* USB Error Counter and 
                                                     Limit */
#define bmLIMIT 0x0f
#define bmEC 0xf0
volatile xdata at 0xe665 unsigned char CLRERRCNT; /* Clear Error Count EC3:0 */
volatile xdata at 0xe666 unsigned char INT2IVEC; /* INTERRUPT 2 (USB) 
                                                    Autovector */
volatile xdata at 0xe667 unsigned char INT4IVEC; /* INTERRUPT 4 (FIFO & GPIF) 
                                                    Autovector */
volatile xdata at 0xe668 unsigned char INTSETUP; /* INT 2 & INT 4 Setup */
#define bmAV4EN 0x01
#define bmINT4SRC 0x02      /* Note, this bitmask is named INT4IN in the 
                               Cypress header. */
#define bmAV2EN 0x08
volatile xdata at 0xe670 unsigned char PORTACFG; /* PORTA Alternate 
                                                    Configuration */
#define bmINT0 0x01
#define bmINT1 0x02
#define bmSLCS 0x40
#define bmFLAGD 0x80
volatile xdata at 0xe671 unsigned char PORTCCFG; /* PORTC Alternate 
                                                    Configuration */
#define bmGPIFA0 0x01
#define bmGPIFA1 0x02
#define bmGPIFA2 0x04
#define bmGPIFA3 0x08
#define bmGPIFA4 0x10
#define bmGPIFA5 0x20
#define bmGPIFA6 0x40
#define bmGPIFA7 0x80
volatile xdata at 0xe672 unsigned char PORTECFG; /* PORTE Alternate 
                                                    Configuration */
#define bmT0OUT 0x01
#define bmT1OUT 0x02
#define bmT2OUT 0x04
#define bmRXD0OUT 0x08
#define bmRXD1OUT 0x10
#define bmINT6 0x20
#define bmT2EX 0x40
#define bmGPIFA8 0x80
volatile xdata at 0xe678 unsigned char I2CS; /* I2C Bus Control and Status.  
                                                Note, these registers don't have the I2C prefix in the Cypress header. */
#define bmI2C_DONE 0x01
#define bmI2C_ACK 0x02
#define bmI2C_BERR 0x04
#define bmI2C_ID0 0x08
#define bmI2C_ID1 0x10
#define bmI2C_ID 0x18
#define bmI2C_LASTRD 0x20
#define bmI2C_STOP 0x40
#define bmI2C_START 0x80
volatile xdata at 0xe679 unsigned char I2DAT; /* I2C Bus Data */
volatile xdata at 0xe67a unsigned char I2CTL; /* I2C Bus Control */
#define bmI2C_400KHZ 0x01
#define bmI2C_STOPIE 0x02
volatile xdata at 0xe67b unsigned char XAUTODAT1; /* AUTOPTR1 MOVX access */
volatile xdata at 0xe67c unsigned char XAUTODAT2; /* AUTOPTR2 MOVX access */
volatile xdata at 0xe67d unsigned char UDMACRCH;
volatile xdata at 0xe67e unsigned char UDMACRCL;
volatile xdata at 0xe67f unsigned char UDMACRCQUAL; /* Called UDMACRCQUALIFIER 
                                                       in the FX2 TRM. */
#define bmQSIGNAL 0x07
#define bmQSTATE 0x08
#define bmQENABLE 0x80
volatile xdata at 0xe680 unsigned char USBCS; /* USB Bus Control and Status */
#define bmSIGRESUME 0x01
#define bmRENUM 0x02
#define bmNOSYNSOF 0x04
#define bmDISCON 0x08
#define bmHSM 0x80
volatile xdata at 0xe681 unsigned char SUSPEND; /* Put Chip into SUSPEND State 
                                                 */
volatile xdata at 0xe682 unsigned char WAKEUPCS; /* Wakeup Control & Status */
#define bmWUEN 0x01
#define bmWU2EN 0x02
#define bmDPEN 0x04
#define bmWUPOL 0x10
#define bmWU2POL 0x20
#define bmWU 0x40
#define bmWU2 0x80
volatile xdata at 0xe683 unsigned char TOGCTL; /* Data Toggle Control */
#define bmTOGCTL_OUT 0x00
#define bmTOGCTL_IN 0x04
#define bmTOGCTLEPMASK 0x0f
#define bmTOGCTL_IO 0x10
#define bmRESETTOGGLE 0x20  /* Called R in the FX2 TRM */
#define bmSETTOGGLE 0x40    /* Called S in the FX2 TRM */
#define bmQUERYTOGGLE 0x80  /* Called Q in the FX2 TRM */
volatile xdata at 0xe684 unsigned char USBFRAMEH; /* USB Frame Count High */
volatile xdata at 0xe685 unsigned char USBFRAMEL; /* USB Frame Count Low */
volatile xdata at 0xe686 unsigned char MICROFRAME; /* USB Microframe Count, 0-7 
                                                    */
volatile xdata at 0xe687 unsigned char FNADDR; /* USB Function Address */
volatile xdata at 0xe68a unsigned char EP0BCH; /* Endpoint 0 Byte Count High */
volatile xdata at 0xe68b unsigned char EP0BCL; /* Endpoint 0 Byte Count Low */
volatile xdata at 0xe68d unsigned char EP1OUTBC; /* Endpoint 1 OUT Byte Count */
volatile xdata at 0xe68f unsigned char EP1INBC; /* Endpoint 1 IN Count Low */
volatile xdata at 0xe690 unsigned char EP2BCH; /* Endpoint 2 Byte Count High */
volatile xdata at 0xe691 unsigned char EP2BCL; /* Endpoint 2 Byte Count Low */
volatile xdata at 0xe694 unsigned char EP4BCH; /* Endpoint 4 Byte Count High */
volatile xdata at 0xe695 unsigned char EP4BCL; /* Endpoint 4 Byte Count Low */
volatile xdata at 0xe698 unsigned char EP6BCH; /* Endpoint 6 Byte Count High */
volatile xdata at 0xe699 unsigned char EP6BCL; /* Endpoint 6 Byte Count Low */
volatile xdata at 0xe69c unsigned char EP8BCH; /* Endpoint 8 Byte Count High */
volatile xdata at 0xe69d unsigned char EP8BCL; /* Endpoint 8 Byte Count Low */
volatile xdata at 0xe6a0 unsigned char EP0CS; /* Endpoint 0 Control and Status 
                                               */
#define bmEPSTALL 0x01
#define bmEPBUSY 0x02
#define bmHSNAK 0x80
volatile xdata at 0xe6a1 unsigned char EP1OUTCS; /* Endpoint 1 OUT Control and 
                                                    Status */
volatile xdata at 0xe6a2 unsigned char EP1INCS; /* Endpoint 1 IN Control and 
                                                   Status */
volatile xdata at 0xe6a3 unsigned char EP2CS; /* Endpoint 2 Control and Status 
                                               */
#define bmEPEMPTY 0x04
#define bmEPFULL 0x08
#define bmNPAK 0x70
volatile xdata at 0xe6a4 unsigned char EP4CS; /* Endpoint 4 Control and Status 
                                               */
volatile xdata at 0xe6a5 unsigned char EP6CS; /* Endpoint 6 Control and Status 
                                               */
volatile xdata at 0xe6a6 unsigned char EP8CS; /* Endpoint 8 Control and Status 
                                               */
volatile xdata at 0xe6a7 unsigned char EP2FIFOFLGS; /* Endpoint 2 FIFO Flags */
#define bmFF 0x01
#define bmEF 0x02
#define bmPF 0x04
volatile xdata at 0xe6a8 unsigned char EP4FIFOFLGS; /* Endpoint 4 FIFO Flags */
volatile xdata at 0xe6a9 unsigned char EP6FIFOFLGS; /* Endpoint 6 FIFO Flags */
volatile xdata at 0xe6aa unsigned char EP8FIFOFLGS; /* Endpoint 8 FIFO Flags */
volatile xdata at 0xe6ab unsigned char EP2FIFOBCH; /* Endpoint 2 FIFO Total 
                                                      Byte Count High */
volatile xdata at 0xe6ac unsigned char EP2FIFOBCL; /* Endpoint 2 FIFO Total 
                                                      Byte Count Low */
volatile xdata at 0xe6ad unsigned char EP4FIFOBCH; /* Endpoint 4 FIFO Total 
                                                      Byte Count High */
volatile xdata at 0xe6ae unsigned char EP4FIFOBCL; /* Endpoint 4 FIFO Total 
                                                      Byte Count Low */
volatile xdata at 0xe6af unsigned char EP6FIFOBCH; /* Endpoint 6 FIFO Total 
                                                      Byte Count High */
volatile xdata at 0xe6b0 unsigned char EP6FIFOBCL; /* Endpoint 6 FIFO Total 
                                                      Byte Count Low */
volatile xdata at 0xe6b1 unsigned char EP8FIFOBCH; /* Endpoint 8 FIFO Total 
                                                      Byte Count High */
volatile xdata at 0xe6b2 unsigned char EP8FIFOBCL; /* Endpoint 8 FIFO Total 
                                                      Byte Count Low */
volatile xdata at 0xe6b3 unsigned char SUDPTRH; /* Setup Data Pointer Address 
                                                   High */
volatile xdata at 0xe6b4 unsigned char SUDPTRL; /* Setup Data Pointer Address 
                                                   Low */
volatile xdata at 0xe6b5 unsigned char SUDPTRCTL; /* Setup Data Pointer Control 
                                                   */
#define bmSDPAUTO 0x01
volatile xdata at 0xe6b8 unsigned char SETUPDAT[8]; /* 8 Bytes of Setup Data */
volatile xdata at 0xe6c0 unsigned char GPIFWFSELECT; /* Waveform Selector */
#define bmFIFORD 0x03
#define bmFIFOWR 0x0c
#define bmSINGLERD 0x30
#define bmSINGLEWR 0xc0
volatile xdata at 0xe6c1 unsigned char GPIFIDLECS; /* GPIF Done, GPIF Idle 
                                                      Drive Mode */
#define bmIDLEDRV 0x01
#define bmIDLESTATE 0x02    /* Just called DONE in the FX2 TRM.  Changed to 
                               avoid conflicts with all other DONE bits. */
volatile xdata at 0xe6c2 unsigned char GPIFIDLECTL; /* CTL Output States in 
                                                       Idle */
#define bmCTL0 0x01
#define bmCTL1 0x02
#define bmCTL2 0x04
#define bmCTL3 0x08
#define bmCTL4 0x10
#define bmCTLOE0 0x10
#define bmCTL5 0x20
#define bmCTLOE1 0x20
#define bmCTLOE2 0x40
#define bmCTLOE3 0x80
volatile xdata at 0xe6c3 unsigned char GPIFCTLCFG; /* CTL Output Drive Type */
#define bmTRICTL 0x80
volatile xdata at 0xe6c4 unsigned char GPIFADRH; /* GPIF Address High */
volatile xdata at 0xe6c5 unsigned char GPIFADRL; /* GPIF Address Low */
volatile xdata at 0xe6c6 unsigned char FLOWSTATE;
#define bmFS 0x07
#define bmFSE 0x80
volatile xdata at 0xe6c7 unsigned char FLOWLOGIC;
#define bmTERMB 0x07
#define bmTERMA 0x38
#define bmLFUNC 0xc0
volatile xdata at 0xe6c8 unsigned char FLOWEQ0CTL; /* See GPIFIDLECTL */
volatile xdata at 0xe6c9 unsigned char FLOWEQ1CTL; /* See GPIFIDLECTL */
volatile xdata at 0xe6ca unsigned char FLOWHOLDOFF;
#define bmHOCTL 0x07
#define bmHOSTATE 0x08
#define bmHOPERIOD 0xf0
volatile xdata at 0xe6cb unsigned char FLOWSTB;
#define bmMSTB 0x07
#define bmSUSTAIN 0x10
#define bmCTLTOGL 0x20
#define bmRDYASYNC 0x40
#define bmSLAVE 0x80
volatile xdata at 0xe6cc unsigned char FLOWSTBEDGE;
#define bmFLOWSTB_RISING 0x01
#define bmFLOWSTB_FALLING 0x02
volatile xdata at 0xe6cd unsigned char FLOWSTBHPERIOD;
volatile xdata at 0xe6ce unsigned char GPIFTCB3; /* GPIF Transaction Count Byte 
                                                    3 */
volatile xdata at 0xe6cf unsigned char GPIFTCB2; /* GPIF Transaction Count Byte 
                                                    2 */
volatile xdata at 0xe6d0 unsigned char GPIFTCB1; /* GPIF Transaction Count Byte 
                                                    1 */
volatile xdata at 0xe6d1 unsigned char GPIFTCB0; /* GPIF Transaction Count Byte 
                                                    0 */
volatile xdata at 0xe6d2 unsigned char EP2GPIFFLGSEL; /* Endpoint 2 GPIF Flag 
                                                         Select */
#define bmGPIFFLG_PROGRAMMABLE 0x00
#define bmGPIFFLG_EMPTY 0x01
#define bmGPIFFLG_FULL 0x02
volatile xdata at 0xe6d3 unsigned char EP2GPIFPFSTOP; /* Endpoint 2 GPIF Stop 
                                                         Transaction */
#define bmGPIFPFSTOP 0x01
volatile xdata at 0xe6d4 unsigned char EP2GPIFTRIG; /* Endpoint 2 GPIF Trigger 
                                                     */
volatile xdata at 0xe6da unsigned char EP4GPIFFLGSEL; /* Endpoint 4 GPIF Flag 
                                                         Select */
volatile xdata at 0xe6db unsigned char EP4GPIFPFSTOP; /* Endpoint 4 GPIF Stop 
                                                         Transaction */
volatile xdata at 0xe6dc unsigned char EP4GPIFTRIG; /* Endpoint 4 GPIF Trigger 
                                                     */
volatile xdata at 0xe6e2 unsigned char EP6GPIFFLGSEL; /* Endpoint 6 GPIF Flag 
                                                         Select */
volatile xdata at 0xe6e3 unsigned char EP6GPIFPFSTOP; /* Endpoint 6 GPIF Stop 
                                                         Transaction */
volatile xdata at 0xe6e4 unsigned char EP6GPIFTRIG; /* Endpoint 6 GPIF Trigger 
                                                     */
volatile xdata at 0xe6ea unsigned char EP8GPIFFLGSEL; /* Endpoint 8 GPIF Flag 
                                                         Select */
volatile xdata at 0xe6eb unsigned char EP8GPIFPFSTOP; /* Endpoint 8 GPIF Stop 
                                                         Transaction */
volatile xdata at 0xe6ec unsigned char EP8GPIFTRIG; /* Endpoint 8 GPIF Trigger 
                                                     */
volatile xdata at 0xe6f0 unsigned char XGPIFSGLDATH; /* GPIF Data High */
volatile xdata at 0xe6f1 unsigned char XGPIFSGLDATLX; /* GPIF Data Low and 
                                                         Trigger */
volatile xdata at 0xe6f2 unsigned char XGPIFSGLDATLNOX; /* GPIF Data Low and NO 
                                                           Trigger */
volatile xdata at 0xe6f3 unsigned char GPIFREADYCFG; /* GPIF RDY Pin 
                                                        Configuration */
#define bmTCXRDY5 0x20
#define bmSAS 0x40
#define bmINTRDY 0x80
volatile xdata at 0xe6f4 unsigned char GPIFREADYSTAT; /* GPIF RDY Pin Status */
#define bmRDY0 0x01
#define bmRDY1 0x02
#define bmRDY2 0x04
#define bmRDY3 0x08
#define bmRDY4 0x10
#define bmRDY5 0x20
volatile xdata at 0xe6f5 unsigned char GPIFABORT; /* Abort GPIF */
volatile xdata at 0xe740 unsigned char EP0BUF[64]; /* EP0 IN/OUT Buffer */
volatile xdata at 0xe780 unsigned char EP1OUTBUF[64]; /* EP1 OUT Buffer */
volatile xdata at 0xe7c0 unsigned char EP1INBUF[64]; /* EP1 IN Buffer */
volatile xdata at 0xf000 unsigned char EP2FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP2/FIFO Buffer */
volatile xdata at 0xf400 unsigned char EP4FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP4/FIFO Buffer */
volatile xdata at 0xf800 unsigned char EP6FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP6/FIFO Buffer */
volatile xdata at 0xfc00 unsigned char EP8FIFOBUF[1024]; /* 512/1024-byte 
                                                            EP8/FIFO Buffer */

#endif /* _FX2REGS_H_ */

