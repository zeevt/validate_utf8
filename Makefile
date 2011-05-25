CFLAGS := -std=c89 -D_POSIX_C_SOURCE=1 -Wall -Wextra -pedantic -O2 -g
LDFLAGS := -Wl,-O1 -Wl,--as-needed -fwhole-program
ifeq (${PGO_GEN},yes)
CFLAGS := $(CFLAGS) -fprofile-generate
endif
ifeq (${PGO_USE},yes)
CFLAGS := $(CFLAGS) -fprofile-use
endif
PERF = ~/linux-2.6/tools/perf/perf

all: test

guess_charset: guess_charset.c

test: guess_charset
	./guess_charset ~/linux-2.6/Makefile
	./guess_charset ~/Downloads/UTF-8-demo.txt

profile: test
	$(PERF) record -F 10000 ./guess_charset testdata
	$(PERF) report
	$(PERF) annotate -l

guess_charset_pgo: guess_charset.c
	$(MAKE) clean
	$(MAKE) PGO_GEN=yes guess_charset
	./guess_charset testdata
	$(MAKE) clean
	$(MAKE) PGO_USE=yes guess_charset

clean:
	rm -f guess_charset
