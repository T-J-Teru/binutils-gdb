#ifndef MRK3_DEBUG_H
#define MRK3_DEBUG_H

#include "../../gdb/defs.h"
#include "p40/gdb_types.h"
#include "p40/gdb_mem_map.h"

// A structure to store the objectfile
// filenames for dynammic objectfile switching.
struct mrk3_objfile_info_t {
	char* name;
	char* full_name;
	uint32_t mem_size_code;
	uint16_t mem_size_globals;
	uint16_t mem_size_rodata;
};


// Save the full and base filename to an objfile info struct.
void mrk3_save_objfile_name(struct mrk3_objfile_info_t* of_info, const char* filename);


// Free an objfile_info structure.
void mrk3_free_objfile_info(struct mrk3_objfile_info_t* of_info);


// Load the symbol information for a mem_space.
void mrk3_load_symbol_info(const uint32_t mem_space);


// Map an address read from Target DWARF2 information to an internal
// mrk3 address.
uint32_t mrk3_map_dwarf2_data_addr(const ULONGEST addr);


// When looking up symbols, switch priorities such that the
// symbol file for the given memory space is preferred.
void mrk3_load_symbol_info(const uint32_t mem_space);


// Search all known objectfiles and return the mem_space for the given
// filename.
uint32_t mrk3_get_memspace_from_objfile_name(const char* base_name);


// Every memory space should have its own objectfile. So this is
// correlated to the MRK3_MEM_SPACE_* constants defined in gdb_mem_map.h
#define MRK3_MAX_OBJFILES 3
extern struct mrk3_objfile_info_t mrk3_objfile_info[MRK3_MAX_OBJFILES];



#endif // MRK3_DEBUG_H
