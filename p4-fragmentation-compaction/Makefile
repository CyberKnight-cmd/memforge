# P4: fragmentation metrics and compaction. Owner: P4.
# Self-contained build so P4 work stays inside this folder. Same C17 standard and warnings as the
# main CMake build (CMakeLists.txt). Usage: make test   (CC=gcc-16 make test to use gcc)

CC ?= cc
CFLAGS = -std=c17 -O1 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
         -Wstrict-prototypes -Wmissing-prototypes -Wvla -Werror
INCLUDES = -Isrc -I../src/common/include -I../tests

SRCS = src/fragmentation.c src/compaction.c
TESTS = build/test_fragmentation build/test_compaction

.PHONY: test clean
test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done
	@echo "All P4 tests passed."

build/%: tests/%.c $(SRCS) src/*.h tests/p4_fixtures.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -Itests $< $(SRCS) -o $@

clean:
	rm -rf build
