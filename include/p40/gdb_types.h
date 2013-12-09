#ifndef GDB_TYPES_H
#define GDB_TYPES_H

// Standard types.
// In the external simulator, they are
// defined in PH_TYPEDEFS_H.
typedef unsigned char uint8_t;
typedef signed char sint8_t;
typedef unsigned short int uint16_t;
typedef signed short int sint16_t;
typedef unsigned int uint32_t;
typedef signed int sint32_t;
#ifndef __cplusplus
	typedef unsigned char bool;
	#define false 0
	#define true 1
#endif

#endif // GDB_TYPES_H
