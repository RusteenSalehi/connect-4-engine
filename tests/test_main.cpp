// The single translation unit that owns doctest's main(). Every other test file only includes
// doctest.h, which keeps the (slow to compile) runner code in one place.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("test harness runs") {
    CHECK(1 + 1 == 2);
}
