#include "utest/utest.h"
#include <stdio.h>

#include "runtime.c"
#include "data.h"

#if _POSIX_C_SOURCE < 200809L
#error "_POSIX_C_SOURCE >= 200809L required for fmemopen function"
#endif

uint32_t fib(uint32_t n)
{
	if (n == 0)
		return 0;
	if (n == 1)
		return 1;
	return fib(n - 1) + fib(n - 2);
}

TEST(fib)
{
	// uint32_t val, instr, changedMem, changedReg;
	computer_t c;
	regvals_t r;
	instruction_t in;
	// Load fib program into memory
	FILE* filein = fmemopen(tests_s_fib_bin, tests_s_fib_bin_len, "r");
	mips_computer_init(&c, filein);
	fclose(filein);

	// test fib for all 1 <= n <= 25
	for (uint32_t n = 1; n <= 25; n++)
	{
		mips_reset_signals(&c.sigs);
		zero_out_instruction(&in);
		bzero(c.registers, sizeof(c.registers));
		// stack
		c.registers[29] = ADDRESS_BEGIN + (MAXNUMINSTRS+MAXNUMDATA) * 4;
		// program counter
		c.pc = ADDRESS_BEGIN;

		// Set fib argument
		c.registers[REG_A0] = n;

		// Run the mips implementation of fib
		while (c.pc <= c.instruction_end)
		{
			/* Fetch -> Decode -> Execute */
			sim_fetch_dec_exec(&c, &in, &r);
			if (c.sigs.syscall && (c.registers[REG_V0] == 10 || c.registers[REG_V0] == 17))
				break;
		}

		// The test assembly code save the fib result in s0
		uint32_t result = c.registers[REG_S0];
		// Test against C implementation of fib
		uint32_t exp = fib(n);
		assert_eq(result, exp);
	}
}

TEST(branch_program)
{
	computer_t c;
	regvals_t r;
	instruction_t in;
	FILE* filein = fmemopen(tests_s_branch_bin, tests_s_branch_bin_len, "r");
	c.pc = ADDRESS_BEGIN;
	mips_computer_init(&c, filein);
	fclose(filein);

	while (c.pc <= c.instruction_end)
	{
		sim_fetch_dec_exec(&c, &in, &r);
	}
	eq(c.pc, (uint32_t)0x00400030);
	eq(c.instruction_end+4, (uint32_t)0x00400030);
	eq(c.registers[REG_T1], (uint32_t)0);
}

struct arithlog_testcase {
	const char* code;
	uint32_t a0, a1;
	uint32_t result;
};

TEST(multiplication)
{
	static struct arithlog_testcase cases[] = {
		{"mult  $a0, $a1", 5000000, 3000000, 0},
		{"mult  $a0, $a1", 1, 2, 0},
		{"mult  $a0, $a1", 0xffffffff, 0xffffffff, 0},
		{"multu $a0, $a1", 0xffffffff, 0xffffffff, 0},
		{"mult  $a0, $a1", 0xfafafafa, 0xefefefef, 0},
		{"mult  $a0, $a1", -9, 3, 0},
		{"multu $a0, $a1", -9, 3, 0},
	};
	static const int tests = sizeof(cases)/sizeof(cases[0]);
	error_t err;
	instruction_t in;
	uint32_t result;
	regvals_t  r;
	computer_t c;
	int i;
	struct arithlog_testcase t;
	for (i = 0; i < tests; i++)
	{
		t = cases[i];
		bzero(c.memory, sizeof(c.memory));
		bzero(c.registers, sizeof(c.registers));
		zero_out_instruction(&in);
		mips_reset_signals(&c.sigs);
		err = assemble(&in, (char*)t.code);
		if (err != OK)
		{
			FAILF("failed to assemble \"%s\": %s\n", t.code, mips_error_message(err));
			continue;
		}
		c.registers[REG_A0] = t.a0;
		c.registers[REG_A1] = t.a1;
		result = mips_execute(&c, &in, &r);
		assert_eq(t.result, result);
		uint64_t a0 = t.a0, a1 = t.a1, hi = c.registers[REG_HI], lo = c.registers[REG_LO];
		hi = hi << 32;
		assert_eq(a0*a1, hi|lo);
	}
}

TEST(arithmetic)
{
	static struct arithlog_testcase cases[] = {
		// addition
		{"addu $v0, $a0, $a1", 0xffff0000, 1, 0xffff0000+1},
		{"add  $v0, $a0, $a1", 0xffff0000, 1, -65536 + 1},
		{"add  $v0, $a0, $a1", 1, -1, 0},
		{"addu $v0, $a0, $a1", 1, -1, 0},
		// subtraction
		{"sub  $v0, $a0, $a1", 10, 3, 7},
		{"subu $v0, $a0, $a1", 10, 3, 7},
		// logic
		{"and  $v0, $a0, $a1", 3, 9, 3&9},
		{"andi $v0, $a0, 9",   3, 0, 3&9},
		{"or   $v0, $a0, $a1", 3, 9, 3|9},
		{"xor  $v0, $a0, $a1", 3, 9, 3^9},
		{"ori  $v0, $a0, 9",   3, 0, 3|9},
		// shift
		{"sll  $v0, $a0, 9",   1, 0, 1 << 9},
		{"sllv $v0, $a0, $a1", 1, 9, 1 << 9},
		{"srl  $v0, $a0, 9",   1, 0, 1 >> 9},
		{"srlv $v0, $a0, $a1", 1, 9, 1 >> 9},
		// jump
		{"j 0x400008", 0, 0, 0x400008 << 2},
		// set less than
		{"slt $v0, $a0, $a1", 0, 0,  0},
		{"slt $v0, $a0, $a1", 0, 10, 1},
		{"slt $v0, $a0, $a1", 10, 0, 0},
	};
	static const int tests = sizeof(cases) / sizeof(cases[0]);

	struct arithlog_testcase t;
	error_t err;
	uint32_t result;
	computer_t c;
	regvals_t r;
	instruction_t in;
	int i;
	for (i = 0; i < tests; i++)
	{
		t = cases[i];
		bzero(c.memory, sizeof(c.memory));
		bzero(c.registers, sizeof(c.registers));
		zero_out_instruction(&in);
		mips_reset_signals(&c.sigs);
		err = assemble(&in, (char*)t.code);
		if (err != OK)
		{
			FAILF("failed to assemble \"%s\": %s\n", t.code, mips_error_message(err));
			continue;
		}
		c.registers[REG_A0] = t.a0;
		c.registers[REG_A1] = t.a1;
		c.sigs.sign_extend = is_unsigned(&in);
		result = mips_execute(&c, &in, &r);
		if (result != t.result)
		{
			FAILF("\"%s\" exec: want 0x%x, got 0x%x\n",
				t.code, t.result, result);
			continue;
		}
	}
}

TEST(branch_jump)
{
	static struct arithlog_testcase cases[] = {
		{"j 0x004ffff4", 0, 0, 0x004ffff4 << 2},
		{"j 0", 0, 0, 0 << 2},
		{"bne $a0, $a1, 10", 10, 4, ADDRESS_BEGIN + 4 + (10 << 2)},
	};
	static const int tests = sizeof(cases) / sizeof(cases[0]);
	struct arithlog_testcase t;
	error_t err;
	uint32_t result;
	computer_t c;
	regvals_t r;
	instruction_t in;
	int i;
	for (i = 0; i < tests; i++)
	{
		t = cases[i];
		bzero(c.memory, sizeof(c.memory));
		bzero(c.registers, sizeof(c.registers));
		c.pc = ADDRESS_BEGIN;
		zero_out_instruction(&in);
		mips_reset_signals(&c.sigs);
		err = assemble(&in, (char*)t.code);
		if (err != OK)
		{
			FAILF("failed to assemble \"%s\": %s\n", t.code, mips_error_message(err));
			continue;
		}
		c.registers[REG_A0] = t.a0;
		c.registers[REG_A1] = t.a1;
		c.sigs.sign_extend = is_unsigned(&in);
		result = mips_execute(&c, &in, &r);
		assert_eq(result, t.result);
		if (result != t.result)
		{
			FAILF("wrong result: got 0x%x, want 0x%x\n", result, t.result);
			continue;
		}
		mips_update_pc(&c, &in, result);
	}
}

struct loadstore_testcase {
	const char* code;
	uint32_t a1, s1;
	uint32_t address;
	uint32_t value; // value stored in memory at the address
};

TEST(load_store)
{
	error_t err;
	uint32_t result, memvalue, change_reg;
	instruction_t in;
	computer_t c;
	regvals_t r;
	int i;
	static struct loadstore_testcase cases[] = {
		{
			.code = "lw $a1, 10($s1)",
			.a1 = 1,
			.s1 = ADDRESS_BEGIN + 17,
			.address = ADDRESS_BEGIN + 17 + 10,
			.value = 69,
		},
		{
			.code = "sw $a1, 5($s1)",
			.a1 = 4, .s1 = ADDRESS_BEGIN + 9,
			.address = ADDRESS_BEGIN + 9 + 5,
			.value = 2,
		}
	};
	int n = sizeof(cases) / sizeof(struct loadstore_testcase);
	for (i = 0; i < n; i++)
	{
		struct loadstore_testcase t = cases[i];
		zero_out_instruction(&in);
		mips_reset_signals(&c.sigs);
		bzero(c.memory, sizeof(c.memory));
		bzero(c.registers, sizeof(c.registers));
		c.registers[REG_A1] = t.a1;
		c.registers[REG_S1] = t.s1;
		mips_write_memory(&c, t.address, t.value); // MEM[t.address] = t.value
		err = assemble(&in, (char*)t.code);
		if (err != OK)
		{
			FAILF("failed to assemble: %s\n", mips_error_message(err));
			continue;
		}
		c.sigs.sign_extend = is_unsigned(&in);
		result = mips_execute(&c, &in, &r);
		if (result != t.address)
		{
			FAILF("wrong result from exec: got %d, want: %d\n", result, t.address);
			continue;
		}
		mips_memory(&c, &in, result, &memvalue);
		mips_reg_write(&c, &in, result, &change_reg);
		if (t.code[0] == 'l')
		{
			uint32_t v = mips_fetch(&c, result);
			if (v != t.value)
				FAILF("lw failed: got %d, want %d\n", v, t.value);
		}
		else if (t.code[0] == 's')
		{
			uint32_t v = mips_fetch(&c, result);
			if (v != t.a1)
				FAILF("sw failed: got %d, want %d\n", v, t.a1);
		}
	}
}

TEST(computer_init_and_fetch)
{
	computer_t c;
	FILE* filein = fmemopen(tests_s_sample_bin, tests_s_sample_bin_len, "r");
	int i, n;
	uint32_t instr, bin;

	mips_computer_init(&c, filein);
	fclose(filein);

	// number of instructions
	n = tests_s_sample_bin_len / 4;

	// Make sure that the first 10 slots of memory have instructions.
	for (i = 0; i < n; i++)
		if (c.memory[i] == 0)
			FAILF("memory address %d should have an instruction\n", i);

	instr = mips_fetch(&c, ADDRESS_BEGIN);
	eq(instr, (uint32_t)c.memory[0]);

	c.pc = ADDRESS_BEGIN;
	for (i = 0; i < n; i++)
	{
		instruction_t in;
		zero_out_instruction(&in);
		mips_reset_signals(&c.sigs);
		instr = mips_fetch(&c, c.pc);
		decode(&in, instr);
		if (
			in.op.fmt == J &&
			(in.op.id == OP_JAL || in.op.id == OP_BEQ || in.op.id == OP_J)
		)
			// Skip if the instruction has a label then skip it
			continue;

		bin = to_binary(&in);
		if (instr != bin)
		{
			char want[64], got[64];
			write_bin(want, 32, instr);
			write_bin(got, 32, bin);
			FAILF("binary conversion not equal:\n  want %s,\n  got  %s\n", want, got);
		}
		c.pc += 4;
	}
}

TEST(execute)
{
	uint32_t result;
	instruction_t in;
	computer_t c;
	regvals_t regs = {0, 0, 0};
	FILE* filein = fmemopen(tests_s_sample_bin, tests_s_sample_bin_len, "r");
	mips_computer_init(&c, filein);
	fclose(filein);
	c.pc = ADDRESS_BEGIN;

	// Begin testing execute

	zero_out_instruction(&in);
	decode(&in, mips_fetch(&c, c.pc));
	mips_reset_signals(&c.sigs);
	result = mips_execute(&c, &in, &regs);
	eq(result, (uint32_t)3);
	eq(result, regs.rt);
	c.registers[in.reg[rt]] = result;
	c.pc += 4;

	zero_out_instruction(&in);
	decode(&in, mips_fetch(&c, c.pc));
	mips_reset_signals(&c.sigs);
	result = mips_execute(&c, &in, &regs);
	eq(result, (uint32_t)2);
	eq(result, regs.rt);
	c.registers[in.reg[rt]] = result;
	c.pc += 4;

	zero_out_instruction(&in);
	assemble(&in, "add $v0, $a0, $a1");
	mips_reset_signals(&c.sigs);
	result = mips_execute(&c, &in, &regs);
	eq(regs.rs, (uint32_t)3);
	eq(regs.rt, (uint32_t)2);
	eq(result, regs.rd);
	eq(result, (uint32_t)5);
}
