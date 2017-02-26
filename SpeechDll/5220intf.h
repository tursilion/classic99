//struct TMS5220interface
//{
//   int baseclock;           /* clock rate = 80 * output sample rate,     */
//                            /* usually 640000 for 8000 Hz sample rate or */
//                            /* usually 800000 for 10000 Hz sample rate.  */
//	int mixing_level;
//	void (*irq)(void);        /* IRQ callback function */
//	int memory_region;        /* memory region where the speech ROM is.  -1 means no speech ROM */
//	int memory_size;          /* memory size of speech rom (0 -> take memory region length) */
//	int volume;					/* volume 0-100 */
//};

int tms5220_sh_start (void);
void tms5220_sh_stop (void);
void tms5220_sh_update (void);

void tms5220_data_w (int offset, int data);
int tms5220_status_r (int offset);
int tms5220_ready_r (void);
int tms5220_int_r (void);

void tms5220_reset (void);

