CC=gcc
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -DDUMB_ASSIGNMENT_EDGECASES=1
TESTFLAGS=$(CFLAGS)
TESTFLAGS+=-I. -Wno-missing-field-initializers
LDLIBS=
MARS ?= $$HOME/school/tools/misc/Mars4_5.jar

all: sim

sim: computer.o sim.o mips.o runtime.o

sim.o:       sim.c computer.h
computer.o:  computer.c computer.h

mips.a: mips.o runtime.o
	$(AR) rcs $@ $^

clean:
	$(RM) -rf *.o **/*.o sim tests/test *.a

test: tests/test
	@$<
	@$(RM) $<

tests/test: tests/utest.o tests/test.c tests/data.h
	$(CC) $(TESTFLAGS) -o $@ $^

TESTBIN=tests/s/sample.bin tests/s/j.bin   \
		tests/s/branch.bin tests/s/fib.bin \
		tests/s/lui.bin tests/s/addiu.bin  \
		tests/s/prog.bin tests/s/mult.bin
TESTFILES=tests/sample.bin.h tests/j.bin.h tests/branch.bin.h tests/fib.bin.h tests/lui.bin.h

tests/data.h: $(TESTFILES)
	echo '#ifndef _TEST_DATA' > $@
	echo '#define _TEST_DATA' >> $@
	echo '' >> $@
	echo '#include "sample.bin.h"' >> $@
	echo '#include "j.bin.h"' >> $@
	echo '#include "branch.bin.h"' >> $@
	echo '#include "fib.bin.h"' >> $@
	echo '#include "lui.bin.h"' >> $@
	echo '' >> $@
	echo '#endif /* _TEST_DATA */' >> $@

dumps: tests/s/j.bin tests/s/branch.bin tests/s/fib.bin tests/s/lui.bin tests/s/addiu.bin tests/s/mult.bin

tests/%.bin.h: tests/s/%.bin
	xxd -c 4 -i $< $@

tests/s/%.bin: tests/s/%.s
	java -jar $(MARS) a db dump .text Binary $@ $<

tests/utest/utest.c: tests/utest
tests/utest.o: tests/utest/utest.c tests/utest
	$(CC) $(CFLAGS) -c $< -o $@
tests/utest:
	git clone https://github.com/harrybrwn/utest $@

clean_all: clean
	$(RM) -rf ./tests/utest $(TESTFILES) $(TESTBIN)

.PHONY: all test clean clean_all

submission: clean
	zip -r tests.zip tests

.PHONY: submission