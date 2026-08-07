//
//  testMock.cpp
//  testrixa
//
//  Test doubles.
//
//  Two things need proving here and they are easy to confuse. One is that the
//  generated class is a real override -- that a call through a base-interface
//  pointer lands in our recorder rather than in the interface. The other is
//  that the recorder counts and returns what it was told to. A mock that only
//  passed the second would look correct in every direct-call test and do
//  nothing at all in the code under test.
//

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <testrixa/mock.h>
#include <testrixa/testrixa.h>
#include <testrixa/thread.h>

// ---------------------------------------------------------------------------
// The interfaces under test
// ---------------------------------------------------------------------------
struct Clock {
    virtual ~Clock() {}
    virtual std::uint64_t now() const = 0;
    virtual void sleep(int ms) = 0;
};

MOCK_BEGIN(MockClock, Clock)
    MOCK_CONST_METHOD(now,   std::uint64_t, (),       ())
    MOCK_METHOD      (sleep, void,          (int ms), (ms))
MOCK_END

// A comma inside a parameter type. The parentheses around the parameter list
// are what let this through -- an arity-numbered macro could not take it.
struct Store {
    virtual ~Store() {}
    virtual std::string lookup(std::map<int, int> keys, int which) = 0;
};

MOCK_BEGIN(MockStore, Store)
    MOCK_METHOD(lookup, std::string, (std::map<int,int> keys, int which), (keys, which))
MOCK_END

// A parameter that cannot be copied. Recording is impossible; counting is not.
struct Sink {
    virtual ~Sink() {}
    virtual void take(std::unique_ptr<int> value) = 0;
};

MOCK_BEGIN(MockSink, Sink)
    MOCK_METHOD(take, void, (std::unique_ptr<int> value), (std::move(value)))
MOCK_END

// A return type with no default constructor: nothing can be invented for it.
struct Handle {
    explicit Handle(int id) : m_id(id) {}
    int m_id;
};

struct Factory {
    virtual ~Factory() {}
    virtual Handle open(const std::string& path) = 0;
};

MOCK_BEGIN(MockFactory, Factory)
    MOCK_METHOD(open, Handle, (const std::string& path), (path))
MOCK_END

// ---------------------------------------------------------------------------
// The code under test -- deliberately talks only to the interface
// ---------------------------------------------------------------------------
namespace {

class Scheduler {
public:
    explicit Scheduler(Clock* clock) : m_clock(clock) {}

    std::uint64_t tick(int pause) {
        const std::uint64_t before = m_clock->now();
        m_clock->sleep(pause);
        return m_clock->now() - before;
    }

private:
    Clock* m_clock;
};

// A function pointer seam, the arrangement memory.h uses for install().
typedef int (*ComputeFn)(int);

int realCompute(int value) { return value * 2; }

ComputeFn g_compute = &realCompute;

int compute(int value) { return g_compute(value); }

testrixa::mock::Method<int(int)> g_fakeCompute("compute");

int fakeComputeThunk(int value) { return g_fakeCompute.invoke(value); }

} // namespace

TESTCASE_BEGIN(testMock)

TESTCASE_BASIC(testMock) {

    TESTCASE_GROUP_START(THE_MOCK_IS_A_REAL_OVERRIDE) {
        // Through a Clock*, so a mock that failed to override would call the
        // pure virtual and abort rather than quietly pass.
        MockClock clock;
        clock.nowMock.returns(1000);

        Clock* asInterface = &clock;
        CHECK_SAME(asInterface->now(), (std::uint64_t)1000);
        CHECK_CALLED(clock.nowMock);
    } TESTCASE_GROUP_END(THE_MOCK_IS_A_REAL_OVERRIDE)

    TESTCASE_GROUP_START(CALLS_ARE_COUNTED_THROUGH_THE_CODE_UNDER_TEST) {
        MockClock clock;
        clock.nowMock.returns(500);

        Scheduler scheduler(&clock);
        (void)scheduler.tick(250);

        CHECK_CALLED_TIMES(clock.nowMock,   2);
        CHECK_CALLED_TIMES(clock.sleepMock, 1);
        CHECK_LAST_CALL(clock.sleepMock, 250);
    } TESTCASE_GROUP_END(CALLS_ARE_COUNTED_THROUGH_THE_CODE_UNDER_TEST)

    TESTCASE_GROUP_START(NOT_CALLED_IS_ALSO_AN_ANSWER) {
        MockClock clock;
        clock.nowMock.returns(1);

        Scheduler scheduler(&clock);
        CHECK_SAME(scheduler.tick(0), (std::uint64_t)0);
        CHECK_NOT_CALLED(MockClock().nowMock);   // a fresh one was never used
    } TESTCASE_GROUP_END(NOT_CALLED_IS_ALSO_AN_ANSWER)

    TESTCASE_GROUP_START(A_SEQUENCE_CAN_BE_SCRIPTED) {
        // The point of returnsOnce: "fails the first time, succeeds the second"
        // is the shape of every retry test there is.
        MockClock clock;
        clock.nowMock.returnsOnce(10);
        clock.nowMock.returnsOnce(20);
        clock.nowMock.returns(99);               // everything after

        CHECK_SAME(clock.now(), (std::uint64_t)10);
        CHECK_SAME(clock.now(), (std::uint64_t)20);
        CHECK_SAME(clock.now(), (std::uint64_t)99);
        CHECK_SAME(clock.now(), (std::uint64_t)99);
        CHECK_CALLED_TIMES(clock.nowMock, 4);
    } TESTCASE_GROUP_END(A_SEQUENCE_CAN_BE_SCRIPTED)

    TESTCASE_GROUP_START(A_BODY_SEES_THE_ARGUMENTS) {
        MockStore store;
        store.lookupMock.does([](std::map<int,int> keys, int which) -> std::string {
            return std::to_string(keys[which]);
        });

        std::map<int, int> keys;
        keys[1] = 41;
        keys[2] = 42;

        Store* asInterface = &store;
        CHECK_SAME(asInterface->lookup(keys, 2), std::string("42"));
        CHECK_CALLED_TIMES(store.lookupMock, 1);
    } TESTCASE_GROUP_END(A_BODY_SEES_THE_ARGUMENTS)

    TESTCASE_GROUP_START(ARGUMENTS_ARE_RECORDED_IN_ORDER) {
        MockClock clock;
        clock.nowMock.returns(0);

        clock.sleep(10);
        clock.sleep(20);
        clock.sleep(30);

        CHECK_SAME(std::get<0>(clock.sleepMock.call(0)), 10);
        CHECK_SAME(std::get<0>(clock.sleepMock.call(1)), 20);
        CHECK_SAME(std::get<0>(clock.sleepMock.lastCall()), 30);
        CHECK(clock.sleepMock.lastCallWas(30));
        CHECK_FALSE(clock.sleepMock.lastCallWas(20));
    } TESTCASE_GROUP_END(ARGUMENTS_ARE_RECORDED_IN_ORDER)

    TESTCASE_GROUP_START(LAST_CALL_ON_AN_UNCALLED_METHOD_IS_FALSE_NOT_A_CRASH) {
        MockClock clock;
        CHECK_FALSE(clock.sleepMock.lastCallWas(0));
    } TESTCASE_GROUP_END(LAST_CALL_ON_AN_UNCALLED_METHOD_IS_FALSE_NOT_A_CRASH)

    TESTCASE_GROUP_START(UNCONFIGURED_CALLS_ARE_VISIBLE) {
        // This is what a strict mock would have shouted about. Here it is a
        // number you can assert on, which keeps the mock free of any framework.
        MockClock clock;
        CHECK_SAME(clock.now(), (std::uint64_t)0);   // nothing configured
        CHECK_SAME(clock.nowMock.unconfiguredCalls(), (std::size_t)1);

        clock.nowMock.returns(7);
        CHECK_SAME(clock.now(), (std::uint64_t)7);
        CHECK_SAME(clock.nowMock.unconfiguredCalls(), (std::size_t)1);

        // A void method has no return value to invent, so calling one with
        // nothing configured is not a gap in the test -- it is the ordinary
        // case. Counting it would make this check useless on any interface
        // with a void method, which is most of them.
        clock.sleep(1);
        clock.sleep(2);
        CHECK_NO_UNSTUBBED_CALLS(clock.sleepMock);
        CHECK_CALLED_TIMES(clock.sleepMock, 2);
    } TESTCASE_GROUP_END(UNCONFIGURED_CALLS_ARE_VISIBLE)

    TESTCASE_GROUP_START(RESET_FORGETS_CALLS_AND_KEEPS_CONFIGURATION) {
        // A fixture wires a mock up once and hands it to every phase.
        MockClock clock;
        clock.nowMock.returns(1234);
        (void)clock.now();
        (void)clock.now();

        clock.nowMock.resetCalls();

        CHECK_NOT_CALLED(clock.nowMock);
        CHECK_SAME(clock.nowMock.unconfiguredCalls(), (std::size_t)0);
        CHECK_SAME(clock.now(), (std::uint64_t)1234);   // still configured
    } TESTCASE_GROUP_END(RESET_FORGETS_CALLS_AND_KEEPS_CONFIGURATION)

    TESTCASE_GROUP_START(RESET_REWINDS_A_SCRIPTED_SEQUENCE) {
        // A spent queue is not "kept configuration": the next phase would run
        // against a mock set up differently from the one the first phase saw.
        MockClock clock;
        clock.nowMock.returnsOnce(1);
        clock.nowMock.returnsOnce(2);

        CHECK_SAME(clock.now(), (std::uint64_t)1);
        CHECK_SAME(clock.now(), (std::uint64_t)2);

        clock.nowMock.resetCalls();

        CHECK_SAME(clock.now(), (std::uint64_t)1);
        CHECK_SAME(clock.now(), (std::uint64_t)2);
    } TESTCASE_GROUP_END(RESET_REWINDS_A_SCRIPTED_SEQUENCE)

    TESTCASE_GROUP_START(A_NON_COPYABLE_ARGUMENT_IS_COUNTED_NOT_RECORDED) {
        // Refusing to compile would be the easy answer and the wrong one: a
        // method taking unique_ptr is worth mocking, and the call count is
        // most of what you wanted anyway.
        MockSink sink;
        static_assert(!decltype(sink.takeMock)::recordsArguments,
                      "unique_ptr must not be recordable");

        Sink* asInterface = &sink;
        asInterface->take(std::unique_ptr<int>(new int(1)));
        asInterface->take(std::unique_ptr<int>(new int(2)));

        CHECK_CALLED_TIMES(sink.takeMock, 2);
    } TESTCASE_GROUP_END(A_NON_COPYABLE_ARGUMENT_IS_COUNTED_NOT_RECORDED)

    TESTCASE_GROUP_START(A_RETURN_TYPE_WITH_NO_DEFAULT_STILL_WORKS_WHEN_CONFIGURED) {
        MockFactory factory;
        factory.openMock.does([](const std::string& path) -> Handle {
            return Handle((int)path.size());
        });

        Factory* asInterface = &factory;
        CHECK_SAME(asInterface->open("abcd").m_id, 4);
        CHECK_LAST_CALL(factory.openMock, std::string("abcd"));
        CHECK_NO_UNSTUBBED_CALLS(factory.openMock);
    } TESTCASE_GROUP_END(A_RETURN_TYPE_WITH_NO_DEFAULT_STILL_WORKS_WHEN_CONFIGURED)

    TESTCASE_GROUP_START(A_BODY_MAY_CALL_BACK_INTO_ITS_OWN_MOCK) {
        // Bodies run with the lock released. If they did not, this deadlocks.
        MockClock clock;
        clock.sleepMock.does([&clock](int ms) -> void {
            if (ms > 1) clock.sleep(ms - 1);
        });

        clock.sleep(3);
        CHECK_CALLED_TIMES(clock.sleepMock, 3);
    } TESTCASE_GROUP_END(A_BODY_MAY_CALL_BACK_INTO_ITS_OWN_MOCK)

    TESTCASE_GROUP_START(A_FUNCTION_POINTER_SEAM_NEEDS_NO_MOCK_CLASS) {
        // The same Method object, standing on its own -- what memory.h's
        // install() seam looks like from a test.
        CHECK_SAME(compute(21), 42);              // the real one

        g_fakeCompute.returns(7);
        g_compute = &fakeComputeThunk;

        CHECK_SAME(compute(21), 7);
        CHECK_CALLED_TIMES(g_fakeCompute, 1);
        CHECK_LAST_CALL(g_fakeCompute, 21);

        g_compute = &realCompute;                 // put it back
        CHECK_SAME(compute(21), 42);
    } TESTCASE_GROUP_END(A_FUNCTION_POINTER_SEAM_NEEDS_NO_MOCK_CLASS)

    TESTCASE_GROUP_START(A_MOCK_SURVIVES_CONCURRENT_CALLERS) {
        // A test double is exactly the object you hand to concurrent code, so
        // it has to hold its own count. Verified under TSan, which is what
        // makes this assertion mean anything beyond "the number came out".
        MockClock clock;
        clock.nowMock.returns(5);

        const testrixa::concurrency::Result result =
            CONCURRENT(4, [&](int) -> bool {
                for (int i = 0; i < 250; ++i) {
                    if (clock.now() != 5) return false;
                    clock.sleep(i);
                }
                return true;
            });

        CHECK(result.clean());
        CHECK_CALLED_TIMES(clock.nowMock,   1000);
        CHECK_CALLED_TIMES(clock.sleepMock, 1000);
    } TESTCASE_GROUP_END(A_MOCK_SURVIVES_CONCURRENT_CALLERS)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testMock) {
    TESTCASE_RETURN
}

TESTCASE_END(testMock)
