#pragma once

#ifndef __TMS5110_H__
#define __TMS5110_H__

#include "mame_interface.h"

/* TMS5110 commands */
                                     /* CTL8  CTL4  CTL2  CTL1  |   PDC's  */
                                     /* (MSB)             (LSB) | required */
#define TMS5110_CMD_RESET        (0) /*    0     0     0     x  |     1    */
#define TMS5110_CMD_LOAD_ADDRESS (2) /*    0     0     1     x  |     2    */
#define TMS5110_CMD_OUTPUT       (4) /*    0     1     0     x  |     3    */
#define TMS5110_CMD_SPKSLOW      (6) /*    0     1     1     x  |     1    | Note: this command is undocumented on the datasheets, it only appears on the patents. It might not actually work properly on some of the real chips as manufactured. Acts the same as CMD_SPEAK, but makes the interpolator take two A cycles whereever it would normally only take one, effectively making speech of any given word take about twice as long as normal. */
#define TMS5110_CMD_READ_BIT     (8) /*    1     0     0     x  |     1    */
#define TMS5110_CMD_SPEAK       (10) /*    1     0     1     x  |     1    */
#define TMS5110_CMD_READ_BRANCH (12) /*    1     1     0     x  |     1    */
#define TMS5110_CMD_TEST_TALK   (14) /*    1     1     1     x  |     3    */

/* clock rate = 80 * output sample rate,     */
/* usually 640000 for 8000 Hz sample rate or */
/* usually 800000 for 10000 Hz sample rate.  */

typedef struct _tms5110_interface tms5110_interface;
struct _tms5110_interface
{
	/* legacy interface */
	int (*M0_callback)(device_t *device);	/* function to be called when chip requests another bit */
	void (*load_address)(device_t *device, int addr);	/* speech ROM load address callback */
	/* new rom controller interface */
	devcb_write_line m0_func;		/* the M0 line */
	devcb_write_line m1_func;		/* the M1 line */
	devcb_write8 addr_func;			/* Write to ADD1,2,4,8 - 4 address bits */
	devcb_read_line data_func;		/* Read one bit from ADD8/Data - voice data */
	/* on a real chip rom_clk is running all the time
     * Here, we only use it to properly emulate the protocol.
     * Do not rely on it to be a timed signal.
     */
	devcb_write_line romclk_func;	/* rom clock - Only used to drive the data lines */
};

WRITE8_DEVICE_HANDLER( tms5110_ctl_w );
READ8_DEVICE_HANDLER( tms5110_ctl_r );
WRITE_LINE_DEVICE_HANDLER( tms5110_pdc_w );

/* this is only used by cvs.c
 * it is not related at all to the speech generation
 * and conflicts with the new rom controller interface.
 */
READ8_DEVICE_HANDLER( tms5110_romclk_hack_r );

/* m58817 status line */
READ8_DEVICE_HANDLER( m58817_status_r );

int tms5110_ready_r(device_t *device);

void tms5110_set_frequency(device_t *device, int frequency);

/* PROM controlled TMS5110 interface */

typedef struct _tmsprom_interface tmsprom_interface;
struct _tmsprom_interface
{
	const char *prom_region;		/* prom memory region - sound region is automatically assigned */
	UINT32 rom_size;				/* individual rom_size */
	UINT8 pdc_bit;					/* bit # of pdc line */
	/* virtual bit 8: constant 0, virtual bit 9:constant 1 */
	UINT8 ctl1_bit;					/* bit # of ctl1 line */
	UINT8 ctl2_bit;					/* bit # of ctl2 line */
	UINT8 ctl4_bit;					/* bit # of ctl4 line */
	UINT8 ctl8_bit;					/* bit # of ctl8 line */
	UINT8 reset_bit;				/* bit # of rom reset */
	UINT8 stop_bit;					/* bit # of stop */
	devcb_write_line pdc_func;		/* tms pdc func */
	devcb_write8 ctl_func;			/* tms ctl func */
};

WRITE_LINE_DEVICE_HANDLER( tmsprom_m0_w );
READ_LINE_DEVICE_HANDLER( tmsprom_data_r );

/* offset is rom # */
WRITE8_DEVICE_HANDLER( tmsprom_rom_csq_w );
WRITE8_DEVICE_HANDLER( tmsprom_bit_w );
WRITE_LINE_DEVICE_HANDLER( tmsprom_enable_w );


////////////////////////////////////////////////////////

typedef struct _tms5110_state device_t;
typedef struct _tms5110_state tms5110_state;
struct _tms5110_state
{
	/* coefficient tables */
	int variant;				/* Variant of the 5110 - see tms5110.h */

	/* coefficient tables */
	const struct tms5100_coeffs *coeff;

	/* these contain data that describes the 64 bits FIFO */
	UINT8 fifo[FIFO_SIZE];
	UINT8 fifo_head;
	UINT8 fifo_tail;
	UINT8 fifo_count;

	/* these contain global status bits */
	UINT8 PDC;
	UINT8 CTL_pins;
	UINT8 speaking_now;
	UINT8 talk_status;
	UINT8 state;

	/* Rom interface */
	UINT32 address;
	UINT8  next_is_address;
	UINT8  schedule_dummy_read;
	UINT8  addr_bit;

	/* external callback */
	int (*M0_callback)(device_t *);
	void (*set_load_address)(device_t *, int);

	/* callbacks */
	devcb_resolved_write_line m0_func;		/* the M0 line */
	devcb_resolved_write_line m1_func;		/* the M1 line */
	devcb_resolved_write8 addr_func;		/* Write to ADD1,2,4,8 - 4 address bits */
	devcb_resolved_read_line data_func;		/* Read one bit from ADD8/Data - voice data */
	devcb_resolved_write_line romclk_func;	/* rom clock - Only used to drive the data lines */


	device_t *device;

	/* these contain data describing the current and previous voice frames */
	UINT16 old_energy;
	UINT16 old_pitch;
	INT32 old_k[10];

	UINT16 new_energy;
	UINT16 new_pitch;
	INT32 new_k[10];


	/* these are all used to contain the current state of the sound generation */
	UINT16 current_energy;
	UINT16 current_pitch;
	INT32 current_k[10];

	UINT16 target_energy;
	UINT16 target_pitch;
	INT32 target_k[10];

	UINT8 interp_count;       /* number of interp periods (0-7) */
	UINT8 sample_count;       /* sample number within interp (0-24) */
	INT32 pitch_count;

	INT32 x[11];

	INT32 RNG;	/* the random noise generator configuration is: 1 + x + x^3 + x^4 + x^13 */

	const tms5110_interface *intf;
	const UINT8 *table;
	sound_stream *stream;
	INT32 speech_rom_bitnum;

	emu_timer *romclk_hack_timer;
	UINT8 romclk_hack_timer_started;
	UINT8 romclk_hack_state;
};

typedef struct _tmsprom_state tmsprom_state;
struct _tmsprom_state
{
	/* Rom interface */
	UINT32 address;
	/* ctl lines */
	UINT8  m0;
	UINT8  enable;
	UINT32  base_address;
	UINT8  bit;

	int prom_cnt;

	int    clock;
	const UINT8 *rom;
	const UINT8 *prom;

	devcb_resolved_write_line pdc_func;		/* tms pdc func */
	devcb_resolved_write8 ctl_func;			/* tms ctl func */

	device_t *device;
	emu_timer *romclk_timer;

	const tmsprom_interface *intf;
};

device_t *tms5110_create() {
	return new tms5110_state;
}

/* Variants */

#define TMS5110_IS_5110A	(1)
#define TMS5110_IS_5100		(2)
#define TMS5110_IS_5110		(3)

#define TMS5110_IS_CD2801	TMS5110_IS_5100
#define TMS5110_IS_TMC0281	TMS5110_IS_5100

#define TMS5110_IS_CD2802	TMS5110_IS_5110
#define TMS5110_IS_M58817	TMS5110_IS_5110

#endif /* __TMS5110_H__ */
