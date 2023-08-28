#include "mips.h"
#include "runtime.h"

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>

#ifdef __STRICT_ANSI__
void bzero(void*, size_t);
#endif

static unsigned int endian_swap(unsigned int);
static error_t sim_fetch_dec_exec(computer_t*, instruction_t*, regvals_t*);

void mips_computer_init(computer_t* c, FILE* filein)
{
	int k;
	unsigned int instr;
	for (k = 0; k < 32; k++)
		c->registers[k] = 0;

	c->pc = ADDRESS_BEGIN;

	/* stack pointer - Initialize to highest address of data segment */
	c->registers[29] = ADDRESS_BEGIN + (MAXNUMINSTRS+MAXNUMDATA) * 4;

	for (k = 0; k < MAXNUMINSTRS+MAXNUMDATA; k++)
		c->memory[k] = 0;

    if (filein == NULL)
    {
        c->instruction_end = ADDRESS_BEGIN + (MAXNUMINSTRS)*4;
        return;
    }

	k = 0;
	while (fread(&instr, 4, 1, filein))
	{
		c->memory[k] = ntohl(endian_swap(instr));
		k++;
		if (k > MAXNUMINSTRS)
		{
			fprintf(stderr, "Program too big.\n");
			exit(1);
		}
	}
	c->instruction_end = ADDRESS_BEGIN + (4*(k-1));
}

int mips_run(computer_t* c, uint32_t instruction_end)
{
	uint32_t a0;
	instruction_t in;
	regvals_t regs;
	error_t err;

	c->pc = ADDRESS_BEGIN;
	while (c->pc <= instruction_end)
	{
		/* Fetch -> Decode -> Execute */
		err = sim_fetch_dec_exec(c, &in, &regs);
		if (err != OK)
		{
			fprintf(stderr, "Error at 0x%x: %s\n", c->pc, mips_error_message(err));
			return -1;
		}

		/* Handle syscalls */
		a0 = c->registers[REG_A0];
		switch (c->sigs.syscall)
		{
        case 0: // No syscall
            break;
        case 1:
            printf("%d", a0);
            break;
        case 2:
            printf("%f", (double)a0);
            break;
        case 3:
            printf("%e", (double)a0);
            break;
        case 4:
            printf("%s", (char*)(c->memory + a0));
            break;
        case 11:
            printf("%c", a0);
            break;
        case 34:
            printf("%x", a0);
            break;
        case 10: // exit(0)
			return 0;
        case 17: // exit(a0)
            return (int)a0;
        default:
            fprintf(stderr, "Syscall %d not supported\n", c->sigs.syscall);
            break;
		}
	}
	return 0;
}

#if 0
typedef enum {
    RUN_ASM,
    NEXT_INPUT,
    NEXT_INSTRUCTION,
    QUIT,
} interactive_result_t;

static interactive_result_t handle_interactive(computer_t* c, char* line, size_t line_len, instruction_t* d);

int mips_run_interactive(computer_t* c, uint32_t instruction_end)
{
	uint32_t instr, val;
	uint32_t ch_mem = -1, ch_reg = -1;
	instruction_t in;
	regvals_t regs;
	interactive_result_t cmd;
	char line[256];
	while (c->pc <= instruction_end)
	{
		zero_out_instruction(&in);
		mips_reset_signals(&(c->sigs));
		cmd = handle_interactive(c, line, sizeof(line), &in);
		switch (cmd)
		{
		case RUN_ASM:
			goto decode_done;
		case NEXT_INPUT:
			continue;
		case NEXT_INSTRUCTION:
			break;
		case QUIT:
			return 0;
		}

		instr = mips_fetch(c, c->pc);
		decode(&in, instr);

	decode_done:
		c->sigs.sign_extend = is_unsigned(&in);
		val = mips_execute(c, &in, &regs);
		if (c->sigs.error != OK)
		{
			fprintf(stderr, "Error at 0x%x: %s\n", c->pc, mips_error_message(c->sigs.error));
			continue;
		}
		mips_update_pc(c, &in, val);
		val = mips_memory(c, &in, val, &ch_mem);
		mips_reg_write(c, &in, val, &ch_reg);
		if (c->sigs.error != OK)
		{
			fprintf(stderr, "Error at 0x%x: %s\n", c->pc, mips_error_message(c->sigs.error));
			continue;
		}
	}
	return 0;
}
#endif

uint32_t mips_fetch(computer_t* c, uint32_t addr)
{
	return c->memory[(addr - ADDRESS_BEGIN) / 4];
}

static uint32_t exec_r(computer_t*, instruction_t*, regvals_t*, uint32_t);
static uint32_t exec_i(computer_t*, instruction_t*, regvals_t*, uint32_t);
static uint32_t exec_j(computer_t*, instruction_t*, regvals_t*, uint32_t);

uint32_t mips_execute(computer_t* c, instruction_t* d, regvals_t* r)
{
	uint32_t signExtImm = 0;
	// if (c->sigs.sign_extend && d->imm >> 15)
	// {
	// 	signExtImm = d->imm | 0xffff0000;
	// }
	// else
	// {
	// 	signExtImm = d->imm;
	// }
	if (d->imm >> 15)
	{
		signExtImm = d->imm | 0xffff0000;
	}
	else
	{
		signExtImm = d->imm;
	}

	r->rs = c->registers[d->reg[rs]];
    r->rt = c->registers[d->reg[rt]];
    r->rd = c->registers[d->reg[rd]];

	switch (d->op.fmt)
	{
	case R:
		return exec_r(c, d, r, signExtImm);
	case I:
		return exec_i(c, d, r, signExtImm);
	case J:
		return exec_j(c, d, r, signExtImm);
	default:
		c->sigs.error = ERR_BAD_FORMAT;
		return -1;
	}
	return -1;
}

int mips_memory(computer_t* c, instruction_t* in, uint32_t val, uint32_t* changed_mem)
{
	if (c->sigs.mem_write)
	{
		*changed_mem = val;
		// Mem[R[rs]+SignExtImm] = R[rt]
		mips_write_memory(c, val, c->registers[in->reg[rt]] & c->sigs.mem_mask);
	}
	else if (c->sigs.mem_read)
	{
		*changed_mem = val;
		return mips_fetch(c, val) & c->sigs.mem_mask;
	}
	else
	{
		*changed_mem = -1;
	}
	return val;
}

void mips_update_pc(computer_t* c, instruction_t* in, uint32_t val)
{
	if (c->sigs.branch)
	{
		c->pc = val;
	}
	else if (c->sigs.jump)
	{
		c->pc = val;
	}
	else
	{
		c->pc += 4;
	}
	c->registers[REG_PC] = c->pc;
}

void mips_reg_write(computer_t* c, instruction_t* in, uint32_t val, uint32_t *changed_reg)
{
	int i;
	if (!(c->sigs.reg_write) && !(c->sigs.mem_to_reg))
	{
		*changed_reg = -1;
		return;
	}
	else if (c->sigs.mem_to_reg && c->sigs.mem_read)
	{
		i = in->reg[rt];
		*changed_reg = i;
		c->registers[i] = val;
		return;
	}
	else if (c->sigs.jump && c->sigs.reg_write)
	{
		*changed_reg = REG_RA;
		return;
	}
	else
	{
		if (c->sigs.reg_dst)
		{
			i = in->reg[rd];
		}
		else
		{
			i = in->reg[rt];
		}
	}

	if (
		i > (int)(sizeof(c->registers)/sizeof(c->registers[0])) ||
		i == REG_ZERO
	) {
		c->sigs.error = ERR_BAD_REG;
		return;
	}

	*changed_reg = i;
	c->registers[i] = val;
}

void mips_write_memory(computer_t*c, uint32_t address, uint32_t value)
{
	c->memory[(address - ADDRESS_BEGIN) / 4] = value;
}

void mips_computer_reset(computer_t* c)
{
	mips_reset_signals(&c->sigs);
	bzero(c->memory, sizeof(c->memory));
	bzero(c->registers, sizeof(c->registers));
	c->pc = ADDRESS_BEGIN;
	c->registers[REG_SP] = ADDRESS_BEGIN + (MAXNUMINSTRS+MAXNUMDATA)*4;
	c->instruction_end = c->pc - 4;
}

void mips_reset_signals(struct signals* s)
{
	s->alu_op      = 0;
	s->alu_src     = 0;
	s->branch      = 0;
	s->jump        = 0;
	s->mem_read    = 0;
	s->mem_to_reg  = 0;
	s->mem_write   = 0;
	s->mem_mask    = 0xffffffff;
	s->reg_dst     = 0;
	s->reg_write   = 0;
	s->sign_extend = 0;
	s->syscall     = 0;
	s->error       = OK;
}

static void mult(uint32_t, uint32_t, uint32_t*, uint32_t*);

static uint32_t exec_r(computer_t* c, instruction_t* in, regvals_t* r, uint32_t signExtImm)
{
	funct_t funct = (funct_t)in->op.id;
	c->sigs.alu_src = 0;
	c->sigs.reg_write = 1;
	c->sigs.reg_dst = REG_DST_RD;
	switch (funct)
	{
	case OP_ADD:
	case OP_ADDU:
		c->sigs.alu_op = ALU_ADD;
		r->rd = r->rs + r->rt;
		break;
	case OP_SUB:
	case OP_SUBU:
		c->sigs.alu_op = ALU_SUB;
		r->rd = r->rs - r->rt;
		break;
	case OP_AND:
		c->sigs.alu_op = ALU_AND;
		r->rd = r->rs & r->rt;
		break;
	case OP_DIV:
	case OP_DIVU:
		c->registers[REG_LO] = r->rs / r->rt;
		c->registers[REG_HI] = r->rs % r->rt;
		break;
	case OP_MULT:
	case OP_MULTU:
		r->rd = 0;
		mult(
			r->rs,
			r->rt,
			c->registers+REG_HI,
			c->registers + REG_LO
		);
		break;
	case OP_OR:
		c->sigs.alu_op = ALU_OR;
		r->rd = r->rs | r->rt;
		break;
	case OP_NOR:
		c->sigs.alu_op = ALU_NOR;
		r->rd = ~(r->rs | r->rt);
		break;
	case OP_XOR:
		r->rd = r->rs ^ r->rt;
		break;
	case OP_MFHI:
		r->rd = c->registers[REG_HI];
		break;
	case OP_MFLO:
		r->rd = c->registers[REG_LO];
		break;
	case OP_MTHI:
		c->sigs.reg_write = 0;
		r->rd = 0;
		c->registers[REG_HI] = r->rs;
		break;
	case OP_MTLO:
		c->sigs.reg_write = 0;
		r->rd = 0;
		c->registers[REG_LO] = r->rs;
		break;
	case OP_SLL:
		r->rd = r->rt << in->shamt;
		break;
	case OP_SLLV:
		r->rd = r->rt << r->rs;
		break;
	case OP_SRA:
		r->rd = r->rt >> in->shamt;
		break;
	case OP_SRAV:
		r->rd = r->rt >> r->rs;
		break;
	case OP_SRL:
		r->rd = r->rt >> in->shamt;
		break;
	case OP_SRLV:
		r->rd = r->rt >> r->rs;
		break;
	case OP_SLT:
		r->rd = r->rs < r->rt;
		break;
	case OP_SLTU:
		r->rd = r-rs < r-rt;
		break;
	case OP_JALR:
		c->sigs.jump = 1;
		c->sigs.reg_write = 1;
		c->registers[REG_RA] = c->pc + 4;
		return r->rs;
	case OP_JR:
		c->sigs.jump = 1;
		c->sigs.reg_write = 0;
		return r->rs;
	// TODO return an error or implement syscalls
	case OP_SYSCALL:
		c->sigs.syscall = c->registers[REG_V0];
		c->sigs.reg_write = 0;
		break;
	}
	return r->rd;
}

static uint32_t exec_i(computer_t* c, instruction_t* in, regvals_t* r, uint32_t signExtImm)
{
	opcode_t op = (opcode_t)in->op.id;
	c->sigs.alu_src = 1;
	c->sigs.reg_dst = REG_DST_RT;
	switch (op)
	{
	case OP_BNE:
		if (r->rs != r->rt)
		{
			c->sigs.mem_read = 0;
			c->sigs.jump = 0;
			c->sigs.branch = 1;
			r->rt = c->pc + 4 + (in->imm << 2);
			break;
		}
		break;
	case OP_BEQ:
		if (r->rs == r->rt)
		{
			c->sigs.mem_read = 0;
			c->sigs.jump = 0;
			c->sigs.branch = 1;
			r->rt = c->pc + 4 + (in->imm << 2);
			break;
		}
		break;
	case OP_ADDI:
		c->sigs.reg_write = 1;
		r->rt = r->rs + signExtImm;
		break;
	case OP_ADDIU:
		c->sigs.reg_write = 1;
		r->rt = r->rs + signExtImm;
		break;
	case OP_ANDI:
		c->sigs.reg_write = 1;
		r->rt = r->rs & in->imm;
		break;
	case OP_ORI:
		c->sigs.reg_write = 1;
		c->sigs.alu_src = 1;
		r->rt = r->rs | in->imm;
		break;
	case OP_XORI:
		c->sigs.reg_write = 1;
		r->rt = r->rs ^ in->imm;
		break;
	case OP_LB:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0x000000ff;
		break;
	case OP_LBU:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0x000000ff;
		return r->rs + signExtImm;
	case OP_LH:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0x0000ffff;
		return r->rs + signExtImm;
	case OP_LHU:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0x0000ffff;
		return r->rs + signExtImm;
	case OP_LHI:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0x0000ffff;
		return r->rs + signExtImm;
	case OP_LW:
		c->sigs.reg_write = 1;
		c->sigs.mem_to_reg = 1;
		c->sigs.mem_read = 1;
		c->sigs.mem_mask = 0xffffffff;
		return r->rs + signExtImm;
	case OP_LUI:
		c->sigs.reg_write = 1;
		r->rt = signExtImm << 16;
		break;
	case OP_SB:
		c->sigs.mem_write = 1;
		c->sigs.mem_mask = 0x000000ff;
		return r->rs + signExtImm;
	case OP_SH:
		c->sigs.mem_write = 1;
		c->sigs.mem_mask = 0x0000ffff;
		return r->rs + signExtImm;
	case OP_SW:
		c->sigs.mem_write = 1;
		c->sigs.mem_mask = 0xffffffff;
		return r->rs + signExtImm;
	case OP_BLEZ:
		break;
	case OP_BGTZ:
		break;
	case OP_LLO:   break;
	case OP_SLTI:  break;
	case OP_SLTIU: break;
	case OP_NOP:
		break;
	case OP_J: case OP_JAL: case OP_TRAP:
		fprintf(stderr, "Internal Exec Error: bad opcode %d\n", op);
		exit(1);
		break;
	}
	return r->rt;
}

static uint32_t exec_j(computer_t* c, instruction_t* in, regvals_t* r, uint32_t signExtImm)
{
	opcode_t op = (opcode_t)in->op.id;

	switch (op)
	{
	case OP_J:
		c->sigs.jump = 1;
		c->sigs.reg_write = 0; // DO NOT WRITE NEW PC
		return in->imm << 2;
	case OP_JAL:
		c->sigs.jump = 1;
		c->sigs.reg_write = 1;
		c->registers[REG_RA] = c->pc + 4;
		return in->imm << 2;
	case OP_TRAP:
		break;
	default:
		break;
	}
	return 0;
}

/* One pipeline cycle */
static error_t sim_fetch_dec_exec(computer_t* c, instruction_t* in, regvals_t* regs)
{
	uint32_t instr, val;
	uint32_t changed_mem = -1, changed_reg = -1;
	zero_out_instruction(in);
	mips_reset_signals(&(c->sigs));

	instr = mips_fetch(c, c->pc);
	decode(in, instr);
	c->sigs.sign_extend = is_unsigned(in);

	val = mips_execute(c, in, regs);
	if (c->sigs.error != OK)
	{
		return c->sigs.error;
	}

	mips_update_pc(c, in, val);
	val = mips_memory(c, in, val, &changed_mem);
	mips_reg_write(c, in, val, &changed_reg);
	return c->sigs.error;
}

static unsigned int endian_swap(unsigned int i) {
    return (i>>24) | (i>>8&0x0000ff00) | (i<<8&0x00ff0000) | (i<<24);
}

static uint32_t get_hi(uint32_t x) { return x >> 16; }
static uint32_t get_lo(uint32_t x) { return ((1 << 16) - 1) & x; }

/**
 * https://en.wikipedia.org/wiki/Multiplication_algorithm#Long_multiplication
 */
static void mult(uint32_t a, uint32_t b, uint32_t* hi, uint32_t* lo)
{
	uint32_t s0, s1, s2, s3;
	uint32_t hia = get_hi(a), loa = get_lo(a);
	uint32_t hib = get_hi(b), lob = get_lo(b);

	uint32_t x = loa * lob;
	s0 = get_lo(x);

	x = hia * lob + get_hi(x);
	s1 = get_lo(x);
	s2 = get_hi(x);

	x = s1 + loa * hib;
	s1 = get_lo(x);

	x = s2 + hia * hib + get_hi(x);
	s2 = get_lo(x);
	s3 = get_hi(x);

	*hi = s3 << 16 | s2;
	*lo = s1 << 16 | s0;
}
