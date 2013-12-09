

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "stdio.h"
#include "bfd.h"
#include "gdb/callback.h"
#include "gdb/signals.h"
#include "libiberty.h"
#include "gdb/remote-sim.h"
#include "dis-asm.h"
#include "sim-utils.h"
#include "elf-bfd.h"
#include "p40/debug.h"
#include "p40/P40_DLL.h"

#ifdef __WIN32__
#include <windows.h>
#else
#define BOOL int
#define HANDLE void *
#endif

// Offsets for addresses of external functions within function address table:
#define CB_GET_REGISTER_COUNT     0
#define CB_GET_REGISTER_NAME      1
#define CB_GET_REGISTER_SIZE      2
#define CB_DWARF2_REG_TO_REGNR    3
#define CB_DWARF2_ADDR_TO_ADDR    4
#define CB_GET_MEM_SPACE          5
#define CB_GET_USR_MEM_SPACE      6
#define CB_GET_PROLOGUE_ANALYSIS  7
#define CB_SIM_RESET              8
#define CB_SIM_RESUME             9
#define CB_SIM_STOP              10
#define CB_SIM_TERMINATE         11
#define CB_SIM_READ              12
#define CB_SIM_WRITE             13
#define CB_SIM_READ_REGISTER     14
#define CB_SIM_WRITE_REGISTER    15
#define CB_INSERT_BREAKPOINT     16
#define CB_REMOVE_BREAKPOINT     17
#define CB_PRINT_INSTRUCTION     18
#define CB_SIM_COMMAND           19
#define CB_MAX                   20

struct dll_function_t
{
	const char* name;
	void* address;
};

struct dll_function_t dll_functions[CB_MAX] = {
	[CB_GET_REGISTER_COUNT] =    { "Dll_GetRegisterCount",    NULL },
	[CB_GET_REGISTER_NAME] =     { "Dll_GetRegisterName",     NULL },
	[CB_GET_REGISTER_SIZE] =     { "Dll_GetRegisterSize",     NULL },
	[CB_DWARF2_REG_TO_REGNR] =   { "Dll_Dwarf2RegToRegnum",   NULL },
	[CB_DWARF2_ADDR_TO_ADDR] =   { "Dll_Dwarf2AddrToAddr",    NULL },
	[CB_GET_MEM_SPACE] =         { "Dll_GetMemSpace",         NULL },
	[CB_GET_USR_MEM_SPACE] =     { "Dll_GetUsrMemSpace",      NULL },
	[CB_GET_PROLOGUE_ANALYSIS] = { "Dll_GetPrologueAnalysis", NULL },
	[CB_SIM_RESET] =             { "Dll_SimReset",            NULL },
	[CB_SIM_RESUME] =            { "Dll_SimResume",           NULL },
	[CB_SIM_STOP] =              { "Dll_SimStop",             NULL },
	[CB_SIM_TERMINATE] =         { "Dll_SimTerminate",        NULL },
	[CB_SIM_READ] =              { "Dll_SimRead",             NULL },
	[CB_SIM_WRITE] =             { "Dll_SimWrite",            NULL },
	[CB_SIM_READ_REGISTER] =     { "Dll_SimReadRegister",     NULL },
	[CB_SIM_WRITE_REGISTER] =    { "Dll_SimWriteRegister",    NULL },
	[CB_INSERT_BREAKPOINT] =     { "Dll_InsertBreakpoint",    NULL },
	[CB_REMOVE_BREAKPOINT] =     { "Dll_RemoveBreakpoint",    NULL },
	[CB_PRINT_INSTRUCTION] =     { "Dll_PrintInsn",           NULL },
	[CB_SIM_COMMAND] =           { "Dll_SimCommand",          NULL }
};

// This is non-zero, if a external simulator dll is loaded.
HANDLE dll_handle = NULL;

void unload_dll(void);
void Dll_SimTerminate(void);

size_t expanded_environment_length(const char* path)
{
	char buffer;
	return ExpandEnvironmentStrings( path, &buffer, 0);
}

void expand_environment(char* dst, const char* src, const size_t size)
{
	int required_size = 0;
	if (size < 2) {
		fprintf(stdout, "Unable to expand [%s] - buffer too small.", src);
		return;
	}

	required_size = ExpandEnvironmentStrings(src, dst, size);
	if (required_size > size) {
		fprintf(stdout, "Unable to expand [%s] - buffer too small.", src);
		return;
	}
}

void load_dll(const char* arg_filename)
{
#ifdef __WIN32__
	unload_dll();

	if (arg_filename == NULL) return;

	char* p_filename = NULL;
	size_t len = expanded_environment_length(arg_filename);
	p_filename = (char*) malloc(len);
	expand_environment(p_filename, arg_filename, len);

	dll_handle = LoadLibrary(p_filename);
	if (dll_handle != NULL)
	{
		int idx_function;
		bool found_all_functions = true;
		for (idx_function = 0; idx_function < CB_MAX; idx_function++) {
			const char* function_name = dll_functions[idx_function].name;
			dll_functions[idx_function].address = GetProcAddress(dll_handle, function_name);
			if (dll_functions[idx_function].address == NULL)
			{
				fprintf(stdout, "Could not get address of [%s] within [%s]: %d\n", function_name, p_filename, GetLastError());
				found_all_functions = false;
			}
		}

		if (found_all_functions)
		{
			printf("Successfully loaded dll from [%s]\n", p_filename);
		}
		else
		{
			Dll_SimTerminate();
			unload_dll();
		}
	}
	else
	{
		fprintf(stdout, "Unable to load library %s: %d\n", p_filename, GetLastError());
	}
	free(p_filename);
#endif
}

void unload_dll(void)
{
#ifdef __WIN32__
	if (dll_handle != NULL)
	{
		BOOL retFreeLibrary = FreeLibrary(dll_handle);
		if (retFreeLibrary != 0)
		{
			dll_handle = NULL;
		}
		else
		{
			printf("Unable to unload dll: %d", GetLastError());
		}
	}
#endif
}


// ---------------------------------------------------------------------------
// Wrapper functions for the calls into the simulator dll. They are there in
// order to prevent GDB from crashing, if no simulator could be loaded.

typedef uint32_t __cdecl (*GetRegisterCountFunc)(void);
uint32_t __cdecl Dll_GetRegisterCount(void)
{
	if (dll_handle != NULL)
		return (*((GetRegisterCountFunc) dll_functions[CB_GET_REGISTER_COUNT].address))();
	else
	{
		fprintf(stdout, "Dll_GetRegisterCount: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef const char* __cdecl (*GetRegisterNameFunc)(const uint32_t regnr);
const char* __cdecl Dll_GetRegisterName(const uint32_t regnr)
{
	static const char* pDefaultRegisterName = "unknown";
	if (dll_handle != NULL)
		return (*((GetRegisterNameFunc) dll_functions[CB_GET_REGISTER_NAME].address))(regnr);
	else
	{
		fprintf(stdout, "Dll_GetRegisterName: No simulator dll was loaded.\n");
		return pDefaultRegisterName;
	}
}

typedef const uint32_t __cdecl (*GetRegisterSizeFunc)(const uint32_t regnr);
uint32_t __cdecl Dll_GetRegisterSize(const uint32_t regnr)
{
	if (dll_handle != NULL)
		return (*((GetRegisterSizeFunc) dll_functions[CB_GET_REGISTER_SIZE].address))(regnr);
	else
	{
		fprintf(stdout, "Dll_GetRegisterSize: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef const uint32_t __cdecl (*Dwarf2RegToRegnumFunc)(const uint32_t dwarf2_regnr);
uint32_t __cdecl Dll_Dwarf2RegToRegnum(const uint32_t dwarf2_regnr)
{
	if (dll_handle != NULL)
		return (*((Dwarf2RegToRegnumFunc) dll_functions[CB_DWARF2_REG_TO_REGNR].address))(dwarf2_regnr);
	else
	{
		fprintf(stdout, "Dll_Dwarf2RegToRegnum: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef const uint32_t __cdecl (*Dwarf2AddrToAddrFunc)(const unsigned long long dwarf2_addr);
uint32_t __cdecl Dll_Dwarf2AddrToAddr(const unsigned long long  dwarf2_addr)
{
	if (dll_handle != NULL)
		return (*((Dwarf2AddrToAddrFunc) dll_functions[CB_DWARF2_ADDR_TO_ADDR].address))(dwarf2_addr);
	else
	{
		fprintf(stdout, "Dll_Dwarf2AddrToAddr: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef const uint32_t __cdecl (*GetMemSpaceFunc)(void);
uint32_t __cdecl Dll_GetMemSpace(void)
{
	if (dll_handle != NULL)
		return (*((GetMemSpaceFunc) dll_functions[CB_GET_MEM_SPACE].address))();
	else
	{
		fprintf(stdout, "Dll_GetMemSpace: No simulator dll was loaded.\n");
		return MRK3_MEM_SPACE_SYS;
	}
}

uint32_t __cdecl Dll_GetUsrMemSpace(void)
{
	if (dll_handle != NULL)
		return (*((GetMemSpaceFunc) dll_functions[CB_GET_USR_MEM_SPACE].address))();
	else
	{
		fprintf(stdout, "Dll_GetUsrMemSpace: No simulator dll was loaded.\n");
		return MRK3_MEM_SPACE_APP1;
	}
}

typedef void __cdecl (*GetPrologueAnalysisFunc)(const uint32_t, const uint32_t, uint32_t*, uint16_t*, uint16_t*, uint16_t*, uint16_t*);
void __cdecl Dll_GetPrologueAnalysis(const uint32_t start, const uint32_t limit,
		uint32_t* pc, uint16_t* psw, uint16_t* ssp, uint16_t* usp, uint16_t* frame_base)
{
	if (dll_handle != NULL)
		(*((GetPrologueAnalysisFunc) dll_functions[CB_GET_PROLOGUE_ANALYSIS].address))(start, limit, pc, psw, ssp, usp, frame_base);
	else
		fprintf(stdout, "Dll_GetPrologueAnalysis: No simulator dll was loaded.\n");
}

typedef void __cdecl (*SimResetFunc)(void);
void __cdecl Dll_SimReset(void)
{
	if (dll_handle != NULL)
		(*((SimResetFunc) dll_functions[CB_SIM_RESET].address))();
	else
		fprintf(stdout, "Dll_SimReset: No simulator dll was loaded.\n");
}

typedef bool __cdecl (*SimResumeFunc)(const bool);
bool __cdecl Dll_SimResume(const bool step)
{
	if (dll_handle != NULL)
		return (*((SimResumeFunc) dll_functions[CB_SIM_RESUME].address))(step);
	else
	{
		fprintf(stdout, "Dll_SimResume: No simulator dll was loaded.\n");
		return true;
	}
}

typedef void __cdecl (*SimStopFunc)(void);
void __cdecl Dll_SimStop(void)
{
	if (dll_handle != NULL)
		(*((SimStopFunc) dll_functions[CB_SIM_STOP].address))();
	else
		fprintf(stdout, "Dll_SimStop: No simulator dll was loaded.\n");
}

typedef void __cdecl (*SimTerminateFunc)(void);
void __cdecl Dll_SimTerminate(void)
{
	if (dll_handle != NULL)
		(*((SimTerminateFunc) dll_functions[CB_SIM_TERMINATE].address))();
	else
		fprintf(stdout, "Dll_SimTerminate: No simulator dll was loaded.\n");
}

typedef uint32_t __cdecl (*SimReadFunc)(const uint32_t, uint8_t*, const uint32_t);
uint32_t __cdecl Dll_SimRead(const uint32_t addr, uint8_t* buffer, const uint32_t size)
{
	if (dll_handle != NULL)
		return (*((SimReadFunc) dll_functions[CB_SIM_READ].address))(addr, buffer, size);
	else
	{
		fprintf(stdout, "Dll_SimRead: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef uint32_t __cdecl (*SimWriteFunc)(const uint32_t, const uint8_t*, const uint32_t);
uint32_t __cdecl Dll_SimWrite(const uint32_t addr, const uint8_t* buffer, const uint32_t size)
{
	if (dll_handle != NULL)
		return (*((SimWriteFunc) dll_functions[CB_SIM_WRITE].address))(addr, buffer, size);
	else
	{
		fprintf(stdout, "Dll_SimWrite: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef uint32_t __cdecl (*SimReadRegisterFunc)(const uint32_t, uint8_t*, const uint32_t);
uint32_t __cdecl Dll_SimReadRegister(const uint32_t regnr, uint8_t* buffer, const uint32_t size)
{
	if (dll_handle != NULL)
		return (*((SimReadRegisterFunc) dll_functions[CB_SIM_READ_REGISTER].address))(regnr, buffer, size);
	else
	{
		fprintf(stdout, "Dll_SimReadRegister: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef uint32_t __cdecl (*SimWriteRegisterFunc)(const uint32_t, const uint8_t*, const uint32_t);
uint32_t __cdecl Dll_SimWriteRegister(const uint32_t regnr, const uint8_t* buffer, const uint32_t size)
{
	if (dll_handle != NULL)
		return (*((SimWriteRegisterFunc) dll_functions[CB_SIM_WRITE_REGISTER].address))(regnr, buffer, size);
	else
	{
		fprintf(stdout, "Dll_SimWriteRegister: No simulator dll was loaded.\n");
		return 0;
	}
}

typedef uint32_t __cdecl (*InsertBreakpointFunc)(const uint32_t);
void __cdecl Dll_InsertBreakpoint(const uint32_t addr)
{
	if (dll_handle != NULL)
		(*((InsertBreakpointFunc) dll_functions[CB_INSERT_BREAKPOINT].address))(addr);
	else
		fprintf(stdout, "Dll_InsertBreakpoint: No simulator dll was loaded.\n");
}

typedef uint32_t __cdecl (*RemoveBreakpointFunc)(const uint32_t);
void __cdecl Dll_RemoveBreakpoint(const uint32_t addr)
{
	if (dll_handle != NULL)
		(*((RemoveBreakpointFunc) dll_functions[CB_REMOVE_BREAKPOINT].address))(addr);
	else
		fprintf(stdout, "Dll_RemoveBreakpoint: No simulator dll was loaded.\n");
}

typedef uint32_t __cdecl (*PrintInsnFunc)(const uint32_t, char*, const uint32_t);
uint32_t __cdecl Dll_PrintInsn(const uint32_t insn, char* buffer, const uint32_t size)
{
	static const char* default_instruction = "no dll loaded";
	if (dll_handle != NULL)
	{
		return (*((PrintInsnFunc) dll_functions[CB_PRINT_INSTRUCTION].address))(insn, buffer, size);
	}
	else
	{
		fprintf(stdout, "Dll_PrintInsn: No simulator dll was loaded.\n");
		strncpy(buffer, default_instruction, size);
		return 1;
	}
}

typedef bool __cdecl (*SimCommandFunc)(const uint8_t*);
bool __cdecl Dll_SimCommand(const uint8_t* command)
{
	if (dll_handle != NULL)
		return (*((SimCommandFunc) dll_functions[CB_SIM_COMMAND].address))(command);
	else
	{
		fprintf(stdout, "Dll_SimCommand: No simulator dll was loaded.\n");
		return false;
	}
}


// ---------------------------------------------------------------------------
// Internal GDB simulator interface.


struct mrk3_sim_status_t {
	// GDB specific status variables.
	enum sim_stop exception;
	int signal;
};

struct mrk3_sim_status_t sim_status;
struct mrk3_objfile_info_t mrk3_objfile_info[MRK3_MAX_OBJFILES];


// Extract the filename from a file command
static inline void mrk3_extract_filename_from_command(char* filename, const char* cmd, const char* prefix) {
	int pos = strlen(prefix);
	int len = strlen(cmd);
	while ((cmd[pos] == ' ') && (pos < len)) pos++;
	// copy the remaining portion of the command to the filename parameter.
	if (pos < len) { strcpy(filename, &cmd[pos]); }
	else { *filename = 0x00; }
}


// Return how many bytes are left until the specified end address.
static inline uint32_t mrk3_bytes_until_end_addr(uint32_t addr, uint32_t size, uint32_t end_addr) {
	if (addr < end_addr) {
		if (addr + size < end_addr) {
			return size;
		}
		else {
			return (end_addr - addr);
		}
	}
	return 0;
}


// Load the contents of an ELF file to the internal memory of the
// simulator. The file can either be specified by filename (prog) or
// by giving a pointer to an already parsed bfd file (abfd).
// To select the memory space which the file is loaded to, the mem_space
// parameter should be used.
static bool mrk3_load_elf(SIM_DESC sd, const char *prog, const struct bfd *abfd, const uint32_t mem_space) {

	struct mrk3_objfile_info_t* of_info = &(mrk3_objfile_info[mrk3_mem_space_index(mem_space)]);
	bool result = false;
	char* filename = (char*) prog;

	bfd *prog_bfd;
	int num_headers;
	Elf_Internal_Phdr *phdrs;
	long sizeof_phdrs;
	int i;

	// Load the file via the bfd parser if necessary.
	if (abfd != NULL) {
		prog_bfd = (bfd*) abfd;
		filename = bfd_get_filename(abfd);
	}
	else { prog_bfd = bfd_openr(prog, NULL); }

	if (!prog_bfd) {
		fprintf (stdout, "mrk3_load_elf: error: Can't read %s\n", prog);
		return false;
	}

	if (bfd_check_format (prog_bfd, bfd_object)) {

		mrk3_save_objfile_name(of_info, filename);
		of_info->mem_size_code = 0;
		of_info->mem_size_globals = 0;
		of_info->mem_size_rodata = 0;
		symbol_file_add(of_info->full_name, 0, NULL, 0);

		sizeof_phdrs = bfd_get_elf_phdr_upper_bound (prog_bfd);
		if (sizeof_phdrs > 0) {
			phdrs = malloc (sizeof_phdrs);
			if (phdrs != NULL) {
				num_headers = bfd_get_elf_phdrs (prog_bfd, phdrs);
				if (num_headers > 0) {
					// Load the segments of the ELF file to the simulator memory.
					for (i = 0; i < num_headers; i++)
					{
						bfd_byte *buf;
						bfd_vma lma = phdrs[i].p_paddr;

						// Skip anything that is not of loadable type.
						if (phdrs[i].p_type != PT_LOAD)
							continue;

						buf = xmalloc (phdrs[i].p_filesz);
						// Read the contents to buf.
						if ((bfd_seek (prog_bfd, phdrs[i].p_offset, SEEK_SET) == 0) &&
										(bfd_bread(buf, phdrs[i].p_filesz, prog_bfd) == phdrs[i].p_filesz)) {

							// If the readable lag is not set for the segment,
							// we do not process it further.
							if (phdrs[i].p_flags & PF_R) {
								// If the executable flag is set for a segment,
								// it has to be put in code memory, otherwise it
								// has to be written to data memory.
								if (phdrs[i].p_flags & PF_X) {

									of_info->mem_size_code += phdrs[i].p_filesz;
								}
								else {
									// The data written to data memory are global
									// variables and constants.
									// Global variables also have a writeable
									// flag set.
									if (phdrs[i].p_flags & PF_W) {
										of_info->mem_size_globals += phdrs[i].p_memsz;
									}
									else {
										of_info->mem_size_rodata += phdrs[i].p_memsz;
									}
								}
							}
							else {
								fprintf (stdout, "mrk3_load_elf: error: No readable flag set for segment at 0x%lx, size 0x%lx\n", lma, phdrs[i].p_filesz);
							}
						}
						else {
							fprintf (stdout, "mrk3_load_elf: error: Could not read segment at 0x%lx, size 0x%lx\n", lma, phdrs[i].p_filesz);
						}
						free (buf);
					}
				}
				else {
					fprintf (stdout, "mrk3_load_elf: error: Failed to read program headers\n");
				}
			}
			else {
				fprintf (stdout, "mrk3_load_elf: error: Failed allocate memory to hold program headers\n");
			}
		}
		else {
			fprintf (stdout, "mrk3_load_elf: error: Failed to get size of program headers\n");
		}
	}
	else {
		fprintf (stdout, "mrk3_load_elf: error: %s not a mrk3 program\n", prog);
	}

	// Finally close the bfd if we opened it.
	if (abfd == NULL && prog_bfd != NULL)
		bfd_close (prog_bfd);
	return true;
}

int sim_read (SIM_DESC sd, SIM_ADDR addr, unsigned char *buffer, int size) {
	return Dll_SimRead(addr, buffer, size);
}

int sim_write (SIM_DESC sd, SIM_ADDR addr, unsigned char *buffer, int size) {
	return Dll_SimWrite(addr, buffer, size);
}

int sim_fetch_register (SIM_DESC sd, int rn, unsigned char *memory, int length) {
	return Dll_SimReadRegister(rn, memory, length);
}

int sim_store_register (SIM_DESC sd, int rn, unsigned char *memory, int length) {
	return Dll_SimWriteRegister(rn, memory, length);
}

void sim_stop_reason (SIM_DESC sd, enum sim_stop * reason,  int *sigrc) {
	*reason = sim_status.exception;
	*sigrc = sim_status.signal;
}

int sim_stop (SIM_DESC sd) {
	Dll_SimStop();
	sim_status.exception = sim_stopped;
	sim_status.signal = TARGET_SIGNAL_INT;
	return true;
}

void sim_resume (SIM_DESC sd, int step, int signal) {
	uint32_t old_mem_space = Dll_GetMemSpace();
	uint32_t new_mem_space;
	sim_status.exception = sim_running;
	sim_status.signal = TARGET_SIGNAL_0;

	// Dll_Simresume returns false only if an exception
	// occured during simulation.
	if (Dll_SimResume(step)) {
		sim_status.exception = sim_stopped;
		sim_status.signal = TARGET_SIGNAL_TRAP;
	}
	else {
		// TODO: We encountered an exception during
		// execution and should forward this information
		// to GDB somehow.
		sim_status.exception = sim_stopped;
		sim_status.signal = TARGET_SIGNAL_TRAP;
	}

	// If the mem_space changed during execution,
	new_mem_space	= Dll_GetMemSpace();
	if (old_mem_space != new_mem_space) {
		// Load the correct symbol information for the current mode.
		mrk3_load_symbol_info(Dll_GetMemSpace());
		// Finally forget everything about cached frames.
		reinit_frame_cache();
	}
}

SIM_DESC sim_open (SIM_OPEN_KIND kind, host_callback *cb, struct bfd *abfd, char **argv)
{
	load_dll(NULL);

	/* Fudge our descriptor for now.  */
	return (SIM_DESC) 1;
}

void
sim_close (SIM_DESC sd, int quitting)
{
	Dll_SimTerminate();
	unload_dll();

	int n = 0;
	// Free the object file info.
	for (n = 0; n < MRK3_MAX_OBJFILES; n++) {
		struct mrk3_objfile_info_t* of_info = &mrk3_objfile_info[n];
		mrk3_free_objfile_info(of_info);
	}
}


SIM_RC sim_load (SIM_DESC sd, char *prog, struct bfd *abfd,	int from_tty ATTRIBUTE_UNUSED) {

	// The file loaded to GDB at startup is the
	// system mode file.
	if (mrk3_load_elf (sd, prog, abfd, MRK3_MEM_SPACE_SYS) == FALSE) {
		return SIM_RC_FAIL;
	}
	return SIM_RC_OK;
}


SIM_RC sim_create_inferior (SIM_DESC sd, struct bfd *prog_bfd, char **argv, char **env)
{
	Dll_SimReset();
	return SIM_RC_OK;
}


bool enable_system_mode_stack_traces = false;

void sim_do_command (SIM_DESC sd, char *cmd) {
	static const char* mrk3_sim_cmd_load_sysfile = "load_sys";
	static const char* mrk3_sim_cmd_load_app1file = "load_app1";
	static const char* mrk3_sim_cmd_load_app2file = "load_app2";
	static const char* mrk3_sim_cmd_dllname = "load_dll";
	static const char* mrk3_sim_cmd_enable_sm_stack_trace = "enable_sm_stack_trace";
	char filename[1024];
	if (cmd == NULL)
		return;

	else if (strncmp (cmd, mrk3_sim_cmd_load_sysfile, strlen(mrk3_sim_cmd_load_sysfile)) == 0) {
		mrk3_extract_filename_from_command(filename, cmd, mrk3_sim_cmd_load_sysfile);
		mrk3_load_elf(sd, filename, NULL, MRK3_MEM_SPACE_SYS);
	}

	else if (strncmp (cmd, mrk3_sim_cmd_load_app1file, strlen(mrk3_sim_cmd_load_app1file)) == 0) {
		mrk3_extract_filename_from_command(filename, cmd, mrk3_sim_cmd_load_app1file);
		mrk3_load_elf(sd, filename, NULL, MRK3_MEM_SPACE_APP1);
	}

	else if (strncmp (cmd, mrk3_sim_cmd_load_app2file, strlen(mrk3_sim_cmd_load_app2file)) == 0) {
		mrk3_extract_filename_from_command(filename, cmd, mrk3_sim_cmd_load_app2file);
		mrk3_load_elf(sd, filename, NULL, MRK3_MEM_SPACE_APP2);
	}

	else if (strncmp (cmd, mrk3_sim_cmd_dllname, strlen(mrk3_sim_cmd_dllname)) == 0) {
		mrk3_extract_filename_from_command(filename, cmd, mrk3_sim_cmd_dllname);
		load_dll(filename);
	}

	else if (strncmp (cmd, mrk3_sim_cmd_enable_sm_stack_trace, strlen(mrk3_sim_cmd_enable_sm_stack_trace)) == 0) {
		int pos = strlen(mrk3_sim_cmd_enable_sm_stack_trace);
		int len = strlen(cmd);
		while ((cmd[pos] == ' ') && (pos < len)) pos++;
		// copy the remaining portion of the command to the filename parameter.
		if (pos < len) 
		{ 
			if ((strncmp(&cmd[pos], "1", len - pos) == 0) || 
				(strncmp(&cmd[pos], "yes", len - pos) == 0) || 
				(strncmp(&cmd[pos], "true", len - pos) == 0))
			{
				enable_system_mode_stack_traces = true;
			}
		}
	}

	// Finally try to pass the command to the dll.
	else if ( ! Dll_SimCommand(cmd) ) {
		printf("Error: \"%s\" is not a valid mrk3 simulator command.\n", cmd);
	}
}

void sim_info (SIM_DESC sd, int verbose) {}


