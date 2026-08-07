//
//  detail/options.hpp
//  command line option registry
//
//  Why a registry instead of the prototype's strcmp ladder
//  ------------------------------------------------------
//  The prototype's parser matched one flag per if-statement inside main().
//  Seven single letters were already taken (-l -h -s -d -b -m -t) and the
//  platform has six more components coming (memory, stress, thread, mock,
//  fixture, reporter, runner). Memory alone wants eight options, and -m is
//  already spoken for by --measure, so it cannot even have its initial.
//
//  Components therefore claim a namespace and register their own options:
//
//      static OptionGroup mem_opts("mem", "Memory Suite");
//      static FlagOption  mem_on(mem_opts, "enable", false, "run the memory phase");
//      static IntOption   mem_bt(mem_opts, "backtrace", 0, 0, 128,
//                                "frames to capture per allocation (0=off)");
//
//  and the user writes --mem.backtrace=16. Parsing, validation and --help come
//  for free, and main() never has to be edited again.
//
//  Registration uses the same self-linking trick the test cases already use
//  (see TEST::list): a constructor pushes onto a static list. The list heads
//  are constant-initialised null pointers, so they are alive before any
//  dynamic initialisation runs and the static init order fiasco does not apply.
//

#ifndef TESTRIXA_DETAIL_OPTIONS_HPP
#define TESTRIXA_DETAIL_OPTIONS_HPP

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>

#include <testrixa/traits.h>

TRX_BEGIN_NAMESPACE

namespace options {

class OptionGroup;

// ---------------------------------------------------------------------------
// OptionBase
// ---------------------------------------------------------------------------
class OptionBase {
public:
    OptionBase(OptionGroup& group, const char* key, const char* help);
    virtual ~OptionBase() {}

    const char*  key()  const { return m_key; }
    const char*  help() const { return m_help; }
    OptionBase*  next() const { return m_next; }
    bool         wasSet() const { return m_set; }

    // value: text after '=', empty when the option was given bare.
    // hasValue: whether an '=' was actually present.
    virtual bool assign(const std::string& value, bool hasValue, std::string& err) = 0;

    virtual std::string syntax()  const = 0;   // e.g. "[=on|off]" or "=<int>"
    virtual std::string current() const = 0;   // value as text, for --help

protected:
    const char* m_key;
    const char* m_help;
    OptionBase* m_next;
    bool        m_set;

    friend class OptionGroup;
};

// ---------------------------------------------------------------------------
// OptionGroup -- one component's namespace
// ---------------------------------------------------------------------------
class OptionGroup {
public:
    OptionGroup(const char* name, const char* description)
    : m_name(name), m_description(description),
      m_head(nullptr), m_tail(nullptr), m_next(nullptr) {
        // append, so --help lists groups in declaration order
        if (s_tail) { s_tail->m_next = this; } else { s_head = this; }
        s_tail = this;
    }

    const char*  name()        const { return m_name; }
    const char*  description() const { return m_description; }
    OptionBase*  options()     const { return m_head; }
    OptionGroup* next()        const { return m_next; }

    static OptionGroup* list() { return s_head; }

    static OptionGroup* find(const std::string& name) {
        for (OptionGroup* g = s_head; g; g = g->m_next) {
            if (name == g->m_name) return g;
        }
        return nullptr;
    }

    OptionBase* findOption(const std::string& key) const {
        for (OptionBase* o = m_head; o; o = o->m_next) {
            if (key == o->key()) return o;
        }
        return nullptr;
    }

    void add(OptionBase* option) {
        option->m_next = nullptr;
        if (m_tail) { m_tail->m_next = option; } else { m_head = option; }
        m_tail = option;
    }

private:
    const char*  m_name;
    const char*  m_description;
    OptionBase*  m_head;
    OptionBase*  m_tail;
    OptionGroup* m_next;

    // Same shape as TEST's statics: with inline variables the initialiser lives
    // in the class, otherwise the declaration does and a TU must define it.
    // Either way these are constant-initialised, so they are already null when
    // the first OptionGroup constructor runs -- no static init order hazard.
#ifndef __cpp_inline_variables
    static TRX_INLINE_VAR OptionGroup* s_head;
    static TRX_INLINE_VAR OptionGroup* s_tail;
#else
    static TRX_INLINE_VAR OptionGroup* s_head = nullptr;
    static TRX_INLINE_VAR OptionGroup* s_tail = nullptr;
#endif
};

inline OptionBase::OptionBase(OptionGroup& group, const char* key, const char* help)
: m_key(key), m_help(help), m_next(nullptr), m_set(false) {
    group.add(this);
}

// ---------------------------------------------------------------------------
// Concrete option types
// ---------------------------------------------------------------------------
class FlagOption : public OptionBase {
public:
    FlagOption(OptionGroup& group, const char* key, bool defaultValue, const char* help)
    : OptionBase(group, key, help), m_value(defaultValue) {}

    bool value() const { return m_value; }
    operator bool() const { return m_value; }

    bool assign(const std::string& value, bool hasValue, std::string& err) override {
        if (!hasValue) { m_value = true; m_set = true; return true; }
        if (value == "on"  || value == "1" || value == "true")  { m_value = true;  m_set = true; return true; }
        if (value == "off" || value == "0" || value == "false") { m_value = false; m_set = true; return true; }
        err = "expected on|off, got '" + value + "'";
        return false;
    }

    std::string syntax()  const override { return "[=on|off]"; }
    std::string current() const override { return m_value ? "on" : "off"; }

private:
    bool m_value;
};

class IntOption : public OptionBase {
public:
    IntOption(OptionGroup& group, const char* key, long defaultValue,
              long minValue, long maxValue, const char* help)
    : OptionBase(group, key, help), m_value(defaultValue),
      m_min(minValue), m_max(maxValue) {}

    long value() const { return m_value; }

    bool assign(const std::string& value, bool hasValue, std::string& err) override {
        if (!hasValue || value.empty()) {
            err = "needs a value, e.g. =" + std::to_string(m_min);
            return false;
        }

        // reject trailing junk -- "30x" must not silently become 30
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || (end && *end != '\0')) {
            err = "expected an integer, got '" + value + "'";
            return false;
        }
        if (parsed < m_min || parsed > m_max) {
            err = "out of range (" + std::to_string(m_min) + " ~ " + std::to_string(m_max) + ")";
            return false;
        }

        m_value = parsed;
        m_set   = true;
        return true;
    }

    std::string syntax()  const override { return "=<int>"; }
    std::string current() const override { return std::to_string(m_value); }

private:
    long m_value;
    long m_min;
    long m_max;
};

class StringOption : public OptionBase {
public:
    // allowed: "a|b|c" to restrict, nullptr for any text
    StringOption(OptionGroup& group, const char* key, const char* defaultValue,
                 const char* allowed, const char* help)
    : OptionBase(group, key, help), m_value(defaultValue ? defaultValue : ""),
      m_allowed(allowed) {}

    const std::string& value() const { return m_value; }

    bool assign(const std::string& value, bool hasValue, std::string& err) override {
        if (!hasValue) { err = "needs a value"; return false; }
        if (m_allowed && !permitted(value)) {
            err = "expected one of " + std::string(m_allowed) + ", got '" + value + "'";
            return false;
        }
        m_value = value;
        m_set   = true;
        return true;
    }

    std::string syntax()  const override {
        return m_allowed ? ("=" + std::string(m_allowed)) : std::string("=<text>");
    }
    std::string current() const override { return m_value; }

private:
    bool permitted(const std::string& candidate) const {
        const std::string all(m_allowed);
        std::string::size_type begin = 0;
        while (begin <= all.size()) {
            const std::string::size_type bar = all.find('|', begin);
            const std::string piece = (bar == std::string::npos)
                                    ? all.substr(begin)
                                    : all.substr(begin, bar - begin);
            if (piece == candidate) return true;
            if (bar == std::string::npos) break;
            begin = bar + 1;
        }
        return false;
    }

    std::string m_value;
    const char* m_allowed;
};

// ---------------------------------------------------------------------------
// Dispatch for --<group>.<key>[=<value>]
// ---------------------------------------------------------------------------
enum class Dispatch {
    NotAnOption,   // does not look like --group.key at all
    Applied,
    Failed         // recognised shape but rejected; message already produced
};

inline Dispatch dispatch(const std::string& argument, std::string& message) {
    if (argument.size() < 3 || argument.compare(0, 2, "--") != 0) {
        return Dispatch::NotAnOption;
    }

    const std::string body = argument.substr(2);
    const std::string::size_type dot = body.find('.');
    if (dot == std::string::npos || dot == 0) return Dispatch::NotAnOption;

    const std::string groupName = body.substr(0, dot);

    // A group name is an identifier. Without this check the dispatcher claims
    // anything with a dot in it -- `--report-junit=/tmp/out.xml` was read as
    // group "report-junit=/tmp/out", and a perfectly good core flag failed with
    // "unknown option group". A real typo like --mme.preset still has an
    // identifier for a group and still gets the precise message below.
    for (char c : groupName) {
        const bool identifier = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                             || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!identifier) return Dispatch::NotAnOption;
    }

    std::string rest = body.substr(dot + 1);
    std::string value;
    bool hasValue = false;
    const std::string::size_type equals = rest.find('=');
    if (equals != std::string::npos) {
        value    = rest.substr(equals + 1);
        rest     = rest.substr(0, equals);
        hasValue = true;
    }

    OptionGroup* group = OptionGroup::find(groupName);
    if (!group) {
        message = "unknown option group '" + groupName + "' in '" + argument + "'";
        return Dispatch::Failed;
    }

    OptionBase* option = group->findOption(rest);
    if (!option) {
        message = "unknown option '" + rest + "' in group '" + groupName + "'";
        return Dispatch::Failed;
    }

    std::string error;
    if (!option->assign(value, hasValue, error)) {
        message = "--" + groupName + "." + rest + ": " + error;
        return Dispatch::Failed;
    }

    return Dispatch::Applied;
}

// ---------------------------------------------------------------------------
// --help rendering for whatever registered
// ---------------------------------------------------------------------------
inline void describe(std::ostream& out) {
    if (!OptionGroup::list()) return;

    for (OptionGroup* group = OptionGroup::list(); group; group = group->next()) {
        out << std::endl << "  " << group->description()
            << " (--" << group->name() << ".*)" << std::endl;

        for (OptionBase* option = group->options(); option; option = option->next()) {
            std::ostringstream spelling;
            spelling << "--" << group->name() << "." << option->key() << option->syntax();
            const std::string spelled = spelling.str();

            // setw alone would run the help text straight into a long spelling
            // ("...|paranoidwhich checkers to run"), so pad by hand and keep a
            // gap even when the column overflows.
            const std::size_t column = 34;
            out << "    " << spelled
                << std::string(spelled.size() < column ? column - spelled.size() : 2, ' ')
                << option->help()
                << " [" << option->current() << "]" << std::endl;
        }
    }
}

} // namespace options

TRX_END_NAMESPACE

#endif // TESTRIXA_DETAIL_OPTIONS_HPP
