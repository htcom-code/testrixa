//
//  testTEST.cpp
//  testrixa
//
//  Created by htjulia on 2/24/26.
//
#include <tuple>
#include <string>
#include <functional>
#include <testrixa/testrixa.h>


using namespace std;

class TestData {
private:
    std::string m_name;
    std::string m_test;
public:
    TestData() :m_name("default"), m_test(""){}
    
    TestData(const std::string &name) :m_name(name), m_test("") {}
    
    TestData(const TestData& self) :m_name(""), m_test("") {
        m_name = self.m_name;
        m_name += " 11";
    }
    
    virtual ~TestData() {
//        std::cout << m_name << " destroy" << std::endl;
    }
    
public: //method
    std::string name() {
        return m_name;
    }
    
    std::string::size_type length() const noexcept  {
        return m_name.length();
    }

    std::string::size_type length2() const {
        return m_name.length();
    }

    std::string::size_type length3() noexcept {
        return m_name.size();
    }
    
    void append_name(const std::string &val) noexcept {
        m_name.append(val);
    }
    
    void append_revert(std::string &val) {
        val.append(m_name);
    }
    
public: //static method
    template <typename T, enable_if_t<is_integral_v<T>>* = nullptr>
    std::string append(T x) {
        m_test.append(std::to_string(x));
        return m_test;
    }
    
    template <typename T, std::enable_if_t<std::is_null_pointer_v<T>>* = nullptr>
    std::string append(T /*value*/) {   // the type is the whole argument here
        m_test.append("nullptr");
        return m_test;
    }
    
    template <typename T, enable_if_t<is_pointer_v<T>>* = nullptr>
    std::string append(T x) {
        intptr_t data = reinterpret_cast<intptr_t>(x);
        m_test.append("A:");
        m_test.append(std::to_string(data));
        return m_test;
    }


    
    
    static unsigned run_loop(int loop) {
        unsigned val = 0;
        for (int i=1; i<=loop; i++) {
            val += i;
        }
        return val;
    }
    
    
public:
    unsigned run_loop_sum(int loop) {
        unsigned val = 0;
        for (int i=1; i<=loop; i++) {
            val += i;
        }
        return val;
    }
    
    void run_loop_void(int loop) {
        unsigned val = 0;
        for (int i=1; i<=loop; i++) {
            val += i;
        }
        
        loop = (int)val;
    }
    
    bool run_type_comapre(uint8_t a, uint16_t b) {
        if ((b & 0xff00) > 0) return false;
        return a == b;
    }
};

inline std::ostream& operator<<(std::ostream &ss, TestData &d) {
    return ss << d.name();
}


TESTCASE_BEGIN(testTEST)

TESTCASE_BASIC(testTEST) {
    TGS(TYPE_TRAITS) {
        TGS(IS_STRING_COMPARE_V) {
            CHECK_SAME(1, is_string_comp_v<std::string&>,       "std::string& expected string");
            CHECK_SAME(1, is_string_comp_v<std::string&&>,      "std::string&& expected string");
            CHECK_SAME(1, is_string_comp_v<const std::string&>, "const std::string& expected string");
            CHECK_SAME(1, is_string_comp_v<const std::string&&>,"const std::string&& expected string");
            CHECK_SAME(1, is_string_comp_v<const char *>,       "const char* expected string");
            CHECK_SAME(0, is_string_comp_v<const char>,         "const char is not string");
        } TGE(IS_STRING_COMPARE_V)

        TGS(IS_NULLPTR_V) {
            CHECK_SAME(1, is_null_pointer_v<std::nullptr_t>);
            CHECK_SAME(1, is_null_pointer_v<decltype(nullptr)>);
            CHECK_SAME(0, is_null_pointer_v<int*>);
            CHECK_SAME(0, is_pointer_v<decltype(nullptr)>);
            CHECK_SAME(1, is_pointer_v<int*>);
        } TGE(IS_NULLPTR_V)
        
    } TGE(TYPE_TRAITS)
    
    TGS(TESTSTRING_CLASS_TEST) {
        const std::string t("aaa");
        
        TGS(BASIC) {
            TestString test0("1212");
            CHECK_SAME(test0.c_str(), "1212", "TestString Constructor (const char *)");
            CHECK(test0.length() == 4, "TestString Length:", test0.length(), "(data:\"", test0.c_str(), "\")");

            TestString test1 = test0;
            CHECK_SAME(test1.c_str(), "1212", "TestString Constructor (TestString)");
            
            intptr_t address = 10;
            TestString addr(address);
            CHECK_SAME(addr.c_str(), "0x0000000a");

            TestString test2;
            test2.appendFormat("%d %d %d", 1, 2, 3);
            CHECK_SAME(test2.c_str(), "1 2 3");

            TestString test3;
            test3 = "1212";
            CHECK_SAME(test3.c_str(), "1212", "TestString = operator (const char *)");
            
            
            std::string string1("1212");
            std::string string2 = test0;
            
            CHECK_SAME(test0.length(), 4);
            CHECK_SAME(test0.c_str(), string1);
            CHECK_SAME(test0.c_str(), string2);
            CHECK_SAME(test0.c_str(), test1.c_str());
        } TGE(BASIC)

        TGS(FORMAT) {
            TestString fms;
            fms.format("%d", 1);
            CHECK_SAME(fms.c_str(), "1", "fms.format(\"%d\", 1);");
            
            fms.appendFormat("  0x%02x", (uint8_t)255);
            CHECK_SAME(fms.c_str(), "1  0xff", "fail appendFormat");
        } TGE(FORMAT)
        
        TGS(OPERATOR) {
            // += operator
            TestString test0;
            test0 += 1;
            CHECK_SAME(test0.c_str(), "1");
            test0 += (uint8_t)2;
            CHECK_SAME(test0.c_str(), "12");
            test0 += (uint16_t)3;
            CHECK_SAME(test0.c_str(), "123");
            test0 += (float)4.5;
            CHECK_SAME(test0.c_str(), "1234.500000");
            
            std::string source0("Test12233");
            test0.clear();
            test0 += "12";
            CHECK_SAME(test0.c_str(), "12");
            test0 += "34";
            CHECK_SAME(test0.c_str(), "1234");
            
            std::string ret = test0;
            CHECK_SAME(ret, "1234");
            CHECK_SAME(ret, test0.str());
            CHECK_SAME(test0.c_str(), ret);

            TestString test1;
            test1 = "2222";
            CHECK_SAME_FALSE(test0.c_str(), test1.c_str());
            CHECK_SAME_FALSE(test0.str(), test1.str());
            


        } TGE(OPERATOR)

    } TGE(TESTSTRING_CLASS_TEST)
    
    TGS(TIMERESULTS_CLASS_TEST) {
        TimeResults tr0;
        TimeResults tr1;
        TimeResults tr2;

        tr0.setNano(10000 * 1000);
        tr1.setMicro(10000);
        tr2.setTime(0, 0, 0, 1, 0 ,0);
        
        CHECK_SAME(tr0.Hour(),   tr1.Hour());
        CHECK_SAME(tr0.Minute(), tr1.Minute());
        CHECK_SAME(tr0.Second(), tr1.Second());
        CHECK_SAME(tr0.Milli(),  tr1.Milli());
        CHECK_SAME(tr0.Micro(),  tr1.Micro());
        CHECK_SAME(tr0.Nano(),   tr1.Nano());
    } TGE(TIMERESULTS_CLASS_TEST)
    
    TGS(GROUPINFO_CLASS_TEST) {
        
        
    } TGE(GROUPINFO_CLASS_TEST)
    
    TGS(CHECK_SAME_TEST) {
        void *pdata = nullptr;
        int iv = 0;
        int *iv3 = &iv;

        CHECK_SAME(0, NULL);
        CHECK_SAME(iv, *iv3, iv, " ", *iv3);

        CHECK_SAME(nullptr, 0);
        CHECK_SAME(0, nullptr);
        CHECK(nullptr == 0);
        CHECK(0 == nullptr);

        //below code is correct but wanring occure.
//        CHECK(nullptr == NULL);
//        CHECK(NULL == nullptr);
        
        CHECK_SAME(nullptr, NULL);
        CHECK_SAME(NULL, nullptr);

        CHECK_SAME(pdata, NULL);
        CHECK_SAME(NULL, pdata);


        CHECK_SAME(pdata, nullptr);
        CHECK_SAME(nullptr, pdata);

        
        CHECK_SAME(nullptr, nullptr);
        CHECK_SAME(pdata, 0);

        CHECK_SAME_FALSE(pdata, 12121);
        
        CHECK_SAME_FALSE("121212", 1);
    } TGE(CHECK_SAME_TEST)
    
    TGS(CHECK_TEST_0) {
        CHECK(1==1);
        CHECK(true==1);
    }TGE(CHECK_TEST_0)
    
    TGS(CHECK_TEST_1) {
        CHECK_FALSE(1==2);
        CHECK_FALSE(true==0);
    }TGE(CHECK_TEST_1)

    TESTCASE_RETURN
}

template <typename Fn>
inline void tt(Fn&& /*fn*/) {
    std::cout << "aa" << std::endl;
}

template <typename Fn, typename ...Args>
void te(Fn&& fn, Args&&... /*args*/) {
    typename std::remove_const<typename std::invoke_result<Fn&&, Args...>::type>::type ret;
    
    std::cout << type_name<decltype(fn)>() << std::endl;

}


static int Func(int n){ std::cout<<n<<std::endl; return n;};


TESTCASE_MEASURE(testTEST) {
    TGS (std::string_sample) {

        TGS(to_string) {
            // string to_string(int __val);
            TGS(convert_int) {
                auto ret_data = CHECK_TIME_FUNCTION(std::bind<std::string(int)>(std::to_string, std::placeholders::_1), 4);
                CHECK(TUPLE_SIZE(ret_data), 2);
                CHECK_SAME(TUPLE_SECOND(ret_data), "4");
            }TGE(convert_int)
            
            //string to_string(long __val);
            TGS(convert_long) {
                auto ret_data = CHECK_TIME_FUNCTION(std::bind<std::string(long)>(std::to_string, std::placeholders::_1), (long)0);
                CHECK(TUPLE_SIZE(ret_data), 2);
                CHECK_SAME(TUPLE_SECOND(ret_data), "0");
            }TGE(convert_long)

            //ramda test.
            TGS(ramda) {
                auto ret_data0 = CHECK_TIME_FUNCTION([](int i)->std::string{return std::to_string(i);}, 32);
                CHECK(TUPLE_SIZE(ret_data0), 2);
                CHECK_SAME(TUPLE_SECOND(ret_data0), "32");
                
                auto ret_data1 = CHECK_TIME_FUNCTION([](long i)->std::string{return std::to_string(i);}, (long)32);
                CHECK(TUPLE_SIZE(ret_data1), 2);
                CHECK_SAME(TUPLE_SECOND(ret_data1), "32");
            } TGE(ramda)
        } TGE(to_string)
        
        TGS(stoXX) {
            // call normal
            CHECK_SAME(std::stoi("13"), 13);
            
            // test
            // int stoi(const string& __str, size_t* __idx = nullptr, int __base = 10);
            auto ret_data = CHECK_TIME_FUNCTION(std::bind<int(const std::string&, size_t*, int)>(std::stoi, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "12", nullptr, 10);
            CHECK(TUPLE_SIZE(ret_data), 2);
            CHECK_SAME(TUPLE_SECOND(ret_data), 12);
            
            //long stol(const string& __str, size_t* __idx = nullptr, int __base = 10);
            auto ret_data2 = CHECK_TIME_FUNCTION(std::bind<long(const std::string&, size_t*, int)>(std::stol, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "12", nullptr, 10);
            CHECK(TUPLE_SIZE(ret_data2), 2);
            CHECK_SAME(TUPLE_SECOND(ret_data2), 12);

            //unsigned long stoul(const string& __str, size_t* __idx = nullptr, int __base = 10);
            auto ret_data3 = CHECK_TIME_FUNCTION(std::bind<unsigned long(const std::string&, size_t*, int)>(std::stoul, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "12", nullptr, 10);
            CHECK(TUPLE_SIZE(ret_data3), 2);
            CHECK_SAME(TUPLE_SECOND(ret_data3), 12);
        } TGE(stoXX)
        
    } TGE(std::string_sample)
    
    TGS(MEASURE_CLASS_INSTANCE_METHOD) {
        TestData td;
        
        //call instance method return tuple. element 0 is bool, element 1 is return value.
        auto ret = CHECK_TIME_OBJECT(&TestData::run_loop_sum, td, 100);
        CHECK(std::get<0>(ret));

        //expected data
        unsigned aa = static_cast<unsigned>(std::get<1>(ret));
        CHECK_SAME(aa, (101*50));

        
        auto ret2 = CHECK_TIME_OBJECT(&TestData::run_loop_void, td, 100);
        // below remark code is compile error.
        // unsigned bb = static_cast<unsigned>(std::get<1>(ret2));
        CHECK(std::get<0>(ret2));
        
        auto ret3 = CHECK_TIME_OBJECT(&TestData::run_type_comapre, td, (uint8_t)12, (uint16_t)12);
        CHECK(std::get<0>(ret3));
        CHECK(std::get<1>(ret3));

        
    } TGE(MEASURE_CLASS_INSTANCE_METHOD)

    TGS(MEASURE_CLASS_STATIC_METHOD) {
        TestData td;
        
        auto ret = CHECK_TIME_OBJECT_DESC("Static method call", &TestData::run_loop, 100);
        CHECK(std::get<0>(ret));
        unsigned bb = std::get<unsigned>(ret);
        CHECK_SAME(bb, (101*50));
        
        auto ret2 = CHECK_TIME_FUNCTION(&TestData::run_loop, 100);
        CHECK(std::get<0>(ret2));
        bb = std::get<unsigned>(ret2);
        CHECK_SAME(bb, (101*50));
        
    } TGE(MEASURE_CLASS_STATIC_METHOD)
    
    TGS(MEASURE_RAMDA_METHOD) {
        // pass parameter
        int param = 100;
        auto ret = CHECK_TIME_FUNCTION_DESC("Ramda method call", [](int param)->unsigned {
            unsigned val =0;
            for (int i=1; i<=param; i++) {
                val += i;
            }
            return val;
        }, param);

        CHECK(std::get<0>(ret));
        unsigned bb = std::get<unsigned>(ret);
        CHECK_SAME(bb, (101*50));

        // pass local variable param.
        auto ret2 = CHECK_TIME_FUNCTION_DESC("Ramda method call", [&param]()->unsigned {
            unsigned val =0;
            for (int i=1; i<=param; i++) {
                val += i;
            }
            return val;
        });

        CHECK(std::get<0>(ret2));
        bb = std::get<unsigned>(ret2);
        CHECK_SAME(bb, (101*50));
        
        
        auto ret3 = CHECK_TIME_FUNCTION([&param]()->unsigned {
            unsigned val =0;
            for (int i=1; i<=param; i++) {
                val += i;
            }
            return val;
        });
        
        CHECK(std::get<0>(ret3));
        bb = std::get<unsigned>(ret3);
        CHECK_SAME(bb, (101*50));
        
        
    } TGE(MEASURE_RAMDA_METHOD)

    TESTCASE_RETURN
}

TESTCASE_END(testTEST)
