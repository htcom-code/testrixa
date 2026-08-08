//
//  detail/core.hpp
//  measure test class
//
//  Created by htjulia on 2023/11/22.
//



#ifndef TESTRIXA_DETAIL_CORE_HPP
#define TESTRIXA_DETAIL_CORE_HPP

#include <stdio.h>
#include <chrono>
#include <vector>
#include <fstream>
#include <list>
#include <cmath>
#include <string>
#include <type_traits>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <limits.h>
#include <testrixa/traits.h>
#include <testrixa/detail/options.hpp>
#include <testrixa/detail/memory/allocator.hpp>
#include <testrixa/detail/memory/tracker.hpp>

/*
 terminal color
     1.foreground background
        black        30         40
        red          31         41
        green        32         42
        yellow       33         43
        blue         34         44
        magenta      35         45
        cyan         36         46
        white        37         47

     2.
        reset               0  (everything back to normal)
        bold/bright         1  (often a brighter shade of the same colour)
        underline           4
        blink               5
        inverse             7  (swap foreground and background colours)
        bold/bright off     21
        underline off       24
        inverse off         27
 */

#if defined(DEBUG) || !defined(TEST_TERM)
#   define TRX_RED_BOLD_COLOR       ""
#   define TRX_RED_BLINK_COLOR      ""
#   define TRX_GREEN_BOLD_COLOR     ""
#   define TRX_YELLOW_BOLD_COLOR    ""
#   define TRX_BLUE_BOLD_COLOR      ""
#   define TRX_MAGENTA_BOLD_COLOR   ""
#   define TRX_CYAN_BOLD_COLOR      ""
#   define TRX_WHITE_BOLD_COLOR     ""
#   define TRX_RESET_COLOR          ""
#else
#   define TRX_RED_BOLD_COLOR       "\033[1;31m"
#   define TRX_RED_BLINK_COLOR      "\033[1;5;31m"
#   define TRX_GREEN_BOLD_COLOR     "\033[1;32m"
#   define TRX_YELLOW_BOLD_COLOR    "\033[1;33m"
#   define TRX_BLUE_BOLD_COLOR      "\033[1;34m"
#   define TRX_MAGENTA_BOLD_COLOR   "\033[1;35m"
#   define TRX_CYAN_BOLD_COLOR      "\033[1;36m"
#   define TRX_WHITE_BOLD_COLOR     "\033[1:37m"
#   define TRX_RESET_COLOR          "\033[0m"
#endif


#define TRX_S_CHECK_TRUE            "CHECK_TRUE "
#define TRX_S_CHECK_FALSE           "CHECK_FALSE"

//config const
#define TRX_LOG_SPACE_COUNT         2

// log basic width
#define TRX_LOG_GROUP_NAME_WIDTH    48
#define TRX_LOG_DEPTH_WIDTH         4
#define TRX_LOG_TOTAL_WIDTH         8
#define TRX_LOG_SUCCESS_WIDTH       8
#define TRX_LOG_FAIL_WIDTH          8
#define TRX_LOG_LINE_WIDTH          TRX_LOG_GROUP_NAME_WIDTH + TRX_LOG_DEPTH_WIDTH + TRX_LOG_TOTAL_WIDTH + TRX_LOG_SUCCESS_WIDTH + TRX_LOG_FAIL_WIDTH + 6

// Memory phase columns. The widths add up to TRX_LOG_LINE_WIDTH so the
// separators drawn by dump_line() line up with this table too.
#define TRX_LOG_MEM_NAME_WIDTH      30
#define TRX_LOG_MEM_COUNT_WIDTH     8
#define TRX_LOG_MEM_BYTES_WIDTH     11

// loop count
#define TRX_MAX_LOOP_COUNT          999999
#define TRX_DEFAULT_LOOP_COUNT      100


// log time width
#define TRX_LOG_TIME_DESC_WIDTH     35
#define TRX_LOG_TIME_WIDTH          17
#define TRX_LOG_TIME_SOURCE         29
#define TRX_LOG_TIME_LINE           5
#define TRX_LOG_TIME_LOOP_WIDTH     5
#define TRX_LOG_TIME_LINE_WIDTH     TRX_LOG_TIME_DESC_WIDTH + TRX_LOG_TIME_LOOP_WIDTH + TRX_LOG_TIME_WIDTH * 3 + 6

static_assert((TRX_LOG_TIME_SOURCE + TRX_LOG_TIME_LINE) == TRX_LOG_TIME_WIDTH * 2, "TIME LINE WIDTH MISMATCH");


#define TRX_SIZEOF_POINTER (UINTPTR_MAX / 255 % 255)

static_assert(TRX_SIZEOF_POINTER == sizeof(uintptr_t));

#define TRX_ADDRESS_FORMAT_64    "0x%016x"
#define TRX_ADDRESS_FORMAT_32    "0x%08x"

#if TRX_SIZEOF_POINTER == 8
#   define TRX_ADDRESS_FORMAT TRX_ADDRESS_FORMAT_64
#elif TRX_SIZEOF_POINTER == 4
#   define TRX_ADDRESS_FORMAT TRX_ADDRESS_FORMAT_32
#else
#   define TRX_ADDRESS_FORMAT "0x%x"
#endif


TRX_BEGIN_NAMESPACE

// using type_trait
template <typename _Tp>
constexpr bool is_string_comp_v =
    std::is_same_v<std::remove_const_t<std::remove_reference_t<_Tp>>, std::string> ||
    (std::is_pointer_v<_Tp> && std::is_same_v<std::remove_const_t<std::remove_pointer_t<_Tp>>, char>);
    typedef enum TEST_STATUS_TYPE {
        TST_NONE        = 0,
        TST_BASIC       = 1,
        TST_MEASURE     = 2,
        TST_DONE        = 3,
        TST_MEMORY      = 4,
        TST_STRESS      = 5
    } TEST_STATUS_TYPE;

    typedef std::chrono::time_point<std::chrono::high_resolution_clock> HighResClock;


    template<typename _Tp, std::enable_if_t<std::is_pointer_v<_Tp>>* = nullptr>
    std::string to_hex_string(_Tp S, size_t len) {
        static const char chex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
        std::string ss;
        
        if (S == NULL) return "";
        uint8_t *wrap = (uint8_t *)S;
        
        ss += "h\"";
        for (unsigned i=0; i<len; i++) {
            ss += chex[((0xf0&wrap[i])>>4)]; //.append(chex[((0xf0&wrap[i])>>4)], 1);
            ss += chex[((0x0f&wrap[i])>>0)]; //.append(chex[((0x0f&wrap[i])>>0)], 1);
            if (i+1 != len) ss.append(" ");
        }
        ss += "\"";
        return ss;
    }


    // description string.
    class TestString {
        std::string m_chars;
    public:
        TestString():m_chars("") {}

        virtual ~TestString() {
//            std::cout << "destroy :: " << m_chars << std::endl;
        }
    public:
        explicit TestString(const char* s) { *this += s; }

        TestString(const TestString& s) { m_chars.append(s.m_chars); }
        
        TestString(const std::string& s) { m_chars.append(s); }
        
        TestString(std::nullptr_t&) {
            m_chars = "nullptr";
        }
        
        TestString(std::intptr_t &addr) {
            if (addr == 0) {
                format("0x%02x", addr);
            } else {
                if (TRX_SIZEOF_POINTER == 8 && addr > 0xffffffff) {
                    format(TRX_ADDRESS_FORMAT, addr);
                } else {
                    format(TRX_ADDRESS_FORMAT_32, addr);
                }
            }
        }
        
        template <typename _Tp, std::enable_if_t<std::is_arithmetic_v<_Tp>>* =nullptr >
        TestString(const _Tp num) { *this += std::to_string(num).c_str(); }
        
        template <typename _Tp, std::enable_if_t<std::is_pointer_v<_Tp>>* =nullptr >
        TestString(_Tp pdata) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(pdata);
            if (addr == 0) {
                format("0x%02x", addr);
            } else {
                if (TRX_SIZEOF_POINTER == 8 && addr > 0xffffffff) {
                    format(TRX_ADDRESS_FORMAT, addr);
                } else {
                    format(TRX_ADDRESS_FORMAT_32, addr);
                }
            }
        }

    public:
        size_t length() const { return m_chars.length(); }

        void clear() { m_chars.clear(); }
        
        const std::string::value_type *c_str() const {return m_chars.c_str();}
        
        std::string str() {
            return m_chars;
        }

        template<typename ...Args>
        TestString& appendFormat(const std::string &fmt, Args ... args) {
            const int measured = std::snprintf( nullptr, 0, fmt.c_str(), args ... ) + 1;
            if (measured > 0) {
                const std::size_t needed = static_cast<std::size_t>( measured );
                std::unique_ptr<char[]> buf( new char[ needed ] );
                std::snprintf( buf.get(), needed, fmt.c_str(), args ... );
                m_chars.append(std::string( buf.get(), buf.get() + needed - 1 ));
            }
            return *this;
        }
        
        // The local is `needed`, not `size`. A consumer is entitled to a global
        // named `size` -- checkNamespace.cpp declares one for exactly that
        // reason -- and MSVC /W4 reports a local that hides it (C4459). Under
        // /WX that is their build broken by our header, so public headers here
        // avoid names a consumer plausibly has at namespace scope. The contract
        // check is what tells us when a new one appears.
        template<typename ...Args>
        TestString& format(const std::string &fmt, Args ... args) {
            const int measured = std::snprintf( nullptr, 0, fmt.c_str(), args ... ) + 1;
            if (measured > 0) {
                const std::size_t needed = static_cast<std::size_t>( measured );
                std::unique_ptr<char[]> buf( new char[ needed ] );
                std::snprintf( buf.get(), needed, fmt.c_str(), args ... );
                m_chars.clear();
                m_chars.append(std::string( buf.get(), buf.get() + needed - 1 ));
            }
            return *this;
        }
        
        operator std::string() const {
            return m_chars;
        }
        
        TestString& operator+=(const char* s) {
            m_chars.append(s, strlen(s));
            return *this;
        }

        TestString& operator+=(const TestString& s) {
            m_chars.append(s);
            return *this;
        }

        TestString& operator=(const char* s) {
            m_chars.clear();
            m_chars.append(s, strlen(s));
            return *this;
        }
        
        TestString& operator=(const TestString& s) {
            m_chars.append(s);
            return *this;
        }
        
        TestString operator+(const char *b) {
            TestString result = (*this);
            result += b;
            return result;
        }
        
        TestString operator+(const TestString& b) {
            TestString result = *this;
            result += b;
            return result;
        }
        
        template <typename _Tp, std::enable_if_t<std::is_arithmetic_v<_Tp>>* =nullptr >
        TestString& operator+=(_Tp i) {
            m_chars.append(std::to_string(i));
            return *this;
        }
        
        friend std::ostream& operator<<(std::ostream& stream, const TestString &value) {
            return stream << value.c_str();
        }
    };

    namespace detail {

    // An enum has no operator== with an integer of another type; its underlying
    // value does.
    template <typename X>
    constexpr auto normalized(X value) {
        if constexpr (std::is_enum_v<X>) {
            return static_cast<std::underlying_type_t<X>>(value);
        } else {
            return value;
        }
    }

    // Value equality that survives mixed signedness.
    //
    // `-1 == 4294967295u` is true in C++: the int converts to unsigned and the
    // bit patterns match. For arithmetic that rule is the language; for an
    // assertion it is a wrong answer, and a test framework that answers wrong
    // about equality is worse than no framework.
    //
    // This is std::cmp_equal's rule, written out because the project targets
    // C++17 and cmp_equal arrived in C++20.
    template <typename A, typename B>
    constexpr bool equalValues(A a, B b) {
        if constexpr (std::is_integral_v<A> && std::is_integral_v<B>
                      && std::is_signed_v<A> != std::is_signed_v<B>) {
            if constexpr (std::is_signed_v<A>) {
                // a negative signed value can never equal an unsigned one
                return a < 0 ? false : static_cast<std::make_unsigned_t<A>>(a) == b;
            } else {
                return b < 0 ? false : a == static_cast<std::make_unsigned_t<B>>(b);
            }
        } else {
            // same signedness, or floating point involved -- plain == is right
            return a == b;
        }
    }

    } // namespace detail

    // What the memory phase accumulates per group. Kept as a plain struct so
    // core.hpp does not have to know about memory::Report -- <testrixa/memory.h>
    // fills it in and hands it over.
    struct MemoryTally {
        std::size_t allocations  = 0;
        std::size_t frees        = 0;
        std::size_t leakedBlocks = 0;
        std::size_t leakedBytes  = 0;
        std::size_t peakBytes    = 0;   // largest peak seen, not a sum
        std::size_t largestBlock = 0;
        std::size_t faults       = 0;   // double free + overflow + use-after-free

        void merge(const MemoryTally& other) {
            allocations  += other.allocations;
            frees        += other.frees;
            leakedBlocks += other.leakedBlocks;
            leakedBytes  += other.leakedBytes;
            faults       += other.faults;
            if (other.peakBytes    > peakBytes)    peakBytes    = other.peakBytes;
            if (other.largestBlock > largestBlock) largestBlock = other.largestBlock;
        }
    };

    // 4200 -> "4.1K". Bytes are read by people, and six digits of them are not.
    inline std::string humanBytes(std::size_t bytes) {
        static const char* unit[] = { "B", "K", "M", "G" };
        double value = (double)bytes;
        int step = 0;
        while (value >= 1024.0 && step < 3) { value /= 1024.0; step++; }

        std::ostringstream out;
        if (step == 0) out << bytes << unit[step];
        else           out << std::fixed << std::setprecision(1) << value << unit[step];
        return out.str();
    }

    class GroupInfo {
    private:
        std::string m_name;
        uint32_t m_total;
        uint32_t m_success;
        uint32_t m_fail;
        uint32_t m_measure;

        int      m_depth;
        
        TEST_STATUS_TYPE m_type;
        using logs = std::list<std::string>;
        logs m_logs;

        MemoryTally m_memory;
    public:
        // The root and scratch groups have no depth. Passing -1 to an unsigned
        // parameter said that by accident and made MSVC /W4 rightly complain;
        // naming the value says it on purpose.
        static const unsigned DEPTH_NONE = (unsigned)-1;

        GroupInfo(const std::string &name, unsigned depth, TEST_STATUS_TYPE type=TST_NONE)
        :m_name(name), m_total(0), m_success(0), m_fail(0), m_measure(0), m_depth(depth), m_type(type) {}
        
        ~GroupInfo() {
            if (m_logs.size() > 0) m_logs.clear();
        }
        
    public:
        unsigned add_count(bool success = true) {
            success?m_success++:m_fail++;
            return ++m_total;
        }
        
        unsigned add_measure_count() {
            return m_measure++;
        }

        void add_memory(const MemoryTally& tally) { m_memory.merge(tally); }

        // Raw name and raw log text, for the machine-readable report -- s_name()
        // pads and truncates for a terminal column, which XML has no use for.
        const std::string& name_str() const { return m_name; }
        std::string logs_str() const {
            std::string out;
            for (const std::string& line : m_logs) out += line;
            return out;
        }
        const MemoryTally& memory() const { return m_memory; }
        
        void add_log(const std::string &log) {
            m_logs.push_back(log);
        }
        
        void change_type(TEST_STATUS_TYPE type) {
            if (m_type != type) {
                m_type = type;
                clear();
            }
        }

        void clear() {
            //count reset and log clear;
            m_total = 0;
            m_success = 0;
            m_fail = 0;
            
            m_logs.clear();
        }
        
    public: //property.
        std::string name() const { return m_name; }

        // The column width is a parameter because the memory phase prints a
        // narrower name column; truncating to the basic table's width there
        // overflows the cell and the whole row stops lining up.
        std::string s_name(std::size_t width = TRX_LOG_GROUP_NAME_WIDTH) {
            std::string ret(" ");
            for (int i=0; i< m_depth; i++) {
                ret.append("*");
            }
            if (m_depth > 0) ret.append(" ");

            if (m_name.length() > (width - ret.length())) {
                ret.append(m_name, 0, width - ret.length() - 3);
                ret.append("...");
            } else {
                ret.append(m_name);
            }
            
            return ret;
        }
        
        unsigned fail() const {
            return m_fail;
        }
        
        unsigned success() const {
            return m_success;
        }
        
        unsigned total() const {
            return m_total;
        }
        
        int depth() const {
            return m_depth;
        }
        
        TEST_STATUS_TYPE type() const {
            return m_type;
        }
        
        bool is_logging() {
            return (m_logs.size() > 0 || m_total > 0);
        }
        
        unsigned measure_count() const {
            return m_measure;
        }
        
    public:
        void log_dump(std::ostream& stream) {
            // TST_BASIC is fail log, TST_TIME is measure log.
            if (m_logs.size() > 0) {
                for (logs::iterator it=m_logs.begin(); it != m_logs.end(); ++it) {
                    stream << *it;
                }
            }
        }
        
        void group_dump(std::ostream& stream) {
            if (m_type == TST_MEMORY) {
                stream << std::setfill(' ') << std::left;
                stream << "|" << std::setw(TRX_LOG_MEM_NAME_WIDTH) << s_name(TRX_LOG_MEM_NAME_WIDTH);
                stream << std::right;
                stream << "|" << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << m_memory.allocations;
                stream << "|" << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << m_memory.frees;
                stream << "|" << (m_memory.leakedBlocks ? TRX_RED_BLINK_COLOR : TRX_RED_BOLD_COLOR)
                       << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << m_memory.leakedBlocks << TRX_RESET_COLOR;
                stream << "|" << std::setw(TRX_LOG_MEM_BYTES_WIDTH) << humanBytes(m_memory.peakBytes);
                stream << "|" << std::setw(TRX_LOG_MEM_BYTES_WIDTH) << humanBytes(m_memory.largestBlock);
                stream << "|" << std::endl;
                return;
            }
            if (m_type == TST_BASIC || m_type == TST_STRESS) {
                stream << std::setfill(' ') << std::left;
                stream << "|" << std::setw(TRX_LOG_GROUP_NAME_WIDTH) << s_name(); // group name print.
                stream << std::right;
                if (m_depth < 0) {
                    stream << "|" << std::setw(TRX_LOG_DEPTH_WIDTH)      << "R";
                } else {
                    stream << "|" << std::setw(TRX_LOG_DEPTH_WIDTH)      << m_depth;
                }
                stream << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_TOTAL_WIDTH)   << m_total   << TRX_RESET_COLOR;
                stream << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_SUCCESS_WIDTH) << m_success << TRX_RESET_COLOR;
                stream << "|" << (m_fail?TRX_RED_BLINK_COLOR:TRX_RED_BOLD_COLOR) << std::setw(TRX_LOG_FAIL_WIDTH) << m_fail << TRX_RESET_COLOR;
                stream << "|" << std::endl;
            } else if (m_type == TST_MEASURE) {
                stream << std::setfill(' ') << std::left;
                stream << "|" << std::setw(TRX_LOG_TIME_LINE_WIDTH-2) << s_name(); // group name print.
                stream << "|" << std::endl;
            }
        }

    public: // operator
        template<typename _Tp, std::enable_if_t<std::is_integral_v<_Tp>>* = nullptr>
        GroupInfo& operator << (_Tp v) {
            m_total++;
            v==0?m_fail++:m_success++;
            return *this;
        }
        
        friend std::ostream& operator<<(std::ostream& stream, GroupInfo &value) {
            value.group_dump(stream);
            value.log_dump(stream);
            return stream;
        }

    };

    typedef std::list<GroupInfo *> GroupList;

    typedef std::list<std::unique_ptr<GroupInfo>> UGroupList;


    template<typename _Tp, std::enable_if_t<std::is_same_v<_Tp, UGroupList>>* = nullptr>
    TRX_INLINE std::ostream& operator << (std::ostream& stream, _Tp &value) {
        for (typename _Tp::iterator it=value.begin(); it!=value.end(); ++it) {
            stream << (*(*it));
        }
        return stream;
    }

    template<typename _Tp, std::enable_if_t<std::is_integral_v<_Tp>>* = nullptr>
    TRX_INLINE void operator << (GroupList &value, _Tp bsucces) {
        for (GroupList::iterator it=value.begin(); it!=value.end(); ++it) {
            *(*it) << bsucces;
        }
    }

    template<typename _Tp, std::enable_if_t<std::is_same_v<_Tp, std::string>>* = nullptr>
    TRX_INLINE void operator << (UGroupList &value, const _Tp &log_str) {
        if (value.size() > 0) {
            value.back()->add_log(log_str);
        }
    }


    class TimeResults {
    private: // private member variable
        unsigned short m_hours;
        unsigned short m_minutes;
        unsigned short m_seconds;
        unsigned short m_millis;
        unsigned short m_micros;
        unsigned short m_nanos;
        
    private: // private method.
        void setinit() {
            m_hours=0; m_minutes=0; m_seconds=0; m_millis=0; m_micros=0; m_nanos=0;
        }

    public: // constructor
        TimeResults():m_hours(0), m_minutes(0), m_seconds(0), m_millis(0),
        m_micros(0), m_nanos(0) {}
        
    public:
        std::ostream& timelog(std::ostream& stream, bool bhour=false) {
            stream << std::right << std::setfill('0');
            if (bhour) {
                stream <<       std::setw(2) << m_hours;
                stream << ":"<< std::setw(2) << m_minutes;
            } else {
                stream << std::setw(2) << m_minutes;
            }
            stream << ":"<< std::setw(2) << m_seconds;
            stream << "."<< std::setw(3) << m_millis;
            stream << "."<< std::setw(3) << m_micros;
            stream << "."<< std::setw(3) << m_nanos;
            return stream;
        }

        
        friend std::ostream& operator<<(std::ostream& stream, TimeResults &value) {
            stream << std::right << std::setfill('0');
            stream <<      std::setw(2) << value.Minute();
            stream << ":"<<std::setw(2) << value.Second();
            stream << "."<<std::setw(3) << value.Milli();
            stream << "."<<std::setw(3) << value.Micro();
            stream << "."<<std::setw(3) << value.Nano();
            return stream;
        }

        
        void setTime(unsigned short h,unsigned short m, unsigned short s,
                     unsigned short mil,unsigned short mic, unsigned short na) {
            m_hours     = h;
            m_minutes   = m;
            m_seconds   = s;
            m_millis    = mil;
            m_micros    = mic;
            m_nanos     = na;
        }
        
        
        void setNano(uint64_t nano) {
            setinit();
            // ms µs ns
            if (nano > 0) {
                m_hours   =  (unsigned short)(nano/(60UL*60UL*1000UL*1000UL*1000UL));
                m_minutes = (nano/(60UL*1000UL*1000UL*1000UL))%60UL;
                m_seconds = (nano/(1000UL*1000UL*1000UL))%60UL;
                m_millis  = (nano/(1000UL*1000UL))%1000UL;
                m_micros  = (nano/1000UL)%1000UL;
                m_nanos   =  nano%1000UL;
            }
        }

        void setMicro(uint64_t micro) {
            setinit();
            if (micro > 0) {
                m_hours   = (unsigned short)(micro/(60UL*60UL*1000UL*1000UL));
                m_minutes = (micro/(60UL*1000UL*1000UL))%60UL;
                m_seconds = (micro/(1000UL*1000UL))%60UL;
                m_millis  = (micro/1000UL)%1000UL;
                m_micros  = micro%1000UL;
            }
        }

        void setMill(uint64_t mill) {
            setinit();
            if (mill >0) {
                m_hours   = (unsigned short)(mill/(60UL*60UL*1000UL));
                m_minutes = (mill/(60UL*1000UL))%60UL;
                m_seconds = (mill/1000UL)%60UL;
                m_millis  = mill%1000UL;
            }
        }

        void setSecond(uint64_t sec) {
            setinit();
            if (sec >0) {
                m_hours   = (unsigned short)(sec/(60UL*60UL));
                m_minutes = (sec/60UL)%60UL;
                m_seconds = sec%60;
            }
        }
        
        unsigned short Hour()    {return m_hours;}
        unsigned short Minute()  {return m_minutes;}
        unsigned short Second()  {return m_seconds;}
        unsigned short Milli()   {return m_millis;}
        unsigned short Micro()   {return m_micros;}
        unsigned short Nano()    {return m_nanos;}
    };

    // ------------------------------------------------------------------
    // Machine-readable results
    //
    // The console table is for a person reading a terminal. CI needs something
    // it can parse: GitHub Actions reads JUnit XML and puts failures straight
    // on the pull request, which our text output can never do.
    //
    // Groups are collected as they finish because GroupInfo does not survive
    // the phase -- dump_group_end() clears the list that owns them.
    // ------------------------------------------------------------------
    struct GroupResult {
        std::string suite;      // "<test case>.<phase>"
        std::string name;       // group name
        unsigned    total   = 0;
        unsigned    failed  = 0;
        double      seconds = 0.0;
        std::string detail;     // failure text, already stripped of colour
    };

    class Results {
    public:
        static std::vector<GroupResult>& all() {
            static std::vector<GroupResult> results;
            return results;
        }

        static void add(const GroupResult& result) { all().push_back(result); }

        // Colour codes are for terminals; an XML attribute has no use for them.
        static std::string stripAnsi(const std::string& text) {
            std::string out;
            out.reserve(text.size());
            for (std::size_t i = 0; i < text.size(); ) {
                if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
                    i += 2;
                    while (i < text.size() && text[i] != 'm') i++;
                    if (i < text.size()) i++;
                } else {
                    out += text[i++];
                }
            }
            return out;
        }

        static std::string escapeXml(const std::string& text) {
            std::string out;
            out.reserve(text.size());
            for (char c : text) {
                switch (c) {
                    case '&':  out += "&amp;";  break;
                    case '<':  out += "&lt;";   break;
                    case '>':  out += "&gt;";   break;
                    case '"':  out += "&quot;"; break;
                    case '\'': out += "&apos;"; break;
                    default:
                        // Control characters are not legal in XML 1.0 content.
                        if ((unsigned char)c < 0x20 && c != '\n' && c != '\t') out += ' ';
                        else out += c;
                }
            }
            return out;
        }

        // JUnit XML. One <testsuite> per test case + phase, one <testcase> per
        // group. Failure text carries the same lines the console printed, so a
        // CI annotation says as much as running it locally would.
        static bool writeJUnit(const std::string& path) {
            std::ofstream out(path.c_str());
            if (!out) return false;

            // Group by suite while keeping the order things ran in: a report
            // that reshuffles the run is harder to compare against the console.
            std::vector<std::string> suites;
            for (const GroupResult& r : all()) {
                bool seen = false;
                for (const std::string& s : suites) if (s == r.suite) { seen = true; break; }
                if (!seen) suites.push_back(r.suite);
            }

            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            out << "<testsuites name=\"testrixa\">\n";

            for (const std::string& suite : suites) {
                unsigned tests = 0, failures = 0;
                double seconds = 0.0;
                for (const GroupResult& r : all()) {
                    if (r.suite != suite) continue;
                    tests++;
                    if (r.failed > 0) failures++;
                    seconds += r.seconds;
                }

                out << "  <testsuite name=\"" << escapeXml(suite)
                    << "\" tests=\"" << tests
                    << "\" failures=\"" << failures
                    << "\" time=\"" << std::fixed << std::setprecision(6) << seconds << "\">\n";

                for (const GroupResult& r : all()) {
                    if (r.suite != suite) continue;
                    out << "    <testcase classname=\"" << escapeXml(suite)
                        << "\" name=\"" << escapeXml(r.name)
                        << "\" time=\"" << std::fixed << std::setprecision(6) << r.seconds << "\"";
                    if (r.failed == 0) {
                        out << "/>\n";
                    } else {
                        out << ">\n";
                        out << "      <failure message=\"" << r.failed << " of " << r.total
                            << " checks failed\">" << escapeXml(r.detail) << "</failure>\n";
                        out << "    </testcase>\n";
                    }
                }
                out << "  </testsuite>\n";
            }

            out << "</testsuites>\n";
            return out.good();
        }
    };

    // Where a worker thread's results go while it runs.
    //
    // The bookkeeping in TEST is single-threaded: add_result() touches m_root
    // and the group stack with no lock, and logconsole() builds strings and
    // appends to lists. Calling CHECK from several threads would be a data race
    // in the tool meant to find data races.
    //
    // Locking that path was the obvious fix and the wrong one: it puts a mutex
    // in the hot path of a project that also measures time, and the benchmark
    // numbers would quietly become the mutex's numbers. So each thread
    // accumulates on its own and the totals are merged after the join, where a
    // single thread is doing the merging.
    struct ThreadTally {
        unsigned total   = 0;
        unsigned success = 0;
        unsigned failed  = 0;
        std::vector<std::string> logs;
    };

    inline ThreadTally*& currentThreadTally() {
        static thread_local ThreadTally* tally = nullptr;
        return tally;
    }

    class TEST {
    public:
        // Re-exported so <testrixa/memory.h> can name it without including this
        // header: that one templates on the test case type and only needs the
        // name to resolve where the type is already complete.
        typedef ::testrixa::MemoryTally MemoryTally;

        // <testrixa/thread.h> templates on the test case type so it does not
        // have to include this header; it needs the tally type by name.
        typedef ::testrixa::ThreadTally ThreadTallyType;

        // Points this thread's assertions at its own tally, or back at the
        // ordinary single-threaded path when given null.
        static void bindThreadTally(ThreadTallyType* tally) {
            currentThreadTally() = tally;
        }

    private: //member variable
        TEST_STATUS_TYPE m_status;
        
        GroupInfo  m_root;          // only total count for basic.
        GroupInfo  m_temp;          // only root logs.
        GroupList  m_group_lst;
        std::vector<std::chrono::steady_clock::time_point> m_groupStart;
        UGroupList m_log_lst;

    private: //static variable
#ifndef __cpp_inline_variables
        static TRX_INLINE_VAR bool TestFailStop;
        static TRX_INLINE_VAR bool TestShowDetail;
        static TRX_INLINE_VAR std::string TestCaseSelect;
        static TRX_INLINE_VAR std::string TestFileRoot;
#else
        static TRX_INLINE_VAR bool TestFailStop = true;
        static TRX_INLINE_VAR bool TestShowDetail = false;
        static TRX_INLINE_VAR std::string TestCaseSelect = "";
        static TRX_INLINE_VAR std::string TestFileRoot = "";
#endif

        
    public: //instance method
        TEST* m_next;
        static TRX_INLINE_VAR TEST* list;
        unsigned m_loop;

        TEST()
        :m_status(TST_NONE), m_root("ROOT", GroupInfo::DEPTH_NONE), m_temp("", GroupInfo::DEPTH_NONE), m_next(nullptr), m_loop(TRX_DEFAULT_LOOP_COUNT) {
            m_next = list;
            list = this;
        }

        virtual ~TEST() {
            m_group_lst.clear();
        }
        
    private: //private method.
        void logconsole(bool bcompare, const std::string& lexpr, const std::string& rexpr, const TestString& source, const TestString& target,
                        bool result, std::string filename, int lineno, const std::string &desc) {
            std::stringstream ss;
            
            ss << std::setfill(' ') << "|  ";
            // compare data
            ss << TRX_RED_BOLD_COLOR << "fail"<< TRX_RESET_COLOR << "[" << TRX_YELLOW_BOLD_COLOR;
            ss << std::right << std::setw(5) << m_root.fail() << TRX_RESET_COLOR << "] ";;

            ss << TRX_RED_BOLD_COLOR << (result?TRX_S_CHECK_TRUE:TRX_S_CHECK_FALSE) << TRX_RESET_COLOR;
            if (bcompare) {
                ss << "[expr:(" << lexpr << ", " << rexpr << ") ";
                ss << "value:(" << TRX_YELLOW_BOLD_COLOR << source << TRX_RESET_COLOR << ", ";
                ss << TRX_BLUE_BOLD_COLOR << target << TRX_RESET_COLOR << ")] ";
            } else {
                ss << "[expr:(" << lexpr << ")] ";
            }

            if (desc.length() >0) {
                ss << "(desc: " << desc << ") ";
            }
            ss << filename << ":" << lineno << std::endl;

            if (ThreadTally* tally = currentThreadTally()) {
                tally->logs.push_back(ss.str());
                return;                       // no dumping from a worker thread
            }

            if (m_log_lst.size() == 0) {
                m_temp.add_log(ss.str());
            } else {
                m_log_lst << ss.str();
            }
            
            // log group dump
            if (TEST::testStop()) stop_dump();
        }

    public:
        void add_result(bool success = true) {
            // Inside a concurrent region this thread keeps its own count and
            // the merge happens after the join.
            if (ThreadTally* tally = currentThreadTally()) {
                tally->total++;
                success ? tally->success++ : tally->failed++;
                return;
            }

            //root add count.
            m_root.add_count(success);
            
            if (m_group_lst.size() == 0) {
                // only basic test only
                m_temp.add_count(success);
            } else {
                //group add count
                m_group_lst << success;
            }
        }

        unsigned add_measure_count() {
            if (m_status == TST_MEASURE) {
                return m_root.add_measure_count();
            }
            return 0;
        }

        // <testrixa/memory.h> calls this for every measured scope so the memory
        // phase can show what the code under test actually did, not just
        // whether the assertion held.
        void add_memory(const MemoryTally& tally) {
            m_root.add_memory(tally);
            if (m_group_lst.size() == 0) m_temp.add_memory(tally);
            else                         m_group_lst.back()->add_memory(tally);
        }

    public: // virtual method
        virtual const char* name( ) =0;
        virtual bool runBasic() = 0;
        virtual bool runMeaure() = 0;

        // Not pure: a test case that has no memory phase stays exactly as it was.
        virtual bool runMemory() { return true; }

        // Fixture hooks. Default no-ops, so a test case that wants none is
        // unchanged. Run around every phase rather than once per case: each
        // phase is a separate run of the code under test and deserves the same
        // starting state, which is the whole point of having a fixture.
        //
        // A setUp that returns false fails the phase without running it --
        // testing against a fixture that did not come up produces noise, not
        // information.
        virtual bool setUp()    { return true; }
        virtual bool tearDown() { return true; }

        // TESTCASE_MEMORY overrides this. Without it the runner would frame an
        // empty MEMORY block for every case that has no memory phase.
        virtual bool hasMemory() { return false; }

        // Repetition, opt-in for the same reason the memory phase is: it is
        // slow by construction and a plain run must not pay for it.
        virtual bool runStress() { return true; }
        virtual bool hasStress() { return false; }
        
    private: // dump
        void dump_line(TEST_STATUS_TYPE type, bool only_line=false) {
            std::cout << std::setfill('-');
            if (only_line) {
                if (type == TST_BASIC || type == TST_MEMORY || type == TST_STRESS) {
                    std::cout << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                } else if (type == TST_MEASURE) {
                    std::cout << std::setw(TRX_LOG_TIME_LINE_WIDTH) << "" << std::endl;
                }
            } else {
                if (type == TST_BASIC || type == TST_MEMORY || type == TST_STRESS) {
                    std::cout << "|" << std::setw(TRX_LOG_LINE_WIDTH-2) << "" << "|" << std::endl;
                } else if (type == TST_MEASURE) {
                    std::cout << "|" << std::setw(TRX_LOG_TIME_LINE_WIDTH-2) << "" << "|" << std::endl;
                }
            }
        }

        void stop_dump() {
            dump_group_end();
        }
        
        void dump_basic_start() {
            std::cout << std::endl;
            std::cout << "[" << TRX_YELLOW_BOLD_COLOR << name() <<"::BASIC" << TRX_RESET_COLOR<< "]" << std::endl;
            dump_line(m_status, true);
            
            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << std::setw(TRX_LOG_GROUP_NAME_WIDTH)    << " Group Name"  ;
            std::cout << std::right;
            std::cout << "|" << std::setw(TRX_LOG_DEPTH_WIDTH)         << "Lvl";
            std::cout << "|" << std::setw(TRX_LOG_TOTAL_WIDTH)         << "Total";
            std::cout << "|" << std::setw(TRX_LOG_SUCCESS_WIDTH)       << "Success";
            std::cout << "|" << std::setw(TRX_LOG_FAIL_WIDTH)          << "Fail";
            std::cout << "|" << std::endl;
            dump_line(m_status, true);
        }

        void dump_basic_end() {
            if (m_temp.is_logging()) {
                std::cout << m_temp;
                m_temp.clear();
                dump_line(m_status);
            }
            
            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR  << std::setw(TRX_LOG_GROUP_NAME_WIDTH) << " Total Test case" << TRX_RESET_COLOR;
            std::cout << std::right;
            std::cout << " " << std::setw(TRX_LOG_DEPTH_WIDTH)         << " ";
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_TOTAL_WIDTH)   << m_root.total()   << TRX_RESET_COLOR;
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_SUCCESS_WIDTH) << m_root.success() << TRX_RESET_COLOR;
            std::cout << "|" << (m_root.fail()?TRX_RED_BLINK_COLOR:TRX_RED_BOLD_COLOR) << std::setw(TRX_LOG_FAIL_WIDTH) << m_root.fail() << TRX_RESET_COLOR;
            std::cout << "|" << std::endl;
            dump_line(m_status, true);
            std::cout << std::endl;
        }

        void dump_stress_start() {
            std::cout << std::endl;
            std::cout << "[" << TRX_YELLOW_BOLD_COLOR << name() << "::STRESS" << TRX_RESET_COLOR << "] ";
            std::cout << "repetition; --stress.scale=<percent> shortens every budget" << std::endl;
            dump_line(m_status, true);

            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << std::setw(TRX_LOG_GROUP_NAME_WIDTH)    << " Group Name";
            std::cout << std::right;
            std::cout << "|" << std::setw(TRX_LOG_DEPTH_WIDTH)         << "Lvl";
            std::cout << "|" << std::setw(TRX_LOG_TOTAL_WIDTH)         << "Total";
            std::cout << "|" << std::setw(TRX_LOG_SUCCESS_WIDTH)       << "Success";
            std::cout << "|" << std::setw(TRX_LOG_FAIL_WIDTH)          << "Fail";
            std::cout << "|" << std::endl;
            dump_line(m_status, true);
        }

        void dump_stress_end() { dump_basic_end(); }

        void dump_memory_start() {
            std::cout << std::endl;
            std::cout << "[" << TRX_YELLOW_BOLD_COLOR << name() << "::MEMORY" << TRX_RESET_COLOR << "] ";
            // Say what is and is not watched. "no leaks" that silently means
            // "no leaks in the part we looked at" is how a memory tool lies.
            std::cout << "coverage: C++ new/delete = full | C malloc = only where "
                         "<testrixa/malloc_shim.h> is included | prebuilt libraries = not tracked" << std::endl;
            dump_line(m_status, true);

            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << std::setw(TRX_LOG_MEM_NAME_WIDTH)   << " Group Name";
            std::cout << std::right;
            std::cout << "|" << std::setw(TRX_LOG_MEM_COUNT_WIDTH)  << "Alloc";
            std::cout << "|" << std::setw(TRX_LOG_MEM_COUNT_WIDTH)  << "Free";
            std::cout << "|" << std::setw(TRX_LOG_MEM_COUNT_WIDTH)  << "Leak";
            std::cout << "|" << std::setw(TRX_LOG_MEM_BYTES_WIDTH)  << "Peak";
            std::cout << "|" << std::setw(TRX_LOG_MEM_BYTES_WIDTH)  << "MaxBlock";
            std::cout << "|" << std::endl;
            dump_line(m_status, true);
        }

        void dump_memory_end() {
            if (m_temp.is_logging()) {
                std::cout << m_temp;
                m_temp.clear();
                dump_line(m_status);
            }

            const MemoryTally& total = m_root.memory();

            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_MEM_NAME_WIDTH)
                      << " Total" << TRX_RESET_COLOR;
            std::cout << std::right;
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << total.allocations << TRX_RESET_COLOR;
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << total.frees << TRX_RESET_COLOR;
            std::cout << "|" << (total.leakedBlocks ? TRX_RED_BLINK_COLOR : TRX_RED_BOLD_COLOR)
                      << std::setw(TRX_LOG_MEM_COUNT_WIDTH) << total.leakedBlocks << TRX_RESET_COLOR;
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_MEM_BYTES_WIDTH) << humanBytes(total.peakBytes) << TRX_RESET_COLOR;
            std::cout << "|" << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_MEM_BYTES_WIDTH) << humanBytes(total.largestBlock) << TRX_RESET_COLOR;
            std::cout << "|" << std::endl;
            dump_line(m_status, true);

            // Anything still allocated once the phase is over leaked for real:
            // deliberate-leak tests reclaim their blocks, so what is left is
            // what nobody freed. This is the only place that can say where it
            // came from, which is the question a leak count cannot answer.
            reportLiveAllocations(std::cout);

            // Leaked bytes and faults do not get a column -- they are usually
            // zero, and a line that only appears when something is wrong is
            // easier to notice than a column of zeroes.
            if (total.leakedBytes > 0 || total.faults > 0) {
                std::cout << TRX_RED_BOLD_COLOR;
                if (total.leakedBytes > 0) {
                    std::cout << "  leaked " << humanBytes(total.leakedBytes)
                              << " in " << total.leakedBlocks << " block(s)";
                }
                if (total.faults > 0) {
                    if (total.leakedBytes > 0) std::cout << " | ";
                    else std::cout << "  ";
                    std::cout << total.faults << " fault(s): double free / overflow / use-after-free";
                }
                std::cout << TRX_RESET_COLOR << std::endl;
            }

            std::cout << std::endl;
        }

        void dump_measure_start() {
            std::cout << std::endl;
            std::cout << "[" << TRX_BLUE_BOLD_COLOR << name() <<"::MEASURE" << TRX_RESET_COLOR<<"] ";
            std::cout << "format[mm:ss.zzz.µµµ.nnn], ";
            std::cout << TRX_YELLOW_BOLD_COLOR << "[D]" << TRX_RESET_COLOR << "Default Loop, ";
            std::cout << TRX_RED_BOLD_COLOR    << "[C]" << TRX_RESET_COLOR << "Custom Loop" << std::endl;
            dump_line(m_status, true);

            std::cout << std::left << std::setfill(' ');
            std::cout << "|" << std::setw(TRX_LOG_TIME_DESC_WIDTH)  << " Group Name / Description";
            std::cout << std::right;
            std::cout << "|" << std::setw(TRX_LOG_TIME_LOOP_WIDTH)  << "Loop";
            std::cout << "|" << std::setw(TRX_LOG_TIME_WIDTH)       << "Average";
            if (TEST::TestShowDetail) {
                std::cout << "|" << std::setw(TRX_LOG_TIME_WIDTH)   << "Minimum";
                std::cout << "|" << std::setw(TRX_LOG_TIME_WIDTH)   << "Maximum";
            } else {
                std::cout << "|" << std::setw(TRX_LOG_TIME_SOURCE)   << "Source";
                std::cout << "|" << std::setw(TRX_LOG_TIME_LINE)   << "Line";
            }
            std::cout << "|" << std::endl;
            dump_line(m_status, true);
        }
        
        void dump_measure_end() {
            if (m_temp.is_logging()) {
                std::cout << m_temp;
                m_temp.clear();
                
                dump_line(m_status);
            }
            
            if (m_root.measure_count() == 0) {
                //test case nothing.
                std::cout << "|" << std::left << std::setfill(' ');
                std::cout << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_TIME_LINE_WIDTH - 2) << " Measure Data is not exists.";
                std::cout << TRX_RESET_COLOR << "|" << std::endl;
                dump_line(m_status, true);
            } else {
                std::string ls(" Measure case : ");
                ls.append(std::to_string(m_root.measure_count()));
                
                
                std::cout << "|" << std::left << std::setfill(' ');
                std::cout << TRX_YELLOW_BOLD_COLOR << std::setw(TRX_LOG_TIME_LINE_WIDTH - 2) << ls;
                std::cout << TRX_RESET_COLOR << "|" << std::endl;
                dump_line(m_status, true);
            }
        }
        
        // Anything still allocated when the memory phase ends leaked for real.
        void reportLiveAllocations(std::ostream& out) {
            memory::Tracker& tracker = memory::Tracker::instance();
            const std::size_t live = tracker.liveBlocks();
            if (live == 0) return;

            out << TRX_RED_BOLD_COLOR << "  " << live
                << " allocation(s) still live at end of phase" << TRX_RESET_COLOR << std::endl;
            tracker.reportLeaks(out, 20);
        }

        void dump_group_start() {}
        
        void dump_group_end() {
            // temp log exist.
            if (m_temp.is_logging()) {
                std::cout << m_temp;
                m_temp.clear();
                dump_line(m_status);
            }
            
            if (m_log_lst.size() > 0) {
                std::cout << m_log_lst;
                dump_line(m_status);
                m_log_lst.clear();
            }
        }
        
    public:
        GroupInfo *info() {
            if (m_group_lst.size() > 0) {
                return m_group_lst.back();
            }
            return &m_temp;
        }

    public: //static
        static bool testStop() { return TestFailStop; }
        static void setTestStop(bool stop) { TestFailStop = stop; }
        static void selectTestCase(const std::string &caseTest) { TestCaseSelect = caseTest;}
        static void setRootPath(const std::string& path) { TestFileRoot = path; }
        static std::string RootPath() { return TestFileRoot; }
        static void setTestShowDetail(bool detail) { TestShowDetail = detail;}
        static bool testShowDetail() {return TestShowDetail;}

        // Returns 0 when every test case passed.
        //   > 0  number of failed test cases
        //   -1   loop count out of range (nothing ran)
        //   -2   no test case ran -- either none is registered or the selector
        //        matched nothing. Treated as a failure on purpose: a test binary
        //        that runs nothing must not report success to CI.
        static int testAll(std::string testcase, bool bStop, bool bBasic, bool bTime, unsigned default_loop=100, bool bMemory=false, bool bStress=false) {
            if (default_loop == 0 || default_loop > TRX_MAX_LOOP_COUNT) {
                std::cout << TRX_RED_BOLD_COLOR << " [FAIL] " << TRX_RESET_COLOR;
                std::cout << "loop count(" << TRX_YELLOW_BOLD_COLOR << default_loop << TRX_RESET_COLOR << ") is out of range(";
                std::cout << TRX_YELLOW_BOLD_COLOR << "range : 1 ~ " << TRX_MAX_LOOP_COUNT << TRX_RESET_COLOR << ")" << std::endl;
                return -1;
            }
            
            
            unsigned total = 0;
            unsigned fail  = 0;
            unsigned scnt  = 0;
            unsigned n_run = 0;
            bool bfail = false;
            
            //global option add
            TEST::setTestStop(bStop);
            TEST::selectTestCase(testcase);

            if (testcase.length() > 0) {
                std::cout << std::endl;
                std::cout << std::setfill('=') << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                std::cout << "Test case selected : " << TRX_YELLOW_BOLD_COLOR << testcase << TRX_RESET_COLOR << std::endl;
            }
            
            for (TEST* test=TEST::list; test; test=test->m_next) {
                if (bfail && bStop) {
                    if (test->run_check() > 0) {
                        n_run++;
                        total++;
                    }
                    continue;
                }
                
                switch (test->run(bBasic, bTime, default_loop, bMemory, bStress)) {
                    case 0: //fail
                        total++;
                        fail++;
                        bfail = true;
                        break;
                    case 1: //success
                        total++;
                        scnt++;
                    default:
                        break;
                }
            }
            
            if (total >0) {
//                std::cout << std::setfill('=') << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                std::cout << std::endl;
                if (n_run > 0) {
                    std::cout << "Test case stoped ";
                } else {
                    std::cout << "ALL test case finised ";
                }
                std::cout << "total["   << TRX_YELLOW_BOLD_COLOR << total << TRX_RESET_COLOR << "] ";
                std::cout << "success[" << TRX_YELLOW_BOLD_COLOR << scnt  << TRX_RESET_COLOR << "] ";
                std::cout << "fail["    << TRX_RED_BOLD_COLOR    << fail  << TRX_RESET_COLOR << "]";
                if (n_run >0) {
                    std::cout << " not running[" << TRX_YELLOW_BOLD_COLOR  << n_run  << TRX_RESET_COLOR << "]";
                }
                std::cout << std::endl;
                std::cout << std::setfill('=') << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                std::cout << std::endl;

                return (int)fail;
            } else {
//                std::cout << std::setfill('=') << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                std::cout << std::endl;
                std::cout << TRX_YELLOW_BOLD_COLOR << "[W]" << TRX_RESET_COLOR << TRX_RED_BOLD_COLOR << "Test case None" << TRX_RESET_COLOR << std::endl;
                std::cout << std::setfill('=') << std::setw(TRX_LOG_LINE_WIDTH) << "" << std::endl;
                std::cout << std::endl;

                return -2;
            }
        }

    public:
        void startGroup(const std::string& name) {
            m_log_lst.push_back(std::make_unique<GroupInfo>(name, (unsigned)m_group_lst.size(), m_status));
            m_group_lst.push_back(m_log_lst.back().get());
            m_groupStart.push_back(std::chrono::steady_clock::now());
            dump_group_start();
        }

        // The name is the closing half of a pair the reader writes; the group
        // stack already knows which one is ending.
        void endGroup(const std::string& /*name*/) {
            if (m_status == TST_BASIC || m_status == TST_MEASURE || m_status == TST_MEMORY
                || m_status == TST_STRESS) {
                if (m_group_lst.size() > 0) {
                    GroupInfo *in = m_group_lst.back();

                    // Capture before dumping: dump_group_end() clears the list
                    // that owns this GroupInfo.
                    GroupResult result;
                    result.suite   = std::string(this->name()) + "." + phaseName();
                    result.name    = in->name_str();
                    result.total   = in->total();
                    result.failed  = in->fail();
                    if (!m_groupStart.empty()) {
                        const auto elapsed = std::chrono::steady_clock::now() - m_groupStart.back();
                        result.seconds = std::chrono::duration<double>(elapsed).count();
                        m_groupStart.pop_back();
                    }
                    if (in->fail() > 0) {
                        result.detail = Results::stripAnsi(in->logs_str());
                    }
                    Results::add(result);

                    if (in->depth() == 0 && m_group_lst.size() == 1) {
                        dump_group_end();
                    }
                    m_group_lst.pop_back();
                }
            }
        }

        const char* phaseName() const {
            switch (m_status) {
                case TST_BASIC:   return "BASIC";
                case TST_MEASURE: return "MEASURE";
                case TST_MEMORY:  return "MEMORY";
                case TST_STRESS:  return "STRESS";
                default:          return "NONE";
            }
        }

        int run(bool run_baisc=true, bool run_measure=true, unsigned loop=TRX_DEFAULT_LOOP_COUNT,
                bool run_memory=false, bool run_stress=false) {
            // 1:success 0:fail -1:pass
            m_loop    = loop;
            int ret = 1;

            if ( (TestCaseSelect.empty() == false && strCaseCmp(TestCaseSelect.c_str(), name()) == 0) || (TestCaseSelect.empty() == true) ) {
                // basic test start.
                if (run_baisc) {
                    m_status = TST_BASIC;
                    m_root.change_type(m_status);
                    m_temp.change_type(m_status);
                    
                    dump_basic_start();
                    if (!enterFixture()) {
                        dump_basic_end();
                        m_status = TST_DONE;
                        return 0;
                    }
                    if (!runBasic()) {
                        if ( TEST::testStop()) {
                            leaveFixture();
                            dump_basic_end();
                            m_status = TST_DONE;
                            return 0;
                        }
                        ret = 0;
                    }
                    if (!leaveFixture()) ret = 0;
                    dump_basic_end();
                }
                
                // maesure test start.
                if (run_measure && loop>0) {
                    m_status = TST_MEASURE;
                    m_root.change_type(m_status);
                    m_temp.change_type(m_status);

                    dump_measure_start();
                    if (!enterFixture()) {
                        dump_measure_end();
                        m_status = TST_DONE;
                        return 0;
                    }
                    if (!runMeaure()) {
                        if (TEST::testStop()) {
                            leaveFixture();
                            dump_measure_end();
                            m_status = TST_DONE;
                            return 0;
                        }
                        ret = 0;
                    }
                    if (!leaveFixture()) ret = 0;
                    dump_measure_end();
                }

                // stress test start.
                if (run_stress && hasStress()) {
                    m_status = TST_STRESS;
                    m_root.change_type(m_status);
                    m_temp.change_type(m_status);

                    dump_stress_start();
                    if (!enterFixture()) {
                        dump_stress_end();
                        m_status = TST_DONE;
                        return 0;
                    }
                    if (!runStress()) {
                        if (TEST::testStop()) {
                            leaveFixture();
                            dump_stress_end();
                            m_status = TST_DONE;
                            return 0;
                        }
                        ret = 0;
                    }
                    if (!leaveFixture()) ret = 0;
                    dump_stress_end();
                }

                // memory test start.
                if (run_memory && hasMemory()) {
                    m_status = TST_MEMORY;
                    m_root.change_type(m_status);
                    m_temp.change_type(m_status);

                    dump_memory_start();
                    if (!enterFixture()) {
                        dump_memory_end();
                        m_status = TST_DONE;
                        return 0;
                    }
                    if (!runMemory()) {
                        if (TEST::testStop()) {
                            leaveFixture();
                            dump_memory_end();
                            m_status = TST_DONE;
                            return 0;
                        }
                        ret = 0;
                    }
                    if (!leaveFixture()) ret = 0;
                    dump_memory_end();
                }
            } else {
                return -1;
            }

            m_status = TST_DONE;
            return ret;
        }

        // A fixture failure is reported through the ordinary counters so it
        // shows up in the table like any other failure, rather than only as a
        // line of text somebody has to notice.
        bool enterFixture() {
            if (setUp()) return true;
            add_result(false);
            m_temp.add_log(std::string("|  ") + TRX_RED_BOLD_COLOR + "fail" + TRX_RESET_COLOR
                           + "[setUp] fixture did not come up; phase skipped\n");
            return false;
        }

        bool leaveFixture() {
            if (tearDown()) return true;
            add_result(false);
            m_temp.add_log(std::string("|  ") + TRX_RED_BOLD_COLOR + "fail" + TRX_RESET_COLOR
                           + "[tearDown] fixture did not come down\n");
            return false;
        }

        // Folds a worker's results into the current group, on the main thread.
        void mergeThreadTally(const ThreadTally& tally) {
            for (unsigned i = 0; i < tally.success; ++i) add_result(true);
            for (unsigned i = 0; i < tally.failed;  ++i) add_result(false);
            for (const std::string& line : tally.logs) {
                if (m_group_lst.size() == 0) m_temp.add_log(line);
                else                         m_log_lst << line;
            }
        }

        int run_check() {
            if ( (TestCaseSelect.empty() == false && strCaseCmp(TestCaseSelect.c_str(), name()) == 0) || (TestCaseSelect.empty() == true) ) {
                return 1;
            }
            return 0;
        }
        
        bool result() {
            return (m_root.fail()==0);
        }
        
        template <typename T>
        size_t print_stream_desc(std::stringstream &ss, T&& arg) {
            ss << arg;
            return ss.str().length();
        }

        template<typename T, typename... Args>
        size_t print_stream_desc(std::stringstream &ss, T&& arg, Args&&... args) {
            ss << arg;
            return print_stream_desc(ss, args...);
        }
        
        size_t print_stream_desc(std::stringstream &ss) {
            return ss.str().length();
        }
        
        template<typename... Args>
        bool fail(const std::string& msg, bool bcheck, const char* filename, int lineno, Args&&... args) {
            add_result(false);
            
            std::stringstream ss;
            print_stream_desc(ss, args...);
            logconsole(false, msg, "", "", "", bcheck, filename, lineno, ss.str());
            return false;
        }

        
        // memory compare
        template<
            typename S, std::enable_if_t<std::is_pointer_v<S> >* = nullptr,
            typename T, std::enable_if_t<std::is_pointer_v<T> >* = nullptr,
            typename... Args
        >
        bool compare_mem(const std::string& lvalue, const std::string& rvalue, S _source, T _target, size_t size,
                         bool expected, const char* filename, int line, Args&&... args) {
            if ((std::memcmp(_source, _target, size) == 0) != expected) {
                add_result(false);

                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, to_hex_string(_source, size), to_hex_string(_target, size),
                           expected, filename, line, ss.str());
                return !expected;
            }
            
            add_result();
            return expected;
        }

        // only use (NULL nullptr, 0 nullptr) true. else false.
        template <typename S, typename T, typename... Args, std::enable_if_t<std::is_null_pointer_v<T> && std::is_integral_v<S>>* = nullptr>
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {

            if ((_source == 0) != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, std::to_string(_source), TestString(_target),
                           expected, filename, line, ss.str());
                
                return !expected;
            }
            return expected;
        }

        // only use (0 nullptr, NULL nullptr) true. else false.
        template <typename S, typename T, typename... Args, std::enable_if_t<std::is_null_pointer_v<S> && std::is_integral_v<T>>* = nullptr>
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {

            if ((_target == 0) != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, TestString(_source), TestString(_target),
                           expected, filename, line, ss.str());
                
                return !expected;
            }
            return expected;
        }

        // only use (nullptr, nullptr)
        template <typename T, typename S, typename... Args, std::enable_if_t<std::is_null_pointer_v<S> && std::is_null_pointer_v<T>>* = nullptr>
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {
            if ((_target == _source) != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, TestString(_source), TestString(_target),
                           expected, filename, line, ss.str());
                
                return !expected;
            }
            return expected;
        }
        
        // only use compare pointer, NULL
        template <typename S, typename T, typename... Args, std::enable_if_t<(std::is_pointer_v<T> && std::is_integral_v<S>) || (std::is_pointer_v<S> && std::is_integral_v<T>)>* = nullptr>
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {
            if (((_source == 0) && (_target == 0)) != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);

                logconsole(true, lvalue, rvalue, TestString(_source), TestString(_target),
                           expected, filename, line, ss.str());
                
                return !expected;
            }
            return expected;
        }

        // nullptr, pointer
        template <typename S, typename T, typename... Args,
            std::enable_if_t<(std::is_pointer_v<T> && std::is_null_pointer_v<S>) || (std::is_pointer_v<S> && std::is_null_pointer_v<T>)>* = nullptr>
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {
            if ((_target == _source) != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, TestString(_source), TestString(_target),
                           expected, filename, line, ss.str());
                
                return !expected;
            }
            return expected;
        }

        // arithmetic, enum compare
        template <
            typename S, std::enable_if_t< std::is_arithmetic_v<S> || std::is_enum_v<S> >* = nullptr,
            typename T, std::enable_if_t< std::is_arithmetic_v<T> || std::is_enum_v<T> >* = nullptr,
            typename... Args
        >
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {
            const bool same = detail::equalValues(detail::normalized(_source),
                                                  detail::normalized(_target));
            if (same != expected) {
                add_result(false);
                
                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                // Normalised here too: std::to_string has no overload for a
                // scoped enum, and this branch is compiled whether or not the
                // check fails.
                logconsole(true, lvalue, rvalue,
                           TestString(std::to_string(detail::normalized(_source))),
                           TestString(std::to_string(detail::normalized(_target))),
                           expected, filename, line, ss.str());
                return !expected;
            }
            add_result();
            return expected;
        }
        
        //string comapre
        template <typename S, typename T, typename... Args,
            std::enable_if_t<is_string_comp_v<S>>* = nullptr,
            std::enable_if_t<is_string_comp_v<T>>* = nullptr
        >
        bool compare(const std::string& lvalue, const std::string& rvalue, S _source, T _target,
                     bool expected, const char* filename, int line, Args&&... args) {
            std::string SS(_source);
            std::string ST(_target);
            
            if ((SS == ST) != expected) {
                add_result(false);

                std::stringstream ss;
                print_stream_desc(ss, args...);
                
                logconsole(true, lvalue, rvalue, "\"" + SS + "\"" , "\"" + ST + "\"", expected, filename, line, ss.str());
                return !expected;
            }
            
            add_result();
            return expected;
        }
        
        static void Initialization() {
#ifndef __cpp_inline_variables
            static bool g_run = false;
            if (!g_run) {
                std::cout << "Initialization is ONLY ONE";
                TEST::TestFailStop   = true;
                TEST::TestShowDetail = false;
                TEST::TestCaseSelect = "";
                TEST::TestFileRoot   = "";
                g_run = true;
            }
#endif
        }
        
        friend class BasicMeasure;
    }; //class TEST


//    bool TEST::is_static = false;


    class BasicMeasure {
    private:
        GroupInfo *m_info;        // GroupInfo.
        unsigned m_loop;
        bool m_loopCustom;
        uint64_t m_nano;
        uint64_t m_min;
        uint64_t m_max;
        bool m_first;
    public: //construct destructor method
        BasicMeasure(unsigned loop, bool loop_custom, GroupInfo *group)
        :m_info(group), m_loop(loop), m_loopCustom(loop_custom), m_nano(0), m_min(0), m_max(0), m_first(true) {}

        virtual ~BasicMeasure(){};
        
    public:
        void testDone(std::chrono::duration<double> elaspedTime) {
            uint64_t val = std::chrono::duration_cast<std::chrono::nanoseconds>(elaspedTime).count();
            
            if (m_first) {
                m_min = val;
                m_max = val;
                m_nano = val;
                m_first = false;
            } else {
                if (val < m_min) m_min = val;
                if (val > m_max) m_max = val;
                m_nano += val;
            }
        }
        
        unsigned loopcount() { return m_loop; }
        
        GroupInfo *info() { return m_info;}
            
        void log_time_result() {
            uint64_t nano=m_nano;
            bool detail = TEST::testShowDetail();

            if (m_nano >0) { nano = (uint64_t)std::llround((double)m_nano / m_loop); }

            TimeResults m_times;
            m_times.setNano(nano);
            
            std::string fname(name());
            if (fname.length()>=TRX_LOG_TIME_DESC_WIDTH-4) {
                fname = fname.substr(0, TRX_LOG_TIME_DESC_WIDTH - 8).append("...");
            }
            
            std::string sloop(std::to_string(m_loop));
            if (sloop.length() > TRX_LOG_TIME_LOOP_WIDTH) {
                sloop = std::to_string(m_loop/1000);
                sloop.append("K");
                
            }
            
            
            std::stringstream ss;
            ss << std::setfill(' ') << std::left;
            ss << "| " << (m_loopCustom?TRX_RED_BOLD_COLOR:TRX_YELLOW_BOLD_COLOR) << (m_loopCustom?"[C]":"[D]") << TRX_RESET_COLOR;
            ss << TRX_BLUE_BOLD_COLOR << std::setw(TRX_LOG_TIME_DESC_WIDTH-4) << std::left << fname << TRX_RESET_COLOR; // function name or desc print
            
            ss << std::right;
            ss << "|" << std::setw(TRX_LOG_TIME_LOOP_WIDTH) << sloop;
            ss << std::left;
            
            // average
            ss << "|" << TRX_CYAN_BOLD_COLOR << m_times << TRX_RESET_COLOR;

            if (detail) {
                // minimum
                m_times.setNano(m_min);
                ss << "|" << TRX_CYAN_BOLD_COLOR << m_times << TRX_RESET_COLOR;

                // maximum
                m_times.setNano(m_max);
                ss << "|" << TRX_CYAN_BOLD_COLOR << m_times << TRX_RESET_COLOR;
            } else {
                // Not `fname` again: that name already holds the description
                // above, and MSVC /W4 rejects the shadowing (C4456). Two
                // different things under one name in one function is worth
                // renaming regardless of who complains.
                std::string source(filename());
                if (source.length() > TRX_LOG_TIME_SOURCE) {
                    source = source.substr(0, TRX_LOG_TIME_SOURCE - 3).append("...");
                }
                
                // file name
                ss << std::setfill(' ');
                ss << "|" << TRX_CYAN_BOLD_COLOR;
                ss << std::setw(TRX_LOG_TIME_SOURCE) << source << TRX_RESET_COLOR;
                
                // line nomber
                ss << "|" << TRX_CYAN_BOLD_COLOR;
                ss << std::right << std::setw(TRX_LOG_TIME_LINE) << line() << TRX_RESET_COLOR;
            }
            ss <<"|" << std::endl;
            
            if (m_info) {
                m_info->add_log(ss.str());
            } else {
                std::cout << ss.str();
            }
        }

    public: //virtual method
        virtual const char *name()=0;
        virtual const char *filename()=0;
        virtual unsigned line()=0;
        
        friend class TimeTest;
    };


    class BenchmarkMeasure {
    private: //member variable
        void *m_tool;
        HighResClock m_start;
    public:
        BenchmarkMeasure(void *tool)
        :m_tool(tool), m_start(std::chrono::high_resolution_clock::now()) {}
        
        
        virtual ~BenchmarkMeasure() {
            HighResClock stop = std::chrono::high_resolution_clock::now();
            if (m_tool) {
                static_cast<BasicMeasure *>(m_tool)->testDone(stop-m_start);
            }
        }
    };

    class TimeTest : public BasicMeasure {
    private:
        unsigned m_index;
        std::string m_desc;         // description
        std::string m_fstring;      // filename string
        unsigned m_lineno;          // line number
    public:
        TimeTest(unsigned index, GroupInfo *info, unsigned loopCount, std::string desc ="",
                 std::string methodName="", const char *filename="-", unsigned line=0, bool loop_custom=false)
        :BasicMeasure(loopCount, loop_custom, info), m_index(index), m_desc(methodName),
            m_fstring(filename), m_lineno(line)
        {
            if (methodName.substr(0,1) == "&") {
                m_desc = methodName.substr(1, methodName.length()-1);
            }

            if (desc.length() > 0 && std::strcmp(methodName.c_str(), desc.c_str()) != 0) {
                m_desc = desc;
            }
        }
    public: //virtual override
        const char *name() override final { return m_desc.c_str();}
        const char *filename() override final {return m_fstring.c_str();}
        unsigned line() override final {return m_lineno;}
    public:
        unsigned index() const {
            return m_index;
        }

    public: //measure methods
        //non void return memeber function
        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type /*return type*/
        measure(R (C::*fn)(Args...), C& obj, Args&&... args) {
            //remove const because const must defined inialized value.
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            
            log_time_result();
            return std::make_tuple(true, ret);
        }

        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type /*return type*/
        measure(R (C::*fn)(Args...) const, C& obj, Args&&... args) {
            //remove const because const must defined inialized value.
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }

        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type /*return type*/
        measure(R (C::*fn)(Args...) const noexcept, C& obj, Args&&... args) {
            //remove const because const must defined inialized value.
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }

        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type /*return type*/
        measure(R (C::*fn)(Args...) noexcept, C& obj, Args&&... args) {
            //remove const because const must defined inialized value.
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }
        
        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type /*return type*/
        measure(R (C::*fn)(Args...) volatile, C& obj, Args&&... args) {
            //remove const because const must defined inialized value.
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }

        template<typename R, class C, typename... Args>
        typename std::enable_if<!std::is_same_v<R, void>, std::tuple<bool, R>>::type
        measure(R (C::*fn)(Args...) volatile const, C& obj, Args&&... args) {
            typename std::remove_const<R>::type ret;
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }

        
        //void return memeber function
        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...), C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }

        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...) const, C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);                        //do not move this code.
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }

        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...) const noexcept, C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);                        //do not move this code.
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }

        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...) noexcept, C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);                        //do not move this code.
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }

        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...) volatile, C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }

        template<class C, typename... Args>
        std::tuple<bool> measure(void (C::*fn)(Args...) volatile const, C& obj, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                (obj.*fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }
        
        template<typename Fn, typename... Args>
//#if TRX_CPP_VER < 201703L
#ifndef __cpp_lib_is_invocable //result_of deprecated 2017 invoke_of 2017 higher
        typename std::enable_if<!std::is_void<typename std::result_of<Fn&&(Args...)>::type>::value,
        std::tuple<bool, typename std::result_of<Fn&&(Args...)>::type>>::type
#else
        typename std::enable_if<!std::is_void_v<typename std::invoke_result<Fn&&, Args...>::type>,
        std::tuple<bool, typename std::invoke_result<Fn&&, Args&&...>::type>>::type
#endif
        measure(Fn&& fn, Args&&... args) {
#if TRX_CPP_VER < 201700L
            typename std::remove_const<typename std::result_of<Fn&&(Args...)>::type>::type ret;
#else
            typename std::remove_const<typename std::invoke_result<Fn&&, Args...>::type>::type ret;
#endif
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                ret = std::forward<Fn>(fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true, ret);
        }

        template<typename Fn, typename... Args>
#ifndef __cpp_lib_is_invocable
        typename std::enable_if<std::is_void<typename std::result_of<Fn&&(Args...)>::type>::value,
            std::tuple<bool> >::type
#else
        typename std::enable_if<std::is_void_v<typename std::invoke_result<Fn&&, Args&&...>::type>,
            std::tuple<bool> >::type
#endif
        measure(Fn&& fn, Args&&... args) {
            for (unsigned loop=0; loop<loopcount(); loop++) {
                BenchmarkMeasure time(this);
                std::forward<Fn>(fn)(std::forward<Args>(args)...);
            }
            log_time_result();
            return std::make_tuple(true);
        }
    };


// class macro
#define INTERNAL_TESTCASE_BEGIN(testcase)                                       \
    using namespace testrixa;                                                \
                                                                                \
    class cls_##testcase : public TEST {                                        \
    public:                                                                     \
        virtual const char* name() override {return #testcase;}                 \

#define BASIC_ATTR(testcase, attr)                                              \
        virtual bool runBasic() override attr
    
#define MEASURE_ATTR(testcase, attr)                                            \
        virtual bool runMeaure() override attr

#define MEMORY_ATTR(testcase, attr)                                             \
        virtual bool hasMemory() override {return true;}                        \
        virtual bool runMemory() override attr

#define STRESS_ATTR(testcase, attr)                                             \
        virtual bool hasStress() override {return true;}                        \
        virtual bool runStress() override attr

#define SETUP_ATTR(testcase, attr)                                              \
        virtual bool setUp() override attr

#define TEARDOWN_ATTR(testcase, attr)                                           \
        virtual bool tearDown() override attr

#define INTERNAL_TESTCASE_END(testcase)                                         \
    };                                                                          \
    static cls_##testcase cls_##testcase##_instance;                            \



#define CHECK_IMP(expr, bcheck, ...)                                            \
    do {                                                                        \
        if (!(!(expr) ^ bcheck)) {                                              \
            this->fail(#expr, bcheck, __FILE_NAME__, __LINE__, ##__VA_ARGS__);  \
            if (TEST::testStop()) return false;                                 \
        } else {add_result(true); }                                             \
    } while(0)

#define CHECK_SAME_IMPL(source, target, expected, ...)                          \
    do {                                                                        \
        if ((this->compare(#source, #target, source, target,                    \
            expected, __FILE_NAME__, __LINE__, ##__VA_ARGS__) != expected)      \
            && TEST::testStop()) return false;                                  \
    } while(0)


#define CHECK_MEM_SAME_IMPL(source, target, size, expected, ...)                \
    do {                                                                        \
        if ((this->compare_mem(#source, #target, source, target, size,          \
            expected, __FILE_NAME__, __LINE__, ##__VA_ARGS__) != expected)      \
            && TEST::testStop()) return false;                                  \
    } while(0)

#define CHECK_REF_IMPL(source, target, expected, ...)                           \
do {                                                                            \
    if ((this->compare_mem2(#source, #target, source, target,                   \
        expected, __FILE_NAME__, __LINE__, ##__VA_ARGS__) != expected)          \
        && TEST::testStop()) return false;                                      \
} while(0)


// test case and group.
#define INTERNAL_TESTCASE_BASIC(testcase) BASIC_ATTR(testcase,)
#define INTERNAL_TESTCASE_MEASURE(testcase) MEASURE_ATTR(testcase,)
#define INTERNAL_TESTCASE_MEMORY(testcase) MEMORY_ATTR(testcase,)
#define INTERNAL_TESTCASE_STRESS(testcase) STRESS_ATTR(testcase,)
#define INTERNAL_TESTCASE_SETUP(testcase) SETUP_ATTR(testcase,)
#define INTERNAL_TESTCASE_TEARDOWN(testcase) TEARDOWN_ATTR(testcase,)
#define INTERNAL_TESTCASE_GROUP_START(groupname)                                \
    {                                                                           \
        startGroup(#groupname);                                                 \

#define INTERNAL_TESTCASE_GROUP_END(groupname)                                  \
        endGroup(#groupname);                                                   \
    }

#define INTERNAL_TESTCASE_RETURN return result();



// basic time measure
#define INTERNAL_CHECK_TIME_FORCE(desc, times, func, ins_obj, ...) \
    TimeTest(add_measure_count(), info(), times, desc, #func, __FILE_NAME__, __LINE__, m_loop != (unsigned)times).measure(func, ins_obj, ##__VA_ARGS__)

#define INTERNAL_CHECK_TIME_FUNCTION_FORCE(desc, times, func, ...) \
    TimeTest(add_measure_count(), info(), times, desc, #func, __FILE_NAME__, __LINE__, m_loop != (unsigned)times).measure(func, ##__VA_ARGS__)


TRX_END_NAMESPACE


#ifdef TEST_RUN_TERM

using namespace testrixa;

void Usage(std::string &pname) {
    std::cout << TRX_YELLOW_BOLD_COLOR<< "Usage: " << TRX_RESET_COLOR << std::endl;
    std::cout << "  " << pname << " [test case] [options]" << std::endl;
    std::cout << "  " << pname << " -l[--list]    -- Test case list" << std::endl;
    std::cout << "  " << pname << " -h[--help]    -- Help command" << std::endl;
    std::cout << "  " << pname << " -s[--no_stop] -- Test don't stop on occure fail (default off)" << std::endl;
    std::cout << "  " << pname << " -t[--times]=N -- Test time measure is " << TRX_DEFAULT_LOOP_COUNT << " times (Range 1 ~ " << TRX_MAX_LOOP_COUNT << ")" << std::endl;
    std::cout << "  " << pname << " -d[--detail]  -- More defail information" << std::endl;
    std::cout << "  " << pname << " -b[--basic]   -- Only Basic test (default off)" << std::endl;
    std::cout << "  " << pname << " -m[--measure] -- Only Measure test (default off)" << std::endl;
    std::cout << "  " << pname << " --mem         -- Only Memory test (= --only=memory)" << std::endl;
    std::cout << "  " << pname << " --stress      -- Only Stress test (= --only=stress)" << std::endl;
    std::cout << "  " << pname << " --only=<phases> -- Run only these phases (basic,measure,memory)" << std::endl;
    std::cout << "  " << pname << " --skip=<phases> -- Skip these phases" << std::endl;
    std::cout << "  " << pname << " --report-junit=<path> -- Write JUnit XML for CI" << std::endl;

    // Whatever registered itself. Components own their own help text; main()
    // does not know their names.
    testrixa::options::describe(std::cout);

    std::cout << std::endl;
    std::cout << TRX_YELLOW_BOLD_COLOR << "Examples: " << TRX_RESET_COLOR << std::endl;
    std::cout << "  " << pname << "       -- default all test case and measure " << TRX_DEFAULT_LOOP_COUNT << "times" << std::endl;
    std::cout << "  " << pname << " -l    -- display test case list" << std::endl;
    std::cout << "  " << pname << " -h    -- display help" << std::endl;
    std::cout << "  " << pname << " -t=30 -- all test case and measure 30 times" << std::endl;
    std::cout << "  " << pname << " -s    -- all test case and no stop" << std::endl;
    std::cout << "  " << pname << " -d    -- display time average, min, max" << std::endl;
    std::cout << "  " << pname << " -b    -- only basic test run" << std::endl;
    std::cout << "  " << pname << " -m    -- only measure test run" << std::endl;
}

// Phase selection. -b / -m are the historical spellings of --only=.
enum : unsigned {
    PHASE_BASIC   = 1u << 0,
    PHASE_MEASURE = 1u << 1,
    PHASE_MEMORY  = 1u << 2,
    PHASE_STRESS  = 1u << 3,
    // The memory phase is opt-in: it installs an allocator and costs real time,
    // so a plain run must not pay for it.
    PHASE_DEFAULT = PHASE_BASIC | PHASE_MEASURE,
    PHASE_ALL     = PHASE_BASIC | PHASE_MEASURE | PHASE_MEMORY | PHASE_STRESS
};

inline void ArgumentError(const std::string& message) {
    std::cout << TRX_RED_BOLD_COLOR << message << TRX_RESET_COLOR << std::endl;
}

// Turns "basic,measure" into a mask. Returns false on an unknown name.
inline bool ParsePhases(const std::string& text, unsigned& mask, std::string& unknown) {
    mask = 0;
    std::string::size_type from = 0;
    while (from <= text.size()) {
        const std::string::size_type comma = text.find(',', from);
        const std::string name = (comma == std::string::npos)
                               ? text.substr(from)
                               : text.substr(from, comma - from);
        if (name == "basic")        mask |= PHASE_BASIC;
        else if (name == "measure") mask |= PHASE_MEASURE;
        else if (name == "memory")  mask |= PHASE_MEMORY;
        else if (name == "stress")  mask |= PHASE_STRESS;
        else if (!name.empty())   { unknown = name; return false; }

        if (comma == std::string::npos) break;
        from = comma + 1;
    }
    return true;
}

void Listup(std::string &pname) {
    std::cout << "Test case list: " << std::endl;
    for (TEST* test=TEST::list; test; test=test->m_next) {
        std::cout  << "  " <<TRX_YELLOW_BOLD_COLOR << std::setfill(' ') << std::setw(30) << std::left << test->name() << TRX_RESET_COLOR << ": " << pname << " " << test->name() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    bool stop = true;
    bool help = false;
    bool wantList = false;
    bool b_basic_only = false;
    bool b_time_only = false;
    unsigned phases = PHASE_DEFAULT;
    unsigned loop = TRX_DEFAULT_LOOP_COUNT;

    TEST::Initialization();

    std::string pname = "";
    std::string command = "";
    std::string junitPath = "";

#ifdef DEBUG_LOG
    std::cout << argv[0] << std::endl;
#endif
    TEST::setRootPath("./");

    if (argc > 0) {
        pname.append(argv[0]);
    }

    // One uniform scan. Unlike the prototype's ladder there is no positional
    // special-casing: -l and -h work anywhere, the test case name works
    // anywhere, and anything unrecognised is an error rather than silence.
    for (int i = 1; i < argc; i++) {
        const std::string arg(argv[i]);
#ifdef DEBUG_LOG
        printf("arg [%d] ==> %s\n", i, argv[i]);
#endif

        // Component options: --<group>.<key>[=<value>], resolved by the registry.
        std::string message;
        const options::Dispatch verdict = options::dispatch(arg, message);
        if (verdict == options::Dispatch::Applied) continue;
        if (verdict == options::Dispatch::Failed) {
            ArgumentError(message);
            return 1;
        }

        if (arg.compare(0, 15, "--report-junit=") == 0) {
            junitPath = arg.substr(15);
            if (junitPath.empty()) {
                ArgumentError(arg + ": needs a path");
                return 1;
            }
            continue;
        }
        if (arg == "--mem")                    { phases = PHASE_MEMORY; continue; }
        if (arg == "--stress")                 { phases = PHASE_STRESS; continue; }
        if (arg == "-l" || arg == "--list")    { wantList = true;  continue; }
        if (arg == "-h" || arg == "--help")    { help = true;  continue; }
        if (arg == "-s" || arg == "--no_stop") { stop = false; continue; }
        if (arg == "-d" || arg == "--detail")  { TEST::setTestShowDetail(true); continue; }

        if (arg == "-m" || arg == "--measure") {
            if (b_basic_only) {
                ArgumentError("Basic Test Option selected. can't use -b -m together");
                return 1;
            }
            b_time_only = true;
            phases = PHASE_MEASURE;
            continue;
        }

        if (arg == "-b" || arg == "--basic") {
            if (b_time_only) {
                ArgumentError("Measure Test Option selected. can't use -b -m together");
                return 1;
            }
            b_basic_only = true;
            phases = PHASE_BASIC;
            continue;
        }

        // --only= / --skip=
        if (arg.compare(0, 7, "--only=") == 0 || arg.compare(0, 7, "--skip=") == 0) {
            const bool skipping = (arg.compare(0, 7, "--skip=") == 0);
            unsigned mask = 0;
            std::string unknown;
            if (!ParsePhases(arg.substr(7), mask, unknown)) {
                ArgumentError(arg + ": unknown phase '" + unknown + "' (basic|measure|memory|stress)");
                return 1;
            }
            if (mask == 0) {
                ArgumentError(arg + ": needs at least one phase (basic|measure|memory|stress)");
                return 1;
            }
            phases = skipping ? (phases & ~mask) : mask;
            b_basic_only = (phases == PHASE_BASIC);
            b_time_only  = (phases == PHASE_MEASURE);
            continue;
        }

        // -t=N / --times=N
        {
            std::string digits;
            bool isLoopOption = false;
            if (arg.compare(0, 3, "-t=") == 0)            { digits = arg.substr(3);  isLoopOption = true; }
            else if (arg.compare(0, 8, "--times=") == 0)  { digits = arg.substr(8);  isLoopOption = true; }
            else if (arg == "-t" || arg == "--times") {
                ArgumentError(arg + ": needs a value, e.g. -t=30");
                return 1;
            }

            if (isLoopOption) {
                char* stop = nullptr;
                const long parsed = std::strtol(digits.c_str(), &stop, 10);
                if (digits.empty() || (stop && *stop != '\0')) {
                    ArgumentError(arg + ": expected an integer, e.g. -t=30");
                    return 1;
                }
                if (parsed < 1 || parsed > (long)TRX_MAX_LOOP_COUNT) {
                    ArgumentError(arg + ": loop count out of range (1 ~ " + std::to_string(TRX_MAX_LOOP_COUNT) + ")");
                    return 1;
                }
                loop = (unsigned)parsed;
                continue;
            }
        }

        if (!arg.empty() && arg[0] == '-') {
            ArgumentError("unknown option '" + arg + "' (try " + pname + " -h)");
            return 1;
        }

        if (!command.empty()) {
            ArgumentError("more than one test case given ('" + command + "' and '" + arg + "')");
            return 1;
        }
        command = arg;
    }

    if (wantList) {
        Listup(pname);
        return 0;
    }

    if (help) {
        Usage(pname);
        return 0;
    }

    if (phases == 0) {
        ArgumentError("every phase was skipped, nothing to run");
        return 1;
    }

#ifdef DEBUG_LOG
    std::cout << "test stop bit["<<TEST::testStop()<<"] stop["<< stop <<"]" <<std::endl;
#endif

    const int rc = TEST::testAll(command, stop,
                                 (phases & PHASE_BASIC)   != 0,
                                 (phases & PHASE_MEASURE) != 0,
                                 loop,
                                 (phases & PHASE_MEMORY)  != 0,
                                 (phases & PHASE_STRESS)  != 0);

    if (!junitPath.empty()) {
        if (!Results::writeJUnit(junitPath)) {
            // A report that silently failed to write is worse than none: CI
            // would show no failures because it found no file to read.
            ArgumentError("could not write JUnit report to '" + junitPath + "'");
            return 1;
        }
    }

    // The exit status is the only thing CTest and CI look at.
    return (rc == 0) ? 0 : 1;
}


#endif

#endif /* TESTRIXA_DETAIL_CORE_HPP */


