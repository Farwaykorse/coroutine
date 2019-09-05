//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include <coroutine/return.h>

#include <concurrency_helper.h>

#include "test.h"
using namespace std;
using namespace coro;

auto concrt_latch_throws_when_negative_from_zero_test() {
    try {

        latch wg{0};
        wg.count_down(2);

    } catch (const underflow_error&) {
        return EXIT_SUCCESS;
    }
    _fail_now_("expect exception", __FILE__, __LINE__);
    return EXIT_FAILURE;
}

#if defined(CMAKE_TEST)
int main(int, char* []) {
    return concrt_latch_throws_when_negative_from_zero_test();
}

#elif __has_include(<CppUnitTest.h>)
#include <CppUnitTest.h>

template <typename T>
using TestClass = ::Microsoft::VisualStudio::CppUnitTestFramework::TestClass<T>;

class concrt_latch_throws_when_negative_from_zero
    : public TestClass<concrt_latch_throws_when_negative_from_zero> {
    TEST_METHOD(test_concrt_latch_throws_when_negative_from_zero) {
        concrt_latch_throws_when_negative_from_zero_test();
    }
};
#endif
