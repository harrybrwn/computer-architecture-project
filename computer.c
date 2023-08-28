#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

#include "mips.h"

// #define SILENT 1

#ifdef __STRICT_ANSI__
void bzero(void*, size_t);
#endif

unsigned int endianSwap(unsigned int);

void         PrintInfo(computer_t*, program_state_t*, int changedReg, int changedMem);
unsigned int Fetch(Computer*, int);
void         Decode(computer_t*, unsigned int, instruction_t*, RegVals*);
int          Execute(Computer*, instruction_t*, RegVals*);
int          Mem(Computer*, instruction_t*, int, int *);
void         RegWrite(Computer*, instruction_t*, int, int *);
void         UpdatePC(Computer*, instruction_t*, int);
void         PrintInstruction (instruction_t*, uint32_t);

/*Globally accessible Computer variable*/
/* Globals are bad... removing these immediately */
//Computer mips;
//RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer(Computer* c, FILE* filein) {
    int k;
    unsigned int instr;

    mips_computer_reset(c);

    /* Initialize registers and memory */
    for (k=0; k<32; k++) {
        c->registers[k] = 0;
    }

    /* stack pointer - Initialize to highest address of data segment */
    c->registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        c->memory[k] = 0;
    }

    if (filein == NULL)
    {
        c->instruction_end = ADDRESS_BEGIN + (MAXNUMINSTRS)*4;
        return;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
        /* swap to big endian, convert to host byte order. Ignore this. */
        c->memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k > MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }
    c->instruction_end = ADDRESS_BEGIN + (4 * (k-1));
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

static void interactive_help(char*);

typedef enum {
    RUN_ASM,
    NEXT_INPUT,
    NEXT_INSTRUCTION,
    QUIT,
} interactive_result_t ;

static interactive_result_t handle_interactive(computer_t* c, program_state_t* config, char* line, size_t line_len, int changedReg, int changedMem, instruction_t* d);

/**
 *  Run the simulation.
 */
void Simulate(Computer* c, program_state_t config) {
    char s[128];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    instruction_t d;
    RegVals rvals;
    uint32_t a0;

    /* Initialize the PC to the start of the code section */
    c->pc = ADDRESS_BEGIN;
    if (config.debugging)
    {
        int n = ((c->instruction_end - ADDRESS_BEGIN) / 4) + 1;
        printf("running %d instructions\n", n);
        printf("running program from 0x%x to 0x%x\n", c->pc, c->instruction_end);
    }

    while (c->pc <= c->instruction_end) {
        zero_out_instruction(&d);
        if (config.interactive) {
            interactive_result_t res = handle_interactive(c, &config, s, sizeof(s), changedReg, changedMem, &d);
            switch (res)
            {
            case RUN_ASM:
                break;     // break from switch statement
            case NEXT_INPUT:
                continue; // continue to next user input
            case QUIT:
                return;   // stop everything
            case NEXT_INSTRUCTION:
                goto InstructionFetch;
            }
        }
        else
        {
        InstructionFetch:
            /* Fetch instr at mips.pc, returning it in instr */
            instr = Fetch(c, c->pc);

            /*
             * Decode instr, putting decoded instr in d
             * Note that we reuse the d struct for each instruction.
             */
            Decode(c, instr, &d, &rvals);
    #ifndef SILENT
            printf("Executing instruction at %8.8x: %8.8x\n", c->pc, instr);
    #endif
        }

    #ifndef SILENT
        /* Print decoded instruction */
        PrintInstruction(&d, c->pc);
    #endif

        /*
         * Perform computation needed to execute d, returning computed value
         * in val
         */
        val = Execute(c, &d, &rvals);
        if (c->sigs.error != OK)
        {
            fprintf(stderr, "Error at 0x%x: %s\n", c->pc, mips_error_message(c->sigs.error));
            return;
        }

        UpdatePC(c, &d, val);

        /*
         * Perform memory load or store. Place the
         * address of any updated memory in *changedMem,
         * otherwise put -1 in *changedMem.
         * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(c, &d, val, &changedMem);

        /*
         * Write back to register. If the instruction modified a register--
         * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(c, &d, val, &changedReg);
        if (c->sigs.error != OK)
        {
            fprintf(stderr, "Error at 0x%x: %s\n", c->pc, mips_error_message(c->sigs.error));
            return;
        }

    #ifndef SILENT
        PrintInfo(c, &config, changedReg, changedMem);
    #endif

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
        case 17: // exit(a0)
            return;
        default:
            fprintf(stderr, "Syscall %d not supported\n", c->sigs.syscall);
            break;
        }

        if (!config.interactive && config.debugging)
        {
            sleep(config.sleep_time);
        }
    }
}

/**
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo(computer_t* c, program_state_t* state, int changedReg, int changedMem) {
    int k, addr;
    printf("New pc = %8.8x\n", c->pc);
    if (!state->printingRegisters && changedReg == -1) {
        printf("No register was updated.\n");
    } else if (!state->printingRegisters) {
        printf("Updated r%2.2d to %8.8x\n",
        changedReg, c->registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf("r%2.2d $%-4s: 0x%8.8x  ", k, register_name(k), c->registers[k]);
            if ((k+1)%4 == 0) {
                printf("\n");
            }
        }
    }
    if (!state->printingMemory && changedMem == -1) {
        printf("No memory location was updated.\n");
    } else if (!state->printingMemory) {
        printf("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch(c, changedMem));
    } else {
        printf("Nonzero memory\n");
        printf("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch(c, addr) != 0) {
                printf("%8.8x  %8.8x\n", addr, Fetch(c, addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch.
 */
unsigned int Fetch(Computer* c, int addr) {
    return c->memory[(addr-0x00400000)/4];
}

/* Decode instr, returning decoded instruction. */
void Decode(computer_t* c, unsigned int instr, instruction_t* d, RegVals* rVals) {
    zero_out_instruction(d);
    decode(d, instr);
    mips_reset_signals(&c->sigs);
    c->sigs.sign_extend = is_unsigned(d);
}

/**
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction(instruction_t* d, uint32_t pc) {
    char buf[128];
    instruction_t in;
    error_t err;
    memset(buf, '\0', 128);
    zero_out_instruction(&in);

    in = *d;
    if (in.op.id == OP_BEQ || in.op.id == OP_BNE)
    {
        in.imm = (pc / 4) + 1 + (in.imm);
    }

    err = to_assembly(&in, buf);
    switch (err)
    {
    case OK:
        break;
    case ERR_BAD_REG:
        fprintf(stderr, "Error: invalid register\n");
        return;
    default:
        fprintf(stderr, "Error: %s\n", mips_error_message(err));
        return;
    }
    printf("%s\n", buf);
}

/* Perform computation needed to execute d, returning computed value */
int Execute(computer_t* c, instruction_t* d, RegVals* r) {
    return mips_execute(c, d, r);
}

/*
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC(Computer* c, instruction_t* d, int val) {
    mips_update_pc(c, d, (uint32_t)val);
}

/*
 * Perform memory load or store. Place the address of any updated memory
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value
 * that is read, otherwise return -1.
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1]
 * with address 0x00400004, and so forth.
 *
 */
int Mem(Computer* c, instruction_t* d, int val, int *changedMem) {
    return mips_memory(c, d, val, (uint32_t*)changedMem);
}

/*
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite(Computer* c, instruction_t* d, int val, int *changedReg) {
    mips_reg_write(c, d, val, (uint32_t*)changedReg);
}

static void dump_memory(computer_t* c, int start, int end);
static void dump_registers(computer_t* c);

static interactive_result_t handle_interactive(computer_t* c, program_state_t* config, char* line, size_t line_len, int changedReg, int changedMem, instruction_t* d)
{
    error_t err;
    char original[256];
    int len;

    bzero(line, line_len);
    bzero(original, sizeof(original));

    printf("> ");
    fgets(line, line_len, stdin);

    len = strlen(line);
    if (line[len-1] == '\n')
        line[len-1] = '\0';
    strncpy(original, line, len);

    line = strtok(line, " ");
    if (line == NULL)
        return NEXT_INPUT;

    if (
        strncmp(line, "q", 2) == 0    ||
        strncmp(line, "quit", 5) == 0 ||
        strncmp(line, "exit", 5) == 0
    ) {
        return QUIT;
    }
    else if (strncmp(line, "ls", 3 || strcmp(line, "info")) == 0)
    {
        PrintInfo(c, config, changedReg, changedMem);
        return NEXT_INPUT;
    }
    else if (strcmp(line, "next") == 0 || strcmp(line, "n") == 0)
    {
        return NEXT_INSTRUCTION;
    }
    else if (strcmp(line, "mem") == 0)
    {
        int as_addrs = 0;
        int start = 0, end = MAXNUMINSTRS + MAXNUMDATA;
        char *sstart, *send;
        sstart = strtok(NULL, " ");
        send = strtok(NULL, " ");

        char* endptr;
        int base;
        if (sstart != NULL)
        {
            errno = 0;
            base = 10;
            if (sstart[0] == '0' && sstart[1] == 'x')
            {
                sstart += 2;
                base = 16;
            }
            start = strtol(sstart, &endptr, base);
            if (errno == ERANGE || (errno != 0 && start == 0))
            {
                fprintf(stderr, "Invalid memory address\n");
                return NEXT_INPUT;
            }
        }
        if (send != NULL)
        {
            errno = 0;
            base = 10;
            if (send[0] == '0' && send[1] == 'x')
            {
                send += 2;
                base = 16;
            }
            end = strtol(send, &endptr, base);
            if (errno == ERANGE || (errno != 0 && end == 0))
            {
                fprintf(stderr, "Invalid memory address\n");
                return NEXT_INPUT;
            }
        }

        if ((line = strtok(NULL, " ")) != NULL)
            as_addrs = strcmp(line, "-address") == 0;

        as_addrs = 1;
        if (as_addrs)
        {
            start = (start - ADDRESS_BEGIN) / 4;
            end = (end - ADDRESS_BEGIN) / 4;
        }
        dump_memory(c, start, end);
        return NEXT_INPUT;
    }
    else if (strcmp(line, "reg") == 0)
    {
        char* name = strtok(NULL, " ");
        if (name == NULL)
        {
            dump_registers(c);
        }
        else
        {
            uint8_t n = get_register(name);
            if (n == (uint8_t)-1)
            {
                fprintf(stderr, "Invalid register\n");
                return NEXT_INPUT;
            }
            printf("%s: 0x%x\n", name, c->registers[n]);
        }
        return NEXT_INPUT;
    }
    else if (strcmp(line, "help") == 0)
    {
        fprintf(stdout, "Running simulator interactively.\n\n");
        interactive_help(strtok(NULL, " "));
        return NEXT_INPUT;
    }
    else
    {
        err = assemble(d, original);
        if (err != OK)
        {
            fprintf(stderr, "Error: Could not parse instruction: %s\n", mips_error_message(err));
            // interactive_help(NULL);
            return NEXT_INPUT;
        }
    }
    return RUN_ASM;
}

static void interactive_help(char* command)
{
    const char* msg;
    const char* main_msg = ""
        "Usage:\n"
        "  > [command | assembly] [arguments...]\n"
        "\n"
        "Commands:\n"
        "  quit             quit the simulator\n"
        "  ls               list info about the current simulation\n"
        "  next             run the next instruction from memory\n"
        "  reg [name]       list the registers and their values\n"
        "  mem [start end]  print out main memory (with an optional `start` and `end` indexes)\n"
        "  help [command]   get this help message\n";
    if (command == NULL)
        msg = main_msg;
    if (strcmp(command, "mem") == 0)
        msg = ""
            "Usage:\n"
            "  > mem [start end] [-address]\n"
            "\n"
            "Options:\n"
            "  -address  treat the `start` and `end` arguments as addresses not indexes\n"
            "";
    else
        msg = main_msg;
    printf("%s\n", msg);
}

static void dump_memory(computer_t* c, int start, int end)
{
    int i;
    if (start == 0 && end == 0)
    {
        start = 0;
        end = MAXNUMINSTRS + MAXNUMDATA;
    }
    printf("ADDRESS    | MEMORY ...\n");
    printf("0x%08x | ", (4 * 0) + ADDRESS_BEGIN);
    for (i = start; i < end; i++)
    {
        printf("0x%08x", c->memory[i]);
        if ((i+1) % 8 == 0)
        {
            printf("\n0x%08x | ", (4 * i) + ADDRESS_BEGIN);
        }
        else
        {
            printf(" ");
        }
    }
    printf("\n");
}

static void dump_registers(computer_t* c)
{
    int i;
    for (i = 0; i < 35; i++)
        printf("$%-4s: 0x%08x\n", register_name(i), c->registers[i]);
}