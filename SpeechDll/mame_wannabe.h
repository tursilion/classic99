#pragma once

#include <cstdint>
#include <algorithm>

// this enables the LOGMASKED to all emit
//#define VERBOSE_DEBUG

// simple wrapper to help the mame code feel more at home

#define DECLARE_DEVICE_TYPE(x,y)
#define WRITE_LINE_MEMBER(name) 
#define fatalerror debug_write

#ifdef VERBOSE_DEBUG
#define LOGMASKED(x,...) if (((x)&(LOG_PARSE_FRAME_DUMP_BIN|LOG_PARSE_FRAME_DUMP_HEX|LOG_CLIP|LOG_LATTICE|LOG_GENERATION|LOG_GENERATION_VERBOSE|LOG_DUMP_INPUT_DATA))==0) debug_write(__FUNCTION__ ": " __VA_ARGS__)
#else
#define LOGMASKED(x,...)
#endif

typedef uint32_t u32;
typedef int device_type;

typedef struct _mc {
    
    
    
} machine_config;
        
class device_t {
public:
    device_t(const machine_config &, int , const void *, device_t *, int clk) 
        : clock_rate(clk)
    { }

    int clock() { return clock_rate; }

private:
    int clock_rate;
};

class device_sound_interface {
public:
    device_sound_interface(const machine_config &, void *) { }
};

void debug_write(char *s, ...);
