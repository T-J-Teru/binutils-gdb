#ifndef P40_GDB_DLL_H
#define P40_GDB_DLL_H

#ifndef __WIN32__
#define __cdecl
#endif

/*
#ifdef __cplusplus
	#include <cstdio>
#else
	#include <stdio.h>
#endif
*/

// Return the number of registers that the processor has.
uint32_t __cdecl Dll_GetRegisterCount(void);

// Return a descriptive name for the given register number.
const char* __cdecl Dll_GetRegisterName(const uint32_t regnr);

// Return the size in bytes for a given register number.
uint32_t __cdecl Dll_GetRegisterSize(const uint32_t regnr);

// Convert a register number as stored in the dwarf2 information
// to a internal register number.
uint32_t __cdecl Dll_Dwarf2RegToRegnum(const uint32_t dwarf2_regnr);

// Map an address read from Target DWARF2 information to an internal
// mrk3 address.
uint32_t __cdecl Dll_Dwarf2AddrToAddr(const unsigned long long dwarf2_addr);

// Return the memory space the simulator is currently running in.
uint32_t __cdecl Dll_GetMemSpace(void);

// Return the mem space of the currently selected application.
uint32_t __cdecl Dll_GetUsrMemSpace(void);

// Analyze the prologue of the current function and return the
// processor status off the caller. This includes
// return address (pc), psw, the stack pointers (ssp and usp).
// Also the base address of the current stack frame is returned.
// This function is required for the stack analysis of the GDB.
void __cdecl Dll_GetPrologueAnalysis(const uint32_t start, const uint32_t limit,
		uint32_t* pc, uint16_t* psw, uint16_t* ssp, uint16_t* usp, uint16_t* frame_base);

// Reset/initialize the simulator.
void __cdecl Dll_SimReset(void);

// Resume the simulation. It step is set to true, only a single
// instruction is executed. Otherwise, execution continues until
// a breakpoint is hit or an exception occurs.
// Returns true, if no exception occred during execution.
// If an exception occured, false is returned.
bool __cdecl Dll_SimResume(const bool step);

// Stop the simulation. Simulation might be continued with a call to
// Dll_SimResume() afterwards.
void __cdecl Dll_SimStop();

// Terminate the simulation and free the simulator.
void __cdecl Dll_SimTerminate();

// Read data from the simulator memory.
uint32_t __cdecl Dll_SimRead(const uint32_t addr, uint8_t* buffer, const uint32_t size);

// Write data to the simulator memory.
uint32_t __cdecl Dll_SimWrite(const uint32_t addr, const uint8_t* buffer, const uint32_t size);

// Read contents of a simulator register.
uint32_t __cdecl Dll_SimReadRegister(const uint32_t regnr, uint8_t* buffer, const uint32_t size);

// Write data to the simulator register.
uint32_t __cdecl Dll_SimWriteRegister(const uint32_t regnr, const uint8_t* buffer, const uint32_t size);

// Insert a breakpoint at the given address. The address is a mapped
// address which can point to code of any application (sys, app1, app2).
void __cdecl Dll_InsertBreakpoint(const uint32_t addr);

// Remove a breakpoint from the given address. The address is a mapped address
// which can point to code of any application (sys, app1, app2).
void __cdecl Dll_RemoveBreakpoint(const uint32_t addr);

// Print a disassembly of given instruction to the buffer. At most size bytes are copied
// to the buffer. The length of the instruction in bytes is returned.
uint32_t __cdecl Dll_PrintInsn(const uint32_t insn, char* buffer, const uint32_t size);

// Allow the GDB to pass arbitrary commands to the simulator. E.g., to allow loading
// a specific simulator configuration. Command is a null-terminated string.
bool __cdecl Dll_SimCommand(const uint8_t* command);

#endif // P40_GDB_DLL_H
