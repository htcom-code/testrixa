//
//  testOptions.cpp
//  testrixa
//
//  Exercises the option registry the way a real component will use it.
//
//  Registers a "demo" group and nothing else -- no TESTCASE, so the runner's
//  output is unchanged. What it buys is a group that cli_test.sh can drive:
//  --demo.<key> must parse, validate, reject and show up under --help exactly
//  as testrixa::memory's options will once they exist.
//
//  The values are observable because --help prints each option's current value,
//  and options are applied before help is rendered:
//
//      ./testAll.out --demo.count=5 -h     ->  ... [5]
//

#include <testrixa/options.h>

using namespace testrixa::options;

static OptionGroup  demo("demo", "Registry demo (test fixture)");

static FlagOption   demo_enable(demo, "enable", false,
                                "a flag that defaults off");

static IntOption    demo_count(demo, "count", 1, 1, 100,
                               "an integer between 1 and 100");

static StringOption demo_mode(demo, "mode", "default", "fast|default|paranoid",
                              "a value restricted to a fixed set");

static StringOption demo_text(demo, "text", "", nullptr,
                              "free text, no restriction");
