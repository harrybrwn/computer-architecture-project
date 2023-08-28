#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef __STRICT_ANSI__
void bzero(void*, size_t);
#endif

#include "mips.h"

#ifndef DUMB_ASSIGNMENT_EDGECASES
# define DUMB_ASSIGNMENT_EDGECASES 1
#endif

#define INVALID_REG ((uint8_t)-1)

#define OPMASK    0xfc000000
#define RSMASK    0x03e00000
#define RTMASK    0x001f0000
#define RDMASK    0x0000f800
#define SHAMTMASK 0x000007c0
#define FUNCTMASK 0x0000003f
#define IMMMASK   0x0000ffff
#define ADDRMASK  0x03ffffff

/* Argument order for each syntax */
static syn_t syntax[][3] = {
	/* ARITHLOG  */ {rd, rs, rt},
	/* DIVMULT   */ {rs, rt, ignore},
	/* SHIFT     */ {rd, rt, shift},
	/* SHIFTV    */ {rd, rt, rs},
	/* JUMPR     */ {rs,     ignore},
	/* MOVEFROM  */ {rd,     ignore},
	/* MOVETO    */ {rs,     ignore},
	/* ARITHLOGI */ {rt, rs, immediate},
	/* LOADI     */ {rt, immediate, ignore},
	/* BRANCH    */ {rs, rt, label},
	/* BRANCHZ   */ {rs, label,  ignore},
	/* LOADSTORE */ {rt, offset, ignore},
	/* JUMP      */ {label,      ignore},
	/* TRAP      */ {trap,       ignore},
	/* NONE      */ {ignore},
};

/* The index of each register name is the register number */
static char* registers[] = {
	"zero",
	"at",
	"v0",
	"v1",
	"a0",
	"a1",
	"a2",
	"a3",
	"t0",
	"t1",
	"t2",
	"t3",
	"t4",
	"t5",
	"t6",
	"t7",
	"s0",
	"s1",
	"s2",
	"s3",
	"s4",
	"s5",
	"s6",
	"s7",
	"t8",
	"t9",
	"k0",
	"k1",
	"gp",
	"sp",
	"fp",
	"ra",
	"pc",
	"hi",
	"lo",
};

/**
 * List of all instructions with opcode/funct,
 * syntax type, and format type
 *
 * This code was auto-generated
 */
static struct op opcodes[] = {
	{ 0b100000, R, ARITHLOG,  "add" },
	{ 0b001000, I, ARITHLOGI, "addi" },
	{ 0b001001, I, ARITHLOGI, "addiu" },
	{ 0b100001, R, ARITHLOG,  "addu" },
	{ 0b100100, R, ARITHLOG,  "and" },
	{ 0b001100, I, ARITHLOGI, "andi" },
	{ 0b000100, I, BRANCH,    "beq" },
	{ 0b000111, I, BRANCHZ,   "bgtz" },
	{ 0b000110, I, BRANCHZ,   "blez" },
	{ 0b000101, I, BRANCH,    "bne" },
	{ 0b011010, R, DIVMULT,   "div" },
	{ 0b011011, R, DIVMULT,   "divu" },
	{ 0b000010, J, JUMP,      "j" },
	{ 0b000011, J, JUMP,      "jal" },
	{ 0b001001, R, JUMPR,     "jalr" },
	{ 0b001000, R, JUMPR,     "jr" },
	{ 0b100000, I, LOADSTORE, "lb" },
	{ 0b100100, I, LOADSTORE, "lbu" },
	{ 0b100001, I, LOADSTORE, "lh" },
	{ 0b011001, I, LOADI,     "lhi" },
	{ 0b100101, I, LOADSTORE, "lhu" },
	{ 0b001111, I, LOADI,     "lui"},
	{ 0b011000, I, LOADI,     "llo" },
	{ 0b100011, I, LOADSTORE, "lw" },
	{ 0b010000, R, MOVEFROM,  "mfhi" },
	{ 0b010010, R, MOVEFROM,  "mflo" },
	{ 0b010001, R, MOVETO,    "mthi" },
	{ 0b010011, R, MOVETO,    "mtlo" },
	{ 0b011000, R, DIVMULT,   "mult" },
	{ 0b011001, R, DIVMULT,   "multu" },
	{ 0b100111, R, ARITHLOG,  "nor" },
	{ 0b100101, R, ARITHLOG,  "or" },
	{ 0b001101, I, ARITHLOGI, "ori" },
	{ 0b101000, I, LOADSTORE, "sb" },
	{ 0b101001, I, LOADSTORE, "sh" },
	{ 0b000000, R, SHIFT,     "sll" },
	{ 0b000100, R, SHIFTV,    "sllv" },
	{ 0b101010, R, ARITHLOG,  "slt" },
	{ 0b001010, I, ARITHLOGI, "slti" },
	{ 0b001011, I, ARITHLOGI, "sltiu" },
	{ 0b101011, R, ARITHLOG,  "sltu" },
	{ 0b000011, R, SHIFT,     "sra" },
	{ 0b000111, R, SHIFTV,    "srav" },
	{ 0b000010, R, SHIFT,     "srl" },
	{ 0b000110, R, SHIFTV,    "srlv" },
	{ 0b100010, R, ARITHLOG,  "sub" },
	{ 0b100011, R, ARITHLOG,  "subu" },
	{ 0b101011, I, LOADSTORE, "sw" },
	{ 0b011010, J, TRAP,      "trap" },
	{ 0b100110, R, ARITHLOG,  "xor" },
	{ 0b001110, I, ARITHLOGI, "xori" },

	{ 0b001100, R, NONE, "syscall" },
	{ 0b000000, J, NONE, "nop" },
};

const char* mips_error_message(error_t err)
{
	switch (err)
	{
	case OK:
		return "<nil>";
	case ERR_MISSING_PAREN:
		return "missing parenthesis";
	case ERR_EXPECTED_HEX:
		return "expected number in hex format";
	case ERR_EXPECTED_DEC:
		return "expected number in decimal format";
	case ERR_BAD_NUMBER:
		return "bad number";
	case ERR_BAD_REG:
		return "bad register";
	case ERR_ARG_TOO_LONG:
		return "instruction argument too long";
	case ERR_OP_NOT_FOUND:
		return "opcode not found";
	case ERR_BAD_FORMAT:
		return "bad instruction format";
	}
	return "<nil>";
}

int is_unsigned(instruction_t* in)
{
	funct_t funct;
	opcode_t op;
	switch (in->op.fmt)
	{
	case R:
		funct = in->op.id;
		switch (funct)
		{
		case OP_MULTU:
		case OP_SLTU:
		case OP_ADDU:
		case OP_SUBU:
		case OP_DIVU:
			return 1;
		case OP_SRA:
		case OP_SLT:
		case OP_MTHI:
		case OP_AND:
		case OP_SLLV:
		case OP_XOR:
		case OP_MFLO:
		case OP_ADD:
		case OP_SRL:
		case OP_MFHI:
		case OP_MTLO:
		case OP_DIV:
		case OP_SRLV:
		case OP_JALR:
		case OP_OR:
		case OP_SUB:
		case OP_MULT:
		case OP_NOR:
		case OP_SLL:
		case OP_SRAV:
		case OP_JR:
		case OP_SYSCALL:
			return 0;
		}
		break;
	case I:
	case J:
		op = in->op.id;

		switch (op)
		{
		case OP_SLTIU:
		case OP_LBU:
		case OP_ADDIU:
		case OP_LHU:
		case OP_LUI:
			return 1;
		case OP_SH:
		case OP_XORI:
		case OP_BGTZ:
		case OP_LHI:
		case OP_SB:
		case OP_ADDI:
		case OP_BEQ:
		case OP_LW:
		case OP_ANDI:
		case OP_SLTI:
		case OP_SW:
		case OP_ORI:
		case OP_BNE:
		case OP_BLEZ:
		case OP_LH:
		case OP_LLO:
		case OP_LB:
		case OP_JAL:
		case OP_TRAP:
		case OP_NOP:
		case OP_J:
			return 0;
		}
		break;
	}
	return 0;
}

/* string to uint32_t */
static uint32_t parse_uint32(char*, error_t*);

/* Register helpers */
static uint8_t get_reg(char*);

/* Assembly parsing */
static int     get_instruction_name(char* raw, int len, char* dest);
static error_t get_instruction_args(char* raw, int len, char dest[3][MAX_LABEL_LEN]);
static error_t parse_offset(char*, uint32_t*, uint8_t*);

/* De-Assembling helper functions */
static struct op decode_op(uint32_t instr);
static struct op decode_funct_op(uint32_t instr);

void zero_out_instruction(instruction_t* in)
{
	in->reg[0] = 0;
	in->reg[1] = 0;
	in->reg[2] = 0;
	in->imm    = 0;
	in->shamt  = 0;
	in->funct  = 0;
}

uint32_t to_binary(instruction_t* in)
{
	uint32_t i;
	switch (in->op.fmt)
	{
	case R:
		i = in->reg[rs] << 21 |
			in->reg[rt] << 16 |
			in->reg[rd] << 11 |
			in->shamt   << 6 |
			in->funct;
		break;
	case I:
		i = in->op.id   << 26 |
			in->reg[rs] << 21 |
			in->reg[rt] << 16 |
			(in->imm & IMMMASK);
		break;
	case J:
		i = in->op.id << 26 | (in->imm & IMMMASK);
		break;
	}
	return i;
}

/**
 * Takes an raw assembly instruction and uses it to set the fields
 * on the instruction pointer
 */
error_t assemble(instruction_t* in, char* instr)
{
	int i, len, off;
	error_t err = OK;
	syn_t* syn;
	char name[MAX_INSTRUCTION_NAME_LEN];
	char args[3][MAX_LABEL_LEN];

	bzero(name, MAX_INSTRUCTION_NAME_LEN);
	for (i = 0; i < 3; i++)
	{
		bzero(args[i], MAX_LABEL_LEN);
	}
	len = strlen(instr);
	if (len == 0)
		return ERR_OP_NOT_FOUND;
	off = get_instruction_name(instr, len, name);
	err = get_instruction_args(instr+off, len-off, args);
	if (err != OK)
		return err;

	in->op = find_op(name);
	if (in->op.name[0] == '\0' && in->op.id == 0 && in->op.fmt == 0)
		return ERR_OP_NOT_FOUND;

	if (in->op.fmt == R)
		in->funct = in->op.id;
	syn = syntax[in->op.syn];

	for (i = 0; i < 3; i++)
	{
		int ix = syn[i];
		switch (ix)
		{
		case ignore:
			goto done;
			break;
		case shift:
			in->shamt = (uint8_t) parse_uint32(args[i], &err);
			break;
		case immediate:
			in->imm = IMMMASK & parse_uint32(args[i], &err);
			break;
		case offset:
			err = parse_offset(args[i], &in->imm, &in->reg[rs]);
			if (err != OK)
				return err;
			break;
		case label:
			in->label = args[i];
			in->imm = parse_uint32(args[i], &err); // try to parse as a number
			if (args[i][0] != '\0')
				err = OK;
			break;
		case trap:
			in->imm = parse_uint32(args[i], &err);
			break;
		default:
			if (args[i][0] == '\0')
				return ERR_BAD_FORMAT;
			in->reg[ix] = get_reg(args[i]);
			if (in->reg[ix] == INVALID_REG)
				return ERR_BAD_REG;
			break;
		}
		if (err != OK)
			return err;
	}
done:
	return OK;
}

/**
 * decode a binary instruction into and store it's
 * fields in an instruction struct
 */
void decode(instruction_t* in, uint32_t instruction)
{
	in->op = decode_op(instruction);
	switch (in->op.fmt)
	{
	case R:
		in->reg[rs] = (instruction & RSMASK) >> 21;
		in->reg[rt] = (instruction & RTMASK) >> 16;
		in->reg[rd] = (instruction & RDMASK) >> 11;
		in->shamt   = (instruction & SHAMTMASK) >> 6;
		in->funct   = (instruction & FUNCTMASK);
		break;
	case I:
		in->reg[rs] = (instruction & RSMASK) >> 21;
		in->reg[rt] = (instruction & RTMASK) >> 16;
		in->imm     = (instruction & IMMMASK);
		break;
	case J:
		in->imm = (instruction & ADDRMASK);
		break;
	}
}

#define INSTRUCTION_SEP '\t'

error_t to_assembly(instruction_t* in, char* buf)
{
	int i = 0, j, k, l;
	syn_t* syn;
	char* name = in->op.name;
	char args[3][MAX_LABEL_LEN];
	char* regname;
	uint32_t imm = in->imm;
	if (is_unsigned(in) && in->imm >> 15)
		imm = in->imm | 0xffff0000;

	while (*name)
	{
		buf[i++] = *name;
		name++;
	}
	buf[i++] = INSTRUCTION_SEP;

	syn = syntax[in->op.syn];
	for (j = 0; j < 3; j++)
	{
		int ix = syn[j];
		bzero(args[j], sizeof(args[0]));
		switch (ix)
		{
		case ignore:
			goto done;
		case shift:
			snprintf(args[j], sizeof(args[0]), "%d", in->shamt);
			break;
		case trap: // fallthrough
		case immediate:
			switch (in->op.syn)
			{
			case ARITHLOGI:
				if (is_unsigned(in))
					snprintf(args[j], sizeof(args[0]), "%d", imm);
				else
					snprintf(args[j], sizeof(args[0]), "0x%x", imm);
				break;
			case LOADI:
				snprintf(args[j], sizeof(args[0]), "0x%x", imm);
				break;
			default:
				snprintf(args[j], sizeof(args[0]), "%d", (int)imm);
				break;
			}
			break;
		case label:
			switch (in->op.syn)
			{
			case BRANCH:
			case JUMP:
				snprintf(args[j], sizeof(args[0]), "0x%08x", imm << 2);
				break;
			default:
				snprintf(args[j], sizeof(args[0]), "0x%x", imm);
				break;
			}
			break;
		case offset:
			regname = register_name(in->reg[rs]);
			if (regname == NULL)
				return ERR_BAD_REG;
			snprintf(args[j], sizeof(args[0]), "%d($%s)", imm, regname);
			break;
		default:
		#if DUMB_ASSIGNMENT_EDGECASES
			snprintf(args[j], sizeof(args[0]), "$%d", in->reg[ix]);
		#else
			regname = register_name(in->reg[ix]);
			if (regname == NULL)
				return ERR_BAD_REG;
			snprintf(args[j], sizeof(args[0]), "$%s", regname);
		#endif
			break;
		}
	}
done:

	for (k = 0; k < j; k++)
	{
		for (l = 0; l < (int)sizeof(args[0]) && args[k][l] != '\0'; l++)
			buf[i++] = args[k][l];
		if (k != j-1)
		{
			buf[i++] = ',';
			buf[i++] = ' ';
		}
	}
	return OK;
}

struct op find_op(char* name)
{
	unsigned long i;
	for (i = 0; i < sizeof(opcodes)/sizeof(struct op); i++)
		if (strcmp(name, opcodes[i].name) == 0)
			return (struct op)opcodes[i];
	return (struct op){0, 0, 0, ""};
}

char* register_name(uint8_t reg)
{
	if (reg > (sizeof(registers)/sizeof(char*)))
		return NULL;
	return registers[reg];
}

/* insert n into the buffer in binary format */
void write_bin(char* buf, int buf_size, unsigned int n)
{
	buf[buf_size] = '\0';
	int i = buf_size-1;
	do {
		if (n == 0)
			buf[i--] = '0';
		else if (n % 2 == 0)
			buf[i--] = '0';
		else
			buf[i--] = '1';
		n = n / 2;
	} while (i >= 0);
}

/**
 * parse either a hex, binary, or decimal formated
 * string into an unsigned, 32 bit integer.
 */
static uint32_t parse_uint32(char* s, error_t* err)
{
	uintmax_t res;
	int negative = 0;
	int base = 10;
	char* end;
	if (s[0] == '\0')
	{
		*err = ERR_BAD_NUMBER;
		return 0;
	}

	if (s[0] == '0' && s[1] == 'x')
	{
		base = 16;
		s += 2;
	}
	else if (s[0] == '0' && s[1] == 'b')
	{
		base = 2;
		s += 2;
	}
	else if (s[0] == '-')
	{
		s++;
		negative = 1;
	}

	res = (uint32_t) strtoumax(s, &end, base);
	if (errno == ERANGE)
	{
		// TODO handle integer overflow errors
		*err = ERR_BAD_NUMBER;
	}
	if (negative)
		return ((signed int)res) * -1;
	*err = OK;
	return res;
}

static error_t parse_offset(char* s, uint32_t* off, uint8_t* reg)
{
	error_t err = OK;
	size_t i, j;
	size_t len = strlen(s);
	char* offset_str;
	char reg_str[8]; // should never be longer that 3
	bzero(reg_str, 8);
	offset_str = malloc(len * sizeof(char));
	bzero(offset_str, len);

	// TODO if a paren is not found return PARSE_ERROR_NOPAREN

	// eat s until a "("
	for (i = 0; ; i++)
	{
		if (s[i] == '(')
			break;
		else if (i >= len)
			return ERR_MISSING_PAREN;
		offset_str[i] = s[i];
	}
	i++; // skip the "("

	// eat s until a ")"
	for (j = 0; ; i++, j++)
	{
		if (s[i] == ')')
			break;
		else if (i >= len)
			return ERR_MISSING_PAREN;
		reg_str[j] = s[i];
	}

	/* Enforce only decimal offset */
	if (offset_str[0] == '0' || offset_str[1] == 'x')
	{
		free(offset_str);
		return ERR_EXPECTED_DEC;
	}

	*off = parse_uint32(offset_str, &err); // sets error
	*reg = get_reg(reg_str);
	if (*reg == INVALID_REG)
		return ERR_BAD_REG;

	free(offset_str);
	return err;
}

uint8_t get_register(char* name)
{
	return get_reg(name);
}

static uint8_t get_reg(char* name)
{
	long unsigned int i;
	if (name[0] == '$')
		name++;
	for (i = 0; i < sizeof(registers)/sizeof(registers[0]); i++)
	{
		if (strcmp(name, registers[i]) == 0)
		{
			return i;
		}
	}
	return INVALID_REG;
}

static int get_instruction_name(char* raw, int len, char* dest)
{
	int i;
	for (i = 0; i < len; i++)
	{
		if (raw[i] == ' ' || raw[i] == '\t')
		{
			i++;
			break;
		}
		dest[i] = raw[i];
	}
	return i;
}

static error_t get_instruction_args(char* raw, int len, char dest[3][MAX_LABEL_LEN])
{
	int i = 0, arg = 0, l = 0;
	while (i < len && raw[i] != '\0')
	{
		if (raw[i] == ',')
		{
			arg++;
			i++;
			l = 0;
			continue;
		}
		else if (raw[i] == ' ' || raw[i] == '\t')
		{
			i++;
		}
		else
		{
			// If the argument is too long return an error
			if (l > MAX_LABEL_LEN)
				return ERR_ARG_TOO_LONG;
			dest[arg][l++] = raw[i++];
		}
	}
	return OK;
}

static struct op decode_op(uint32_t instr)
{
    uint32_t opcode = (instr & OPMASK) >> 26;
    switch (opcode)
    {
    case 0:          return decode_funct_op(instr & FUNCTMASK);
    case OP_LLO:     return (struct op){OP_LLO, I, LOADI, "llo"};
    case OP_LHU:     return (struct op){OP_LHU, I, LOADSTORE, "lhu"};
    case OP_SLTI:    return (struct op){OP_SLTI, I, ARITHLOGI, "slti"};
    case OP_BGTZ:    return (struct op){OP_BGTZ, I, BRANCHZ, "bgtz"};
    case OP_LH:      return (struct op){OP_LH, I, LOADSTORE, "lh"};
    case OP_SLTIU:   return (struct op){OP_SLTIU, I, ARITHLOGI, "sltiu"};
    case OP_SH:      return (struct op){OP_SH, I, LOADSTORE, "sh"};
    case OP_ADDI:    return (struct op){OP_ADDI, I, ARITHLOGI, "addi"};
    case OP_LUI:     return (struct op){OP_LUI, I, LOADI, "lui"};
    case OP_SW:      return (struct op){OP_SW, I, LOADSTORE, "sw"};
    case OP_BEQ:     return (struct op){OP_BEQ, I, BRANCH, "beq"};
    case OP_SB:      return (struct op){OP_SB, I, LOADSTORE, "sb"};
    case OP_XORI:    return (struct op){OP_XORI, I, ARITHLOGI, "xori"};
    case OP_BLEZ:    return (struct op){OP_BLEZ, I, BRANCHZ, "blez"};
    case OP_LB:      return (struct op){OP_LB, I, LOADSTORE, "lb"};
    case OP_LBU:     return (struct op){OP_LBU, I, LOADSTORE, "lbu"};
    case OP_ANDI:    return (struct op){OP_ANDI, I, ARITHLOGI, "andi"};
    case OP_ORI:     return (struct op){OP_ORI, I, ARITHLOGI, "ori"};
    case OP_LHI:     return (struct op){OP_LHI, I, LOADI, "lhi"};
    case OP_ADDIU:   return (struct op){OP_ADDIU, I, ARITHLOGI, "addiu"};
    case OP_LW:      return (struct op){OP_LW, I, LOADSTORE, "lw"};
    case OP_BNE:     return (struct op){OP_BNE, I, BRANCH, "bne"};
    case OP_TRAP:    return (struct op){OP_TRAP, J, TRAP, "trap"};
    case OP_J:       return (struct op){OP_J, J, JUMP, "j"};
    case OP_JAL:     return (struct op){OP_JAL, J, JUMP, "jal"};
    }
    return (struct op){0b0, R, NONE, ""};
}

static struct op decode_funct_op(funct_t funct)
{
	switch (funct)
	{
	case OP_ADDU:    return (struct op){OP_ADDU, R, ARITHLOG, "addu"};
	case OP_JR:      return (struct op){OP_JR, R, JUMPR, "jr"};
	case OP_SYSCALL: return (struct op){OP_SYSCALL, R, NONE, "syscall"};
	case OP_NOR:     return (struct op){OP_NOR, R, ARITHLOG, "nor"};
	case OP_SLTU:    return (struct op){OP_SLTU, R, ARITHLOG, "sltu"};
	case OP_ADD:     return (struct op){OP_ADD, R, ARITHLOG, "add"};
	case OP_AND:     return (struct op){OP_AND, R, ARITHLOG, "and"};
	case OP_MFHI:    return (struct op){OP_MFHI, R, MOVEFROM, "mfhi"};
	case OP_SLLV:    return (struct op){OP_SLLV, R, SHIFTV, "sllv"};
	case OP_SRL:     return (struct op){OP_SRL, R, SHIFT, "srl"};
	case OP_OR:      return (struct op){OP_OR, R, ARITHLOG, "or"};
	case OP_MTHI:    return (struct op){OP_MTHI, R, MOVETO, "mthi"};
	case OP_MTLO:    return (struct op){OP_MTLO, R, MOVETO, "mtlo"};
	case OP_MFLO:    return (struct op){OP_MFLO, R, MOVEFROM, "mflo"};
	case OP_MULTU:   return (struct op){OP_MULTU, R, DIVMULT, "multu"};
	case OP_SLL:     return (struct op){OP_SLL, R, SHIFT, "sll"};
	case OP_SUB:     return (struct op){OP_SUB, R, ARITHLOG, "sub"};
	case OP_DIV:     return (struct op){OP_DIV, R, DIVMULT, "div"};
	case OP_DIVU:    return (struct op){OP_DIVU, R, DIVMULT, "divu"};
	case OP_MULT:    return (struct op){OP_MULT, R, DIVMULT, "mult"};
	case OP_SLT:     return (struct op){OP_SLT, R, ARITHLOG, "slt"};
	case OP_XOR:     return (struct op){OP_XOR, R, ARITHLOG, "xor"};
	case OP_JALR:    return (struct op){OP_JALR, R, JUMPR, "jalr"};
	case OP_SRAV:    return (struct op){OP_SRAV, R, SHIFTV, "srav"};
	case OP_SRA:     return (struct op){OP_SRA, R, SHIFT, "sra"};
	case OP_SRLV:    return (struct op){OP_SRLV, R, SHIFTV, "srlv"};
	case OP_SUBU:    return (struct op){OP_SUBU, R, ARITHLOG, "subu"};
	}
	return (struct op){0b0, R, NONE, ""};
}
