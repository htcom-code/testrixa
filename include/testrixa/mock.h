//
//  mock.h
//  Mock -- test doubles for virtual interfaces
//
//  What this covers, and what it deliberately does not:
//
//    virtual interface     macros generate a derived class     <- here
//    function pointer seam  Method works standalone            <- here, see below
//    free function, no seam linker substitution, weak symbols   NOT covered
//    non-virtual member     needs code generation               NOT covered
//
//  The two that are not covered need the build system's cooperation or a code
//  generator, and both would make this header something you install rather than
//  something you include.
//
//      struct Clock {
//          virtual ~Clock() {}
//          virtual std::uint64_t now() const = 0;
//          virtual void sleep(int ms) = 0;
//      };
//
//      TRX_MOCK_BEGIN(MockClock, Clock)
//          TRX_MOCK_CONST_METHOD(now,   std::uint64_t, (),       ())
//          TRX_MOCK_METHOD      (sleep, void,          (int ms), (ms))
//      TRX_MOCK_END
//
//      MockClock clock;
//      clock.nowMock.returns(1000);
//
//      Scheduler scheduler(&clock);
//      scheduler.tick();
//
//      CHECK_CALLED_TIMES(clock.nowMock, 2);
//      CHECK_LAST_CALL(clock.sleepMock, 250);
//
//  Each mocked method gets a member named <method>Mock. That object is where
//  everything happens: you configure it before the call and question it after.
//
//  Why the parameter list is written twice
//  ---------------------------------------
//  `(int ms)` declares the override, `(ms)` forwards to the recorder. The
//  preprocessor cannot derive the second from the first without a macro
//  argument-counting library -- roughly 150 lines of expansion no one can debug
//  when it goes wrong. The duplication is the price of not having that layer,
//  and the two are checked against each other by the compiler on the spot: a
//  mismatch is an error at that line, not a silent misbehaviour.
//
//  It buys two other things. Any number of parameters, and the parentheses
//  protect commas -- (std::map<int,int> m, int i) passes through intact, which
//  arity-numbered macros cannot do.
//
//  Verify after, do not expect before
//  ----------------------------------
//  There are no EXPECT_CALL-style declarations. A mock records what happened
//  and you assert on it afterwards, which is how <testrixa/memory.h> already
//  works (measure() hands back a Report you check). Expecting up front means
//  the mock has to report a failure from its own destructor, which means it has
//  to know which test case is running, which means a global registry -- and an
//  assertion firing during stack unwinding is its own problem. The cost is
//  real and worth naming: forget to verify and nothing tells you.
//
//  A function pointer seam
//  -----------------------
//  Method does not depend on the generated class, so it works on its own
//  wherever the code under test already routes through a replaceable pointer --
//  the arrangement <testrixa/memory.h> uses for install():
//
//      static testrixa::mock::Method<int(int)> fakeCompute("compute");
//      static int fakeComputeThunk(int value) { return fakeCompute.invoke(value); }
//
//      install(&fakeComputeThunk);
//      fakeCompute.returns(7);
//
//  Thread safety
//  -------------
//  Method is thread-safe; the rest of the framework is not. A test double is
//  exactly the object you hand to the concurrent code under test, and a data
//  race inside it would be the same defect we already had to fix in the result
//  tally (see ThreadTally in detail/core.hpp). The lock is not what costs here:
//  recording a call appends to a vector, which is more expensive than taking an
//  uncontended mutex. Bodies run with the lock released, so a body may call
//  back into its own mock.
//
//  The lock makes Method non-copyable, and so a generated mock class is
//  non-copyable too. Hold mocks by reference or pointer.
//
//  Known limits, all of them compile-time and loud:
//    - a return type containing a comma needs a typedef (the parameter list
//      does not: it is parenthesised)
//    - so does an interface named with template arguments
//    - the interface must be default-constructible
//    - default arguments cannot be given in the parameter list
//    - a method returning a reference needs does(), not returns()
//    - noexcept / final: use TRX_MOCK_METHOD_SPEC and pass them yourself
//
//  One asymmetry worth knowing: every check here takes an optional failure
//  message as its last argument, except CHECK_LAST_CALL, whose trailing
//  arguments are the expected call. Passing a message to that one does not
//  silently become an expectation -- it fails to compile, because the tuple
//  cannot be built from it.
//
//  MOCK_METHOD is also gmock's macro. Define TESTRIXA_NO_SHORT_MACROS if both
//  are in the same translation unit; the TRX_ names always work.
//

#ifndef TESTRIXA_MOCK_H
#define TESTRIXA_MOCK_H

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <testrixa/testrixa.h>

TRX_BEGIN_NAMESPACE

namespace mock {

// No --mock.* option group. Every other component registered one because it had
// a knob worth turning at run time; this one has none, and an empty group in
// --help is noise pretending to be a feature.

template <typename Signature> class Method;

// ---------------------------------------------------------------------------
// Method -- one mocked function: what it should do, and what was done to it
// ---------------------------------------------------------------------------
template <typename Ret, typename... Args>
class Method<Ret(Args...)> {
public:
    typedef std::tuple<typename std::decay<Args>::type...> CallArgs;
    typedef std::function<Ret(Args...)>                    Action;

    // Recording a call means copying its arguments, and not every argument can
    // be copied -- a method taking unique_ptr is a perfectly reasonable thing
    // to mock. Rather than refusing to compile, such a method still counts its
    // calls and drops only the arguments. Ask before trusting lastCall().
    static constexpr bool recordsArguments =
        (std::is_copy_constructible<typename std::decay<Args>::type>::value && ...);

    explicit Method(const char* name) : m_name(name ? name : "?") {}

    Method(const Method&)            = delete;
    Method& operator=(const Method&) = delete;

    const char* name() const { return m_name; }

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    // Return this value for every call.
    template <typename R = Ret>
    Method& returns(R value) {
        static_assert(!std::is_void<Ret>::value,
                      "returns() on a method that returns void -- use does()");
        return does([value](Args...) -> Ret { return value; });
    }

    // Return this value for the next call only. Queue several to script a
    // sequence: fails the first time, succeeds the second.
    template <typename R = Ret>
    Method& returnsOnce(R value) {
        static_assert(!std::is_void<Ret>::value,
                      "returnsOnce() on a method that returns void -- use doesOnce()");
        return doesOnce([value](Args...) -> Ret { return value; });
    }

    // Run this for every call. The arguments are passed through, so a body can
    // decide what to return, record something of its own, or throw.
    Method& does(Action body) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_body = std::move(body);
        return *this;
    }

    Method& doesOnce(Action body) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_once.push_back(std::move(body));
        return *this;
    }

    // -----------------------------------------------------------------------
    // Observation
    // -----------------------------------------------------------------------
    std::size_t calls() const {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    bool called() const { return calls() != 0; }

    // Calls that had to invent a return value because nothing was configured.
    // Non-zero means the test is exercising a path it never described -- this
    // is what a "strict mock" would have shouted about.
    //
    // A void method never counts here however often it is called: there is no
    // return value to invent, and doing nothing is the whole of what it was
    // asked to do. Counting those would have made this useless for the most
    // ordinary interface there is -- one with a void method on it.
    std::size_t unconfiguredCalls() const {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_unconfigured;
    }

    // Both return by value: under a lock, a reference into the vector would be
    // dangling the moment another thread records a call.
    CallArgs call(std::size_t index) const {
        static_assert(recordsArguments,
                      "arguments of this method are not recorded -- one of them "
                      "is not copy-constructible");
        const std::lock_guard<std::mutex> lock(m_mutex);
        return index < m_recorded.size() ? m_recorded[index] : CallArgs();
    }

    CallArgs lastCall() const {
        static_assert(recordsArguments,
                      "arguments of this method are not recorded -- one of them "
                      "is not copy-constructible");
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_recorded.empty() ? CallArgs() : m_recorded.back();
    }

    // False when the method was never called at all, so "was it called with X"
    // and "was it called" do not need two assertions.
    template <typename... Expected>
    bool lastCallWas(const Expected&... expected) const {
        static_assert(recordsArguments,
                      "arguments of this method are not recorded -- one of them "
                      "is not copy-constructible");
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_recorded.empty()) return false;
        return m_recorded.back() == CallArgs(expected...);
    }

    // Forgets what happened, keeps what was configured -- so a fixture can hand
    // the same wired-up mock to each phase.
    //
    // The queue from returnsOnce() rewinds rather than staying spent. It is
    // configuration, and leaving it consumed would mean the second phase quietly
    // ran against a mock configured differently from the first.
    void resetCalls() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_count        = 0;
        m_unconfigured = 0;
        m_onceUsed     = 0;
        m_recorded.clear();
    }

    // -----------------------------------------------------------------------
    // Invocation -- what the generated override calls
    // -----------------------------------------------------------------------
    Ret invoke(Args... args) {
        Action action;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_count++;
            if constexpr (recordsArguments) {
                m_recorded.push_back(CallArgs(args...));
            }

            if (m_onceUsed < m_once.size()) {
                action = m_once[m_onceUsed++];
            } else if (m_body) {
                action = m_body;
            } else if constexpr (!std::is_void<Ret>::value) {
                m_unconfigured++;
            }
        }

        // Outside the lock on purpose: a body is arbitrary user code and may
        // call back into this very mock.
        if (action) return action(std::forward<Args>(args)...);
        return unconfigured();
    }

private:
    Ret unconfigured() const {
        if constexpr (std::is_void<Ret>::value) {
            return;
        } else if constexpr (std::is_default_constructible<Ret>::value) {
            return Ret();
        } else {
            // Nothing to hand back and nothing to build. Throwing would be the
            // other answer, but this header works with -fno-exceptions and a
            // test that got here is wrong in a way worth stopping for.
            std::fprintf(stderr,
                         "testrixa: mock method '%s' was called with nothing "
                         "configured, and its return type cannot be "
                         "default-constructed. Set returns() or does() first.\n",
                         m_name);
            std::abort();
        }
    }

    mutable std::mutex   m_mutex;
    const char*          m_name;
    Action               m_body;
    std::vector<Action>  m_once;
    std::size_t          m_onceUsed     = 0;
    std::size_t          m_count        = 0;
    std::size_t          m_unconfigured = 0;
    std::vector<CallArgs> m_recorded;
};

} // namespace mock

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// Class generation
// ---------------------------------------------------------------------------
#define TRX_MOCK_BEGIN(mockName, interfaceType)        \
    class mockName : public interfaceType {            \
    public:                                            \
        typedef interfaceType TrxMockedInterface;      \
        virtual ~mockName() {}

// The general form. specs is whatever follows the parameter list:
// `const`, `noexcept`, `const noexcept`, `final`, or nothing.
#define TRX_MOCK_METHOD_SPEC(name, returnType, params, arguments, specs)     \
    mutable ::testrixa::mock::Method<returnType params> name##Mock{#name};   \
    returnType name params specs override {                                  \
        return this->name##Mock.invoke arguments;                            \
    }

#define TRX_MOCK_METHOD(name, returnType, params, arguments) \
    TRX_MOCK_METHOD_SPEC(name, returnType, params, arguments, )

#define TRX_MOCK_CONST_METHOD(name, returnType, params, arguments) \
    TRX_MOCK_METHOD_SPEC(name, returnType, params, arguments, const)

#define TRX_MOCK_END };

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------
#define TRX_CHECK_CALLED(method, ...) \
    TRX_CHECK((method).called(), ##__VA_ARGS__)

#define TRX_CHECK_NOT_CALLED(method, ...) \
    TRX_CHECK(!(method).called(), ##__VA_ARGS__)

// The cast is the point: calls() is size_t and a literal 3 is int, which
// -Wsign-compare rejects under -Werror. Writing it out at every call site is
// how CHECK_SAME's sign-compare defect got found in the first place.
#define TRX_CHECK_CALLED_TIMES(method, times, ...) \
    TRX_CHECK_SAME((method).calls(), (std::size_t)(times), ##__VA_ARGS__)

#define TRX_CHECK_LAST_CALL(method, ...) \
    TRX_CHECK((method).lastCallWas(__VA_ARGS__))

// Nothing reached a default-built return value: every call the test made was
// one the test described.
#define TRX_CHECK_NO_UNSTUBBED_CALLS(method, ...) \
    TRX_CHECK_SAME((method).unconfiguredCalls(), (std::size_t)0, ##__VA_ARGS__)

#ifndef TESTRIXA_NO_SHORT_MACROS

#define MOCK_BEGIN(mockName, interfaceType)  TRX_MOCK_BEGIN(mockName, interfaceType)
#define MOCK_END                             TRX_MOCK_END
#define MOCK_METHOD(name, returnType, params, arguments) \
    TRX_MOCK_METHOD(name, returnType, params, arguments)
#define MOCK_CONST_METHOD(name, returnType, params, arguments) \
    TRX_MOCK_CONST_METHOD(name, returnType, params, arguments)
#define MOCK_METHOD_SPEC(name, returnType, params, arguments, specs) \
    TRX_MOCK_METHOD_SPEC(name, returnType, params, arguments, specs)

#define CHECK_CALLED(method, ...)               TRX_CHECK_CALLED(method, ##__VA_ARGS__)
#define CHECK_NOT_CALLED(method, ...)           TRX_CHECK_NOT_CALLED(method, ##__VA_ARGS__)
#define CHECK_CALLED_TIMES(method, times, ...)  TRX_CHECK_CALLED_TIMES(method, times, ##__VA_ARGS__)
#define CHECK_LAST_CALL(method, ...)            TRX_CHECK_LAST_CALL(method, __VA_ARGS__)
#define CHECK_NO_UNSTUBBED_CALLS(method, ...)   TRX_CHECK_NO_UNSTUBBED_CALLS(method, ##__VA_ARGS__)

#endif // TESTRIXA_NO_SHORT_MACROS

#endif // TESTRIXA_MOCK_H
