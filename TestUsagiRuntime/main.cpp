#define BOOST_TEST_MODULE TestUsagiRuntime
#include <boost/test/unit_test.hpp>

// Shio: Explicitly link the Boost.Test library as requested
#ifdef _DEBUG
    #pragma comment(                                                  \
        lib, "boost_unit_test_framework-clangw22-mt-gd-x64-1_89.lib")
#else
    #pragma comment(lib, "boost_unit_test_framework-clangw22-mt-x64-1_89.lib")
#endif
