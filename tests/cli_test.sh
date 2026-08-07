#!/bin/sh
#
#  cli_test.sh
#  testrixa
#
#  Regression test for the terminal runner's command line.
#
#  The prototype shipped this parser with no tests at all, so there was nothing
#  to compare against when replacing it. This script pins the observed behaviour
#  first: the option registry that replaces the parser has to keep every
#  assertion below green, or change it on purpose.
#
#  Cases marked QUIRK record behaviour that is arguably wrong. They are pinned
#  anyway -- the point is that changing them must be a decision, not an accident.
#
#  usage: cli_test.sh <path-to-runner>
#

RUNNER="${1:-./testAll.out}"

if [ ! -x "$RUNNER" ]; then
    echo "cli_test: runner not found or not executable: $RUNNER" >&2
    exit 2
fi

pass=0
fail=0
tmp="${TMPDIR:-/tmp}/testrixa_cli_$$"
trap 'rm -f "$tmp"' EXIT

# run <args...> -- fills $tmp with ANSI-stripped output, sets $rc
run() {
    "$RUNNER" "$@" 2>&1 | sed 's/\x1b\[[0-9;]*m//g' > "$tmp"
    rc=$?
    # $? above is sed's; re-run for the real status without the pipe
    "$RUNNER" "$@" > /dev/null 2>&1
    rc=$?
}

ok()   { pass=$((pass + 1)); }
bad()  { fail=$((fail + 1)); echo "  FAIL: $1"; }

expect_rc() {
    if [ "$rc" -eq "$1" ]; then ok; else bad "$2 -- expected exit $1, got $rc"; fi
}

expect_out() {
    if grep -q -- "$1" "$tmp"; then ok; else bad "$2 -- output missing: $1"; fi
}

expect_no_out() {
    if grep -q -- "$1" "$tmp"; then bad "$2 -- output should not contain: $1"; else ok; fi
}

echo "cli_test: $RUNNER"

# --- no arguments ----------------------------------------------------------
run
expect_rc 0     "no args"
expect_out '::BASIC'   "no args runs basic"
expect_out '::MEASURE' "no args runs measure"

# --- listing and help ------------------------------------------------------
run -l
expect_rc 0   "-l"
expect_out 'Test case list:' "-l prints the list"
expect_out 'testTEST'        "-l names the registered case"
expect_no_out '::BASIC'      "-l does not run tests"

run --list
expect_out 'Test case list:' "--list"

run -h
expect_rc 0   "-h"
expect_out 'Usage:'   "-h prints usage"
expect_no_out '::BASIC' "-h does not run tests"

run --help
expect_out 'Usage:' "--help"

# --- phase selection -------------------------------------------------------
run -b
expect_rc 0   "-b"
expect_out '::BASIC'      "-b runs basic"
expect_no_out '::MEASURE' "-b skips measure"

run --basic
expect_no_out '::MEASURE' "--basic skips measure"

run -m
expect_rc 0   "-m"
expect_out '::MEASURE'  "-m runs measure"
expect_no_out '::BASIC' "-m skips basic"

run --measure
expect_no_out '::BASIC' "--measure skips basic"

run -b -m
expect_rc 1   "-b -m is rejected"
expect_out "can't use -b -m together" "-b -m explains itself"

run -m -b
expect_rc 1   "-m -b is rejected"

# --- loop count ------------------------------------------------------------
run -m -t=30
expect_rc 0 "-t=30"
if grep -qE '\|  *30\|' "$tmp"; then ok; else bad "-t=30 -- loop column is not 30"; fi

run -m --times=7
if grep -qE '\|  *7\|' "$tmp"; then ok; else bad "--times=7 -- loop column is not 7"; fi

# WAS A QUIRK: -t=0 used to be swallowed -- atoi()==0 failed the `> 0` guard and
# the default loop count survived. Now it is rejected like any other bad value.
run -m -t=0
expect_rc 1 "-t=0 is rejected"
expect_out 'out of range' "-t=0 explains itself"

# WAS A QUIRK: bare -t used to be swallowed too.
run -m -t
expect_rc 1 "bare -t is rejected"
expect_out 'needs a value' "bare -t explains itself"

# Trailing junk must not be read as a number.
run -m -t=30x
expect_rc 1 "-t=30x is rejected"
expect_out 'expected an integer' "-t=30x explains itself"

run -t=9999999
expect_rc 1 "-t=9999999 is out of range"
expect_out 'out of range' "-t=9999999 explains itself"

# --- detail ----------------------------------------------------------------
run -m -d
expect_out 'Minimum' "-d shows Minimum"
expect_out 'Maximum' "-d shows Maximum"

run -m
expect_out 'Source'     "without -d the Source column is shown"
expect_no_out 'Minimum' "without -d there is no Minimum column"

# --- test case selection ---------------------------------------------------
run testTEST
expect_rc 0 "named case"
expect_out 'Test case selected : testTEST' "named case is selected"

run nosuch
expect_rc 1 "unknown case name fails"
expect_out 'Test case None' "unknown case name reports nothing ran"

# --- position no longer matters (the scan is uniform) ----------------------
# WAS A QUIRK: -l and -h were only honoured as the FIRST argument; anywhere else
# they were not options at all and the run proceeded normally.
run -s -l
expect_out 'Test case list:' "-l works in any position"
expect_no_out '::BASIC'      "-l in any position does not run tests"

run -s -h
expect_out 'Usage:' "-h works in any position"

# WAS A QUIRK: a test case name was only recognised at position 0, so any
# preceding option silently dropped it -- `-b testTEST` ran every case.
run -b testTEST
expect_rc 0 "case name after an option"
expect_out 'Test case selected : testTEST' "case name is honoured in any position"

run testTEST extraName
expect_rc 1 "two case names are rejected"
expect_out 'more than one test case' "two case names explain themselves"

# --- unknown input is refused, not ignored ---------------------------------
# WAS A QUIRK: unknown options were accepted silently. A --mem.* option that
# quietly does nothing is exactly the failure the memory design forbids, so the
# parser now refuses anything it does not recognise.
run -zzz
expect_rc 1 "unknown option is rejected"
expect_out 'unknown option' "unknown option explains itself"

# --- phase selection by name -----------------------------------------------
run --only=basic
expect_rc 0 "--only=basic"
expect_out '::BASIC'      "--only=basic runs basic"
expect_no_out '::MEASURE' "--only=basic skips measure"

run --only=basic,measure
expect_out '::BASIC'   "--only=basic,measure runs basic"
expect_out '::MEASURE' "--only=basic,measure runs measure"

run --skip=measure
expect_out '::BASIC'      "--skip=measure runs basic"
expect_no_out '::MEASURE' "--skip=measure skips measure"

run --only=nosuchphase
expect_rc 1 "--only= with an unknown phase is rejected"
expect_out 'unknown phase' "--only= unknown phase explains itself"

run --skip=basic,measure
expect_rc 1 "skipping every phase is rejected"
expect_out 'nothing to run' "skipping every phase explains itself"

# --- the memory phase is opt-in --------------------------------------------
run
expect_no_out '::MEMORY' "a plain run does not pay for the memory phase"

run --mem
expect_rc 0 "--mem"
expect_out '::MEMORY'     "--mem runs the memory phase"
expect_no_out '::BASIC'   "--mem skips basic"
expect_no_out '::MEASURE' "--mem skips measure"
expect_out 'coverage:'    "--mem states what it does and does not watch"

run --only=memory
expect_out '::MEMORY'   "--only=memory runs the memory phase"
expect_no_out '::BASIC' "--only=memory skips basic"

run --only=basic,memory
expect_out '::BASIC'  "--only=basic,memory runs basic"
expect_out '::MEMORY' "--only=basic,memory runs memory"

# --- machine-readable report -----------------------------------------------
# CI cannot read the console table. GitHub Actions reads JUnit XML and puts
# failures straight on the pull request, which is the whole reason this exists.
report="${TMPDIR:-/tmp}/testrixa_junit_$$.xml"
rm -f "$report"
run --report-junit="$report"
expect_rc 0 "--report-junit"
if [ -s "$report" ]; then ok; else bad "--report-junit -- no file written"; fi
if grep -q '<testsuites' "$report"; then ok; else bad "--report-junit -- not JUnit XML"; fi
if grep -q '<testcase ' "$report"; then ok; else bad "--report-junit -- no test cases in report"; fi
if grep -q 'classname=' "$report"; then ok; else bad "--report-junit -- no classname attribute"; fi
rm -f "$report"

run --report-junit=
expect_rc 1 "--report-junit= with no path is rejected"
expect_out 'needs a path' "empty report path explains itself"

# The component-option dispatcher used to claim anything containing a dot, so
# --report-junit=/tmp/out.xml was read as group "report-junit=/tmp/out" and a
# perfectly good core flag failed with "unknown option group". A group name is
# an identifier; a path is not.
run --report-junit=/dev/null
expect_rc 0 "a report path with dots is not mistaken for a component option"
expect_no_out 'unknown option group' "a path is not read as a group name"

# --- component option registry ---------------------------------------------
# Driven through the "demo" group registered by testOptions.cpp. --help prints
# each option's current value, and options are applied before help is rendered,
# so `--demo.x=v -h` shows whether the value actually landed.
run -h
expect_out 'Registry demo'    "--help lists registered groups"
expect_out -- '--demo.count'  "--help lists registered options"

run --demo.count=5 -h
expect_rc 0 "--demo.count=5"
expect_out '\[5\]' "--demo.count=5 takes effect"

run --demo.enable -h
expect_out '\[on\]' "a bare flag turns on"

run --demo.enable=off -h
expect_out '\[off\]' "a flag accepts =off"

run --demo.mode=paranoid -h
expect_out '\[paranoid\]' "a restricted string accepts an allowed value"

run --demo.mode=bogus
expect_rc 1 "a restricted string rejects a disallowed value"
expect_out 'expected one of' "the rejection lists what is allowed"

run --demo.count=999
expect_rc 1 "an int option enforces its range"
expect_out 'out of range' "the range rejection explains itself"

run --demo.count=abc
expect_rc 1 "an int option rejects non-numeric text"

run --demo.count
expect_rc 1 "an int option needs a value"

run --demo.nosuchkey=1
expect_rc 1 "an unknown key in a known group is rejected"
expect_out 'unknown option' "unknown key explains itself"

run --nosuchgroup.key=1
expect_rc 1 "an unknown group is rejected"
expect_out 'unknown option group' "unknown group explains itself"

# --- result ----------------------------------------------------------------
echo "cli_test: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
