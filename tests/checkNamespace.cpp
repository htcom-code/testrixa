//
//  checkNamespace.cpp
//  testrixa
//
//  Compile-only contract check: including testrixa must not put anything into
//  the consumer's global namespace that they might already be using.
//
//  Built by `make check-namespace` with -fsyntax-only and never linked.
//  Building it IS the assertion.
//
//  This one is not hypothetical. core.hpp carried a global `using namespace
//  std;` -- in a header-only library, so every translation unit that included
//  testrixa got all of std dumped into the global namespace. A consumer with
//  their own `string`, `count` or `vector` at namespace scope failed to
//  compile with "reference to 'string' is ambiguous", and nothing in the
//  library's own tests would ever have noticed: our sources are inside
//  namespace testrixa, where the directive was harmless.
//
//  The directive existed only so six type-trait names could be written
//  unqualified. They are spelled std:: now.
//

//  Every public header belongs in this list. stress.h and thread.h were added
//  to the library without being added here, so for two merge requests the
//  contract covered less than it appeared to.

#include <testrixa/testrixa.h>
#include <testrixa/memory.h>
#include <testrixa/mock.h>
#include <testrixa/options.h>
#include <testrixa/stress.h>
#include <testrixa/thread.h>

// Names a consumer is entitled to own. Each of these was ambiguous before.
struct string { int value; };
struct array  { int value; };
struct byte   { int value; };

template <class T> struct vector { T value; };
template <class T> struct list   { T value; };

int    count = 0;
int    data  = 0;
int    size  = 0;
double distance = 0.0;

int swap(int a, int) { return a; }
int begin(int a)     { return a; }
int end(int a)       { return a; }

// And they must still be usable, not merely declarable.
inline int useThem() {
    string   s{1};
    array    a{2};
    byte     b{3};
    vector<int> v{4};
    list<int>   l{5};
    return s.value + a.value + b.value + v.value + l.value
         + count + data + size + (int)distance
         + swap(0, 0) + begin(0) + end(0);
}
