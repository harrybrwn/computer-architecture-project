#ifndef __MIPS_COMPUTER_H
#define __MIPS_COMPUTER_H

#include "mips.h"
#include "runtime.h"

#define MAXNUMINSTRS 1024	/* max # instrs in a program */
#define MAXNUMDATA 3072		/* max # data words */

// typedef struct SimulatedComputer {
//     int memory [MAXNUMINSTRS+MAXNUMDATA];
//     int registers [32];
//     int pc;

//     /* Control signals */
//     struct { } sig;
// } computer_t;

typedef computer_t Computer;

typedef struct program_state {
    int printingRegisters;
    int printingMemory;
    int interactive;
    int debugging;
    unsigned int sleep_time;
} program_state_t;

// typedef enum { R=0, I, J } InstrType;
typedef format_t InstrType;

typedef struct {
  int rs;
  int rt;
  int rd;
  int shamt;
  int funct;
} rregs_t;

typedef struct {
  int rs;
  int rt;
  int addr_or_immed;
} iregs_t;

typedef struct {
  int target;
} jregs_t;

typedef struct {
  InstrType type;
  int op;
  union {
    rregs_t r;
    iregs_t i;
    jregs_t j;
  } regs;
} DecodedInstr; // I'm not using this

// typedef struct {
//   int rs; /*Value in register rs*/
//   int rt;
//   int rd;
// } RegVals;
typedef regvals_t RegVals;

void InitComputer(Computer*, FILE*);

void Simulate(Computer*, program_state_t);

#endif /* __MIPS_COMPUTER_H */
