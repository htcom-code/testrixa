//
//  options.h
//  testrixa
//
//  Public entry point for declaring component command line options.
//
//  A component claims a namespace and registers its options as file-scope
//  statics; the runner then accepts --<group>.<key>[=<value>] and lists them
//  under --help without main() knowing anything about them.
//
//      #include <testrixa/options.h>
//
//      using namespace testrixa::options;
//
//      static OptionGroup   mem("mem", "Memory Suite");
//      static FlagOption    mem_enable   (mem, "enable", false, "run the memory phase");
//      static StringOption  mem_preset   (mem, "preset", "default",
//                                         "fast|default|paranoid", "checker preset");
//      static IntOption     mem_backtrace(mem, "backtrace", 0, 0, 128,
//                                         "frames to capture per allocation (0=off)");
//
//      // ... later, at run time
//      if (mem_enable.value()) { ... }
//
//  Unknown groups and unknown keys are rejected with a message rather than
//  ignored: an option that silently does nothing is worse than one that fails.
//

#ifndef TESTRIXA_OPTIONS_H
#define TESTRIXA_OPTIONS_H

#include <testrixa/detail/options.hpp>

#endif // TESTRIXA_OPTIONS_H
