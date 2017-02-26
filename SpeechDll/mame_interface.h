// wrapper for the mame crap

#define devcb_write_line devcb_resolved_write_line
#define WRITE8_DEVICE_HANDLER( x ) void x(int data)
#define READ8_DEVICE_HANDLER( x ) int x()
#define WRITE_LINE_DEVICE_HANDLER( x ) void x(int state)

#define DEVICE_START_CALL(x) ##x_start()
#define DEVICE_START(x) ##x_start()
#define DEVICE_RESET(x) ##xx_reset()
#define TIMER_CALLBACK(x) ##xx_timer_callback()
#define STREAM_UPDATE(x) ##xx_update(short *buffer, int samples)

typedef (void) device_t;

class devcb_resolved_write_line {
public:
	devcb_resolved_write_line() {
		callback_func = NULL;
	}

	bool isnull() {
		if (callback_func != NULL) {
			return false;
		} else {
			return true;
		}
	}

	void resolve(callback_func p, device_t d) {
		p(d);
	}

	int (*callback_func)(device_t *);
};

class devcb_write8 {
public:
	devcb_write8() {
		callback_func = NULL;
	}

	bool isnull() {
		if (callback_func != NULL) {
			return false;
		} else {
			return true;
		}
	}

	void resolve(callback_func p, device_t d) {
		p(0, d);
	}

	int (*callback_func)(device_t *);
}

class devcb_read_line {
public:
	devcb_read_line() {
		callback_func = NULL;
	}

	bool isnull() {
		if (callback_func != NULL) {
			return false;
		} else {
			return true;
		}
	}

	void resolve(callback_func p, device_t d) {
		p(d);
	}

	int (*callback_func)(device_t *);
}
