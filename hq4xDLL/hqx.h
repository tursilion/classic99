// simple header for the DLL

#define HQX_CALLCONV
#define HQX_API

typedef unsigned int uint32_t;

HQX_API void HQX_CALLCONV hqxInit(void);
HQX_API void HQX_CALLCONV hq4x_32( uint32_t * src, uint32_t * dest, int width, int height );
