//
//  traits.h
//  define macro and type_trait
//
//  Created by htjulia on 7/7/25.
//

#ifndef TESTRIXA_TRAITS_H
#define TESTRIXA_TRAITS_H

#include <cstring>      // strrchr -- the __FILE_NAME__ fallback below uses it
#include <string>

//__has_include c++17 higher
#ifndef __has_include
#   define  __has_include(x) 0
#endif


#if __has_include(<cxxabi.h>)
#   include <cxxabi.h>
#   include <cstdlib>
#   include <memory>
#endif


#if defined(_MSC_VER)
#   include <intrin.h>      // uss _popcnt
#endif


/*
    __has_feature
 */
#ifndef __has_feature
#   define __has_feature(x) 0
#endif

/*
    __has_attribute
 */
#ifndef __has_attribute
#   define __has_attribute(x) 0
#endif

/*
    set compiler
 */
#if defined(__clang__)
#   if !defined(TRX_COMPILER_CLANG)
#       define TRX_COMPILER_CLANG
#   endif
#elif defined(_MSC_VER)
#   if !defined(TRX_COMPILER_MSVC)
        #define TRX_COMPILER_MSVC
#   endif
#elif defined(__GNUC__)
#   if !defined(TRX_COMPILER_GCC)
#       define TRX_COMPILER_GCC
#   endif
#endif



/*
    microsoft's  __cplusplus is fixed 199711L except option /Zc:__cplusplus-
    but this option can modify VisualStuio 2017(15.7) higher
    https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
    https://learn.microsoft.com/ko-kr/cpp/build/reference/zc-cplusplus?view=msvc-170
    https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170
    https://learn.microsoft.com/ko-kr/cpp/preprocessor/predefined-macros?view=msvc-170
 */

// for use c++ version check
#if !defined(TRX_CPP_VER)
#   if defined(_MSC_VER)
#       define TRX_CPP_VER _MSVC_LANG
#   else
#       define TRX_CPP_VER __cplusplus
#   endif
#endif






// namespace testrixa
//
// The prototype used a two-level `julia::test`. It is flattened to a single
// `testrixa` here: upcoming components live in sub-namespaces of their own
// (testrixa::memory, testrixa::options), so an extra `::test` level would only
// push the core one step deeper for no gain.
//
// The macro names no longer start with `_` followed by an uppercase letter --
// that form is reserved for the implementation ([lex.name]/3).
#define TRX_BEGIN_NAMESPACE namespace testrixa {
#define TRX_END_NAMESPACE }

// namespace testrixa::utils
#if TRX_CPP_VER < 201703L
#   define TRX_BEGIN_NAMESPACE_UTILS namespace testrixa { inline namespace utils {
#   define TRX_END_NAMESPACE_UTILS }}
#else
#   define TRX_BEGIN_NAMESPACE_UTILS namespace testrixa::utils {
#   define TRX_END_NAMESPACE_UTILS }
#endif


/* used macro list
     __FILE_NAME__      : only filename
     __FILE__           : full path filename
     __LINE__           : line number
*/
//
// gcc and clang provide __FILE_NAME__ as a built-in, so the fallback below is
// dead code there -- and was therefore never compiled until Windows was tried.
// It needs <cstring> for strrchr, which this header now includes.
//
// MSVC paths can carry either separator: a path written into the project file
// with '/' survives into __FILE__ even though the platform separator is '\'.
// Checking only one of them leaves the full path in the report, so the MSVC
// branch takes whichever appears last.
//
#if !defined(__FILE_NAME__)
#   if defined(_MSC_VER)
#       define TRX_FILE_NAME_SEP_(p) \
            (std::strrchr(p, '\\') > std::strrchr(p, '/') \
                ? std::strrchr(p, '\\') : std::strrchr(p, '/'))
#       define __FILE_NAME__ \
            (TRX_FILE_NAME_SEP_(__FILE__) ? TRX_FILE_NAME_SEP_(__FILE__) + 1 : __FILE__)
#   else
#       define __FILE_NAME__ \
            (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#   endif
#endif




/*
    inline attribute
 */
#if defined(__cplusplus)
#   define TRX_INLINE     inline
#elif defined(_MSC_VER)
#   define TRX_INLINE     __inline
#elif defined(__GNUC__)
#   define TRX_INLINE     __inline__
#else
#   define TRX_INLINE     inline
#endif

#if defined(__cpp_inline_variables)
#   define TRX_INLINE_VAR TRX_INLINE
#else
#   define TRX_INLINE_VAR
#endif








/*
    OS macro
 */
#if defined(__CYGWIN__)
#   define TRX_OS_CYGWIN 1
#elif defined(_WIN32)
#   define TRX_OS_WINDOWS 1
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   if defined(WINAPI_FAMILY_PARTITION)
#       if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#           define TRX_OS_WINDOWS_WIN32 1
#       elif WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#           define TRX_OS_WINDOWS_RT 1
#       endif
#   endif
#   if defined(__MINGW32__)
#       define TRX_OS_MINGW 1
#   endif
#elif defined(__APPLE__)
#   define TRX_OS_APPLE 1
#   include "TargetConditionals.h"
#   if defined(TARGET_OS_MAC)
#       define TRX_OS_MACOSX 1
#       if defined(TARGET_OS_IPHONE)
#           define TRX_OS_IOS 1
#       endif
#   endif
#elif defined(__FreeBSD__)
#   define TRX_OS_FREEBSD 1
#elif defined(__NetBSD__)
#   define TRX_OS_NETBSD 1
#elif defined(__OpenBSD__)
#   define TRX_OS_OPENBSD 1
#elif defined(__DragonFly__)
#   define TRX_OS_DRAGONFLY 1
#elif defined(__linux__)
#   define TRX_OS_LINUX 1
#elif defined(__native_client__)
#   define TRX_OS_NACL 1
#elif defined(__EMSCRIPTEN__)
#   define TRX_OS_EMSCRIPTEN 1
#elif defined(__rtems__)
#   define TRX_OS_RTEMS 1
#elif defined(__Fuchsia__)
#   define TRX_OS_FUCHSIA 1
#elif defined (__SVR4) && defined (__sun)
#   define TRX_OS_SOLARIS 1
#elif defined(__QNX__)
#   define TRX_OS_QNX 1
#elif defined(__MVS__)
#   define TRX_OS_ZOS 1
#elif defined(__hexagon__)
#   define TRX_OS_QURT 1
#endif

/*
 constexpr : since c++11 https://en.cppreference.com/w/cpp/language/constexpr.html
 inline    : since c++11 https://en.cppreference.com/w/cpp/language/inline.html
 */


//#if TRX_CPP_VER < 201100
//#endif





TRX_BEGIN_NAMESPACE

template <typename T> std::string type_name() {
    using TR = typename std::remove_reference<T>::type;
    std::unique_ptr<char, void(*)(void*)> own (
#if !defined(TRX_COMPILER_MSVC)
        abi::__cxa_demangle (typeid(TR).name(), nullptr,
            nullptr, nullptr),
#else
            nullptr,
#endif
            std::free
    );

    std::string r = (own != nullptr ? own.get() : typeid(TR).name());
    
    if (std::is_const<TR>::value)
        r += " const";
    if (std::is_volatile<TR>::value)
        r += " volatile";
    if (std::is_lvalue_reference<T>::value)
        r += " &";
    else if (std::is_rvalue_reference<T>::value)
        r += " &&";

    return r;
}

TRX_END_NAMESPACE


//#define TYPE_NAME_INSTANCE(val)
//    typen_name_ins(val)

#define TRX_TYPE_NAME_INSTANCE(val) \
    ::testrixa::type_name<decltype(val)>()

#define TRX_TUPLE_SIZE(val) \
    std::tuple_size<decltype(val)>::value

#define TRX_TUPLE_FIRST(val) \
    std::get<0>(val)

#define TRX_TUPLE_SECOND(val) \
    std::get<1>(val)

// Short aliases -- see <testrixa/testrixa.h> for the naming contract.
#ifndef TESTRIXA_NO_SHORT_MACROS
#   define TYPE_NAME_INSTANCE(val) TRX_TYPE_NAME_INSTANCE(val)
#   define TUPLE_SIZE(val)         TRX_TUPLE_SIZE(val)
#   define TUPLE_FIRST(val)        TRX_TUPLE_FIRST(val)
#   define TUPLE_SECOND(val)       TRX_TUPLE_SECOND(val)
#endif


#endif // #ifndef TESTRIXA_TRAITS_H
