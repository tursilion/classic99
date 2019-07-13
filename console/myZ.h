#pragma once

#ifndef CPU_Z80_STATIC
#	define CPU_Z80_STATIC
#endif

#include <stddef.h>

/* <Z/constants/base.h> */

#ifndef NULL
#	ifdef __cplusplus
#		define NULL 0
#	else
#		define NULL ((void *)0)
#	endif
#endif

#ifndef TRUE
#	define TRUE 1
#endif

#ifndef FALSE
#	define FALSE 0
#endif


/* Replacement for <Z/macros/language.h> */

#if (defined(_WIN64) || defined(_WIN32)) && defined(_MSC_VER)
#	define Z_API               __declspec(dllimport)
#	define Z_API_EXPORT        __declspec(dllexport)
#	define Z_STRICT_SIZE_BEGIN __pragma(pack(push, 1))
#	define Z_STRICT_SIZE_END   __pragma(pack(pop))
#	define Z_INLINE		   __forceinline
#else
#	define Z_API
#	define Z_API_EXPORT
#	define Z_STRICT_SIZE_BEGIN
#	define Z_STRICT_SIZE_END   __attribute__((packed))
#	define Z_INLINE            __inline__ __attribute__((always_inline))
#endif

#ifdef ofsetof
#	define Z_OFFSET_OF ofsetof
#else
#	define Z_OFFSET_OF(type, member) (((size_t)&((type *)(1))->member) - 1)
#endif

#ifdef __cplusplus
#	define Z_C_SYMBOLS_BEGIN extern "C" {
#	define Z_C_SYMBOLS_END	 }
#else
#	define Z_C_SYMBOLS_BEGIN
#	define Z_C_SYMBOLS_END
#endif


/* <Z/macros/pasting.h> */

#define Z_EMPTY_(dummy)
#define Z_EMPTY Z_EMPTY_(.)


/* <Z/macros/pointer.h> */

#define Z_BOP(type, base, offset) \
	((type)(((zuint8 *)(base)) + (offset)))


/* <Z/macros/structure.h> */

#define Z_DEFINE_STRICT_STRUCTURE_BEGIN Z_STRICT_SIZE_BEGIN typedef struct {
#define Z_DEFINE_STRICT_STRUCTURE_END   } Z_STRICT_SIZE_END
#define Z_DEFINE_STRICT_UNION_BEGIN     Z_STRICT_SIZE_BEGIN typedef union {
#define Z_DEFINE_STRICT_UNION_END       } Z_STRICT_SIZE_END


/* <Z/macros/value.h> */

#define Z_8BIT_ROTATE_LEFT(value, rotation) \
	(((value) << (rotation)) | ((value) >> (8 - (rotation))))

#define Z_8BIT_ROTATE_RIGHT(value, rotation) \
	(((value) >> (rotation)) | ((value) << (8 - (rotation))))


/* Replacement for <Z/types/base.h> (little-endian only implementation) */

#if (defined(_WIN64) || defined(_WIN32)) && defined(_MSC_VER)

#	define Z_UINT32(value) value##U

#	if _MSC_VER < 1300
		typedef unsigned char      zuint8;
		typedef unsigned short int zuint16;
		typedef unsigned int       zuint32;
		typedef signed char        zsint8;
		typedef signed short int   zsint16;
		typedef signed int         zsint32;
#	else
		typedef unsigned __int8  zuint8;
		typedef unsigned __int16 zuint16;
		typedef unsigned __int32 zuint32;
		typedef signed __int8    zsint8;
		typedef signed __int16   zsint16;
		typedef signed __int32   zsint32;
#	endif
#else
#	include <stdint.h>

	typedef uint8_t  zuint8;
	typedef uint16_t zuint16;
	typedef uint32_t zuint32;
	typedef int8_t   zsint8;
	typedef int16_t  zsint16;
	typedef int32_t  zsint32;

#	define Z_UINT32 UINT32_C
#endif

typedef unsigned int zuint;
typedef signed int   zsint;
typedef size_t       zusize;
typedef zuint8       zboolean;

Z_DEFINE_STRICT_UNION_BEGIN
	zuint16 value_uint16;
	zsint16 value_sint16;
	zuint8	array_uint8[2];
	zsint8	array_sint8[2];

	struct {zuint8 index0;
		zuint8 index1;
	} values_uint8;

	struct {zsint8 index0;
		zsint8 index1;
	} values_sint8;
Z_DEFINE_STRICT_UNION_END Z16Bit;

Z_DEFINE_STRICT_UNION_BEGIN
	zuint32 value_uint32;
	zsint32 value_sint32;
	zuint16 array_uint16[2];
	zsint16 array_sint16[2];
	zuint8	array_uint8 [4];
	zsint8	array_sint8 [4];

	struct {zuint16 index0;
		zuint16 index1;
	} values_uint16;

	struct {zsint16 index0;
		zsint16 index1;
	} values_sint16;

	struct {zuint8 index0;
		zuint8 index1;
		zuint8 index2;
		zuint8 index3;
	} values_uint8;

	struct {zsint8 index0;
		zsint8 index1;
		zsint8 index2;
		zsint8 index3;
	} values_sint8;
Z_DEFINE_STRICT_UNION_END Z32Bit;


/* <Z/hardware/CPU/architecture/Z80.h> */

#define Z_Z80_ADDRESS_NMI_POINTER 0x0066

#define Z_Z80_VALUE_AFTER_POWER_ON_PC     0x0000
#define Z_Z80_VALUE_AFTER_POWER_ON_SP     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_AF     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_BC     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_DE     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_HL     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_IX     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_IY     0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_AF_    0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_BC_    0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_DE_    0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_HL_    0xFFFF
#define Z_Z80_VALUE_AFTER_POWER_ON_I      0x00
#define Z_Z80_VALUE_AFTER_POWER_ON_R      0x00
#define Z_Z80_VALUE_AFTER_POWER_ON_MEMPTR 0x0000
#define Z_Z80_VALUE_AFTER_POWER_ON_IFF1   0
#define Z_Z80_VALUE_AFTER_POWER_ON_IFF2   0
#define Z_Z80_VALUE_AFTER_POWER_ON_IM     0

#define Z_Z80_VALUE_AFTER_RESET_PC     Z_Z80_VALUE_AFTER_POWER_ON_PC
#define Z_Z80_VALUE_AFTER_RESET_SP     Z_Z80_VALUE_AFTER_POWER_ON_SP
#define Z_Z80_VALUE_AFTER_RESET_AF     Z_Z80_VALUE_AFTER_POWER_ON_AF
#define Z_Z80_VALUE_AFTER_RESET_BC     Z_Z80_VALUE_AFTER_POWER_ON_BC
#define Z_Z80_VALUE_AFTER_RESET_DE     Z_Z80_VALUE_AFTER_POWER_ON_DE
#define Z_Z80_VALUE_AFTER_RESET_HL     Z_Z80_VALUE_AFTER_POWER_ON_HL
#define Z_Z80_VALUE_AFTER_RESET_IX     Z_Z80_VALUE_AFTER_POWER_ON_IX
#define Z_Z80_VALUE_AFTER_RESET_IY     Z_Z80_VALUE_AFTER_POWER_ON_IY
#define Z_Z80_VALUE_AFTER_RESET_AF_    Z_Z80_VALUE_AFTER_POWER_ON_AF_
#define Z_Z80_VALUE_AFTER_RESET_BC_    Z_Z80_VALUE_AFTER_POWER_ON_BC_
#define Z_Z80_VALUE_AFTER_RESET_DE_    Z_Z80_VALUE_AFTER_POWER_ON_DE_
#define Z_Z80_VALUE_AFTER_RESET_HL_    Z_Z80_VALUE_AFTER_POWER_ON_HL_
#define Z_Z80_VALUE_AFTER_RESET_I      Z_Z80_VALUE_AFTER_POWER_ON_I
#define Z_Z80_VALUE_AFTER_RESET_R      Z_Z80_VALUE_AFTER_POWER_ON_R
#define Z_Z80_VALUE_AFTER_RESET_MEMPTR Z_Z80_VALUE_AFTER_POWER_ON_MEMPTR
#define Z_Z80_VALUE_AFTER_RESET_IFF1   Z_Z80_VALUE_AFTER_POWER_ON_IFF1
#define Z_Z80_VALUE_AFTER_RESET_IFF2   Z_Z80_VALUE_AFTER_POWER_ON_IFF2
#define Z_Z80_VALUE_AFTER_RESET_IM     Z_Z80_VALUE_AFTER_POWER_ON_IM

#define Z_Z80_RESET_IS_EQUAL_TO_POWER_ON TRUE

Z_DEFINE_STRICT_STRUCTURE_BEGIN
	zuint16 pc,  sp;
	Z16Bit  af,  bc,  de,  hl,  ix, iy;
	Z16Bit  af_, bc_, de_, hl_;
	zuint8  r,   i;

	zuint16 memptr;

	struct {zuint8 halt :1;
		zuint8 irq  :1;
		zuint8 nmi  :1;
		zuint8 iff1 :1;
		zuint8 iff2 :1;
		zuint8 ei   :1;
		zuint8 im   :2;
	} internal;
Z_DEFINE_STRICT_STRUCTURE_END ZZ80State;

#define Z_Z80_STATE_AF(    object) (object)->af.value_uint16
#define Z_Z80_STATE_BC(    object) (object)->bc.value_uint16
#define Z_Z80_STATE_DE(    object) (object)->de.value_uint16
#define Z_Z80_STATE_HL(    object) (object)->hl.value_uint16
#define Z_Z80_STATE_IX(    object) (object)->ix.value_uint16
#define Z_Z80_STATE_IY(    object) (object)->iy.value_uint16
#define Z_Z80_STATE_PC(    object) (object)->pc
#define Z_Z80_STATE_SP(    object) (object)->sp
#define Z_Z80_STATE_AF_(   object) (object)->af_.value_uint16
#define Z_Z80_STATE_BC_(   object) (object)->bc_.value_uint16
#define Z_Z80_STATE_DE_(   object) (object)->de_.value_uint16
#define Z_Z80_STATE_HL_(   object) (object)->hl_.value_uint16
#define Z_Z80_STATE_A(     object) (object)->af.values_uint8.index1
#define Z_Z80_STATE_F(     object) (object)->af.values_uint8.index0
#define Z_Z80_STATE_B(     object) (object)->bc.values_uint8.index1
#define Z_Z80_STATE_C(     object) (object)->bc.values_uint8.index0
#define Z_Z80_STATE_D(     object) (object)->de.values_uint8.index1
#define Z_Z80_STATE_E(     object) (object)->de.values_uint8.index0
#define Z_Z80_STATE_H(     object) (object)->hl.values_uint8.index1
#define Z_Z80_STATE_L(     object) (object)->hl.values_uint8.index0
#define Z_Z80_STATE_IXH(   object) (object)->ix.values_uint8.index1
#define Z_Z80_STATE_IXL(   object) (object)->ix.values_uint8.index0
#define Z_Z80_STATE_IYH(   object) (object)->iy.values_uint8.index1
#define Z_Z80_STATE_IYL(   object) (object)->iy.values_uint8.index0
#define Z_Z80_STATE_A_(    object) (object)->af_.values_uint8.index1
#define Z_Z80_STATE_F_(    object) (object)->af_.values_uint8.index0
#define Z_Z80_STATE_B_(    object) (object)->bc_.values_uint8.index1
#define Z_Z80_STATE_C_(    object) (object)->bc_.values_uint8.index0
#define Z_Z80_STATE_D_(    object) (object)->de_.values_uint8.index1
#define Z_Z80_STATE_E_(    object) (object)->de_.values_uint8.index0
#define Z_Z80_STATE_H_(    object) (object)->hl_.values_uint8.index1
#define Z_Z80_STATE_L_(    object) (object)->hl_.values_uint8.index0
#define Z_Z80_STATE_I(     object) (object)->i
#define Z_Z80_STATE_R(     object) (object)->r
#define Z_Z80_STATE_MEMPTR(object) (object)->memptr
#define Z_Z80_STATE_HALT(  object) (object)->internal.halt
#define Z_Z80_STATE_IFF1(  object) (object)->internal.iff1
#define Z_Z80_STATE_IFF2(  object) (object)->internal.iff2
#define Z_Z80_STATE_EI(    object) (object)->internal.ei
#define Z_Z80_STATE_IM(    object) (object)->internal.im
#define Z_Z80_STATE_NMI(   object) (object)->internal.nmi
#define Z_Z80_STATE_IRQ(   object) (object)->internal.irq

#define Z_Z80_STATE_MEMBER_AF     af.value_uint16
#define Z_Z80_STATE_MEMBER_BC     bc.value_uint16
#define Z_Z80_STATE_MEMBER_DE     de.value_uint16
#define Z_Z80_STATE_MEMBER_HL     hl.value_uint16
#define Z_Z80_STATE_MEMBER_IX     ix.value_uint16
#define Z_Z80_STATE_MEMBER_IY     iy.value_uint16
#define Z_Z80_STATE_MEMBER_PC     pc
#define Z_Z80_STATE_MEMBER_SP     sp
#define Z_Z80_STATE_MEMBER_AF_    af_.value_uint16
#define Z_Z80_STATE_MEMBER_BC_    bc_.value_uint16
#define Z_Z80_STATE_MEMBER_DE_    de_.value_uint16
#define Z_Z80_STATE_MEMBER_HL_    hl_.value_uint16
#define Z_Z80_STATE_MEMBER_A      af.values_uint8.index1
#define Z_Z80_STATE_MEMBER_F      af.values_uint8.index0
#define Z_Z80_STATE_MEMBER_B      bc.values_uint8.index1
#define Z_Z80_STATE_MEMBER_C      bc.values_uint8.index0
#define Z_Z80_STATE_MEMBER_D      de.values_uint8.index1
#define Z_Z80_STATE_MEMBER_E      de.values_uint8.index0
#define Z_Z80_STATE_MEMBER_H      hl.values_uint8.index1
#define Z_Z80_STATE_MEMBER_L      hl.values_uint8.index0
#define Z_Z80_STATE_MEMBER_IXH    ix.values_uint8.index1
#define Z_Z80_STATE_MEMBER_IXL    ix.values_uint8.index0
#define Z_Z80_STATE_MEMBER_IYH    iy.values_uint8.index1
#define Z_Z80_STATE_MEMBER_IYL    iy.values_uint8.index0
#define Z_Z80_STATE_MEMBER_A_     af_.values_uint8.index1
#define Z_Z80_STATE_MEMBER_F_     af_.values_uint8.index0
#define Z_Z80_STATE_MEMBER_B_     bc_.values_uint8.index1
#define Z_Z80_STATE_MEMBER_C_     bc_.values_uint8.index0
#define Z_Z80_STATE_MEMBER_D_     de_.values_uint8.index1
#define Z_Z80_STATE_MEMBER_E_     de_.values_uint8.index0
#define Z_Z80_STATE_MEMBER_H_     hl_.values_uint8.index1
#define Z_Z80_STATE_MEMBER_L_     hl_.values_uint8.index0
#define Z_Z80_STATE_MEMBER_I      i
#define Z_Z80_STATE_MEMBER_R      r
#define Z_Z80_STATE_MEMPTR_MEMPTR memptr
#define Z_Z80_STATE_MEMBER_HALT   internal.halt
#define Z_Z80_STATE_MEMBER_IFF1   internal.iff1
#define Z_Z80_STATE_MEMBER_IFF2   internal.iff2
#define Z_Z80_STATE_MEMBER_EI     internal.ei
#define Z_Z80_STATE_MEMBER_IM     internal.im
#define Z_Z80_STATE_MEMBER_NMI    internal.nmi
#define Z_Z80_STATE_MEMBER_IRQ    internal.irq

