#ifndef _MIPS_RUNTIME
#define _MIPS_RUNTIME

#include <stdio.h>
#include "mips.h"

#define ADDRESS_BEGIN 0x00400000

#define MAXNUMINSTRS 1024	/* max # instructions in a program */
#define MAXNUMDATA 3072		/* max # data words */

typedef struct {
	uint32_t rs;
	uint32_t rt;
	uint32_t rd;
} regvals_t;

#define REG_DST_RT 0
#define REG_DST_RD 1

enum alu_op {
	ALU_AND = 0b0000,
	ALU_OR  = 0b0001,
	ALU_ADD = 0b0010,
	ALU_SUB = 0b0110,
	ALU_SLT = 0b0111,
	ALU_NOR = 0b1100,
};

/* Control signals */
struct signals {
	int branch : 1;
	int jump   : 1;

	int alu_src : 1;
	enum alu_op alu_op  : 4;

	int mem_read   : 1;
	int mem_write  : 1;
	int mem_to_reg : 1;

	/**
	 * Used for the variants of 'sw' and 'lw'
	 * like 'sb', 'lh', or 'lhu'
	 *
	 * Should default to 0xffffffff
	 */
	uint32_t mem_mask;

	int reg_write : 1;
	int reg_dst   : 1;

	int sign_extend;

	int syscall;
	error_t error;
};

typedef struct {
	/* Main memory */
	uint32_t memory[MAXNUMINSTRS + MAXNUMDATA];
	/* Register values indexed by register number */
	uint32_t registers[32];
	/* Program counter */
	uint32_t pc;
	/* Control signals */
	struct signals sigs;
	/**
	 * Address of the last instruction in memory. Should
	 * be reset when a program is loaded.
	 */
	uint32_t instruction_end;
} computer_t;

/**
 * Initialize a computer from an input file
 */
void mips_computer_init(computer_t* c, FILE* infile);

/**
 * Run a program that has been loaded into memory
 * given the address of the last instruction
 */
int mips_run(computer_t* c, uint32_t instruction_end);

/**
 * Fetch the instruction at the address.
 */
uint32_t mips_fetch(computer_t* c, uint32_t addr);

/**
 * Execute an instruction
 */
uint32_t mips_execute(computer_t* c, instruction_t* in, regvals_t* state);

void mips_write_memory(computer_t*c, uint32_t address, uint32_t value);

/**
 * Check the control signals and perform memory access per instruction.
 */
int mips_memory(computer_t* c, instruction_t* in, uint32_t val, uint32_t *changed_reg);

void mips_update_pc(computer_t*, instruction_t*, uint32_t);

/**
 * Check the control signals for register writes and perform register writing
 */
void mips_reg_write(computer_t* c, instruction_t* in, uint32_t val, uint32_t *changed_reg);

void mips_computer_reset(computer_t* c);
void mips_reset_signals(struct signals *s);

#endif /* _MIPS_RUNTIME */