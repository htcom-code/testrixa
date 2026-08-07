CC=g++
MACRO+=-DTEST_TERM
CPPLAGS=-std=c++17
# -Werror: the build is warning-free on gcc 12 and clang 14, and staying
# that way is only enforceable if a warning stops the build.
# -Wno-unused-parameter is deliberately absent: it was hiding two warnings
# that consumers building with -Wextra saw in our public headers and we did
# not. Suppressing a warning in our own build only blinds us to it.
WARNFLAGS?=-Wall -Wextra -Wno-uninitialized -Werror

# SANITIZER=address|undefined|thread. Sanitizers have to reach the link too --
# they were not, so any sanitizer build failed with undefined references from
# the runtime. Same for LTO and anything else that spans both stages.
SANITIZER?=
ifneq ($(SANITIZER),)
SANFLAGS=-fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g -O1
else
SANFLAGS=-O2
endif

CPPFLAGS=${CPPLAGS} ${SANFLAGS} ${WARNFLAGS}
LDFLAGS?=-pthread
TARGET=testAll.out
OBJS=testMain.o testTEST.o testOptions.o testMemory.o testLeak.o testCompare.o testShim.o testQuarantine.o testReporter.o testBacktrace.o testFixture.o testStress.o testThread.o testMock.o
INC_DIR=../include
INCS=-I${INC_DIR}

# Every object depends on every public header. Without this a header edit
# leaves stale objects behind: only testMain.o rebuilt, so the test cases
# compiled into testTEST.o kept registering into the previous namespace's
# TEST::list and the runner reported "Test case None".
HDRS=${INC_DIR}/testrixa/testrixa.h \
     ${INC_DIR}/testrixa/traits.h \
     ${INC_DIR}/testrixa/options.h \
     ${INC_DIR}/testrixa/detail/core.hpp \
     ${INC_DIR}/testrixa/detail/options.hpp \
     ${INC_DIR}/testrixa/detail/memory/allocator.hpp \
     ${INC_DIR}/testrixa/detail/memory/platform.hpp \
     ${INC_DIR}/testrixa/detail/memory/block.hpp \
     ${INC_DIR}/testrixa/detail/memory/tracker.hpp \
     ${INC_DIR}/testrixa/memory.h \
     ${INC_DIR}/testrixa/malloc_shim.h \
     ${INC_DIR}/testrixa/stress.h \
     ${INC_DIR}/testrixa/thread.h \
     ${INC_DIR}/testrixa/mock.h

.PHONY: all check-prefix check-namespace check-cli check clean rebuild tests

all: $(TARGET)

check-cli: $(TARGET)
	./cli_test.sh ./$(TARGET)

# Compile-only contract checks. Deliberately not linked into $(TARGET): they
# must not add test cases to the runner's output. Building them is the
# assertion.
check-prefix: checkPrefix.cpp $(HDRS)
	$(CC) -fsyntax-only ${CPPFLAGS} ${MACRO} ${INCS} checkPrefix.cpp

# Including testrixa must not drop anything into the consumer's global
# namespace. core.hpp used to carry a global `using namespace std;`.
check-namespace: checkNamespace.cpp $(HDRS)
	$(CC) -fsyntax-only ${CPPFLAGS} ${MACRO} ${INCS} checkNamespace.cpp

check: check-prefix check-namespace check-cli $(TARGET)
	./$(TARGET)

$(TARGET):$(OBJS)
	$(CC) ${CPPLAGS} ${SANFLAGS} ${LDFLAGS} -o $@ $(OBJS)

# main() lives in testMain.cpp, which defines TEST_RUN_TERM.
testMain.o:testMain.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testMain.o testMain.cpp

testTEST.o:testTEST.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testTEST.o testTEST.cpp

testOptions.o:testOptions.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testOptions.o testOptions.cpp

testMemory.o:testMemory.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testMemory.o testMemory.cpp

testLeak.o:testLeak.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testLeak.o testLeak.cpp

testCompare.o:testCompare.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testCompare.o testCompare.cpp

testShim.o:testShim.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testShim.o testShim.cpp

testQuarantine.o:testQuarantine.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testQuarantine.o testQuarantine.cpp

testReporter.o:testReporter.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testReporter.o testReporter.cpp

testBacktrace.o:testBacktrace.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testBacktrace.o testBacktrace.cpp

testFixture.o:testFixture.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testFixture.o testFixture.cpp

testStress.o:testStress.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testStress.o testStress.cpp

testThread.o:testThread.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testThread.o testThread.cpp

testMock.o:testMock.cpp $(HDRS)
	$(CC) -c ${CPPFLAGS} ${MACRO} ${INCS} -o testMock.o testMock.cpp


clean:
	rm -f $(OBJS)
	rm -f *.out
	rm -rf *.dSYM

rebuild:
	${MAKE} clean
	${MAKE}

tests:
	${MAKE} check
