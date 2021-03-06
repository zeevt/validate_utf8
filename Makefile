CFLAGS := -std=gnu89 -Wall -O2 -g
LDFLAGS := -Wl,-O1 -Wl,--as-needed
ifneq (${CC},icc)
CFLAGS := $(CFLAGS) -Wextra -pedantic
LDFLAGS := $(LDFLAGS) -fwhole-program
endif
ifeq (${CC},clang)
CFLAGS := $(CFLAGS) -fomit-frame-pointer
endif
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
	$(PERF) record -F 10000 ./guess_charset testdata_utf8
	$(PERF) report
	$(PERF) annotate -l

benchmark: guess_charset
	$(PERF) stat ./guess_charset testdata_ascii
	$(PERF) stat ./guess_charset testdata_utf8

guess_charset_pgo: guess_charset.c
	$(MAKE) clean
	$(MAKE) PGO_GEN=yes guess_charset
	./guess_charset testdata_ascii
	./guess_charset testdata_utf8
	$(MAKE) clean
	$(MAKE) PGO_USE=yes guess_charset

clean:
	rm -f guess_charset
