//
//  testFixture.cpp
//  testrixa
//
//  setUp / tearDown.
//
//  They run around every phase rather than once per test case. Each phase is a
//  separate run of the code under test, and giving the second one whatever
//  state the first left behind is how tests start depending on their order.
//
//  Fixture state is just members of the test case class: everything between
//  TESTCASE_BEGIN and the phase macros is inside the class body.
//

#include <cstddef>
#include <string>
#include <vector>

#include <testrixa/testrixa.h>

namespace {

// Observed from outside the test case, because the point is what happens
// between phases -- and a member would be reset by nothing at all.
int g_setUpCalls    = 0;
int g_tearDownCalls = 0;

// The failing-setUp path is deliberately not exercised here: a test case whose
// setUp fails turns the suite red, which is the correct behaviour and the
// reason it cannot live in the suite. It is verified by a standalone probe --
// see the commit that introduced fixtures.

} // namespace

// ---------------------------------------------------------------------------
// A test case with a fixture. The vector is the fixture's state: it must be
// full inside every phase and empty again between them.
// ---------------------------------------------------------------------------
TESTCASE_BEGIN(testFixture)

    std::vector<int> resource;

TESTCASE_SETUP(testFixture) {
    g_setUpCalls++;
    resource.assign(4, 7);
    return true;
}

TESTCASE_TEARDOWN(testFixture) {
    g_tearDownCalls++;
    resource.clear();
    return true;
}

TESTCASE_BASIC(testFixture) {
    TESTCASE_GROUP_START(SETUP_RAN_BEFORE_THIS_PHASE) {
        CHECK_SAME(resource.size(), (std::size_t)4);
        CHECK_SAME(resource[0], 7);
        CHECK(g_setUpCalls >= 1);
    } TESTCASE_GROUP_END(SETUP_RAN_BEFORE_THIS_PHASE)

    TESTCASE_GROUP_START(THE_PHASE_MAY_DIRTY_THE_FIXTURE) {
        // The next phase must not see this.
        resource.push_back(99);
        CHECK_SAME(resource.size(), (std::size_t)5);
    } TESTCASE_GROUP_END(THE_PHASE_MAY_DIRTY_THE_FIXTURE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testFixture) {
    TESTCASE_RETURN
}

TESTCASE_MEMORY(testFixture) {
    TESTCASE_GROUP_START(THE_MEMORY_PHASE_GETS_A_FRESH_FIXTURE) {
        // Rebuilt by setUp, not inherited from the basic phase's push_back.
        // This is the assertion the whole design exists for.
        CHECK_SAME(resource.size(), (std::size_t)4);
        CHECK_SAME(resource[0], 7);
    } TESTCASE_GROUP_END(THE_MEMORY_PHASE_GETS_A_FRESH_FIXTURE)

    TESTCASE_GROUP_START(THE_HOOKS_PAIR_UP) {
        // Deliberately not "setUp ran three times": which phases run is a
        // command line choice (--only=memory runs one), so a count would pass
        // on a full run and fail on --mem. The invariant that holds either way
        // is the pairing -- inside a phase, setUp has run once more than
        // tearDown, whichever phase this is.
        CHECK(g_setUpCalls >= 1);
        CHECK_SAME(g_setUpCalls - g_tearDownCalls, 1);
    } TESTCASE_GROUP_END(THE_HOOKS_PAIR_UP)

    TESTCASE_RETURN
}

TESTCASE_END(testFixture)


// ---------------------------------------------------------------------------
// No fixture at all: the default hooks must leave the case exactly as it was.
// ---------------------------------------------------------------------------
TESTCASE_BEGIN(testNoFixture)

TESTCASE_BASIC(testNoFixture) {
    TESTCASE_GROUP_START(A_CASE_WITHOUT_HOOKS_STILL_RUNS) {
        CHECK(true);
        CHECK_SAME(1 + 1, 2);
    } TESTCASE_GROUP_END(A_CASE_WITHOUT_HOOKS_STILL_RUNS)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testNoFixture) {
    TESTCASE_RETURN
}

TESTCASE_END(testNoFixture)
