# A header-only library should build with a compiler and nothing else.
# This Makefile exists to keep that claim honest -- no CMake, no configure step,
# one command.

CXX?=c++
SANITIZER?=
ifneq ($(SANITIZER),)
SANFLAGS=-fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g -O1
else
SANFLAGS=-O2
endif

CXXFLAGS?=-std=c++17 $(SANFLAGS) -Wall -Wextra -Werror
INCS=-I../include
TARGET=example.out

.PHONY: all run clean

all: $(TARGET)

$(TARGET): testExample.cpp ../include/testrixa/testrixa.h \
           ../include/testrixa/traits.h \
           ../include/testrixa/detail/core.hpp
	$(CXX) $(CXXFLAGS) $(INCS) -o $@ testExample.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf *.dSYM
