#include "utest/utest.h"

// we need access to private variables so include the .c file
#ifndef _MIPS_C
# define _MIPS_C
# include "mips.c"
#endif

#define assert_instruction_eq(A, B)          \
	do {                                     \
		assert_eq((A).op.id, (B).op.id);     \
		assert_eq((A).op.fmt, (B).op.fmt);   \
		assert_eq((A).op.syn, (B).op.syn);   \
		assert_eq((A).reg[rs], (B).reg[rs]); \
		assert_eq((A).reg[rt], (B).reg[rt]); \
		assert_eq((A).reg[rd], (B).reg[rd]); \
		assert_eq((A).shamt,   (B).shamt);   \
		assert_eq((A).imm,     (B).imm);     \
		assert_eq((A).funct,   (B).funct);   \
	} while(0)

struct testcase {
	char* as;
	instruction_t exp;
	uint32_t bin;
};

size_t n_testcases = 0;

static struct testcase* get_testcases()
{
	struct testcase cases[] = {
		{
			"add\t$s0, $t0, $t1",
			{
				opcodes[0], .reg={0b01000, 0b01001, 0b10000},
				.shamt=0, .imm=0, .funct=0b100000,
			},
			0b00000001000010011000000000100000,
		},
		{
			"addiu\t$s0, $t0, 66",
			{
				.op=find_op("addiu"), .reg={0b01000, 0b10000, 0},
				.shamt=0, .imm=0b0000000001000010, .funct=0, "",
			},
			0b00100101000100000000000001000010,
		},
		{
			"subu\t$v0, $s7, $t1",
			{
				.op=find_op("subu"), .reg={0b10111, 0b01001, 0b00010},
				.shamt=0, .imm=0, .funct=0b100011,
			},
			0b00000010111010010001000000100011,
		},
		{
			"sll\t$s6, $s7, 8",
			{
				.op=find_op("sll"), .reg={0b00000, 0b10111, 0b10110},
				.shamt=0b01000, .imm=0, .funct=0b000000,
			},
			0b00000000000101111011001000000000,
		},
		{
			"srl\t$s6, $s7, 8",
			{
				.op=find_op("srl"), .reg={0b00000, 0b10111, 0b10110},
				.shamt=0b01000, .imm=0, .funct=0b000010,
			},
			0b00000000000101111011001000000010,
		},
		{
			"and\t$t0, $s0, $s1",
			{
				.op=find_op("and"), .reg={0b10000, 0b10001, 0b01000},
				.shamt=0, .imm=0, .funct=0b100100,
			},
			0b00000010000100010100000000100100,
		},
		{
			"andi\t$t0, $s1, 0x4d",
			{
				.op=find_op("andi"), .reg={0b10001, 0b01000, 0},
				.shamt=0, .imm=0b0000000001001101, .funct=0,
			},
			0b00110010001010000000000001001101,
		},
		{
			"or\t$t0, $s0, $s1",
			{
				.op=find_op("or"), .reg={0b10000, 0b10001, 0b01000},
				.shamt=0, .imm=0, .funct=0b100101,
			},
			0b00000010000100010100000000100101
		},
		{
			"ori\t$t0, $s1, 0x4d",
			{
				.op=find_op("ori"), .reg={0b10001, 0b01000, 0},
				.shamt=0, .imm=0b0000000001001101, .funct=0,
			},
			0b00110110001010000000000001001101,
		},
		{
			"j\tra",
			{
				.op=opcodes[12], .reg={0,0,0},
				.shamt=0, .imm=0b0, .funct=0,
			},
			0b00001000000000000000000000000000,
		},
		{
			"lw\t$t0, 16($t1)",
			{
				.op=find_op("lw"), .reg={0b01001, 0b01000, 0},
				.shamt=0, .imm=0b0000000000010000, .funct=0, "",
			},
			0b10001101001010000000000000010000,
		},
		{
			"div\t$t0, $a0",
			{
				.op=find_op("div"), .reg={0b01000, 0b00100, 0},
				.shamt=0, .imm=0, .funct=0b011010, "",
			},
			0b00000001000001000000000000011010,
		},
		{
			"jr\t$t3",
			{
				.op=find_op("jr"), .reg={0b01011, 0, 0},
				.shamt=0, .imm=0, .funct= 0b001000,
			},
			0b00000001011000000000000000001000,
		},
		{
			"mfhi\t$t4",
			{
				.op=find_op("mfhi"), .reg={0, 0, 0b001100},
				.shamt=0, .imm=0, .funct=0b010000,
			},
			0b00000000000000000110000000010000,
		},
		{
			"mthi\t$v1",
			{
				.op=find_op("mthi"), .reg={0b00011, 0, 0},
				.shamt=0, .imm=0, .funct=0b010001,
			},
			0b00000000011000000000000000010001,
		},
		{
			"sltu\t$zero, $zero, $zero",
			{
				.op=find_op("sltu"), .reg={0,0,0},
				.shamt=0,.imm=0,.funct=0b101011,
			},
			0b00000000000000000000000000101011,
		},
		{
			"addiu\t$a0, $zero, -9",
			{
				.op=find_op("addiu"), .reg={0x00, 0x04, 0x00},
				.shamt=0, .imm=0xfff7, .funct=0,
			},
			0x2404fff7,
		},
		{
			"sllv\t$v1, $a0, $a1",
			{
				.op=find_op("sllv"), .reg={0b00101, 0b00100, 0b00011},
				.shamt=0b00000, .imm=0, .funct=0b000100,
			},
			0b00000000101001000001100000000100,
		},
		{
			"slt\t$v1, $a0, $a1",
			{
				.op=find_op("slt"), .reg={0b00100, 0b00101, 0b00011},
				.shamt=0b00000, .imm=0, .funct=0b101010,
			},
			0b00000000100001010001100000101010,
		},
		{
			"sll\t$v1, $a0, 18",
			{
				.op=find_op("sll"), .reg={0b00000, 0b00100, 0b00011},
				.shamt=0b10010, .imm=0, .funct=000000,
			},
			0b00000000000001000001110010000000,
		},
		{
			"lw\t$t0, 16($t1)",
			{
				.op=find_op("lw"), .reg={0b01001, 0b01000},
				.imm=0b0000000000010000, .shamt=0, .funct=0,
			},
			0b10001101001010000000000000010000,
		},
		{
			"div\t$t0, $a0",
			{
				.op=find_op("div"), .reg={0b01000, 0b00100, 0},
				.shamt=0, .imm=0, .funct=0b011010,
			},
			0b00000001000001000000000000011010,
		},
		{
			"lhi\t$t8, 0xf",
			{
				.op=find_op("lhi"), .reg={0, 24, 0},
				.shamt=0, .imm=15, .funct=0,
			},
			0b01100100000110000000000000001111,
		},
		{
			"llo\t$t8, 0xf",
			{
				.op=find_op("llo"), .reg={0, 24, 0},
				.imm=15, .shamt=0, .funct=0,
			},
			0b01100000000110000000000000001111,
		},
		{
			"lui\t$t3, 0x24",
			{
				.op=find_op("lui"), .reg={0x00, 0x0b, 0x00},
				.imm=0x24, .shamt=0, .funct=0,
			},
			0b00111100000010110000000000100100,
		},
		{
			"addu\t$t4, $t3, $t2",
			// "addu\t$12, $11, $10",
			{
				.op=find_op("addu"), .reg={0b01011, 0b01010, 0b01100},
				.shamt=0, .imm=0, .funct=0b100001,
			},
			0b00000001011010100110000000100001,
		},
		{
			"bne\t$v1, $a0, 0x4",
			{
				.op=find_op("bne"), .reg={0b00011, 0b00100},
				.shamt=0, .imm = 0b0000000000000100, .funct=0,
			},
			0b00010100011001000000000000000100,
		},
		{
			"jr\t$ra",
			{
				.op=find_op("jr"), .reg={0b11111},
				.shamt = 0, .imm = 0, .funct = 0b001000,
			},
			0b00000011111000000000000000001000,
		},
		{
			"j\t0x10",
			{
				.op=find_op("j"), .reg={0,0,0},
				.shamt = 0, .imm = 0x10, .funct = 0,
			},
			0b00001000000000000000000000010000,
		}
	};
	n_testcases = (size_t)(sizeof(cases) / sizeof(cases[0]));
	struct testcase* testcases = malloc(n_testcases * sizeof(struct testcase));
	for (size_t i = 0; i < n_testcases; i++)
		*(testcases+i) = cases[i];
	return testcases;
}

TEST(assemble_instruction)
{
	struct testcase* cases = get_testcases();
	instruction_t in;
	size_t i;
	error_t err;
	for (i = 0; i < n_testcases; i++)
	{
		struct testcase t = cases[i];
		zero_out_instruction(&in);
		err = assemble(&in, t.as);
		if (err != OK)
		{
			FAILF("assemble(\"%s\") returned bad error code: %d \"%s\"\n", t.as, err, mips_error_message(err), t.as);
			continue;
		}
		if (t.exp.imm != in.imm)
		{
			FAILF("\"%s\" wrong imm: got %x, want %x\n", t.as, in.imm, t.exp.imm);
			continue;
		}
		assert_instruction_eq(in, t.exp);
		assert_eq(to_binary(&in), t.bin);
	}
	free(cases);
}

TEST(decode_instruction)
{
	struct testcase* cases = get_testcases();
	size_t i;
	instruction_t in;
	for (i = 0; i < n_testcases; i++)
	{
		struct testcase t = cases[i];
		zero_out_instruction(&in);
		decode(&in, t.bin);
		assert_instruction_eq(in, t.exp);
		if (in.imm != t.exp.imm)
		{
			FAILF("bad imm: want 0x%x, got 0x%x\n", t.exp.imm, in.imm);
		}
	}
	free(cases);
}

TEST(instruction_to_assembly, .ignore = 1)
{
	struct testcase* cases = get_testcases();
	size_t i;
	error_t err;
	instruction_t in;
	char buf[128];
	for (i = 0; i < n_testcases; i++)
	{
		struct testcase t = cases[i];
		zero_out_instruction(&in);
		// Skip if it has a label
		if (
			t.exp.op.syn == BRANCH  ||
			t.exp.op.syn == BRANCHZ ||
			t.exp.op.syn == JUMP
		)
		{
			continue;
		}
		memset(buf, '\0', sizeof(buf));
		err = to_assembly(&t.exp, buf);
		if (err != OK)
		{
			FAILF("could not convert to assembly: \"%s\"\n", mips_error_message(err));
			continue;
		}
		if (strcmp(buf, t.as) != 0)
		{
			FAILF("wrong instruction assembly: got \"%s\", want \"%s\"\n", t.as, buf);
			continue;
		}
	}
	free(cases);
}

TEST(decode_op)
{
	struct testcase* cases = get_testcases();
	size_t i;
	for (i = 0; i < n_testcases; i++)
	{
		struct testcase t = cases[i];
		struct op o = decode_op(t.bin);
		eq(o.fmt, t.exp.op.fmt);
		eq(o.id, t.exp.op.id);
		eq(o.syn, t.exp.op.syn);
		eq(o.name, t.exp.op.name);
	}
	free(cases);
}