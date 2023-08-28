#ifndef __MIPS_H
#define __MIPS_H

#include <inttypes.h>

/* This enum was auto-generated */
typedef enum {
  OP_LB      = 0b100000,
  OP_ADDIU   = 0b001001,
  OP_ANDI    = 0b001100,
  OP_BLEZ    = 0b000110,
  OP_SH      = 0b101001,
  OP_LHI     = 0b011001,
  OP_SLTIU   = 0b001011,
  OP_LW      = 0b100011,
  OP_LHU     = 0b100101,
  OP_SW      = 0b101011,
  OP_LUI     = 0b001111,
  OP_ORI     = 0b001101,
  OP_ADDI    = 0b001000,
  OP_SLTI    = 0b001010,
  OP_XORI    = 0b001110,
  OP_BEQ     = 0b000100,
  OP_LH      = 0b100001,
  OP_BNE     = 0b000101,
  OP_LBU     = 0b100100,
  OP_SB      = 0b101000,
  OP_LLO     = 0b011000,
  OP_BGTZ    = 0b000111,
  OP_JAL     = 0b000011,
  OP_TRAP    = 0b011010,
  OP_NOP     = 0b000000,
  OP_J       = 0b000010,
} opcode_t;

/* This enum was auto-generated */
typedef enum {
  OP_SLL     = 0b000000,
  OP_SLT     = 0b101010,
  OP_MTHI    = 0b010001,
  OP_JALR    = 0b001001,
  OP_XOR     = 0b100110,
  OP_JR      = 0b001000,
  OP_DIVU    = 0b011011,
  OP_SRA     = 0b000011,
  OP_SRLV    = 0b000110,
  OP_SRAV    = 0b000111,
  OP_NOR     = 0b100111,
  OP_OR      = 0b100101,
  OP_SRL     = 0b000010,
  OP_SLLV    = 0b000100,
  OP_MULT    = 0b011000,
  OP_MFLO    = 0b010010,
  OP_MULTU   = 0b011001,
  OP_SUB     = 0b100010,
  OP_ADD     = 0b100000,
  OP_AND     = 0b100100,
  OP_SLTU    = 0b101011,
  OP_MFHI    = 0b010000,
  OP_MTLO    = 0b010011,
  OP_SYSCALL = 0b001100,
  OP_ADDU    = 0b100001,
  OP_DIV     = 0b011010,
  OP_SUBU    = 0b100011,
} funct_t;

enum reg_num {
  REG_ZERO = 0,
  REG_AT = 1,
  REG_V0 = 2,
  REG_V1 = 3,
  REG_A0 = 4,
  REG_A1 = 5,
  REG_A2 = 6,
  REG_A3 = 7,
  REG_T0 = 8,
  REG_T1 = 9,
  REG_T2 = 10,
  REG_T3 = 11,
  REG_T4 = 12,
  REG_T5 = 13,
  REG_T6 = 14,
  REG_T7 = 15,
  REG_S0 = 16,
  REG_S1 = 17,
  REG_S2 = 18,
  REG_S3 = 19,
  REG_S4 = 20,
  REG_S5 = 21,
  REG_S6 = 22,
  REG_S7 = 23,
  REG_T8 = 24,
  REG_T9 = 25,
  REG_K0 = 26,
  REG_K1 = 27,
  REG_GP = 28,
  REG_SP = 29,
  REG_FP = 30,
  REG_RA = 31,
  REG_PC = 32,
  REG_HI = 33,
  REG_LO = 34,
};

#define MAX_INSTRUCTION_NAME_LEN 5
#define MAX_LABEL_LEN 128

typedef enum {
	R,
	I,
	J,
} format_t;

typedef enum {
	rs = 0,
	rt = 1,
	rd = 2,
	shift,
	immediate,
	offset,
	label,
	trap,
	ignore,
} syn_t; // TODO rename this to syn_element_t or something

typedef enum {
	ARITHLOG,
	DIVMULT,
	SHIFT,
	SHIFTV,
	JUMPR,
	MOVEFROM,
	MOVETO,
	ARITHLOGI,
	LOADI,
	BRANCH,
	BRANCHZ,
	LOADSTORE,
	JUMP,
	TRAP,
	NONE,
} syntax_t;

/* Parsing errors */
typedef enum {
	OK,
	ERR_MISSING_PAREN,
	ERR_EXPECTED_HEX, // should be hex format
	ERR_EXPECTED_DEC, // should be decimal format
	ERR_BAD_NUMBER,
	ERR_BAD_REG,
	ERR_ARG_TOO_LONG,
	ERR_OP_NOT_FOUND,
  ERR_BAD_FORMAT,
} error_t;

struct op {
	uint8_t  id; // either the opcode or funct
	format_t fmt; // R, I, or J format
	syntax_t syn;
	char*    name;
};

struct r_fields {
	uint8_t rs;
	uint8_t rt;
	uint8_t rd;
	uint8_t shamt;
	uint8_t funct;
};

struct i_fields {
	uint8_t  rs;
	uint8_t  rt;
	uint32_t imm;
};

struct j_fields {
	uint32_t target;
};

/**
 * Instruction struct holding the info on the operation (see struct op),
 * the three pseudo registers, the shift amount, the immediate value, the
 * function, and sometimes the raw string representation of a label.
 */
typedef struct {
	struct op op;
	uint8_t   reg[3]; // rs, rt, rd
	uint8_t   shamt;
	uint32_t  imm;
	uint8_t   funct;
	char*     label; // raw label from assembly
	union {
		struct r_fields r;
		struct i_fields i;
		struct j_fields j;
	} fields;
} instruction_t;

/**
 * Convert an instruction to a
 * 32 bit binary number.
 */
uint32_t to_binary(instruction_t*);

/**
 * Takes an raw assembly instruction and
 * parses it to set the fields on the
 * instruction pointer.
 */
error_t assemble(instruction_t* in, char* instr);

/**
 * Converts a decoded instruction back to
 * a human readable assembly instruction.
 * Does not do any bounds checking on the buffer.
 */
error_t to_assembly(instruction_t* in, char* buf);

/**
 * Decode a binary instruction into and store it's
 * fields in an instruction struct
 */
void decode(instruction_t* in, uint32_t instruction);

/**
 * Set all fields in an instruction to
 * a zero value
 */
void zero_out_instruction(instruction_t*);

/**
 * Get the name of a register given the register number.
 * Returns NULL if the register number is out of bounds.
 */
char* register_name(uint8_t reg);

/**
 * Convert n into binary and write it into the
 * buffer of size buf_size.
 */
void write_bin(char* buf, int buf_size, unsigned int n);

const char* mips_error_message(error_t);

struct op find_op(char*);
uint8_t get_register(char* name);

int is_unsigned(instruction_t* in);

#endif /* __MIPS_H */