#ifndef MRK3_REGISTERS_H
#define MRK3_REGISTERS_H


// Enumerate all registers of the architecture.
enum mrk3_sim_reg {
	mrk3_sim_reg_r0,
	mrk3_sim_reg_r1,
	mrk3_sim_reg_r2,
	mrk3_sim_reg_r3,
	mrk3_sim_reg_r4,
	mrk3_sim_reg_r5,
	mrk3_sim_reg_r6,
	mrk3_sim_reg_sp,
	mrk3_sim_reg_pc,
	mrk3_sim_reg_psw,
	mrk3_sim_reg_ssp,
	mrk3_sim_reg_usp,

// Enumerate all pseudo registers.
	mrk3_sim_pseudo_reg_r0l,
	mrk3_sim_pseudo_reg_r1l,
	mrk3_sim_pseudo_reg_r2l,
	mrk3_sim_pseudo_reg_r3l,
	mrk3_sim_pseudo_reg_r0h,
	mrk3_sim_pseudo_reg_r1h,
	mrk3_sim_pseudo_reg_r2h,
	mrk3_sim_pseudo_reg_r3h,

	mrk3_sim_pseudo_reg_r4e,
	mrk3_sim_pseudo_reg_r5e,
	mrk3_sim_pseudo_reg_r6e,

	mrk3_sim_pseudo_reg_r4l,
	mrk3_sim_pseudo_reg_r5l,
	mrk3_sim_pseudo_reg_r6l,

	mrk3_sim_pseudo_reg_s_flag,
	mrk3_sim_pseudo_reg_i_lvl,
	mrk3_sim_pseudo_reg_z_flag,
	mrk3_sim_pseudo_reg_n_flag,
	mrk3_sim_pseudo_reg_o_flag,
	mrk3_sim_pseudo_reg_c_flag,

	mrk3_sim_reg_num_regs,
};


/* Register names as displayed by GDB. This includes real registers and pseudo
 * registers. Special function registers are dependent on the simulator and not
 * included in this list. */
static const char * const mrk3_sim_reg_names[] =
{
	// real registers:
	"R0", "R1", "R2", "R3", "R4", "R5", "R6", "SP", "PC", "PSW", "SSP", "USP",
	// pseudo registers:
	"R0L", "R1L", "R2L", "R3L", "R0H", "R1H", "R2H", "R3H",

	"R4e", "R5e", "R6e", "R4l", "R5l", "R6l",

	"SYS", "INT", "ZERO", "NEG", "OVERFLOW", "CARRY",
};


#endif // MRK3_REGISTERS_H
