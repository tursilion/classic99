#ifndef tms5220_h
#define tms5220_h

#define USE_OBSOLETE_HACK 0

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#define logerror debug_write
void debug_write(char *s, ...);

void *tms5220_create(int index);
void tms5220_destroy(void *chip);

void tms5220_reset_chip(void *chip);
void tms5220_set_irq(void *chip, void (*func)(int));

bool tms5220_data_write(void *chip, int data, bool fRetry);
int tms5220_status_read(void *chip);
int tms5220_ready_read(void *chip);
int tms5220_cycles_to_ready(void *chip);
int tms5220_int_read(void *chip);

void tms5220_process(void *chip, INT16 *buffer, unsigned int size);

/* three variables added by R Nabet */
void tms5220_set_read(void *chip, int (*func)(int));
void tms5220_set_load_address(void *chip, void (*func)(int));
void tms5220_set_read_and_branch(void *chip, void (*func)(void));

typedef enum
{
	variant_tms5220,
	variant_tms0285
} tms5220_variant;

void tms5220_set_variant(void *chip, tms5220_variant new_variant);

struct tms5220
{
	/* these contain data that describes the 128-bit data FIFO */
	#define FIFO_SIZE 32	// real is 16
	#define FIFO_LOW 24		// real is 8

	UINT8 fifo[FIFO_SIZE];
	UINT8 fifo_head;
	UINT8 fifo_tail;
	UINT8 fifo_count;
	UINT8 fifo_bits_taken;

	/* these contain global status bits */
	/*
        R Nabet : speak_external is only set when a speak external command is going on.
        tms5220_speaking is set whenever a speak or speak external command is going on.
        Note that we really need to do anything in tms5220_process and play samples only when
        tms5220_speaking is true.  Else, we can play nothing as well, which is a
        speed-up...
    */
	UINT8 tms5220_speaking;	/* Speak or Speak External command in progress */
	UINT8 speak_external;	/* Speak External command in progress */
	#if USE_OBSOLETE_HACK
	UINT8 speak_delay_frames;
	#endif
	UINT8 talk_status; 		/* tms5220 is really currently speaking */
	UINT8 first_frame;		/* we have just started speaking, and we are to parse the first frame */
	UINT8 last_frame;		/* we are doing the frame of sound */
	UINT8 buffer_low;		/* FIFO has less than 8 bytes in it */
	UINT8 buffer_empty;		/* FIFO is empty*/
	UINT8 irq_pin;			/* state of the IRQ pin (output) */

	void (*irq_func)(int state); /* called when the state of the IRQ pin changes */


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

	UINT8 interp_count;		/* number of interp periods (0-7) */
	UINT8 sample_count;		/* sample number within interp (0-24) */
	UINT16 pitch_count;

	INT32 u[11];
	INT32 x[10];

	INT8 randbit;

	/* R Nabet : These have been added to emulate speech Roms */
	int (*read_callback)(int count);
	void (*load_address_callback)(int data);
	void (*read_and_branch_callback)(void);
	UINT8 schedule_dummy_read;		/* set after each load address, so that next read operation is preceded by a dummy read */

	UINT8 data_register;			/* data register, used by read command */
	UINT8 RDB_flag;					/* whether we should read data register or status register */

	/* flag for tms0285 emulation */
	/* The tms0285 is an early variant of the tms5220 used in the ti-99/4(a)
    computer.  The exact relationship of this chip with tms5200 & tms5220 is
    unknown, but it seems to use slightly different tables for LPC parameters. */
	tms5220_variant variant;
};


#endif

