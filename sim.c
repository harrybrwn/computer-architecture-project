#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "computer.h"

#define TRUE  1
#define FALSE 0

void help()
{
    static char* msg = ""
        "Usage sim [...options] [file]\n"
        "\n"
        "Options:\n"
        "  -r    print out registers\n"
        "  -m    print out memory\n"
        "  -i    run interactivly\n"
        "  -d    enable debugging mode\n"
        "  -s n  sleep for `n` seconds every cycle\n"
        "  -h    print help message\n";
    fprintf(stderr, "%s", msg);
}

void usage()
{
    static char* msg = "\nUsage sim [file] [...options]";
    fprintf(stderr, "%s\n", msg);
}

int main (int argc, char *argv[])
{
    int argIndex;
    FILE *filein;
    program_state_t state = {0, 0, 0, 0, 0};

    if (argc < 2)
    {
        fprintf(stderr, "Error: Not enough arguments.\n");
        usage();
        exit (1);
    }

    for (argIndex=1; argIndex<argc && argv[argIndex][0]=='-'; argIndex++)
    {
        /* Argument is an option, we hope one of -r, -m, -i, -d. */
        int i = 1;
        for (i = 1; argv[argIndex][i] != '\0'; i++)
        {
            switch (argv[argIndex][i]) {
            case 'r':
                state.printingRegisters = TRUE;
                break;
            case 'm':
                state.printingMemory = TRUE;
                break;
            case 'i':
                state.interactive = TRUE;
                break;
            case 'd':
                state.debugging = TRUE;
                break;
            case 's':
                if (argIndex >= argc)
                {
                    fprintf(stderr, "No sleep time given for -s flag argument.\n");
                    help();
                    exit(1);
                }
                else
                {
                    char* endptr, *str;
                    long val;
                    argIndex++;
                    errno = 0;
                    str = argv[argIndex];
                    val = (int)strtol(str, &endptr, 10);
                    if (
                            (errno == ERANGE) ||
                            (errno != 0 && val == 0)
                       ) {
                        fprintf(stderr, "Invalid number given to -s: '%s'\n", str);
                        exit(1);
                    }
                    if (endptr == str)
                    {
                        fprintf(stderr, "Empty number given to -s\n");
                        exit(1);
                    }

                    state.sleep_time = (int)val;
                    goto ArgLoopEnd;
                }
                break;
            case 'h':
                help();
                exit(0);
            default:
                fprintf(stderr, "Invalid option \"%s\".\n", argv[argIndex]);
                fprintf(stderr, "Correct options are -r, -m, -i, -d.\n\n");
                help();
                exit(1);
            }
        }
    ArgLoopEnd:;
    }

    /* A file is optional if running interactivly */
    if (argIndex == argc && !state.interactive)
    {
        fprintf(stderr, "No file name given.\n");
        usage();
        exit(1);
    }
    else if (argIndex < argc-1)
    {
        fprintf(stderr, "Too many arguments.\n");
        usage();
        exit(1);
    }

    if (argIndex < argc)
    {
        filein = fopen(argv[argIndex], "r");
        if (filein == NULL)
        {
            fprintf(stderr, "Can't open file: %s\n", argv[argIndex]);
            exit(1);
        }
    }
    else
    {
        filein = NULL;
    }

    Computer c;
    InitComputer(&c, filein);
    if (filein != NULL)
        fclose(filein);
    Simulate(&c, state);
    return 0;
}
